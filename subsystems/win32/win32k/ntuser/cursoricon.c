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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * We handle two types of cursors/icons:
 * - Private
 *   Loaded without LR_SHARED flag
 *   Private to a process
 *   Can be deleted by calling NtDestroyCursorIcon()
 *   CurIcon->hModule, CurIcon->hRsrc and CurIcon->hGroupRsrc set to NULL
 * - Shared
 *   Loaded with LR_SHARED flag
 *   Possibly shared by multiple processes
 *   Immune to NtDestroyCursorIcon()
 *   CurIcon->hModule, CurIcon->hRsrc and CurIcon->hGroupRsrc are valid
 * There's a M:N relationship between processes and (shared) cursor/icons.
 * A process can have multiple cursor/icons and a cursor/icon can be used
 * by multiple processes. To keep track of this we keep a list of all
 * cursor/icons (CurIconList) and per cursor/icon we keep a list of
 * CURICON_PROCESS structs starting at CurIcon->ProcessList.
 */

#include <win32k.h>

#define NDEBUG
#include <debug.h>

static PAGED_LOOKASIDE_LIST gProcessLookasideList;
static LIST_ENTRY gCurIconList;

SYSTEM_CURSORINFO gSysCursorInfo;

BOOL
InitCursorImpl()
{
    ExInitializePagedLookasideList(&gProcessLookasideList,
                                   NULL,
                                   NULL,
                                   0,
                                   sizeof(CURICON_PROCESS),
                                   TAG_DIB,
                                   128);
    InitializeListHead(&gCurIconList);

     gSysCursorInfo.Enabled = FALSE;
     gSysCursorInfo.ButtonsDown = 0;
     gSysCursorInfo.CursorClipInfo.IsClipped = FALSE;
     gSysCursorInfo.LastBtnDown = 0;
     gSysCursorInfo.CurrentCursorObject = NULL;
     gSysCursorInfo.ShowingCursor = 0;
     gSysCursorInfo.ClickLockActive = FALSE;
     gSysCursorInfo.ClickLockTime = 0;

    return TRUE;
}

PSYSTEM_CURSORINFO
IntGetSysCursorInfo()
{
    return &gSysCursorInfo;
}

/* This function creates a reference for the object! */
PCURICON_OBJECT FASTCALL UserGetCurIconObject(HCURSOR hCurIcon)
{
    PCURICON_OBJECT CurIcon;

    if (!hCurIcon)
    {
        SetLastWin32Error(ERROR_INVALID_CURSOR_HANDLE);
        return NULL;
    }

    CurIcon = (PCURICON_OBJECT)UserReferenceObjectByHandle(hCurIcon, otCursorIcon);
    if (!CurIcon)
    {
        /* we never set ERROR_INVALID_ICON_HANDLE. lets hope noone ever checks for it */
        SetLastWin32Error(ERROR_INVALID_CURSOR_HANDLE);
        return NULL;
    }

    ASSERT(CurIcon->head.cLockObj >= 1);
    return CurIcon;
}

HCURSOR
FASTCALL
UserSetCursor(
    PCURICON_OBJECT NewCursor,
    BOOL ForceChange)
{
    PSYSTEM_CURSORINFO CurInfo;
    PCURICON_OBJECT OldCursor;
    HCURSOR hOldCursor = (HCURSOR)0;
    HDC hdcScreen;
    BOOL bResult;
	
	CurInfo = IntGetSysCursorInfo();

    OldCursor = CurInfo->CurrentCursorObject;
    if (OldCursor)
    {
        hOldCursor = (HCURSOR)OldCursor->Self;
    }

    /* Is the new cursor the same as the old cursor? */
    if (OldCursor == NewCursor)
    {
        /* Nothing to to do in this case */
        return hOldCursor;
    }

    /* Get the screen DC */
    if(!(hdcScreen = IntGetScreenDC()))
    {
        return (HCURSOR)0;
    }

    /* Do we have a new cursor? */
    if (NewCursor)
    {
        UserReferenceObject(NewCursor);

        CurInfo->ShowingCursor = 1;
        CurInfo->CurrentCursorObject = NewCursor;

        /* Call GDI to set the new screen cursor */
        bResult = GreSetPointerShape(hdcScreen,
                                     NewCursor->IconInfo.hbmMask,
                                     NewCursor->IconInfo.hbmColor,
                                     NewCursor->IconInfo.xHotspot,
                                     NewCursor->IconInfo.yHotspot,
                                     gpsi->ptCursor.x,
                                     gpsi->ptCursor.y);


    }
    else
    {
        /* Check if were diplaying a cursor */
        if (OldCursor && CurInfo->ShowingCursor)
        {
            /* Remove the cursor */
            GreMovePointer(hdcScreen, -1, -1);
            DPRINT("Removing pointer!\n");
        }

        CurInfo->CurrentCursorObject = NULL;
        CurInfo->ShowingCursor = 0;
    }

    /* OldCursor is not in use anymore */
    if (OldCursor)
    {
        UserDereferenceObject(OldCursor);
    }

    /* Return handle of the old cursor */
    return hOldCursor;
}

BOOL UserSetCursorPos( INT x, INT y, BOOL SendMouseMoveMsg)
{
    PWINDOW_OBJECT DesktopWindow;
    PSYSTEM_CURSORINFO CurInfo;
    HDC hDC;
    MSG Msg;

    if(!(hDC = IntGetScreenDC()))
    {
        return FALSE;
    }

    CurInfo = IntGetSysCursorInfo();

    DesktopWindow = UserGetDesktopWindow();

    if (DesktopWindow)
    {
        if(x >= DesktopWindow->Wnd->rcClient.right)
            x = DesktopWindow->Wnd->rcClient.right - 1;
        if(y >= DesktopWindow->Wnd->rcClient.bottom)
            y = DesktopWindow->Wnd->rcClient.bottom - 1;
    }

    if(x < 0)
        x = 0;
    if(y < 0)
        y = 0;

    //Clip cursor position
    if(CurInfo->CursorClipInfo.IsClipped)
    {
       if(x >= (LONG)CurInfo->CursorClipInfo.Right)
           x = (LONG)CurInfo->CursorClipInfo.Right - 1;
       if(x < (LONG)CurInfo->CursorClipInfo.Left)
           x = (LONG)CurInfo->CursorClipInfo.Left;
       if(y >= (LONG)CurInfo->CursorClipInfo.Bottom)
           y = (LONG)CurInfo->CursorClipInfo.Bottom - 1;
       if(y < (LONG)CurInfo->CursorClipInfo.Top)
           y = (LONG)CurInfo->CursorClipInfo.Top;
    }

    //Store the new cursor position
    gpsi->ptCursor.x = x;
    gpsi->ptCursor.y = y;

    //Move the mouse pointer
    GreMovePointer(hDC, x, y);

    if (!SendMouseMoveMsg)
       return TRUE;

    //Generate a mouse move message
    Msg.message = WM_MOUSEMOVE;
    Msg.wParam = CurInfo->ButtonsDown;
    Msg.lParam = MAKELPARAM(x, y);
    Msg.pt = gpsi->ptCursor;
    MsqInsertSystemMessage(&Msg);

    return TRUE;
}

/* Called from NtUserCallOneParam with Routine ONEPARAM_ROUTINE_SHOWCURSOR
 * User32 macro NtUserShowCursor */
int UserShowCursor(BOOL bShow)
{
    PSYSTEM_CURSORINFO CurInfo = IntGetSysCursorInfo();
    HDC hdcScreen;

    if (!(hdcScreen = IntGetScreenDC()))
    {
        return 0; /* No mouse */
    }

    if (bShow == FALSE)
    {
        /* Check if were diplaying a cursor */
        if (CurInfo->ShowingCursor == 1)
        {
            /* Remove the pointer */
            GreMovePointer(hdcScreen, -1, -1);
            DPRINT("Removing pointer!\n");
        }
        CurInfo->ShowingCursor--;
    }
    else
    {
        if (CurInfo->ShowingCursor == 0)
        {
            /*Show the pointer*/
            GreMovePointer(hdcScreen, gpsi->ptCursor.x, gpsi->ptCursor.y);
        }
        CurInfo->ShowingCursor++;
    }

    return CurInfo->ShowingCursor;
}

/*
 * We have to register that this object is in use by the current
 * process. The only way to do that seems to be to walk the list
 * of cursor/icon objects starting at W32Process->CursorIconListHead.
 * If the object is already present in the list, we don't have to do
 * anything, if it's not present we add it and inc the ProcessCount
 * in the object. Having to walk the list kind of sucks, but that's
 * life...
 */
static BOOLEAN FASTCALL
ReferenceCurIconByProcess(PCURICON_OBJECT CurIcon)
{
    PPROCESSINFO Win32Process;
    PCURICON_PROCESS Current;

    Win32Process = PsGetCurrentProcessWin32Process();

    LIST_FOR_EACH(Current, &CurIcon->ProcessList, CURICON_PROCESS, ListEntry)
    {
        if (Current->Process == Win32Process)
        {
            /* Already registered for this process */
            return TRUE;
        }
    }

    /* Not registered yet */
    Current = ExAllocateFromPagedLookasideList(&gProcessLookasideList);
    if (NULL == Current)
    {
        return FALSE;
    }
    InsertHeadList(&CurIcon->ProcessList, &Current->ListEntry);
    Current->Process = Win32Process;

    return TRUE;
}

PCURICON_OBJECT FASTCALL
IntFindExistingCurIconObject(HMODULE hModule,
                             HRSRC hRsrc, LONG cx, LONG cy)
{
    PCURICON_OBJECT CurIcon;

    LIST_FOR_EACH(CurIcon, &gCurIconList, CURICON_OBJECT, ListEntry)
    {

        //    if(NT_SUCCESS(UserReferenceObjectByPointer(Object, otCursorIcon))) //<- huh????
//      UserReferenceObject(  CurIcon);
//      {
        if ((CurIcon->hModule == hModule) && (CurIcon->hRsrc == hRsrc))
        {
            if (cx && ((cx != CurIcon->Size.cx) || (cy != CurIcon->Size.cy)))
            {
//               UserDereferenceObject(CurIcon);
                continue;
            }
            if (! ReferenceCurIconByProcess(CurIcon))
            {
                return NULL;
            }

            return CurIcon;
        }
//      }
//      UserDereferenceObject(CurIcon);

    }

    return NULL;
}

PCURICON_OBJECT
IntCreateCurIconHandle()
{
    PCURICON_OBJECT CurIcon;
    HANDLE hCurIcon;

    CurIcon = UserCreateObject(gHandleTable, NULL, &hCurIcon, otCursorIcon, sizeof(CURICON_OBJECT));

    if (!CurIcon)
    {
        SetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    CurIcon->Self = hCurIcon;
    InitializeListHead(&CurIcon->ProcessList);

    if (! ReferenceCurIconByProcess(CurIcon))
    {
        DPRINT1("Failed to add process\n");
        UserDeleteObject(hCurIcon, otCursorIcon);
        UserDereferenceObject(CurIcon);
        return NULL;
    }

    InsertHeadList(&gCurIconList, &CurIcon->ListEntry);

    return CurIcon;
}

BOOLEAN FASTCALL
IntDestroyCurIconObject(PCURICON_OBJECT CurIcon, BOOL ProcessCleanup)
{
    PSYSTEM_CURSORINFO CurInfo;
    HBITMAP bmpMask, bmpColor;
    BOOLEAN Ret;
    PCURICON_PROCESS Current = NULL;
    PPROCESSINFO W32Process = PsGetCurrentProcessWin32Process();

    /* Private objects can only be destroyed by their own process */
    if (NULL == CurIcon->hModule)
    {
        ASSERT(CurIcon->ProcessList.Flink->Flink == &CurIcon->ProcessList);
        Current = CONTAINING_RECORD(CurIcon->ProcessList.Flink, CURICON_PROCESS, ListEntry);
        if (Current->Process != W32Process)
        {
            DPRINT1("Trying to destroy private icon/cursor of another process\n");
            return FALSE;
        }
    }
    else if (! ProcessCleanup)
    {
        DPRINT("Trying to destroy shared icon/cursor\n");
        return FALSE;
    }

    /* Now find this process in the list of processes referencing this object and
       remove it from that list */
    LIST_FOR_EACH(Current, &CurIcon->ProcessList, CURICON_PROCESS, ListEntry)
    {
        if (Current->Process == W32Process)
        {
            RemoveEntryList(&Current->ListEntry);
            break;
        }
    }

    ExFreeToPagedLookasideList(&gProcessLookasideList, Current);

    /* If there are still processes referencing this object we can't destroy it yet */
    if (! IsListEmpty(&CurIcon->ProcessList))
    {
        return TRUE;
    }


    if (! ProcessCleanup)
    {
        RemoveEntryList(&CurIcon->ListEntry);
    }

    CurInfo = IntGetSysCursorInfo();

    if (CurInfo->CurrentCursorObject == CurIcon)
    {
        /* Hide the cursor if we're destroying the current cursor */
        UserSetCursor(NULL, TRUE);
    }

    bmpMask = CurIcon->IconInfo.hbmMask;
    bmpColor = CurIcon->IconInfo.hbmColor;

    /* delete bitmaps */
    if (bmpMask)
    {
        GDIOBJ_SetOwnership(bmpMask, PsGetCurrentProcess());
        GreDeleteObject(bmpMask);
        CurIcon->IconInfo.hbmMask = NULL;
    }
    if (bmpColor)
    {
        GDIOBJ_SetOwnership(bmpColor, PsGetCurrentProcess());
        GreDeleteObject(bmpColor);
        CurIcon->IconInfo.hbmColor = NULL;
    }

    /* We were given a pointer, no need to keep the reference anylonger! */
    UserDereferenceObject(CurIcon);
    Ret = UserDeleteObject(CurIcon->Self, otCursorIcon);

    return Ret;
}

VOID FASTCALL
IntCleanupCurIcons(struct _EPROCESS *Process, PPROCESSINFO Win32Process)
{
    PCURICON_OBJECT CurIcon, tmp;
    PCURICON_PROCESS ProcessData;

    LIST_FOR_EACH_SAFE(CurIcon, tmp, &gCurIconList, CURICON_OBJECT, ListEntry)
    {
        UserReferenceObject(CurIcon);
        //    if(NT_SUCCESS(UserReferenceObjectByPointer(Object, otCursorIcon)))
        {
            LIST_FOR_EACH(ProcessData, &CurIcon->ProcessList, CURICON_PROCESS, ListEntry)
            {
                if (Win32Process == ProcessData->Process)
                {
                    RemoveEntryList(&CurIcon->ListEntry);
                    IntDestroyCurIconObject(CurIcon, TRUE);
                    CurIcon = NULL;
                    break;
                }
            }

//         UserDereferenceObject(Object);
        }

        if (CurIcon)
        {
            UserDereferenceObject(CurIcon);
        }
    }

}

/*
 * @implemented
 */
BOOL
APIENTRY
NtUserGetIconInfo(
    HANDLE hCurIcon,
    PICONINFO IconInfo,
    PUNICODE_STRING lpInstName, // optional
    PUNICODE_STRING lpResName,  // optional
    LPDWORD pbpp,               // optional
    BOOL bInternal)
{
    ICONINFO ii;
    PCURICON_OBJECT CurIcon;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOL Ret = FALSE;
    DWORD colorBpp = 0;

    DPRINT("Enter NtUserGetIconInfo\n");
    UserEnterExclusive();

    if (!IconInfo)
    {
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        goto leave;
    }

    if (!(CurIcon = UserGetCurIconObject(hCurIcon)))
    {
        goto leave;
    }

    RtlCopyMemory(&ii, &CurIcon->IconInfo, sizeof(ICONINFO));

    /* Copy bitmaps */
    ii.hbmMask = BITMAP_CopyBitmap(CurIcon->IconInfo.hbmMask);
    ii.hbmColor = BITMAP_CopyBitmap(CurIcon->IconInfo.hbmColor);

    if (pbpp)
    {
        PSURFACE psurfBmp;

        psurfBmp = SURFACE_LockSurface(CurIcon->IconInfo.hbmColor);
        if (psurfBmp)
        {
            colorBpp = BitsPerFormat(psurfBmp->SurfObj.iBitmapFormat);
            SURFACE_UnlockSurface(psurfBmp);
        }
    }

    /* Copy fields */
    _SEH2_TRY
    {
        ProbeForWrite(IconInfo, sizeof(ICONINFO), 1);
        RtlCopyMemory(IconInfo, &ii, sizeof(ICONINFO));

        if (pbpp)
        {
            ProbeForWrite(pbpp, sizeof(DWORD), 1);
            *pbpp = colorBpp;
        }
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END

    if (NT_SUCCESS(Status))
        Ret = TRUE;
    else
        SetLastNtError(Status);

    UserDereferenceObject(CurIcon);

leave:
    DPRINT("Leave NtUserGetIconInfo, ret=%i\n", Ret);
    UserLeave();

    return Ret;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserGetIconSize(
    HANDLE hCurIcon,
    UINT istepIfAniCur,
    PLONG plcx,       // &size.cx
    PLONG plcy)       // &size.cy
{
    PCURICON_OBJECT CurIcon;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOL bRet = FALSE;

    DPRINT("Enter NtUserGetIconSize\n");
    UserEnterExclusive();

    if (!(CurIcon = UserGetCurIconObject(hCurIcon)))
    {
        goto cleanup;
    }

    _SEH2_TRY
    {
        ProbeForWrite(plcx, sizeof(LONG), 1);
        RtlCopyMemory(plcx, &CurIcon->Size.cx, sizeof(LONG));
        ProbeForWrite(plcy, sizeof(LONG), 1);
        RtlCopyMemory(plcy, &CurIcon->Size.cy, sizeof(LONG));
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END

    if (NT_SUCCESS(Status))
        bRet = TRUE;
    else
        SetLastNtError(Status); // maybe not, test this

    UserDereferenceObject(CurIcon);

cleanup:
    DPRINT("Leave NtUserGetIconSize, ret=%i\n", bRet);
    UserLeave();
    return bRet;
}


/*
 * @unimplemented
 */
DWORD
APIENTRY
NtUserGetCursorFrameInfo(
    DWORD Unknown0,
    DWORD Unknown1,
    DWORD Unknown2,
    DWORD Unknown3)
{
    UNIMPLEMENTED

    return 0;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserGetCursorInfo(
    PCURSORINFO pci)
{
    CURSORINFO SafeCi;
    PSYSTEM_CURSORINFO CurInfo;
    NTSTATUS Status = STATUS_SUCCESS;
    PCURICON_OBJECT CurIcon;
    BOOL Ret = FALSE;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserGetCursorInfo\n");
    UserEnterExclusive();

    CurInfo = IntGetSysCursorInfo();
    CurIcon = (PCURICON_OBJECT)CurInfo->CurrentCursorObject;

    SafeCi.cbSize = sizeof(CURSORINFO);
    SafeCi.flags = ((CurInfo->ShowingCursor && CurIcon) ? CURSOR_SHOWING : 0);
    SafeCi.hCursor = (CurIcon ? (HCURSOR)CurIcon->Self : (HCURSOR)0);

    SafeCi.ptScreenPos = gpsi->ptCursor;

    _SEH2_TRY
    {
        if (pci->cbSize == sizeof(CURSORINFO))
        {
            ProbeForWrite(pci, sizeof(CURSORINFO), 1);
            RtlCopyMemory(pci, &SafeCi, sizeof(CURSORINFO));
            Ret = TRUE;
        }
        else
        {
            SetLastWin32Error(ERROR_INVALID_PARAMETER);
        }
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END;
    if (!NT_SUCCESS(Status))
    {
        SetLastNtError(Status);
    }

    RETURN(Ret);

CLEANUP:
    DPRINT("Leave NtUserGetCursorInfo, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserClipCursor(
    RECTL *UnsafeRect)
{
    /* FIXME - check if process has WINSTA_WRITEATTRIBUTES */
    PSYSTEM_CURSORINFO CurInfo;
    RECTL Rect;
    PWINDOW_OBJECT DesktopWindow = NULL;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserClipCursor\n");
    UserEnterExclusive();

    if (NULL != UnsafeRect && ! NT_SUCCESS(MmCopyFromCaller(&Rect, UnsafeRect, sizeof(RECT))))
    {
        SetLastWin32Error(ERROR_INVALID_PARAMETER);
        RETURN(FALSE);
    }

    CurInfo = IntGetSysCursorInfo();

    DesktopWindow = UserGetDesktopWindow();

    if ((Rect.right > Rect.left) && (Rect.bottom > Rect.top)
            && DesktopWindow && UnsafeRect != NULL)
    {

        CurInfo->CursorClipInfo.IsClipped = TRUE;
        CurInfo->CursorClipInfo.Left = max(Rect.left, DesktopWindow->Wnd->rcWindow.left);
        CurInfo->CursorClipInfo.Top = max(Rect.top, DesktopWindow->Wnd->rcWindow.top);
        CurInfo->CursorClipInfo.Right = min(Rect.right, DesktopWindow->Wnd->rcWindow.right);
        CurInfo->CursorClipInfo.Bottom = min(Rect.bottom, DesktopWindow->Wnd->rcWindow.bottom);

        UserSetCursorPos(gpsi->ptCursor.x, gpsi->ptCursor.y, FALSE);

        RETURN(TRUE);
    }

    CurInfo->CursorClipInfo.IsClipped = FALSE;
    RETURN(TRUE);

CLEANUP:
    DPRINT("Leave NtUserClipCursor, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserDestroyCursor(
    HANDLE hCurIcon,
    DWORD Unknown)
{
    PCURICON_OBJECT CurIcon;
    BOOL ret;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserDestroyCursorIcon\n");
    UserEnterExclusive();

    if (!(CurIcon = UserGetCurIconObject(hCurIcon)))
    {
        RETURN(FALSE);
    }

    ret = IntDestroyCurIconObject(CurIcon, FALSE);
    /* Note: IntDestroyCurIconObject will remove our reference for us! */

    RETURN(ret);

CLEANUP:
    DPRINT("Leave NtUserDestroyCursorIcon, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
HICON
APIENTRY
NtUserFindExistingCursorIcon(
    HMODULE hModule,
    HRSRC hRsrc,
    LONG cx,
    LONG cy)
{
    PCURICON_OBJECT CurIcon;
    HANDLE Ret = (HANDLE)0;
    DECLARE_RETURN(HICON);

    DPRINT("Enter NtUserFindExistingCursorIcon\n");
    UserEnterExclusive();

    CurIcon = IntFindExistingCurIconObject(hModule, hRsrc, cx, cy);
    if (CurIcon)
    {
        Ret = CurIcon->Self;

//      IntReleaseCurIconObject(CurIcon);//faxme: is this correct? does IntFindExistingCurIconObject add a ref?
        RETURN(Ret);
    }

    SetLastWin32Error(ERROR_INVALID_CURSOR_HANDLE);
    RETURN((HANDLE)0);

CLEANUP:
    DPRINT("Leave NtUserFindExistingCursorIcon, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserGetClipCursor(
    RECTL *lpRect)
{
    /* FIXME - check if process has WINSTA_READATTRIBUTES */
    PSYSTEM_CURSORINFO CurInfo;
    RECTL Rect;
    NTSTATUS Status;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserGetClipCursor\n");
    UserEnterExclusive();

    if (!lpRect)
        RETURN(FALSE);

    CurInfo = IntGetSysCursorInfo();
    if (CurInfo->CursorClipInfo.IsClipped)
    {
        Rect.left = CurInfo->CursorClipInfo.Left;
        Rect.top = CurInfo->CursorClipInfo.Top;
        Rect.right = CurInfo->CursorClipInfo.Right;
        Rect.bottom = CurInfo->CursorClipInfo.Bottom;
    }
    else
    {
        Rect.left = 0;
        Rect.top = 0;
        Rect.right = UserGetSystemMetrics(SM_CXSCREEN);
        Rect.bottom = UserGetSystemMetrics(SM_CYSCREEN);
    }

    Status = MmCopyToCaller(lpRect, &Rect, sizeof(RECT));
    if (!NT_SUCCESS(Status))
    {
        SetLastNtError(Status);
        RETURN(FALSE);
    }

    RETURN(TRUE);

CLEANUP:
    DPRINT("Leave NtUserGetClipCursor, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
HCURSOR
APIENTRY
NtUserSetCursor(
    HCURSOR hCursor)
{
    PCURICON_OBJECT CurIcon;
    HICON OldCursor;
    DECLARE_RETURN(HCURSOR);

    DPRINT("Enter NtUserSetCursor\n");
    UserEnterExclusive();

    if (hCursor)
    {
        if (!(CurIcon = UserGetCurIconObject(hCursor)))
        {
            RETURN(NULL);
        }
    }
    else
    {
        CurIcon = NULL;
    }

    OldCursor = UserSetCursor(CurIcon, FALSE);

    if (CurIcon)
    {
        UserDereferenceObject(CurIcon);
    }

    RETURN(OldCursor);

CLEANUP:
    DPRINT("Leave NtUserSetCursor, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
BOOL
APIENTRY
NtUserSetCursorContents(
    HANDLE hCurIcon,
    PICONINFO UnsafeIconInfo)
{
    PCURICON_OBJECT CurIcon;
    ICONINFO IconInfo;
    PSURFACE psurfBmp;
    NTSTATUS Status;
    BOOL Ret = FALSE;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserSetCursorContents\n");
    UserEnterExclusive();

    if (!(CurIcon = UserGetCurIconObject(hCurIcon)))
    {
        RETURN(FALSE);
    }

    /* Copy fields */
    Status = MmCopyFromCaller(&IconInfo, UnsafeIconInfo, sizeof(ICONINFO));
    if (!NT_SUCCESS(Status))
    {
        SetLastNtError(Status);
        goto done;
    }

    /* Delete old bitmaps */
    if ((CurIcon->IconInfo.hbmColor)
		&& (CurIcon->IconInfo.hbmColor != IconInfo.hbmColor))
    {
        GreDeleteObject(CurIcon->IconInfo.hbmColor);
    }
    if ((CurIcon->IconInfo.hbmMask)
		&& (CurIcon->IconInfo.hbmMask != IconInfo.hbmMask))
    {
        GreDeleteObject(CurIcon->IconInfo.hbmMask);
    }

    /* Copy new IconInfo field */
    CurIcon->IconInfo = IconInfo;

    psurfBmp = SURFACE_LockSurface(CurIcon->IconInfo.hbmColor);
    if (psurfBmp)
    {
        CurIcon->Size.cx = psurfBmp->SurfObj.sizlBitmap.cx;
        CurIcon->Size.cy = psurfBmp->SurfObj.sizlBitmap.cy;
        SURFACE_UnlockSurface(psurfBmp);
        GDIOBJ_SetOwnership(CurIcon->IconInfo.hbmColor, NULL);
    }
    else
    {
        psurfBmp = SURFACE_LockSurface(CurIcon->IconInfo.hbmMask);
        if (!psurfBmp)
            goto done;

        CurIcon->Size.cx = psurfBmp->SurfObj.sizlBitmap.cx;
        CurIcon->Size.cy = psurfBmp->SurfObj.sizlBitmap.cy / 2;

        SURFACE_UnlockSurface(psurfBmp);
        GDIOBJ_SetOwnership(CurIcon->IconInfo.hbmMask, NULL);
    }

    Ret = TRUE;

done:

    if (CurIcon)
    {
        UserDereferenceObject(CurIcon);
    }
    RETURN(Ret);

CLEANUP:
    DPRINT("Leave NtUserSetCursorContents, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}


/*
 * @implemented
 */
#if 0
BOOL
APIENTRY
NtUserSetCursorIconData(
    HANDLE Handle,
    HMODULE hModule,
    PUNICODE_STRING pstrResName,
    PICONINFO pIconInfo)
{
    PCURICON_OBJECT CurIcon;
    PSURFACE psurfBmp;
    NTSTATUS Status = STATUS_SUCCESS;
    BOOL Ret = FALSE;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserSetCursorIconData\n");
    UserEnterExclusive();

    if (!(CurIcon = UserGetCurIconObject(Handle)))
    {
        RETURN(FALSE);
    }

    CurIcon->hModule = hModule;
    CurIcon->hRsrc = NULL; //hRsrc;
    CurIcon->hGroupRsrc = NULL; //hGroupRsrc;

    _SEH2_TRY
    {
        ProbeForRead(pIconInfo, sizeof(ICONINFO), 1);
        RtlCopyMemory(&CurIcon->IconInfo, pIconInfo, sizeof(ICONINFO));

        CurIcon->IconInfo.hbmMask = BITMAP_CopyBitmap(pIconInfo->hbmMask);
        CurIcon->IconInfo.hbmColor = BITMAP_CopyBitmap(pIconInfo->hbmColor);

        if (CurIcon->IconInfo.hbmColor)
        {
            if ((psurfBmp = SURFACE_LockSurface(CurIcon->IconInfo.hbmColor)))
            {
                CurIcon->Size.cx = psurfBmp->SurfObj.sizlBitmap.cx;
                CurIcon->Size.cy = psurfBmp->SurfObj.sizlBitmap.cy;
                SURFACE_UnlockSurface(psurfBmp);
                GDIOBJ_SetOwnership(GdiHandleTable, CurIcon->IconInfo.hbmMask, NULL);
            }
        }
        if (CurIcon->IconInfo.hbmMask)
        {
            if (CurIcon->IconInfo.hbmColor == NULL)
            {
                if ((psurfBmp = SURFACE_LockSurface(CurIcon->IconInfo.hbmMask)))
                {
                    CurIcon->Size.cx = psurfBmp->SurfObj.sizlBitmap.cx;
                    CurIcon->Size.cy = psurfBmp->SurfObj.sizlBitmap.cy;
                    SURFACE_UnlockSurface(psurfBmp);
                }
            }
            GDIOBJ_SetOwnership(GdiHandleTable, CurIcon->IconInfo.hbmMask, NULL);
        }
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        Status = _SEH2_GetExceptionCode();
    }
    _SEH2_END

    if (!NT_SUCCESS(Status))
        SetLastNtError(Status);
    else
        Ret = TRUE;

    UserDereferenceObject(CurIcon);
    RETURN(Ret);

CLEANUP:
    DPRINT("Leave NtUserSetCursorIconData, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}
#else
BOOL
APIENTRY
NtUserSetCursorIconData(
    HANDLE hCurIcon,
    PBOOL fIcon,
    POINT *Hotspot,
    HMODULE hModule,
    HRSRC hRsrc,
    HRSRC hGroupRsrc)
{
    PCURICON_OBJECT CurIcon;
    NTSTATUS Status;
    POINT SafeHotspot;
    BOOL Ret = FALSE;
    DECLARE_RETURN(BOOL);

    DPRINT("Enter NtUserSetCursorIconData\n");
    UserEnterExclusive();

    if (!(CurIcon = UserGetCurIconObject(hCurIcon)))
    {
        RETURN(FALSE);
    }

    CurIcon->hModule = hModule;
    CurIcon->hRsrc = hRsrc;
    CurIcon->hGroupRsrc = hGroupRsrc;

    /* Copy fields */
    if (fIcon)
    {
        Status = MmCopyFromCaller(&CurIcon->IconInfo.fIcon, fIcon, sizeof(BOOL));
        if (!NT_SUCCESS(Status))
        {
            SetLastNtError(Status);
            goto done;
        }
    }
    else
    {
        if (!Hotspot)
            Ret = TRUE;
    }

    if (Hotspot)
    {
        Status = MmCopyFromCaller(&SafeHotspot, Hotspot, sizeof(POINT));
        if (NT_SUCCESS(Status))
        {
            CurIcon->IconInfo.xHotspot = SafeHotspot.x;
            CurIcon->IconInfo.yHotspot = SafeHotspot.y;

            Ret = TRUE;
        }
        else
            SetLastNtError(Status);
    }

    if (!fIcon && !Hotspot)
    {
        Ret = TRUE;
    }

done:
	if(Ret)
	{
		/* This icon is shared now */
		GDIOBJ_SetOwnership(CurIcon->IconInfo.hbmMask, NULL);
		if(CurIcon->IconInfo.hbmColor)
		{
			GDIOBJ_SetOwnership(CurIcon->IconInfo.hbmColor, NULL);
		}
	}
    UserDereferenceObject(CurIcon);
    RETURN(Ret);


CLEANUP:
    DPRINT("Leave NtUserSetCursorIconData, ret=%i\n",_ret_);
    UserLeave();
    END_CLEANUP;
}
#endif

/*
 * @unimplemented
 */
BOOL
APIENTRY
NtUserSetSystemCursor(
    HCURSOR hcur,
    DWORD id)
{
    return FALSE;
}

/* Mostly inspired from wine code */
BOOL
UserDrawIconEx(
    HDC hDc,
    INT xLeft,
    INT yTop,
    PCURICON_OBJECT pIcon,
    INT cxWidth,
    INT cyHeight,
    UINT istepIfAniCur,
    HBRUSH hbrFlickerFreeDraw,
    UINT diFlags)
{
    BOOL Ret = FALSE;
    HBITMAP hbmMask, hbmColor;
    BITMAP bmpColor, bm;
    BOOL DoFlickerFree;
    INT iOldBkColor = 0, iOldTxtColor = 0;

    HDC hMemDC, hDestDC = hDc;
    HGDIOBJ hOldOffBrush = 0;
    HGDIOBJ hOldOffBmp = 0;
    HBITMAP hTmpBmp = 0, hOffBmp = 0;
    BOOL bAlpha = FALSE;
    INT x=xLeft, y=yTop;

    hbmMask = pIcon->IconInfo.hbmMask;
    hbmColor = pIcon->IconInfo.hbmColor;

    if (istepIfAniCur)
        DPRINT1("NtUserDrawIconEx: istepIfAniCur is not supported!\n");

    if (!hbmMask || !IntGdiGetObject(hbmMask, sizeof(BITMAP), (PVOID)&bm))
    {
        return FALSE;
    }

    if (hbmColor && !IntGdiGetObject(hbmColor, sizeof(BITMAP), (PVOID)&bmpColor))
    {
        return FALSE;
    }

    if(!(hMemDC = NtGdiCreateCompatibleDC(hDc)))
    {
        DPRINT1("NtGdiCreateCompatibleDC failed!\n");
        return FALSE;
    }

    /* Check for alpha */
    if (hbmColor
            && (bmpColor.bmBitsPixel == 32)
            && (diFlags & DI_IMAGE))
    {
        SURFACE *psurfOff = NULL;
        PFN_DIB_GetPixel fnSource_GetPixel = NULL;
        INT i, j;

        /* In order to correctly display 32 bit icons Windows first scans the image,
           because information about transparency is not stored in any image's headers */
        psurfOff = SURFACE_LockSurface(hbmColor);
        if (psurfOff)
        {
            fnSource_GetPixel = DibFunctionsForBitmapFormat[psurfOff->SurfObj.iBitmapFormat].DIB_GetPixel;
            if (fnSource_GetPixel)
            {
                for (i = 0; i < psurfOff->SurfObj.sizlBitmap.cx; i++)
                {
                    for (j = 0; j < psurfOff->SurfObj.sizlBitmap.cy; j++)
                    {
                        bAlpha = ((BYTE)(fnSource_GetPixel(&psurfOff->SurfObj, i, j) >> 24) & 0xff);
                        if (bAlpha)
                            break;
                    }
                    if (bAlpha)
                        break;
                }
            }
            SURFACE_UnlockSurface(psurfOff);
        }
    }

    if (!cxWidth)
        cxWidth = ((diFlags & DI_DEFAULTSIZE) ?
                   UserGetSystemMetrics(SM_CXICON) : pIcon->Size.cx);

    if (!cyHeight)
        cyHeight = ((diFlags & DI_DEFAULTSIZE) ?
                    UserGetSystemMetrics(SM_CYICON) : pIcon->Size.cy);

    DoFlickerFree = (hbrFlickerFreeDraw &&
                     (GDI_HANDLE_GET_TYPE(hbrFlickerFreeDraw) == GDI_OBJECT_TYPE_BRUSH));

    if (DoFlickerFree)
    {
        hDestDC = NtGdiCreateCompatibleDC(hDc);
        if(!hDestDC)
        {
            DPRINT1("NtGdiCreateCompatibleDC failed!\n");
            Ret = FALSE;
            goto Cleanup ;
        }
        hOffBmp = NtGdiCreateCompatibleBitmap(hDc, cxWidth, cyHeight);
        if(!hOffBmp)
        {
            DPRINT1("NtGdiCreateCompatibleBitmap failed!\n");
            goto Cleanup ;
        }
        hOldOffBmp = NtGdiSelectBitmap(hDestDC, hOffBmp);
        hOldOffBrush = NtGdiSelectBrush(hDestDC, hbrFlickerFreeDraw);
        NtGdiPatBlt(hDestDC, 0, 0, cxWidth, cyHeight, PATCOPY);
        NtGdiSelectBrush(hDestDC, hOldOffBrush);
        x=y=0;
    }

    /* Set Background/foreground colors */
    iOldTxtColor = IntGdiSetTextColor(hDc, 0); //black
    iOldBkColor = IntGdiSetBkColor(hDc, 0x00FFFFFF); //white

	if(bAlpha && (diFlags & DI_IMAGE))
	{
		BLENDFUNCTION pixelblend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        DWORD Pixel;
        BYTE Red, Green, Blue, Alpha;
        DWORD Count = 0;
        INT i, j;
        PSURFACE psurf;
        PBYTE pBits ;
        HBITMAP hMemBmp = NULL;

        pBits = ExAllocatePoolWithTag(PagedPool,
                                      bmpColor.bmWidthBytes * abs(bmpColor.bmHeight),
                                      TAG_BITMAP);
        if (pBits == NULL)
        {
            Ret = FALSE;
            goto CleanupAlpha;
        }

        hMemBmp = BITMAP_CopyBitmap(hbmColor);
        if(!hMemBmp)
        {
            DPRINT1("BITMAP_CopyBitmap failed!");
            goto CleanupAlpha;
        }

        psurf = SURFACE_LockSurface(hMemBmp);
        if(!psurf)
        {
            DPRINT1("SURFACE_LockSurface failed!\n");
            goto CleanupAlpha;
        }
        /* get color bits */
        IntGetBitmapBits(psurf,
                         bmpColor.bmWidthBytes * abs(bmpColor.bmHeight),
                         pBits);

        /* premultiply with the alpha channel value */
        for (i = 0; i < abs(bmpColor.bmHeight); i++)
        {
			Count = i*bmpColor.bmWidthBytes;
            for (j = 0; j < bmpColor.bmWidth; j++)
            {
                Pixel = *(DWORD *)(pBits + Count);

                Alpha = ((BYTE)(Pixel >> 24) & 0xff);

                Red   = (((BYTE)(Pixel >>  0)) * Alpha) / 0xff;
                Green = (((BYTE)(Pixel >>  8)) * Alpha) / 0xff;
                Blue  = (((BYTE)(Pixel >> 16)) * Alpha) / 0xff;

                *(DWORD *)(pBits + Count) = (DWORD)(Red | (Green << 8) | (Blue << 16) | (Alpha << 24));

                Count += sizeof(DWORD);
            }
        }

        /* set mem bits */
        IntSetBitmapBits(psurf,
                         bmpColor.bmWidthBytes * abs(bmpColor.bmHeight),
                         pBits);
        SURFACE_UnlockSurface(psurf);

        hTmpBmp = NtGdiSelectBitmap(hMemDC, hMemBmp);

        Ret = NtGdiAlphaBlend(hDestDC,
					          x,
						      y,
                              cxWidth,
                              cyHeight,
                              hMemDC,
                              0,
                              0,
                              pIcon->Size.cx,
                              pIcon->Size.cy,
                              pixelblend,
                              NULL);
        NtGdiSelectBitmap(hMemDC, hTmpBmp);
    CleanupAlpha:
        if(pBits) ExFreePoolWithTag(pBits, TAG_BITMAP);
        if(hMemBmp) NtGdiDeleteObjectApp(hMemBmp);
		if(Ret) goto done;
    }


    if (diFlags & DI_MASK)
    {
        hTmpBmp = NtGdiSelectBitmap(hMemDC, hbmMask);
        NtGdiStretchBlt(hDestDC,
                        x,
                        y,
                        cxWidth,
                        cyHeight,
                        hMemDC,
                        0,
                        0,
                        pIcon->Size.cx,
                        pIcon->Size.cy,
                        SRCAND,
                        0);
        NtGdiSelectBitmap(hMemDC, hTmpBmp);
    }

    if(diFlags & DI_IMAGE)
    {
		if (hbmColor)
        {
            DWORD rop = (diFlags & DI_MASK) ? SRCINVERT : SRCCOPY ;
            hTmpBmp = NtGdiSelectBitmap(hMemDC, hbmColor);
            NtGdiStretchBlt(hDestDC,
                            x,
                            y,
                            cxWidth,
                            cyHeight,
                            hMemDC,
                            0,
                            0,
                            pIcon->Size.cx,
                            pIcon->Size.cy,
                            rop,
                            0);
            NtGdiSelectBitmap(hMemDC, hTmpBmp);
        }
        else
        {
            /* Mask bitmap holds the information in its second half */
            DWORD rop = (diFlags & DI_MASK) ? SRCINVERT : SRCCOPY ;
            hTmpBmp = NtGdiSelectBitmap(hMemDC, hbmMask);
            NtGdiStretchBlt(hDestDC,
                            x,
                            y,
                            cxWidth,
                            cyHeight,
                            hMemDC,
                            0,
                            pIcon->Size.cy,
                            pIcon->Size.cx,
                            pIcon->Size.cy,
                            rop,
                            0);
            NtGdiSelectBitmap(hMemDC, hTmpBmp);
        }
    }

done:
    if(hDestDC != hDc)
    {
        NtGdiBitBlt(hDc, xLeft, yTop, cxWidth, cyHeight, hDestDC, 0, 0, SRCCOPY, 0, 0);
    }

    /* Restore foreground and background colors */
    IntGdiSetBkColor(hDc, iOldBkColor);
    IntGdiSetTextColor(hDc, iOldTxtColor);

    Ret = TRUE ;

Cleanup:
    NtGdiDeleteObjectApp(hMemDC);
    if(hDestDC != hDc)
    {
        if(hOldOffBmp) NtGdiSelectBitmap(hDestDC, hOldOffBmp);
        NtGdiDeleteObjectApp(hDestDC);
        if(hOffBmp) NtGdiDeleteObjectApp(hOffBmp);
    }

    return Ret;
}

/*
 * @implemented
 */
BOOL
APIENTRY
NtUserDrawIconEx(
    HDC hdc,
    int xLeft,
    int yTop,
    HICON hIcon,
    int cxWidth,
    int cyHeight,
    UINT istepIfAniCur,
    HBRUSH hbrFlickerFreeDraw,
    UINT diFlags,
    BOOL bMetaHDC, // When TRUE, GDI functions need to be handled in User32!
    PVOID pDIXData)
{
    PCURICON_OBJECT pIcon;
    BOOL Ret;

    DPRINT("Enter NtUserDrawIconEx\n");
    UserEnterExclusive();

    if (!(pIcon = UserGetCurIconObject(hIcon)))
    {
        DPRINT1("UserGetCurIconObject() failed!\n");
        UserLeave();
        return FALSE;
    }

    Ret = UserDrawIconEx(hdc,
                         xLeft,
                         yTop,
                         pIcon,
                         cxWidth,
                         cyHeight,
                         istepIfAniCur,
                         hbrFlickerFreeDraw,
                         diFlags);

    UserDereferenceObject(pIcon);

    UserLeave();
    return Ret;
}

