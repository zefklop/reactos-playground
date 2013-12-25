/*
 * MPR undocumented functions
 *
 * Copyright 1999 Ulrich Weigand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "precomp.h"
#include "wnetpriv.h"

 /*
  * FIXME: The following routines should use a private heap ...
  */

/*****************************************************************
 *  @  [MPR.22]
 */
LPVOID WINAPI MPR_Alloc( DWORD dwSize )
{
    return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize );
}

/*****************************************************************
 *  @  [MPR.23]
 */
LPVOID WINAPI MPR_ReAlloc( LPVOID lpSrc, DWORD dwSize )
{
    if ( lpSrc )
        return HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, lpSrc, dwSize );
    else
        return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize );
}

/*****************************************************************
 *  @  [MPR.24]
 */
BOOL WINAPI MPR_Free( LPVOID lpMem )
{
    if ( lpMem )
        return HeapFree( GetProcessHeap(), 0, lpMem );
    else
        return FALSE;
}

/*****************************************************************
 *  @  [MPR.25]
 */
BOOL WINAPI _MPR_25( LPBYTE lpMem, INT len )
{
    FIXME( "(%p, %d): stub\n", lpMem, len );

    return FALSE;
}

/*****************************************************************
 *  DllMain  [MPR.init]
 */
BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls( hinstDLL );
            wnetInit(hinstDLL);
            break;

        case DLL_PROCESS_DETACH:
            if (lpvReserved) break;
            wnetFree();
            break;
    }
    return TRUE;
}
