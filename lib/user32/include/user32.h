/*
 * COPYRIGHT:   See COPYING in the top level directory
 * PROJECT:     ReactOS user32.dll
 * FILE:        include/user32.h
 * PURPOSE:     Global user32 definitions
 */
#include <windows.h>
#include <win32k/win32k.h>

extern HANDLE ProcessHeap;
VOID
User32FreeHeap(PVOID Block);
PVOID
User32AllocHeap(ULONG Size);
VOID
User32ConvertUnicodeString(PWSTR SrcString, PSTR DestString, ULONG DestSize);
PWSTR
User32ConvertString(PCSTR String);
VOID
User32FreeString(PWSTR String);
