#ifndef _WIN32K_DESKTOP_H
#define _WIN32K_DESKTOP_H

#include <windows.h>
#include <ddk/ntddk.h>
#include <internal/ex.h>
#include <internal/ps.h>
#include "msgqueue.h"
#include "window.h"

extern PDESKTOP_OBJECT InputDesktop;
extern HDESK InputDesktopHandle; 
extern PWNDCLASS_OBJECT DesktopWindowClass;
extern HDC ScreenDeviceContext;

NTSTATUS FASTCALL
InitDesktopImpl(VOID);

NTSTATUS FASTCALL
CleanupDesktopImpl(VOID);

LRESULT CALLBACK
IntDesktopWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HDC FASTCALL
IntGetScreenDC(VOID);

PUSER_MESSAGE_QUEUE FASTCALL
IntGetFocusMessageQueue(VOID);

VOID FASTCALL
IntSetFocusMessageQueue(PUSER_MESSAGE_QUEUE NewQueue);

PDESKTOP_OBJECT FASTCALL
IntGetActiveDesktop(VOID);

NTSTATUS FASTCALL
IntShowDesktop(PDESKTOP_OBJECT Desktop, ULONG Width, ULONG Height);

NTSTATUS FASTCALL
IntHideDesktop(PDESKTOP_OBJECT Desktop);

#endif /* _WIN32K_DESKTOP_H */

/* EOF */
