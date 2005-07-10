/*
 * RPC support routines
 *
 * Copyright 2005 Eric Kohl
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <windows.h>
#include <rpc.h>
#include <rpcdce.h>
#include "rpc_private.h"


static RPC_BINDING_HANDLE LocalBindingHandle = NULL;


RPC_STATUS
PnpBindRpc(LPCWSTR pszMachine,
           RPC_BINDING_HANDLE* BindingHandle)
{
    PWSTR pszStringBinding = NULL;
    RPC_STATUS Status;

    Status = RpcStringBindingComposeW(NULL,
                                      L"ncacn_np",
                                      (LPWSTR)pszMachine,
                                      L"\\pipe\\umpnpmgr",
                                      NULL,
                                      &pszStringBinding);
    if (Status != RPC_S_OK)
        return Status;

    Status = RpcBindingFromStringBindingW(pszStringBinding,
                                          BindingHandle);

    RpcStringFreeW(&pszStringBinding);

    return Status;
}


RPC_STATUS
PnpUnbindRpc(RPC_BINDING_HANDLE *BindingHandle)
{
    if (BindingHandle != NULL)
    {
        RpcBindingFree(*BindingHandle);
        *BindingHandle = NULL;
    }

    return RPC_S_OK;
}


RPC_STATUS
PnpGetLocalBindingHandle(RPC_BINDING_HANDLE *BindingHandle)
{
    if (LocalBindingHandle != NULL)
    {
        BindingHandle = LocalBindingHandle;
        return RPC_S_OK;
    }

    return PnpBindRpc(NULL, BindingHandle);
}


RPC_STATUS
PnpUnbindLocalBindingHandle(VOID)
{
    return PnpUnbindRpc(&LocalBindingHandle);
}


void __RPC_FAR * __RPC_USER
midl_user_allocate(size_t len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}


void __RPC_USER
midl_user_free(void __RPC_FAR * ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}

/* EOF */
