/*
 * Copyright (C) 2007 Jeff Latimer
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "netfw.h"

#include "wine/debug.h"
#include "hnetcfg_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(hnetcfg);

typedef HRESULT (*fnCreateInstance)( IUnknown *pUnkOuter, LPVOID *ppObj );

typedef struct
{
    const struct IClassFactoryVtbl *vtbl;
    fnCreateInstance pfnCreateInstance;
} hnetcfg_cf;

static inline hnetcfg_cf *impl_from_IClassFactory( IClassFactory *iface )
{
    return (hnetcfg_cf *)((char *)iface - FIELD_OFFSET( hnetcfg_cf, vtbl ));
}

static HRESULT WINAPI hnetcfg_cf_QueryInterface( IClassFactory *iface, REFIID riid, LPVOID *ppobj )
{
    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IClassFactory))
    {
        IClassFactory_AddRef( iface );
        *ppobj = iface;
        return S_OK;
    }
    FIXME("interface %s not implemented\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI hnetcfg_cf_AddRef( IClassFactory *iface )
{
    return 2;
}

static ULONG WINAPI hnetcfg_cf_Release( IClassFactory *iface )
{
    return 1;
}

static HRESULT WINAPI hnetcfg_cf_CreateInstance( IClassFactory *iface, LPUNKNOWN pOuter,
                                                  REFIID riid, LPVOID *ppobj )
{
    hnetcfg_cf *This = impl_from_IClassFactory( iface );
    HRESULT r;
    IUnknown *punk;

    TRACE("%p %s %p\n", pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if (pOuter)
        return CLASS_E_NOAGGREGATION;

    r = This->pfnCreateInstance( pOuter, (LPVOID *)&punk );
    if (FAILED(r))
        return r;

    r = IUnknown_QueryInterface( punk, riid, ppobj );
    if (FAILED(r))
        return r;

    IUnknown_Release( punk );
    return r;
}

static HRESULT WINAPI hnetcfg_cf_LockServer( IClassFactory *iface, BOOL dolock )
{
    FIXME("(%p)->(%d)\n", iface, dolock);
    return S_OK;
}

static const struct IClassFactoryVtbl hnetcfg_cf_vtbl =
{
    hnetcfg_cf_QueryInterface,
    hnetcfg_cf_AddRef,
    hnetcfg_cf_Release,
    hnetcfg_cf_CreateInstance,
    hnetcfg_cf_LockServer
};

static hnetcfg_cf fw_manager_cf = { &hnetcfg_cf_vtbl, NetFwMgr_create };
static hnetcfg_cf fw_app_cf = { &hnetcfg_cf_vtbl, NetFwAuthorizedApplication_create };

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n",hInstDLL,fdwReason,lpvReserved);

    switch(fdwReason) {
        case DLL_WINE_PREATTACH:
            return FALSE;
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

HRESULT WINAPI DllGetClassObject( REFCLSID rclsid, REFIID iid, LPVOID *ppv )
{
    IClassFactory *cf = NULL;

    TRACE("%s %s %p\n", debugstr_guid(rclsid), debugstr_guid(iid), ppv);

    if (IsEqualGUID( rclsid, &CLSID_NetFwMgr ))
    {
       cf = (IClassFactory *)&fw_manager_cf.vtbl;
    }
    else if (IsEqualGUID( rclsid, &CLSID_NetFwAuthorizedApplication ))
    {
       cf = (IClassFactory *)&fw_app_cf.vtbl;
    }

    if (!cf) return CLASS_E_CLASSNOTAVAILABLE;
    return IClassFactory_QueryInterface( cf, iid, ppv );
}

HRESULT WINAPI DllCanUnloadNow( void )
{
    return S_FALSE;
}
