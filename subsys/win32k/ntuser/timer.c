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
/* $Id: timer.c,v 1.34 2004/07/30 09:16:06 weiden Exp $
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * PURPOSE:          Window timers messages
 * FILE:             subsys/win32k/ntuser/timer.c
 * PROGRAMER:        Gunnar
 *                   Thomas Weidenmueller (w3seek@users.sourceforge.net)
 * REVISION HISTORY: 10/04/2003 Implemented System Timers
 *
 */

/* INCLUDES ******************************************************************/

#include <w32k.h>

#define NDEBUG
#include <debug.h>

/* GLOBALS *******************************************************************/

//windows 2000 has room for 32768 window-less timers
#define NUM_WINDOW_LESS_TIMERS   1024

static FAST_MUTEX     Mutex;
static LIST_ENTRY     TimerListHead;
static KTIMER         Timer;
static RTL_BITMAP     WindowLessTimersBitMap;
static PVOID          WindowLessTimersBitMapBuffer;
static ULONG          HintIndex = 0;
static HANDLE        MsgTimerThreadHandle;
static CLIENT_ID     MsgTimerThreadId;


#define IntLockTimerList() \
  ExAcquireFastMutex(&Mutex)

#define IntUnLockTimerList() \
  ExReleaseFastMutex(&Mutex)

/* FUNCTIONS *****************************************************************/


//return true if the new timer became the first entry
//must hold mutex while calling this
BOOL FASTCALL
IntInsertTimerAscendingOrder(PMSG_TIMER_ENTRY NewTimer)
{
  PLIST_ENTRY current;

  current = TimerListHead.Flink;
  while (current != &TimerListHead)
  {
    if (CONTAINING_RECORD(current, MSG_TIMER_ENTRY, ListEntry)->Timeout.QuadPart >=\
        NewTimer->Timeout.QuadPart)
    {
      break;
    }
    current = current->Flink;
  }

  InsertTailList(current, &NewTimer->ListEntry);

  return TimerListHead.Flink == &NewTimer->ListEntry;
}


//must hold mutex while calling this
PMSG_TIMER_ENTRY FASTCALL
IntRemoveTimer(HWND hWnd, UINT_PTR IDEvent, BOOL SysTimer)
{
  PMSG_TIMER_ENTRY MsgTimer;
  PLIST_ENTRY EnumEntry;
  PUSER_MESSAGE_QUEUE MessageQueue = PsGetWin32Thread()->MessageQueue;
  
  //remove timer if already in the queue
  EnumEntry = TimerListHead.Flink;
  while (EnumEntry != &TimerListHead)
  {
    MsgTimer = CONTAINING_RECORD(EnumEntry, MSG_TIMER_ENTRY, ListEntry);
    EnumEntry = EnumEntry->Flink;
      
    if (MsgTimer->Msg.hwnd == hWnd && 
        MsgTimer->Msg.wParam == (WPARAM)IDEvent &&
        MsgTimer->MessageQueue == MessageQueue &&
        (MsgTimer->Msg.message == WM_SYSTIMER) == SysTimer)
    {
      RemoveEntryList(&MsgTimer->ListEntry);
      return MsgTimer;
    }
  }
  
  return NULL;
}


/* 
 * NOTE: It doesn't kill the timer. It just removes them from the list.
 */
VOID FASTCALL
RemoveTimersThread(PUSER_MESSAGE_QUEUE MessageQueue)
{
  PMSG_TIMER_ENTRY MsgTimer;
  PLIST_ENTRY EnumEntry;
  
  if(MessageQueue == NULL)
  {
    return;
  }
  
  IntLockTimerList();
  
  EnumEntry = TimerListHead.Flink;
  while (EnumEntry != &TimerListHead)
  {
    MsgTimer = CONTAINING_RECORD(EnumEntry, MSG_TIMER_ENTRY, ListEntry);
    EnumEntry = EnumEntry->Flink;
    
    if (MsgTimer->MessageQueue == MessageQueue)
    {
      if (MsgTimer->Msg.hwnd == NULL)
      {
        RtlClearBits(&WindowLessTimersBitMap, ((UINT_PTR)MsgTimer->Msg.wParam) - 1, 1);   
      }
      
      RemoveEntryList(&MsgTimer->ListEntry);
      
      IntDereferenceMessageQueue(MsgTimer->MessageQueue);
      
      ExFreePool(MsgTimer);
    }
  }
  
  IntUnLockTimerList();
}


/* 
 * NOTE: It doesn't kill the timer. It just removes them from the list.
 */
VOID FASTCALL
RemoveTimersWindow(HWND Wnd)
{
  PMSG_TIMER_ENTRY MsgTimer;
  PLIST_ENTRY EnumEntry;

  IntLockTimerList();
  
  EnumEntry = TimerListHead.Flink;
  while (EnumEntry != &TimerListHead)
  {
    MsgTimer = CONTAINING_RECORD(EnumEntry, MSG_TIMER_ENTRY, ListEntry);
    EnumEntry = EnumEntry->Flink;
    
    if (MsgTimer->Msg.hwnd == Wnd)
    {
      RemoveEntryList(&MsgTimer->ListEntry);
      
      IntDereferenceMessageQueue(MsgTimer->MessageQueue);
      
      ExFreePool(MsgTimer);
    }
  }
  
  IntUnLockTimerList();
}


UINT_PTR FASTCALL
IntSetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc, BOOL SystemTimer)
{
  PMSG_TIMER_ENTRY MsgTimer = NULL;
  PMSG_TIMER_ENTRY NewTimer;
  LARGE_INTEGER CurrentTime;
  PWINDOW_OBJECT WindowObject;
  UINT_PTR Ret = 0;
 
  KeQuerySystemTime(&CurrentTime);
  IntLockTimerList();
  
  if((hWnd == NULL) && !SystemTimer)
  {
    /* find a free, window-less timer id */
    nIDEvent = RtlFindClearBitsAndSet(&WindowLessTimersBitMap, 1, HintIndex);
    
    if(nIDEvent == (UINT_PTR) -1)
    {
      IntUnLockTimerList();
      return 0;
    }
    
    HintIndex = ++nIDEvent;
  }
  else
  {
    WindowObject = IntGetWindowObject(hWnd);
    if(!WindowObject)
    {
      IntUnLockTimerList();
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return 0;
    }
    
    if(WindowObject->OwnerThread != PsGetCurrentThread())
    {
      IntUnLockTimerList();
      IntReleaseWindowObject(WindowObject);
      SetLastWin32Error(ERROR_ACCESS_DENIED);
      return 0;
    }
    IntReleaseWindowObject(WindowObject);
    
    /* remove timer if already in the queue */
    MsgTimer = IntRemoveTimer(hWnd, nIDEvent, SystemTimer);
  }
  
  #if 1
  
  /* Win NT/2k/XP */
  if(uElapse > 0x7fffffff)
    uElapse = 1;
  
  #else
  
  /* Win Server 2003 */
  if(uElapse > 0x7fffffff)
    uElapse = 0x7fffffff;
  
  #endif
  
  /* Win 2k/XP */
  if(uElapse < 10)
    uElapse = 10;
  
  if(MsgTimer)
  {
    /* modify existing (removed) timer */
    NewTimer = MsgTimer;
    
    NewTimer->Period = uElapse;
    NewTimer->Timeout.QuadPart = CurrentTime.QuadPart + (uElapse * 10000);
    NewTimer->Msg.lParam = (LPARAM)lpTimerFunc;
  }
  else
  {
    /* FIXME: use lookaside? */
    NewTimer = ExAllocatePoolWithTag(PagedPool, sizeof(MSG_TIMER_ENTRY), TAG_TIMER);
    if(!NewTimer)
    {
      IntUnLockTimerList();
      SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
      return 0;
    }
    
    NewTimer->Msg.hwnd = hWnd;
    NewTimer->Msg.message = (SystemTimer ? WM_SYSTIMER : WM_TIMER);
    NewTimer->Msg.wParam = (WPARAM)nIDEvent;
    NewTimer->Msg.lParam = (LPARAM)lpTimerFunc;
    NewTimer->Period = uElapse;
    NewTimer->Timeout.QuadPart = CurrentTime.QuadPart + (uElapse * 10000);
    NewTimer->MessageQueue = PsGetWin32Thread()->MessageQueue;
    
    IntReferenceMessageQueue(NewTimer->MessageQueue);
  }
  
  Ret = nIDEvent; // FIXME - return lpTimerProc if it's not a system timer
  
  if(IntInsertTimerAscendingOrder(NewTimer))
  {
     /* new timer is first in queue and expires first */
     KeSetTimer(&Timer, NewTimer->Timeout, NULL);
  }

  IntUnLockTimerList();

  return Ret;
}


BOOL FASTCALL
IntKillTimer(HWND hWnd, UINT_PTR uIDEvent, BOOL SystemTimer)
{
  PMSG_TIMER_ENTRY MsgTimer;
  PWINDOW_OBJECT WindowObject;
  
  IntLockTimerList();
  
  /* window-less timer? */
  if((hWnd == NULL) && !SystemTimer)
  {
    if(!RtlAreBitsSet(&WindowLessTimersBitMap, uIDEvent - 1, 1))
    {
      IntUnLockTimerList();
      /* bit was not set */
      /* FIXME: set the last error */
      return FALSE;
    }
    RtlClearBits(&WindowLessTimersBitMap, uIDEvent - 1, 1);
  }
  else
  {
    WindowObject = IntGetWindowObject(hWnd);
    if(!WindowObject)
    {
      IntUnLockTimerList(); 
      SetLastWin32Error(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
    }
    if(WindowObject->OwnerThread != PsGetCurrentThread())
    {
      IntUnLockTimerList();
      IntReleaseWindowObject(WindowObject);
      SetLastWin32Error(ERROR_ACCESS_DENIED);
      return FALSE;
    }
    IntReleaseWindowObject(WindowObject);
  }
  
  MsgTimer = IntRemoveTimer(hWnd, uIDEvent, SystemTimer);
  
  IntUnLockTimerList();
  
  if(MsgTimer == NULL)
  {
    /* didn't find timer */
    /* FIXME: set the last error */
    return FALSE;
  }
  
  IntReferenceMessageQueue(MsgTimer->MessageQueue);
  
  /* FIXME: use lookaside? */
  ExFreePool(MsgTimer);
  
  return TRUE;
}

static VOID STDCALL
TimerThreadMain(PVOID StartContext)
{
  NTSTATUS Status;
  LARGE_INTEGER CurrentTime;
  PLIST_ENTRY EnumEntry;
  PMSG_TIMER_ENTRY MsgTimer;
  
  for(;;)
  {
    Status = KeWaitForSingleObject(&Timer,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   NULL);
    if (!NT_SUCCESS(Status))
    {
      DPRINT1("Error waiting in TimerThreadMain\n");
      KEBUGCHECK(0);
    }
    
    IntLockTimerList();
    
    KeQuerySystemTime(&CurrentTime);

    for (EnumEntry = TimerListHead.Flink;
         EnumEntry != &TimerListHead;
         EnumEntry = EnumEntry->Flink)
    {
       MsgTimer = CONTAINING_RECORD(EnumEntry, MSG_TIMER_ENTRY, ListEntry);
       if (CurrentTime.QuadPart < MsgTimer->Timeout.QuadPart)
          break;
    }

    EnumEntry = TimerListHead.Flink;
    while (EnumEntry != &TimerListHead)
    {
      MsgTimer = CONTAINING_RECORD(EnumEntry, MSG_TIMER_ENTRY, ListEntry);
      
      if (CurrentTime.QuadPart >= MsgTimer->Timeout.QuadPart)
      {
        EnumEntry = EnumEntry->Flink;

        RemoveEntryList(&MsgTimer->ListEntry);
        
        /* 
         * FIXME: 1) Find a faster way of getting the thread message queue? (lookup by id is slow)
         */
        
        MsqPostMessage(MsgTimer->MessageQueue, &MsgTimer->Msg, FALSE);
        
        //set up next periodic timeout
        //FIXME: is this calculation really necesary (and correct)? -Gunnar
        do
          {
            MsgTimer->Timeout.QuadPart += (MsgTimer->Period * 10000);
          }
        while (MsgTimer->Timeout.QuadPart <= CurrentTime.QuadPart);
        IntInsertTimerAscendingOrder(MsgTimer);
      }
      else
      {
        break;
      }

    }
    
    
    //set up next timeout from first entry (if any)
    if (!IsListEmpty(&TimerListHead))
    {
      MsgTimer = CONTAINING_RECORD( TimerListHead.Flink, MSG_TIMER_ENTRY, ListEntry);
      KeSetTimer(&Timer, MsgTimer->Timeout, NULL);
    }
    
    IntUnLockTimerList();
  }
}

NTSTATUS FASTCALL
InitTimerImpl(VOID)
{
  NTSTATUS Status;
  ULONG BitmapBytes;
  
  BitmapBytes = ROUND_UP(NUM_WINDOW_LESS_TIMERS, sizeof(ULONG) * 8) / 8;
  
  InitializeListHead(&TimerListHead);
  KeInitializeTimerEx(&Timer, SynchronizationTimer);
  ExInitializeFastMutex(&Mutex);
  
  WindowLessTimersBitMapBuffer = ExAllocatePoolWithTag(PagedPool, BitmapBytes, TAG_TIMERBMP);
  RtlInitializeBitMap(&WindowLessTimersBitMap,
                      WindowLessTimersBitMapBuffer,
                      BitmapBytes * 8);
  
  //yes we need this, since ExAllocatePool isn't supposed to zero out allocated memory
  RtlClearAllBits(&WindowLessTimersBitMap); 
  
  Status = PsCreateSystemThread(&MsgTimerThreadHandle,
                                THREAD_ALL_ACCESS,
                                NULL,
                                NULL,
                                &MsgTimerThreadId,
                                TimerThreadMain,
                                NULL);
  return Status;
}


UINT_PTR
STDCALL
NtUserSetTimer
(
 HWND hWnd,
 UINT_PTR nIDEvent,
 UINT uElapse,
 TIMERPROC lpTimerFunc
)
{
  return IntSetTimer(hWnd, nIDEvent, uElapse, lpTimerFunc, FALSE);
}


BOOL
STDCALL
NtUserKillTimer
(
 HWND hWnd,
 UINT_PTR uIDEvent
)
{
  return IntKillTimer(hWnd, uIDEvent, FALSE);
}


UINT_PTR
STDCALL
NtUserSetSystemTimer(
 HWND hWnd,
 UINT_PTR nIDEvent,
 UINT uElapse,
 TIMERPROC lpTimerFunc
)
{
  return IntSetTimer(hWnd, nIDEvent, uElapse, lpTimerFunc, TRUE);
}


BOOL
STDCALL
NtUserKillSystemTimer(
 HWND hWnd,
 UINT_PTR uIDEvent
)
{
  return IntKillTimer(hWnd, uIDEvent, TRUE);
}

/* EOF */
