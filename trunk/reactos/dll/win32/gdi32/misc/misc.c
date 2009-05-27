/*
 *  ReactOS GDI lib
 *  Copyright (C) 2003 ReactOS Team
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
/* $Id$
 *
 * PROJECT:         ReactOS gdi32.dll
 * FILE:            lib/gdi32/misc/misc.c
 * PURPOSE:         Miscellaneous functions
 * PROGRAMMER:      Thomas Weidenmueller <w3seek@reactos.com>
 * UPDATE HISTORY:
 *      2004/09/04  Created
 */

#include "precomp.h"

#define NDEBUG
#include <debug.h>

PGDI_TABLE_ENTRY GdiHandleTable = NULL;
PGDI_SHARED_HANDLE_TABLE GdiSharedHandleTable = NULL;
HANDLE CurrentProcessId = NULL;
DWORD GDI_BatchLimit = 1;


BOOL
WINAPI
GdiAlphaBlend(
            HDC hDCDst,
            int DstX,
            int DstY,
            int DstCx,
            int DstCy,
            HDC hDCSrc,
            int SrcX,
            int SrcY,
            int SrcCx,
            int SrcCy,
            BLENDFUNCTION BlendFunction
            )
{
   if ( hDCSrc == NULL ) return FALSE;

   if (GDI_HANDLE_GET_TYPE(hDCSrc) == GDI_OBJECT_TYPE_METADC) return FALSE;

   return NtGdiAlphaBlend(
                      hDCDst,
                        DstX,
                        DstY,
                       DstCx,
                       DstCy,
                      hDCSrc,
                        SrcX,
                        SrcY,
                       SrcCx,
                       SrcCy,
               BlendFunction,
                           0 );
}

/*
 * @implemented
 */
HGDIOBJ
WINAPI
GdiFixUpHandle(HGDIOBJ hGdiObj)
{
    PGDI_TABLE_ENTRY Entry;

    if (((ULONG_PTR)(hGdiObj)) & GDI_HANDLE_UPPER_MASK )
    {
        return hGdiObj;
    }

    /* FIXME is this right ?? */

    Entry = GdiHandleTable + GDI_HANDLE_GET_INDEX(hGdiObj);

   /* Rebuild handle for Object */
    return hGdiObj = (HGDIOBJ)(((LONG_PTR)(hGdiObj)) | (Entry->Type << GDI_ENTRY_UPPER_SHIFT));
}

/*
 * @implemented
 */
PVOID
WINAPI
GdiQueryTable(VOID)
{
  return (PVOID)GdiHandleTable;
}

BOOL GdiIsHandleValid(HGDIOBJ hGdiObj)
{
  PGDI_TABLE_ENTRY Entry = GdiHandleTable + GDI_HANDLE_GET_INDEX(hGdiObj);
// We are only looking for TYPE not the rest here, and why is FullUnique filled up with CRAP!?
// DPRINT1("FullUnique -> %x\n", Entry->FullUnique);
  if((Entry->Type & GDI_ENTRY_BASETYPE_MASK) != 0 &&
     ( (Entry->Type << GDI_ENTRY_UPPER_SHIFT) & GDI_HANDLE_TYPE_MASK ) == 
                                                                   GDI_HANDLE_GET_TYPE(hGdiObj))
  {
    HANDLE pid = (HANDLE)((ULONG_PTR)Entry->ProcessId & ~0x1);
    if(pid == NULL || pid == CurrentProcessId)
    {
      return TRUE;
    }
  }
  return FALSE;
}

BOOL GdiGetHandleUserData(HGDIOBJ hGdiObj, DWORD ObjectType, PVOID *UserData)
{
  if ( GdiHandleTable )
  {
     PGDI_TABLE_ENTRY Entry = GdiHandleTable + GDI_HANDLE_GET_INDEX(hGdiObj);
     if((Entry->Type & GDI_ENTRY_BASETYPE_MASK) == ObjectType &&
       ( (Entry->Type << GDI_ENTRY_UPPER_SHIFT) & GDI_HANDLE_TYPE_MASK ) == 
                                                                   GDI_HANDLE_GET_TYPE(hGdiObj))
     {
       HANDLE pid = (HANDLE)((ULONG_PTR)Entry->ProcessId & ~0x1);
       if(pid == NULL || pid == CurrentProcessId)
       {
       //
       // Need to test if we have Read & Write access to the VM address space.
       //
         BOOL Result = TRUE;
         if(Entry->UserData)
         {
            volatile CHAR *Current = (volatile CHAR*)Entry->UserData;
            _SEH2_TRY
            {
              *Current = *Current;
            }
            _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
            {
              Result = FALSE;
            }
            _SEH2_END
         }
         else
            Result = FALSE; // Can not be zero.
         if (Result) *UserData = Entry->UserData;
         return Result;
       }
     }
     SetLastError(ERROR_INVALID_PARAMETER);
     return FALSE;
  }
  else
  {
    DPRINT1("!GGHUD: Warning System Initialization Error!!!! GdiHandleTable == 0x%x !!!\n",GdiHandleTable);
    *UserData = NULL;
  }
  return FALSE;
}

PLDC
FASTCALL
GdiGetLDC(HDC hDC)
{
  if ( GdiHandleTable )
  {
     PDC_ATTR Dc_Attr;
     PGDI_TABLE_ENTRY Entry = GdiHandleTable + GDI_HANDLE_GET_INDEX((HGDIOBJ) hDC);
     HANDLE pid = (HANDLE)((ULONG_PTR)Entry->ProcessId & ~0x1);
     // Don't check the mask, just the object type.
     if ( Entry->ObjectType == GDIObjType_DC_TYPE &&
          (pid == NULL || pid == CurrentProcessId) )
     {
        BOOL Result = TRUE;
        if (Entry->UserData)
        {
           volatile CHAR *Current = (volatile CHAR*)Entry->UserData;
           _SEH2_TRY
           {
             *Current = *Current;
           }
           _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
           {
             Result = FALSE;
           }
           _SEH2_END
        }
        else
           Result = FALSE;

        if (Result)
        {
           Dc_Attr = (PDC_ATTR)Entry->UserData;
           return Dc_Attr->pvLDC;
        }
     }
     return NULL;
  }
  else
  {
     DPRINT1("!LDC: Warning System Initialization Error!!!! GdiHandleTable == 0x%x !!!\n",GdiHandleTable);
  }
  return NULL;
}

VOID GdiSAPCallback(PLDC pldc)
{
    DWORD Time, NewTime = GetTickCount();

    Time = NewTime - pldc->CallBackTick;

    if ( Time < SAPCALLBACKDELAY) return;

    pldc->CallBackTick = NewTime;

    if ( !pldc->pAbortProc(pldc->hDC, 0) )
    {
       CancelDC(pldc->hDC);
       AbortDoc(pldc->hDC);
    }
}

/*
 * @implemented
 */
DWORD
WINAPI
GdiSetBatchLimit(DWORD	Limit)
{
    DWORD OldLimit = GDI_BatchLimit;

    if ( (!Limit) ||
         (Limit >= GDI_BATCH_LIMIT))
    {
        return Limit;
    }

    GdiFlush();
    GDI_BatchLimit = Limit;
    return OldLimit;
}


/*
 * @implemented
 */
DWORD
WINAPI
GdiGetBatchLimit()
{
    return GDI_BatchLimit;
}

/*
 * @unimplemented
 */
BOOL
WINAPI
GdiReleaseDC(HDC hdc)
{
    return 0;
}

INT
WINAPI
ExtEscape(HDC hDC,
          int nEscape,
          int cbInput,
          LPCSTR lpszInData,
          int cbOutput,
          LPSTR lpszOutData)
{
    return NtGdiExtEscape(hDC, NULL, 0, nEscape, cbInput, (LPSTR)lpszInData, cbOutput, lpszOutData);
}

/*
 * @implemented
 */
VOID
WINAPI
GdiSetLastError(DWORD dwErrCode)
{
    NtCurrentTeb()->LastErrorValue = (ULONG) dwErrCode;
}

BOOL
WINAPI
GdiAddGlsBounds(HDC hdc,LPRECT prc)
{
    //FIXME: Lookup what 0x8000 means
    return NtGdiSetBoundsRect(hdc, prc, 0x8000 |  DCB_ACCUMULATE ) ? TRUE : FALSE;
}

