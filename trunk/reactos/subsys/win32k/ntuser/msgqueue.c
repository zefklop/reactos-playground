/*
 *  ReactOS W32 Subsystem
 *  Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* $Id: msgqueue.c,v 1.81 2004/04/07 21:12:08 gvg Exp $
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Message queues
 * FILE:             subsys/win32k/ntuser/msgqueue.c
 * PROGRAMER:        Casper S. Hornstrup (chorns@users.sourceforge.net)
 * REVISION HISTORY:
 *       06-06-2001  CSH  Created
 */

/* INCLUDES ******************************************************************/

#include <ddk/ntddk.h>
#include <win32k/win32k.h>
#include <include/msgqueue.h>
#include <include/callback.h>
#include <include/window.h>
#include <include/winpos.h>
#include <include/winsta.h>
#include <include/desktop.h>
#include <include/class.h>
#include <include/object.h>
#include <include/input.h>
#include <include/cursoricon.h>
#include <include/focus.h>
#include <include/caret.h>
#include <include/tags.h>

#define NDEBUG
#include <win32k/debug1.h>
#include <debug.h>

/* GLOBALS *******************************************************************/

#define SYSTEM_MESSAGE_QUEUE_SIZE           (256)

static MSG SystemMessageQueue[SYSTEM_MESSAGE_QUEUE_SIZE];
static ULONG SystemMessageQueueHead = 0;
static ULONG SystemMessageQueueTail = 0;
static ULONG SystemMessageQueueCount = 0;
static ULONG SystemMessageQueueMouseMove = -1;
static KSPIN_LOCK SystemMessageQueueLock;

static ULONG volatile HardwareMessageQueueStamp = 0;
static LIST_ENTRY HardwareMessageQueueHead;
static KMUTEX HardwareMessageQueueLock;

static KEVENT HardwareMessageEvent;

static PAGED_LOOKASIDE_LIST MessageLookasideList;

#define IntLockSystemMessageQueue(OldIrql) \
  KeAcquireSpinLock(&SystemMessageQueueLock, &OldIrql)

#define IntUnLockSystemMessageQueue(OldIrql) \
  KeReleaseSpinLock(&SystemMessageQueueLock, OldIrql)

#define IntUnLockSystemHardwareMessageQueueLock(Wait) \
  KeReleaseMutex(&HardwareMessageQueueLock, Wait)

/* FUNCTIONS *****************************************************************/

/* set some queue bits */
inline VOID MsqSetQueueBits( PUSER_MESSAGE_QUEUE Queue, WORD Bits )
{
    Queue->WakeBits |= Bits;
    Queue->ChangedBits |= Bits;
    if (MsqIsSignaled( Queue )) KeSetEvent(&Queue->NewMessages, IO_NO_INCREMENT, FALSE);
}

/* clear some queue bits */
inline VOID MsqClearQueueBits( PUSER_MESSAGE_QUEUE Queue, WORD Bits )
{
    Queue->WakeBits &= ~Bits;
    Queue->ChangedBits &= ~Bits;
}

VOID FASTCALL
MsqIncPaintCountQueue(PUSER_MESSAGE_QUEUE Queue)
{
  IntLockMessageQueue(Queue);
  Queue->PaintCount++;
  Queue->PaintPosted = TRUE;
  KeSetEvent(&Queue->NewMessages, IO_NO_INCREMENT, FALSE);
  IntUnLockMessageQueue(Queue);
}

VOID FASTCALL
MsqDecPaintCountQueue(PUSER_MESSAGE_QUEUE Queue)
{
  IntLockMessageQueue(Queue);
  Queue->PaintCount--;
  if (Queue->PaintCount == 0)
    {
      Queue->PaintPosted = FALSE;
    }
  IntUnLockMessageQueue(Queue);
}


NTSTATUS FASTCALL
MsqInitializeImpl(VOID)
{
  /*CurrentFocusMessageQueue = NULL;*/
  InitializeListHead(&HardwareMessageQueueHead);
  KeInitializeEvent(&HardwareMessageEvent, NotificationEvent, 0);
  KeInitializeSpinLock(&SystemMessageQueueLock);
  KeInitializeMutex(&HardwareMessageQueueLock, 0);

  ExInitializePagedLookasideList(&MessageLookasideList,
				 NULL,
				 NULL,
				 0,
				 sizeof(USER_MESSAGE),
				 0,
				 256);

  return(STATUS_SUCCESS);
}

VOID FASTCALL
MsqInsertSystemMessage(MSG* Msg, BOOL RemMouseMoveMsg)
{
  LARGE_INTEGER LargeTickCount;
  KIRQL OldIrql;
  ULONG mmov = (ULONG)-1;
  
  KeQueryTickCount(&LargeTickCount);
  Msg->time = LargeTickCount.u.LowPart;

  IntLockSystemMessageQueue(OldIrql);

  /* only insert WM_MOUSEMOVE messages if not already in system message queue */
  if((Msg->message == WM_MOUSEMOVE) && RemMouseMoveMsg)
    mmov = SystemMessageQueueMouseMove;

  if(mmov != (ULONG)-1)
  {
    /* insert message at the queue head */
    while (mmov != SystemMessageQueueHead )
    {
      ULONG prev = mmov ? mmov - 1 : SYSTEM_MESSAGE_QUEUE_SIZE - 1;
      ASSERT(mmov >= 0);
      ASSERT(mmov < SYSTEM_MESSAGE_QUEUE_SIZE);
      SystemMessageQueue[mmov] = SystemMessageQueue[prev];
      mmov = prev;
    }
    SystemMessageQueue[SystemMessageQueueHead] = *Msg;
  }
  else
  {
    if (SystemMessageQueueCount == SYSTEM_MESSAGE_QUEUE_SIZE)
    {
      IntUnLockSystemMessageQueue(OldIrql);
      return;
    }
    SystemMessageQueue[SystemMessageQueueTail] = *Msg;
    if(Msg->message == WM_MOUSEMOVE)
      SystemMessageQueueMouseMove = SystemMessageQueueTail;
    SystemMessageQueueTail =
      (SystemMessageQueueTail + 1) % SYSTEM_MESSAGE_QUEUE_SIZE;
    SystemMessageQueueCount++;
  }
  IntUnLockSystemMessageQueue(OldIrql);
  KeSetEvent(&HardwareMessageEvent, IO_NO_INCREMENT, FALSE);
}

BOOL STATIC FASTCALL
MsqIsDblClk(PWINDOW_OBJECT Window, PUSER_MESSAGE Message, BOOL Remove)
{
  PWINSTATION_OBJECT WinStaObject;
  PSYSTEM_CURSORINFO CurInfo;
  NTSTATUS Status;
  LONG dX, dY;
  BOOL Res;
  
  Status = IntValidateWindowStationHandle(PROCESS_WINDOW_STATION(),
                                          KernelMode,
                                          0,
                                          &WinStaObject);
  if (!NT_SUCCESS(Status))
  {
    return FALSE;
  }
  CurInfo = &WinStaObject->SystemCursor;
  Res = (Message->Msg.hwnd == (HWND)CurInfo->LastClkWnd) && 
        ((Message->Msg.time - CurInfo->LastBtnDown) < CurInfo->DblClickSpeed);
  if(Res)
  {
    
    dX = CurInfo->LastBtnDownX - Message->Msg.pt.x;
    dY = CurInfo->LastBtnDownY - Message->Msg.pt.y;
    if(dX < 0) dX = -dX;
    if(dY < 0) dY = -dY;
    
    Res = (dX <= CurInfo->DblClickWidth) &&
          (dY <= CurInfo->DblClickHeight);
  }

  if(Remove)
  {
    if (Res)
    {
      CurInfo->LastBtnDown = 0;
      CurInfo->LastBtnDownX = Message->Msg.pt.x;
      CurInfo->LastBtnDownY = Message->Msg.pt.y;
      CurInfo->LastClkWnd = NULL;
    }
    else
    {
      CurInfo->LastBtnDownX = Message->Msg.pt.x;
      CurInfo->LastBtnDownY = Message->Msg.pt.y;
      CurInfo->LastClkWnd = (HANDLE)Message->Msg.hwnd;
      CurInfo->LastBtnDown = Message->Msg.time;
    }
  }
  
  ObDereferenceObject(WinStaObject);
  return Res;
}

BOOL STATIC STDCALL
MsqTranslateMouseMessage(HWND hWnd, UINT FilterLow, UINT FilterHigh,
			 PUSER_MESSAGE Message, BOOL Remove, PBOOL Freed, BOOL RemoveWhenFreed,
			 PWINDOW_OBJECT ScopeWin, PUSHORT HitTest,
			 PPOINT ScreenPoint, BOOL FromGlobalQueue)
{
  PUSER_MESSAGE_QUEUE ThreadQueue;
  USHORT Msg = Message->Msg.message;
  PWINDOW_OBJECT CaptureWin, Window = NULL;
  POINT Point;

  if (Msg == WM_LBUTTONDOWN || 
      Msg == WM_MBUTTONDOWN ||
      Msg == WM_RBUTTONDOWN ||
      Msg == WM_XBUTTONDOWN)
  {
    *HitTest = WinPosWindowFromPoint(ScopeWin, !FromGlobalQueue, &Message->Msg.pt, &Window);
    if(Window && FromGlobalQueue && (PsGetWin32Thread()->MessageQueue == Window->MessageQueue))
    {
      *HitTest = IntSendMessage(Window->Self, WM_NCHITTEST, 0, 
                                MAKELONG(Message->Msg.pt.x, Message->Msg.pt.y));
    }
    /*
    **Make sure that we have a window that is not already in focus
    */
    if (Window)
    {
      if(Window->Self != IntGetFocusWindow())
      {
        
        if(*HitTest != (USHORT)HTTRANSPARENT)
        {
          LRESULT Result;
          Result = IntSendMessage(Window->Self, WM_MOUSEACTIVATE, (WPARAM)NtUserGetParent(Window->Self), (LPARAM)MAKELONG(*HitTest, Msg));
          
          switch (Result)
          {
              case MA_NOACTIVATEANDEAT:
                  *Freed = FALSE;
                  IntReleaseWindowObject(Window);
                  return TRUE;
              case MA_NOACTIVATE:
                  break;
              case MA_ACTIVATEANDEAT:
                  IntMouseActivateWindow(Window);
                  IntReleaseWindowObject(Window);
                  *Freed = FALSE;
                  return TRUE;
/*              case MA_ACTIVATE:
              case 0:*/
              default:
                  IntMouseActivateWindow(Window);
                  break;
          }
        }
        else
        {
          IntReleaseWindowObject(Window);
          if(RemoveWhenFreed)
          {
            RemoveEntryList(&Message->ListEntry);
          }
          ExFreePool(Message);
          *Freed = TRUE;
          return(FALSE);
        }
      }
    }

  }
  
  CaptureWin = IntGetCaptureWindow();

  if (CaptureWin == NULL)
  {
    if(Msg == WM_MOUSEWHEEL)
    {
      *HitTest = HTCLIENT;
      if(Window)
        IntReleaseWindowObject(Window);
      Window = IntGetWindowObject(IntGetFocusWindow());
    }
    else
    {
      if(!Window)
      {
        *HitTest = WinPosWindowFromPoint(ScopeWin, !FromGlobalQueue, &Message->Msg.pt, &Window);
        if(Window && FromGlobalQueue && (PsGetWin32Thread()->MessageQueue == Window->MessageQueue))
        {
          *HitTest = IntSendMessage(Window->Self, WM_NCHITTEST, 0, 
                                    MAKELONG(Message->Msg.pt.x, Message->Msg.pt.y));
        }
        if(!Window)
        {
          /* change the cursor on desktop background */
          IntLoadDefaultCursors(TRUE);
        }
      }
    }
  }
  else
  {
    if(Window)
      IntReleaseWindowObject(Window);
    Window = IntGetWindowObject(CaptureWin);
    *HitTest = HTCLIENT;
  }

  if (Window == NULL)
  {
    if(RemoveWhenFreed)
    {
      RemoveEntryList(&Message->ListEntry);
    }
    ExFreePool(Message);
    *Freed = TRUE;
    return(FALSE);
  }
  
  ThreadQueue = PsGetWin32Thread()->MessageQueue;
  if (Window->MessageQueue != ThreadQueue)
  {
    if (! FromGlobalQueue)
    {
      DPRINT("Moving msg between private queues\n");
      /* This message is already queued in a private queue, but we need
       * to move it to a different queue, perhaps because a new window
       * was created which now covers the screen area previously taken
       * by another window. To move it, we need to take it out of the
       * old queue. Note that we're already holding the lock mutexes of the
       * old queue */
      RemoveEntryList(&Message->ListEntry);
      
      /* remove the pointer for the current WM_MOUSEMOVE message in case we
         just removed it */
      if(ThreadQueue->MouseMoveMsg == Message)
      {
        ThreadQueue->MouseMoveMsg = NULL;
      }
    }
    IntLockHardwareMessageQueue(Window->MessageQueue);
    if((Message->Msg.message == WM_MOUSEMOVE) && Window->MessageQueue->MouseMoveMsg)
    {
      /* we do not hold more than one WM_MOUSEMOVE message in the queue */
      Window->MessageQueue->MouseMoveMsg->Msg = Message->Msg;
      if(RemoveWhenFreed && FromGlobalQueue)
      {
        RemoveEntryList(&Message->ListEntry);
      }
      *Freed = TRUE;
    }
    else
    {
      InsertTailList(&Window->MessageQueue->HardwareMessagesListHead,
                     &Message->ListEntry);
      if(Message->Msg.message == WM_MOUSEMOVE)
      {
        Window->MessageQueue->MouseMoveMsg = Message;
      }
      *Freed = FALSE;
    }
    IntUnLockHardwareMessageQueue(Window->MessageQueue);
    if(*Freed)
    {
      ExFreePool(Message);
    }
    KeSetEvent(&Window->MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
    IntReleaseWindowObject(Window);
    return(FALSE);
  }
  
  if (hWnd != NULL && Window->Self != hWnd &&
      !IntIsChildWindow(hWnd, Window->Self))
  {
    IntLockHardwareMessageQueue(Window->MessageQueue);
    if((Message->Msg.message == WM_MOUSEMOVE) && Window->MessageQueue->MouseMoveMsg)
    {
      /* we do not hold more than one WM_MOUSEMOVE message in the queue */
      Window->MessageQueue->MouseMoveMsg->Msg = Message->Msg;
      if(RemoveWhenFreed)
      {
        RemoveEntryList(&Message->ListEntry);
      }
      *Freed = TRUE;
    }
    else
    {
      InsertTailList(&Window->MessageQueue->HardwareMessagesListHead,
                     &Message->ListEntry);
      if(Message->Msg.message == WM_MOUSEMOVE)
      {
        Window->MessageQueue->MouseMoveMsg = Message;
      }
      *Freed = FALSE;
    }
    IntUnLockHardwareMessageQueue(Window->MessageQueue);
    if(*Freed)
    {
      ExFreePool(Message);
    }
    KeSetEvent(&Window->MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
    IntReleaseWindowObject(Window);
    return(FALSE);
  }
  
  switch (Msg)
  {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_XBUTTONDOWN:
    {
      if ((((*HitTest) != HTCLIENT) || 
          (IntGetClassLong(Window, GCL_STYLE, FALSE) & CS_DBLCLKS)) &&
          MsqIsDblClk(Window, Message, Remove))
      {
        Msg += WM_LBUTTONDBLCLK - WM_LBUTTONDOWN;
      }
      break;
    }
  }
  
  *ScreenPoint = Message->Msg.pt;
  Point = Message->Msg.pt;

  if ((*HitTest) != HTCLIENT)
  {
    Msg += WM_NCMOUSEMOVE - WM_MOUSEMOVE;
    if((Msg == WM_NCRBUTTONUP) && 
       (((*HitTest) == HTCAPTION) || ((*HitTest) == HTSYSMENU)))
    {
      Msg = WM_CONTEXTMENU;
      Message->Msg.wParam = (WPARAM)Window->Self;
    }
    else
    {
      Message->Msg.wParam = *HitTest;
    }
  }
  else
  {
    if(Msg != WM_MOUSEWHEEL)
    {
      Point.x -= Window->ClientRect.left;
      Point.y -= Window->ClientRect.top;
    }
  }
  
  /* FIXME: Check message filter. */

  if (Remove)
    {
      Message->Msg.hwnd = Window->Self;
      Message->Msg.message = Msg;
      Message->Msg.lParam = MAKELONG(Point.x, Point.y);
    }
  
  IntReleaseWindowObject(Window);
  *Freed = FALSE;
  return(TRUE);
}

BOOL STDCALL
MsqPeekHardwareMessage(PUSER_MESSAGE_QUEUE MessageQueue, HWND hWnd,
		       UINT FilterLow, UINT FilterHigh, BOOL Remove,
		       PUSER_MESSAGE* Message)
{
  KIRQL OldIrql;
  USHORT HitTest;
  POINT ScreenPoint;
  BOOL Accept, Freed;
  PLIST_ENTRY CurrentEntry;
  PWINDOW_OBJECT DesktopWindow;
  PVOID WaitObjects[2];
  NTSTATUS WaitStatus;

  if( !IntGetScreenDC() || 
      PsGetWin32Thread()->MessageQueue == W32kGetPrimitiveMessageQueue() ) 
  {
    return FALSE;
  }

  DesktopWindow = IntGetWindowObject(IntGetDesktopWindow());

  WaitObjects[1] = &MessageQueue->NewMessages;
  WaitObjects[0] = &HardwareMessageQueueLock;
  do
    {
      WaitStatus = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, UserRequest,
                                            UserMode, TRUE, NULL, NULL);
      while (MsqDispatchOneSentMessage(MessageQueue))
        {
          ;
        }
    }
  while (NT_SUCCESS(WaitStatus) && STATUS_WAIT_0 != WaitStatus);

  /* Process messages in the message queue itself. */
  IntLockHardwareMessageQueue(MessageQueue);
  CurrentEntry = MessageQueue->HardwareMessagesListHead.Flink;
  while (CurrentEntry != &MessageQueue->HardwareMessagesListHead)
    {
      PUSER_MESSAGE Current =
	CONTAINING_RECORD(CurrentEntry, USER_MESSAGE, ListEntry);
      CurrentEntry = CurrentEntry->Flink;
      if (Current->Msg.message >= WM_MOUSEFIRST &&
	  Current->Msg.message <= WM_MOUSELAST)
	{
	  Accept = MsqTranslateMouseMessage(hWnd, FilterLow, FilterHigh,
					    Current, Remove, &Freed, TRUE,
					    DesktopWindow, &HitTest,
					    &ScreenPoint, FALSE);
	  if (Accept)
	    {
	      if (Remove)
		{
		  RemoveEntryList(&Current->ListEntry);
		  if(MessageQueue->MouseMoveMsg == Current)
		  {
		    MessageQueue->MouseMoveMsg = NULL;
		  }
		}
	      IntUnLockHardwareMessageQueue(MessageQueue);
              IntUnLockSystemHardwareMessageQueueLock(FALSE);
	      *Message = Current;
	      IntReleaseWindowObject(DesktopWindow);
	      return(TRUE);
	    }

	}
    }
  IntUnLockHardwareMessageQueue(MessageQueue);

  /* Now try the global queue. */

  /* Transfer all messages from the DPC accessible queue to the main queue. */
  IntLockSystemMessageQueue(OldIrql);
  while (SystemMessageQueueCount > 0)
    {
      PUSER_MESSAGE UserMsg;
      MSG Msg;

      ASSERT(SystemMessageQueueHead < SYSTEM_MESSAGE_QUEUE_SIZE);
      Msg = SystemMessageQueue[SystemMessageQueueHead];
      SystemMessageQueueHead =
	(SystemMessageQueueHead + 1) % SYSTEM_MESSAGE_QUEUE_SIZE;
      SystemMessageQueueCount--;
      IntUnLockSystemMessageQueue(OldIrql);
      UserMsg = ExAllocateFromPagedLookasideList(&MessageLookasideList);
      /* What to do if out of memory? For now we just panic a bit in debug */
      ASSERT(UserMsg);
      UserMsg->Msg = Msg;
      InsertTailList(&HardwareMessageQueueHead, &UserMsg->ListEntry);
      IntLockSystemMessageQueue(OldIrql);
    }
  /*
   * we could set this to -1 conditionally if we find one, but
   * this is more efficient and just as effective.
   */
  SystemMessageQueueMouseMove = -1;
  HardwareMessageQueueStamp++;
  IntUnLockSystemMessageQueue(OldIrql);

  /* Process messages in the queue until we find one to return. */
  CurrentEntry = HardwareMessageQueueHead.Flink;
  while (CurrentEntry != &HardwareMessageQueueHead)
    {
      PUSER_MESSAGE Current =
	CONTAINING_RECORD(CurrentEntry, USER_MESSAGE, ListEntry);
      CurrentEntry = CurrentEntry->Flink;
      RemoveEntryList(&Current->ListEntry);
      HardwareMessageQueueStamp++;
      if (Current->Msg.message >= WM_MOUSEFIRST &&
	  Current->Msg.message <= WM_MOUSELAST)
	{
	  const ULONG ActiveStamp = HardwareMessageQueueStamp;
	  /* Translate the message. */
	  Accept = MsqTranslateMouseMessage(hWnd, FilterLow, FilterHigh,
					    Current, Remove, &Freed, FALSE,
					    DesktopWindow, &HitTest,
					    &ScreenPoint, TRUE);
	  if (Accept)
	    {
	      /* Check for no more messages in the system queue. */
	      IntLockSystemMessageQueue(OldIrql);
	      if (SystemMessageQueueCount == 0 &&
		  IsListEmpty(&HardwareMessageQueueHead))
		{
		  KeClearEvent(&HardwareMessageEvent);
		}
	      IntUnLockSystemMessageQueue(OldIrql);

	      /*
		 If we aren't removing the message then add it to the private
		 queue.
	      */
	      if (!Remove)
		{
                  IntLockHardwareMessageQueue(MessageQueue);
                  if((Current->Msg.message == WM_MOUSEMOVE) && MessageQueue->MouseMoveMsg)
                  {
                    /* we do not hold more than one WM_MOUSEMOVE message in the queue */
                    MessageQueue->MouseMoveMsg->Msg = Current->Msg;
                    *Message = MessageQueue->MouseMoveMsg;
                    ExFreePool(Current);
                  }
                  else
                  {
		    InsertTailList(&MessageQueue->HardwareMessagesListHead,
				   &Current->ListEntry);
		    if(Current->Msg.message == WM_MOUSEMOVE)
		    {
		      MessageQueue->MouseMoveMsg = Current;
		    }
		  }
                  IntUnLockHardwareMessageQueue(MessageQueue);
		}
	      IntUnLockSystemHardwareMessageQueueLock(FALSE);
	      *Message = Current;
	      IntReleaseWindowObject(DesktopWindow);
	      return(TRUE);
	    }
	  /* If the contents of the queue changed then restart processing. */
	  if (HardwareMessageQueueStamp != ActiveStamp)
	    {
	      CurrentEntry = HardwareMessageQueueHead.Flink;
	      continue;
	    }
	}
    }
  IntReleaseWindowObject(DesktopWindow);
  /* Check if the system message queue is now empty. */
  IntLockSystemMessageQueue(OldIrql);
  if (SystemMessageQueueCount == 0 && IsListEmpty(&HardwareMessageQueueHead))
    {
      KeClearEvent(&HardwareMessageEvent);
    }
  IntUnLockSystemMessageQueue(OldIrql);
  IntUnLockSystemHardwareMessageQueueLock(FALSE);

  return(FALSE);
}

VOID STDCALL
MsqPostKeyboardMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  PUSER_MESSAGE_QUEUE FocusMessageQueue;
  MSG Msg;

  DPRINT("MsqPostKeyboardMessage(uMsg 0x%x, wParam 0x%x, lParam 0x%x)\n",
    uMsg, wParam, lParam);

  Msg.hwnd = 0;
  Msg.message = uMsg;
  Msg.wParam = wParam;
  Msg.lParam = lParam;
  /* FIXME: Initialize time and point. */
  
  FocusMessageQueue = IntGetFocusMessageQueue();
  if( !IntGetScreenDC() ) {
    if( W32kGetPrimitiveMessageQueue() ) {
      MsqPostMessage(W32kGetPrimitiveMessageQueue(), &Msg);
    }
  } else {
    if (FocusMessageQueue == NULL)
      {
	DPRINT("No focus message queue\n");
	return;
      }
    
    if (FocusMessageQueue->FocusWindow != (HWND)0)
      {
	Msg.hwnd = FocusMessageQueue->FocusWindow;
        DPRINT("Msg.hwnd = %x\n", Msg.hwnd);
	MsqPostMessage(FocusMessageQueue, &Msg);
      }
    else
      {
	DPRINT("Invalid focus window handle\n");
      }
  }
}

VOID STDCALL
MsqPostHotKeyMessage(PVOID Thread, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
  PWINDOW_OBJECT Window;
  PW32THREAD Win32Thread;
  PW32PROCESS Win32Process;
  MSG Mesg;
  NTSTATUS Status;

  Status = ObReferenceObjectByPointer (Thread,
				       THREAD_ALL_ACCESS,
				       PsThreadType,
				       KernelMode);
  if (!NT_SUCCESS(Status))
    return;

  Win32Thread = ((PETHREAD)Thread)->Win32Thread;
  if (Win32Thread == NULL || Win32Thread->MessageQueue == NULL)
    {
      ObDereferenceObject ((PETHREAD)Thread);
      return;
    }

  Win32Process = ((PETHREAD)Thread)->ThreadsProcess->Win32Process;
  if (Win32Process == NULL || Win32Process->WindowStation == NULL)
    {
      ObDereferenceObject ((PETHREAD)Thread);
      return;
    }

  Status = ObmReferenceObjectByHandle(Win32Process->WindowStation->HandleTable,
                                      hWnd, otWindow, (PVOID*)&Window);
  if (!NT_SUCCESS(Status))
    {
      ObDereferenceObject ((PETHREAD)Thread);
      return;
    }

  Mesg.hwnd = hWnd;
  Mesg.message = WM_HOTKEY;
  Mesg.wParam = wParam;
  Mesg.lParam = lParam;
//      Mesg.pt.x = PsGetWin32Process()->WindowStation->SystemCursor.x;
//      Mesg.pt.y = PsGetWin32Process()->WindowStation->SystemCursor.y;
//      KeQueryTickCount(&LargeTickCount);
//      Mesg.time = LargeTickCount.u.LowPart;
  MsqPostMessage(Window->MessageQueue, &Mesg);
  ObmDereferenceObject(Window);
  ObDereferenceObject (Thread);

//  IntLockMessageQueue(pThread->MessageQueue);
//  InsertHeadList(&pThread->MessageQueue->PostedMessagesListHead,
//		 &Message->ListEntry);
//  KeSetEvent(&pThread->MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
//  IntUnLockMessageQueue(pThread->MessageQueue);

}

PUSER_MESSAGE FASTCALL
MsqCreateMessage(LPMSG Msg)
{
  PUSER_MESSAGE Message;

  Message = ExAllocateFromPagedLookasideList(&MessageLookasideList);
  if (!Message)
    {
      return NULL;
    }

  RtlMoveMemory(&Message->Msg, Msg, sizeof(MSG));

  return Message;
}

VOID FASTCALL
MsqDestroyMessage(PUSER_MESSAGE Message)
{
  ExFreeToPagedLookasideList(&MessageLookasideList, Message);
}

VOID FASTCALL
MsqDispatchSentNotifyMessages(PUSER_MESSAGE_QUEUE MessageQueue)
{
  PLIST_ENTRY ListEntry;
  PUSER_SENT_MESSAGE_NOTIFY Message;

  while (!IsListEmpty(&MessageQueue->SentMessagesListHead))
  {
    IntLockMessageQueue(MessageQueue);
    ListEntry = RemoveHeadList(&MessageQueue->SentMessagesListHead);
    Message = CONTAINING_RECORD(ListEntry, USER_SENT_MESSAGE_NOTIFY,
				ListEntry);
    IntUnLockMessageQueue(MessageQueue);

    IntCallSentMessageCallback(Message->CompletionCallback,
				Message->hWnd,
				Message->Msg,
				Message->CompletionCallbackContext,
				Message->Result);
  }
}

BOOLEAN FASTCALL
MsqPeekSentMessages(PUSER_MESSAGE_QUEUE MessageQueue)
{
  return(!IsListEmpty(&MessageQueue->SentMessagesListHead));
}

BOOLEAN FASTCALL
MsqDispatchOneSentMessage(PUSER_MESSAGE_QUEUE MessageQueue)
{
  PUSER_SENT_MESSAGE Message;
  PLIST_ENTRY Entry;
  LRESULT Result;
  PUSER_SENT_MESSAGE_NOTIFY NotifyMessage;

  IntLockMessageQueue(MessageQueue);
  if (IsListEmpty(&MessageQueue->SentMessagesListHead))
    {
      IntUnLockMessageQueue(MessageQueue);
      return(FALSE);
    }
  Entry = RemoveHeadList(&MessageQueue->SentMessagesListHead);
  Message = CONTAINING_RECORD(Entry, USER_SENT_MESSAGE, ListEntry);
  IntUnLockMessageQueue(MessageQueue);

  /* Call the window procedure. */
  Result = IntSendMessage(Message->Msg.hwnd,
                          Message->Msg.message,
                          Message->Msg.wParam,
                          Message->Msg.lParam);

  /* Let the sender know the result. */
  if (Message->Result != NULL)
    {
      *Message->Result = Result;
    }

  /* Notify the sender. */
  if (Message->CompletionEvent != NULL)
    {
      KeSetEvent(Message->CompletionEvent, IO_NO_INCREMENT, FALSE);
    }

  /* Notify the sender if they specified a callback. */
  if (Message->CompletionCallback != NULL)
    {
      NotifyMessage = ExAllocatePoolWithTag(NonPagedPool,
					    sizeof(USER_SENT_MESSAGE_NOTIFY), TAG_USRMSG);
      NotifyMessage->CompletionCallback =
	Message->CompletionCallback;
      NotifyMessage->CompletionCallbackContext =
	Message->CompletionCallbackContext;
      NotifyMessage->Result = Result;
      NotifyMessage->hWnd = Message->Msg.hwnd;
      NotifyMessage->Msg = Message->Msg.message;
      MsqSendNotifyMessage(Message->CompletionQueue, NotifyMessage);
    }

  ExFreePool(Message);
  return(TRUE);
}

VOID FASTCALL
MsqSendNotifyMessage(PUSER_MESSAGE_QUEUE MessageQueue,
		     PUSER_SENT_MESSAGE_NOTIFY NotifyMessage)
{
  IntLockMessageQueue(MessageQueue);
  InsertTailList(&MessageQueue->NotifyMessagesListHead,
		 &NotifyMessage->ListEntry);
  KeSetEvent(&MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
  IntUnLockMessageQueue(MessageQueue);
}

NTSTATUS FASTCALL
MsqSendMessage(PUSER_MESSAGE_QUEUE MessageQueue,
	       HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam,
               UINT uTimeout, BOOL Block, ULONG_PTR *uResult)
{
  PUSER_SENT_MESSAGE Message;
  KEVENT CompletionEvent;
  NTSTATUS WaitStatus;
  LRESULT Result;
  PUSER_MESSAGE_QUEUE ThreadQueue;
  LARGE_INTEGER Timeout;
  PLIST_ENTRY Entry;

  KeInitializeEvent(&CompletionEvent, NotificationEvent, FALSE);

  Message = ExAllocatePoolWithTag(PagedPool, sizeof(USER_SENT_MESSAGE), TAG_USRMSG);
  Message->Msg.hwnd = Wnd;
  Message->Msg.message = Msg;
  Message->Msg.wParam = wParam;
  Message->Msg.lParam = lParam;
  Message->CompletionEvent = &CompletionEvent;
  Message->Result = &Result;
  Message->CompletionQueue = NULL;
  Message->CompletionCallback = NULL;

  IntLockMessageQueue(MessageQueue);
  InsertTailList(&MessageQueue->SentMessagesListHead, &Message->ListEntry);
  IntUnLockMessageQueue(MessageQueue);
  KeSetEvent(&MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
  
  Timeout.QuadPart = uTimeout * -10000;
  
  ThreadQueue = PsGetWin32Thread()->MessageQueue;
  if(Block)
  {
    /* don't process messages sent to the thread */
    WaitStatus = KeWaitForSingleObject(&CompletionEvent, UserRequest, UserMode, 
                                       FALSE, (uTimeout ? &Timeout : NULL));
  }
  else
  {
    PVOID WaitObjects[2];
    
    WaitObjects[0] = &CompletionEvent;
    WaitObjects[1] = &ThreadQueue->NewMessages;
    do
      {
        WaitStatus = KeWaitForMultipleObjects(2, WaitObjects, WaitAny, UserRequest,
                                              UserMode, FALSE, (uTimeout ? &Timeout : NULL), NULL);
        if(WaitStatus == STATUS_TIMEOUT)
          {
            IntLockMessageQueue(MessageQueue);
            Entry = MessageQueue->SentMessagesListHead.Flink;
            while (Entry != &MessageQueue->SentMessagesListHead)
              {
                if ((PUSER_SENT_MESSAGE) CONTAINING_RECORD(Entry, USER_SENT_MESSAGE, ListEntry)
                    == Message)
                  {
                    Message->CompletionEvent = NULL;
                    break;
                  }
                Entry = Entry->Flink;
              }
            IntUnLockMessageQueue(MessageQueue);
            DbgPrint("MsqSendMessage timed out\n");
            break;
          }
        while (MsqDispatchOneSentMessage(ThreadQueue))
          {
            ;
          }
      }
    while (NT_SUCCESS(WaitStatus) && STATUS_WAIT_0 != WaitStatus);
  }
  
  if(WaitStatus != STATUS_TIMEOUT)
    *uResult = (STATUS_WAIT_0 == WaitStatus ? Result : -1);
  
  return WaitStatus;
}

VOID FASTCALL
MsqPostMessage(PUSER_MESSAGE_QUEUE MessageQueue, MSG* Msg)
{
  PUSER_MESSAGE Message;
  
  if(!(Message = MsqCreateMessage(Msg)))
  {
    return;
  }
  IntLockMessageQueue(MessageQueue);
  InsertTailList(&MessageQueue->PostedMessagesListHead,
		 &Message->ListEntry);
  KeSetEvent(&MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
  IntUnLockMessageQueue(MessageQueue);
}

VOID FASTCALL
MsqPostQuitMessage(PUSER_MESSAGE_QUEUE MessageQueue, ULONG ExitCode)
{
  IntLockMessageQueue(MessageQueue);
  MessageQueue->QuitPosted = TRUE;
  MessageQueue->QuitExitCode = ExitCode;
  KeSetEvent(&MessageQueue->NewMessages, IO_NO_INCREMENT, FALSE);
  IntUnLockMessageQueue(MessageQueue);
}

BOOLEAN STDCALL
MsqFindMessage(IN PUSER_MESSAGE_QUEUE MessageQueue,
	       IN BOOLEAN Hardware,
	       IN BOOLEAN Remove,
	       IN HWND Wnd,
	       IN UINT MsgFilterLow,
	       IN UINT MsgFilterHigh,
	       OUT PUSER_MESSAGE* Message)
{
  PLIST_ENTRY CurrentEntry;
  PUSER_MESSAGE CurrentMessage;
  PLIST_ENTRY ListHead;

  if (Hardware)
    {
      return(MsqPeekHardwareMessage(MessageQueue, Wnd,
				    MsgFilterLow, MsgFilterHigh,
				    Remove, Message));
    }

  IntLockMessageQueue(MessageQueue);
  CurrentEntry = MessageQueue->PostedMessagesListHead.Flink;
  ListHead = &MessageQueue->PostedMessagesListHead;
  while (CurrentEntry != ListHead)
    {
      CurrentMessage = CONTAINING_RECORD(CurrentEntry, USER_MESSAGE,
					 ListEntry);
      if ((Wnd == 0 || Wnd == CurrentMessage->Msg.hwnd) &&
	  ((MsgFilterLow == 0 && MsgFilterHigh == 0) ||
	   (MsgFilterLow <= CurrentMessage->Msg.message &&
	    MsgFilterHigh >= CurrentMessage->Msg.message)))
	{
	  if (Remove)
	    {
	      RemoveEntryList(&CurrentMessage->ListEntry);
	    }
	  IntUnLockMessageQueue(MessageQueue);
	  *Message = CurrentMessage;
	  return(TRUE);
	}
      CurrentEntry = CurrentEntry->Flink;
    }
  IntUnLockMessageQueue(MessageQueue);
  return(FALSE);
}

NTSTATUS FASTCALL
MsqWaitForNewMessages(PUSER_MESSAGE_QUEUE MessageQueue)
{
  PVOID WaitObjects[2] = {&MessageQueue->NewMessages, &HardwareMessageEvent};
  return(KeWaitForMultipleObjects(2,
				  WaitObjects,
				  WaitAny,
				  Executive,
				  UserMode,
				  TRUE,
				  NULL,
				  NULL));
}

BOOL FASTCALL
MsqIsHung(PUSER_MESSAGE_QUEUE MessageQueue)
{
  LARGE_INTEGER LargeTickCount;
  
  KeQueryTickCount(&LargeTickCount);
  return ((LargeTickCount.u.LowPart - MessageQueue->LastMsgRead) > MSQ_HUNG);
}

VOID FASTCALL
MsqInitializeMessageQueue(struct _ETHREAD *Thread, PUSER_MESSAGE_QUEUE MessageQueue)
{
  LARGE_INTEGER LargeTickCount;
  
  MessageQueue->Thread = Thread;
  MessageQueue->CaretInfo = (PTHRDCARETINFO)(MessageQueue + 1);
  InitializeListHead(&MessageQueue->PostedMessagesListHead);
  InitializeListHead(&MessageQueue->SentMessagesListHead);
  InitializeListHead(&MessageQueue->HardwareMessagesListHead);
  KeInitializeMutex(&MessageQueue->HardwareLock, 0);
  ExInitializeFastMutex(&MessageQueue->Lock);
  MessageQueue->QuitPosted = FALSE;
  MessageQueue->QuitExitCode = 0;
  KeInitializeEvent(&MessageQueue->NewMessages, SynchronizationEvent, FALSE);
  KeQueryTickCount(&LargeTickCount);
  MessageQueue->LastMsgRead = LargeTickCount.u.LowPart;
  MessageQueue->FocusWindow = NULL;
  MessageQueue->PaintPosted = FALSE;
  MessageQueue->PaintCount = 0;
}

VOID FASTCALL
MsqFreeMessageQueue(PUSER_MESSAGE_QUEUE MessageQueue)
{
  PLIST_ENTRY CurrentEntry;
  PUSER_MESSAGE CurrentMessage;

  CurrentEntry = MessageQueue->PostedMessagesListHead.Flink;
  while (CurrentEntry != &MessageQueue->PostedMessagesListHead)
    {
      CurrentMessage = CONTAINING_RECORD(CurrentEntry, USER_MESSAGE,
					 ListEntry);
      CurrentEntry = CurrentEntry->Flink;
      MsqDestroyMessage(CurrentMessage);
    }
}

PUSER_MESSAGE_QUEUE FASTCALL
MsqCreateMessageQueue(struct _ETHREAD *Thread)
{
  PUSER_MESSAGE_QUEUE MessageQueue;

  MessageQueue = (PUSER_MESSAGE_QUEUE)ExAllocatePoolWithTag(PagedPool,
				   sizeof(USER_MESSAGE_QUEUE) + sizeof(THRDCARETINFO),
				   TAG_MSGQ);
  RtlZeroMemory(MessageQueue, sizeof(USER_MESSAGE_QUEUE) + sizeof(THRDCARETINFO));
  if (!MessageQueue)
    {
      return NULL;
    }

  MsqInitializeMessageQueue(Thread, MessageQueue);

  return MessageQueue;
}

VOID FASTCALL
MsqDestroyMessageQueue(PUSER_MESSAGE_QUEUE MessageQueue)
{
  MsqFreeMessageQueue(MessageQueue);
  ExFreePool(MessageQueue);
}

PHOOKTABLE FASTCALL
MsqGetHooks(PUSER_MESSAGE_QUEUE Queue)
{
  return Queue->Hooks;
}

VOID FASTCALL
MsqSetHooks(PUSER_MESSAGE_QUEUE Queue, PHOOKTABLE Hooks)
{
  Queue->Hooks = Hooks;
}

LPARAM FASTCALL
MsqSetMessageExtraInfo(LPARAM lParam)
{
  LPARAM Ret;
  PUSER_MESSAGE_QUEUE MessageQueue;
  
  MessageQueue = PsGetWin32Thread()->MessageQueue;
  if(!MessageQueue)
  {
    return 0;
  }
  
  Ret = MessageQueue->ExtraInfo;
  MessageQueue->ExtraInfo = lParam;
  
  return Ret;
}

LPARAM FASTCALL
MsqGetMessageExtraInfo(VOID)
{
  PUSER_MESSAGE_QUEUE MessageQueue;
  
  MessageQueue = PsGetWin32Thread()->MessageQueue;
  if(!MessageQueue)
  {
    return 0;
  }
  
  return MessageQueue->ExtraInfo;
}

/* EOF */
