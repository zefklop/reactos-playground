/*
 * PROJECT:     ReactOS Service Control Manager
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        base/system/services/rpcserver.c
 * PURPOSE:     RPC server interface for the advapi32 calls
 * COPYRIGHT:   Copyright 2005-2006 Eric Kohl
 *              Copyright 2006-2007 Herv� Poussineau <hpoussin@reactos.org>
 *              Copyright 2007 Ged Murphy <gedmurphy@reactos.org>
 */

/* INCLUDES ****************************************************************/

#include "services.h"

#define NDEBUG
#include <debug.h>

/* GLOBALS *****************************************************************/

#define MANAGER_TAG 0x72674D68  /* 'hMgr' */
#define SERVICE_TAG 0x63765368  /* 'hSvc' */

typedef struct _SCMGR_HANDLE
{
    DWORD Tag;
    DWORD DesiredAccess;
} SCMGR_HANDLE;


typedef struct _MANAGER_HANDLE
{
    SCMGR_HANDLE Handle;
    WCHAR DatabaseName[1];
} MANAGER_HANDLE, *PMANAGER_HANDLE;


typedef struct _SERVICE_HANDLE
{
    SCMGR_HANDLE Handle;
    PSERVICE ServiceEntry;
} SERVICE_HANDLE, *PSERVICE_HANDLE;


#define SC_MANAGER_READ \
  (STANDARD_RIGHTS_READ | \
   SC_MANAGER_QUERY_LOCK_STATUS | \
   SC_MANAGER_ENUMERATE_SERVICE)

#define SC_MANAGER_WRITE \
  (STANDARD_RIGHTS_WRITE | \
   SC_MANAGER_MODIFY_BOOT_CONFIG | \
   SC_MANAGER_CREATE_SERVICE)

#define SC_MANAGER_EXECUTE \
  (STANDARD_RIGHTS_EXECUTE | \
   SC_MANAGER_LOCK | \
   SC_MANAGER_ENUMERATE_SERVICE | \
   SC_MANAGER_CONNECT | \
   SC_MANAGER_CREATE_SERVICE)


#define SERVICE_READ \
  (STANDARD_RIGHTS_READ | \
   SERVICE_INTERROGATE | \
   SERVICE_ENUMERATE_DEPENDENTS | \
   SERVICE_QUERY_STATUS | \
   SERVICE_QUERY_CONFIG)

#define SERVICE_WRITE \
  (STANDARD_RIGHTS_WRITE | \
   SERVICE_CHANGE_CONFIG)

#define SERVICE_EXECUTE \
  (STANDARD_RIGHTS_EXECUTE | \
   SERVICE_USER_DEFINED_CONTROL | \
   SERVICE_PAUSE_CONTINUE | \
   SERVICE_STOP | \
   SERVICE_START)

#define TAG_ARRAY_SIZE 32

/* VARIABLES ***************************************************************/

static GENERIC_MAPPING
ScmManagerMapping = {SC_MANAGER_READ,
                     SC_MANAGER_WRITE,
                     SC_MANAGER_EXECUTE,
                     SC_MANAGER_ALL_ACCESS};

static GENERIC_MAPPING
ScmServiceMapping = {SERVICE_READ,
                     SERVICE_WRITE,
                     SERVICE_EXECUTE,
                     SERVICE_ALL_ACCESS};


/* FUNCTIONS ***************************************************************/

VOID
ScmStartRpcServer(VOID)
{
    RPC_STATUS Status;

    DPRINT("ScmStartRpcServer() called\n");

    Status = RpcServerUseProtseqEpW(L"ncacn_np",
                                    10,
                                    L"\\pipe\\ntsvcs",
                                    NULL);
    if (Status != RPC_S_OK)
    {
        DPRINT1("RpcServerUseProtseqEpW() failed (Status %lx)\n", Status);
        return;
    }

    Status = RpcServerRegisterIf(svcctl_v2_0_s_ifspec,
                                 NULL,
                                 NULL);
    if (Status != RPC_S_OK)
    {
        DPRINT1("RpcServerRegisterIf() failed (Status %lx)\n", Status);
        return;
    }

    Status = RpcServerListen(1, 20, TRUE);
    if (Status != RPC_S_OK)
    {
        DPRINT1("RpcServerListen() failed (Status %lx)\n", Status);
        return;
    }

    DPRINT("ScmStartRpcServer() done\n");
}


static DWORD
ScmCreateManagerHandle(LPWSTR lpDatabaseName,
                       SC_HANDLE *Handle)
{
    PMANAGER_HANDLE Ptr;

    if (lpDatabaseName == NULL)
        lpDatabaseName = SERVICES_ACTIVE_DATABASEW;

    if (_wcsicmp(lpDatabaseName, SERVICES_FAILED_DATABASEW) == 0)
    {
        DPRINT("Database %S, does not exist\n", lpDatabaseName);
        return ERROR_DATABASE_DOES_NOT_EXIST;
    }
    else if (_wcsicmp(lpDatabaseName, SERVICES_ACTIVE_DATABASEW) != 0)
    {
        DPRINT("Invalid Database name %S.\n", lpDatabaseName);
        return ERROR_INVALID_NAME;
    }

    Ptr = HeapAlloc(GetProcessHeap(),
                    HEAP_ZERO_MEMORY,
                    FIELD_OFFSET(MANAGER_HANDLE, DatabaseName[wcslen(lpDatabaseName) + 1]));
    if (Ptr == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    Ptr->Handle.Tag = MANAGER_TAG;

    wcscpy(Ptr->DatabaseName, lpDatabaseName);

    *Handle = (SC_HANDLE)Ptr;

    return ERROR_SUCCESS;
}


static DWORD
ScmCreateServiceHandle(PSERVICE lpServiceEntry,
                       SC_HANDLE *Handle)
{
    PSERVICE_HANDLE Ptr;

    Ptr = HeapAlloc(GetProcessHeap(),
                    HEAP_ZERO_MEMORY,
                    sizeof(SERVICE_HANDLE));
    if (Ptr == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    Ptr->Handle.Tag = SERVICE_TAG;

    Ptr->ServiceEntry = lpServiceEntry;

    *Handle = (SC_HANDLE)Ptr;

    return ERROR_SUCCESS;
}


static PMANAGER_HANDLE
ScmGetServiceManagerFromHandle(SC_RPC_HANDLE Handle)
{
    PMANAGER_HANDLE pManager = NULL;

    _SEH2_TRY
    {
        if (((PMANAGER_HANDLE)Handle)->Handle.Tag == MANAGER_TAG)
            pManager = (PMANAGER_HANDLE)Handle;
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT1("Exception: Invalid Service Manager handle!\n");
    }
    _SEH2_END;

    return pManager;
}


static PSERVICE_HANDLE
ScmGetServiceFromHandle(SC_RPC_HANDLE Handle)
{
    PSERVICE_HANDLE pService = NULL;

    _SEH2_TRY
    {
        if (((PSERVICE_HANDLE)Handle)->Handle.Tag == SERVICE_TAG)
            pService = (PSERVICE_HANDLE)Handle;
    }
    _SEH2_EXCEPT(EXCEPTION_EXECUTE_HANDLER)
    {
        DPRINT1("Exception: Invalid Service handle!\n");
    }
    _SEH2_END;

    return pService;
}


static DWORD
ScmCheckAccess(SC_HANDLE Handle,
               DWORD dwDesiredAccess)
{
    PMANAGER_HANDLE hMgr;

    hMgr = (PMANAGER_HANDLE)Handle;
    if (hMgr->Handle.Tag == MANAGER_TAG)
    {
        RtlMapGenericMask(&dwDesiredAccess,
                          &ScmManagerMapping);

        hMgr->Handle.DesiredAccess = dwDesiredAccess;

        return ERROR_SUCCESS;
    }
    else if (hMgr->Handle.Tag == SERVICE_TAG)
    {
        RtlMapGenericMask(&dwDesiredAccess,
                          &ScmServiceMapping);

        hMgr->Handle.DesiredAccess = dwDesiredAccess;

        return ERROR_SUCCESS;
    }

    return ERROR_INVALID_HANDLE;
}


DWORD
ScmAssignNewTag(PSERVICE lpService)
{
    HKEY hKey = NULL;
    DWORD dwError;
    DWORD dwGroupTagCount = 0;
    PDWORD pdwGroupTags = NULL;
    DWORD dwFreeTag = 0;
    DWORD dwTagUsedBase = 1;
    BOOLEAN TagUsed[TAG_ARRAY_SIZE];
    INT nTagOffset;
    DWORD i;
    DWORD cbDataSize;
    PLIST_ENTRY ServiceEntry;
    PSERVICE CurrentService;

    ASSERT(lpService != NULL);
    ASSERT(lpService->lpGroup != NULL);

    dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"System\\CurrentControlSet\\Control\\GroupOrderList",
                            0,
                            KEY_READ,
                            &hKey);

    if (dwError != ERROR_SUCCESS)
        goto findFreeTag;

    /* query value length */
    cbDataSize = 0;
    dwError = RegQueryValueExW(hKey,
                               lpService->lpGroup->szGroupName,
                               NULL,
                               NULL,
                               NULL,
                               &cbDataSize);

    if (dwError != ERROR_SUCCESS && dwError != ERROR_MORE_DATA)
        goto findFreeTag;

    pdwGroupTags = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbDataSize);
    if (!pdwGroupTags)
    {
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        goto cleanup;
    }

    dwError = RegQueryValueExW(hKey,
                               lpService->lpGroup->szGroupName,
                               NULL,
                               NULL,
                               (LPBYTE)pdwGroupTags,
                               &cbDataSize);

    if (dwError != ERROR_SUCCESS)
        goto findFreeTag;

    if (cbDataSize < sizeof(pdwGroupTags[0]))
        goto findFreeTag;

    dwGroupTagCount = min(pdwGroupTags[0], cbDataSize / sizeof(pdwGroupTags[0]) - 1);

findFreeTag:
    do
    {
        /* mark all tags as unused */
        for (i = 0; i < TAG_ARRAY_SIZE; i++)
            TagUsed[i] = FALSE;

        /* mark tags in GroupOrderList as used */
        for (i = 1; i <= dwGroupTagCount; i++)
        {
            nTagOffset = pdwGroupTags[i] - dwTagUsedBase;
            if (nTagOffset >= 0 && nTagOffset < TAG_ARRAY_SIZE)
                TagUsed[nTagOffset] = TRUE;
        }

        /* mark tags in service list as used */
        ServiceEntry = lpService->ServiceListEntry.Flink;
        while (ServiceEntry != &lpService->ServiceListEntry)
        {
            ASSERT(ServiceEntry != NULL);
            CurrentService = CONTAINING_RECORD(ServiceEntry, SERVICE, ServiceListEntry);
            if (CurrentService->lpGroup == lpService->lpGroup)
            {
                nTagOffset = CurrentService->dwTag - dwTagUsedBase;
                if (nTagOffset >= 0 && nTagOffset < TAG_ARRAY_SIZE)
                    TagUsed[nTagOffset] = TRUE;
            }

            ServiceEntry = ServiceEntry->Flink;
        }

        /* find unused tag, if any */
        for (i = 0; i < TAG_ARRAY_SIZE; i++)
        {
            if (!TagUsed[i])
            {
                dwFreeTag = dwTagUsedBase + i;
                break;
            }
        }

        dwTagUsedBase += TAG_ARRAY_SIZE;
    } while (!dwFreeTag);

cleanup:
    if (pdwGroupTags)
        HeapFree(GetProcessHeap(), 0, pdwGroupTags);

    if (hKey)
        RegCloseKey(hKey);

    if (dwFreeTag)
    {
        lpService->dwTag = dwFreeTag;
        DPRINT("Assigning new tag %lu to service %S in group %S\n",
               lpService->dwTag, lpService->lpServiceName, lpService->lpGroup->szGroupName);
        dwError = ERROR_SUCCESS;
    }
    else
    {
        DPRINT1("Failed to assign new tag to service %S, error=%lu\n",
                lpService->lpServiceName, dwError);
    }

    return dwError;
}


/* Create a path suitable for the bootloader out of the full path */
DWORD
ScmConvertToBootPathName(wchar_t *CanonName, wchar_t **RelativeName)
{
    SIZE_T ServiceNameLen, ExpandedLen;
    DWORD BufferSize;
    WCHAR Dest;
    WCHAR *Expanded;
    UNICODE_STRING NtPathName, SystemRoot, LinkTarget;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    HANDLE SymbolicLinkHandle;

    DPRINT("ScmConvertToBootPathName %S\n", CanonName);

    if (!RelativeName)
        return ERROR_INVALID_PARAMETER;

    *RelativeName = NULL;

    ServiceNameLen = wcslen(CanonName);

    /* First check, if it's already good */
    if (ServiceNameLen > 12 &&
        !_wcsnicmp(L"\\SystemRoot\\", CanonName, 12))
    {
        *RelativeName = HeapAlloc(GetProcessHeap(),
                                  HEAP_ZERO_MEMORY,
                                  (ServiceNameLen + 1) * sizeof(WCHAR));
        if (*RelativeName == NULL)
        {
            DPRINT("Error allocating memory for boot driver name!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        /* Copy it */
        wcscpy(*RelativeName, CanonName);

        DPRINT("Bootdriver name %S\n", *RelativeName);
        return ERROR_SUCCESS;
    }

    /* If it has %SystemRoot% prefix, substitute it to \System*/
    if (ServiceNameLen > 13 &&
        !_wcsnicmp(L"%SystemRoot%\\", CanonName, 13))
    {
        /* There is no +sizeof(wchar_t) because the name is less by 1 wchar */
        *RelativeName = HeapAlloc(GetProcessHeap(),
                                  HEAP_ZERO_MEMORY,
                                  ServiceNameLen * sizeof(WCHAR));

        if (*RelativeName == NULL)
        {
            DPRINT("Error allocating memory for boot driver name!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        /* Copy it */
        wcscpy(*RelativeName, L"\\SystemRoot\\");
        wcscat(*RelativeName, CanonName + 13);

        DPRINT("Bootdriver name %S\n", *RelativeName);
        return ERROR_SUCCESS;
    }

    /* Get buffer size needed for expanding env strings */
    BufferSize = ExpandEnvironmentStringsW(L"%SystemRoot%\\", &Dest, 1);

    if (BufferSize <= 1)
    {
        DPRINT("Error during a call to ExpandEnvironmentStringsW()\n");
        return ERROR_INVALID_ENVIRONMENT;
    }

    /* Allocate memory, since the size is known now */
    Expanded = HeapAlloc(GetProcessHeap(),
                         HEAP_ZERO_MEMORY,
                         (BufferSize + 1) * sizeof(WCHAR));
    if (!Expanded)
    {
        DPRINT("Error allocating memory for boot driver name!\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /* Expand it */
    if (ExpandEnvironmentStringsW(L"%SystemRoot%\\", Expanded, BufferSize) >
        BufferSize)
    {
        DPRINT("Error during a call to ExpandEnvironmentStringsW()\n");
        HeapFree(GetProcessHeap(), 0, Expanded);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /* Convert to NT-style path */
    if (!RtlDosPathNameToNtPathName_U(Expanded, &NtPathName, NULL, NULL))
    {
        DPRINT("Error during a call to RtlDosPathNameToNtPathName_U()\n");
        return ERROR_INVALID_ENVIRONMENT;
    }

    DPRINT("Converted to NT-style %wZ\n", &NtPathName);

    /* No need to keep the dos-path anymore */
    HeapFree(GetProcessHeap(), 0, Expanded);

    /* Copy it to the allocated place */
    Expanded = HeapAlloc(GetProcessHeap(),
                         HEAP_ZERO_MEMORY,
                         NtPathName.Length + sizeof(UNICODE_NULL));
    if (!Expanded)
    {
        DPRINT("Error allocating memory for boot driver name!\n");
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    ExpandedLen = NtPathName.Length / sizeof(WCHAR);
    wcsncpy(Expanded, NtPathName.Buffer, ExpandedLen);
    Expanded[ExpandedLen] = UNICODE_NULL;

    if (ServiceNameLen > ExpandedLen &&
        !_wcsnicmp(Expanded, CanonName, ExpandedLen))
    {
        /* Only \SystemRoot\ is missing */
        *RelativeName = HeapAlloc(GetProcessHeap(),
                                  HEAP_ZERO_MEMORY,
                                  (ServiceNameLen - ExpandedLen) * sizeof(WCHAR) + 13*sizeof(WCHAR));
        if (*RelativeName == NULL)
        {
            DPRINT("Error allocating memory for boot driver name!\n");
            HeapFree(GetProcessHeap(), 0, Expanded);
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        wcscpy(*RelativeName, L"\\SystemRoot\\");
        wcscat(*RelativeName, CanonName + ExpandedLen);

        RtlFreeUnicodeString(&NtPathName);
        return ERROR_SUCCESS;
    }

    /* The most complex case starts here */
    RtlInitUnicodeString(&SystemRoot, L"\\SystemRoot");
    InitializeObjectAttributes(&ObjectAttributes,
                               &SystemRoot,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    /* Open this symlink */
    Status = NtOpenSymbolicLinkObject(&SymbolicLinkHandle, SYMBOLIC_LINK_QUERY, &ObjectAttributes);

    if (NT_SUCCESS(Status))
    {
        LinkTarget.Length = 0;
        LinkTarget.MaximumLength = 0;

        DPRINT("Opened symbolic link object\n");

        Status = NtQuerySymbolicLinkObject(SymbolicLinkHandle, &LinkTarget, &BufferSize);
        if (NT_SUCCESS(Status) || Status == STATUS_BUFFER_TOO_SMALL)
        {
            /* Check if required buffer size is sane */
            if (BufferSize > 0xFFFD)
            {
                DPRINT("Too large buffer required\n");

                if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                HeapFree(GetProcessHeap(), 0, Expanded);
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            /* Alloc the string */
            LinkTarget.Length = (USHORT)BufferSize;
            LinkTarget.MaximumLength = LinkTarget.Length + sizeof(UNICODE_NULL);
            LinkTarget.Buffer = HeapAlloc(GetProcessHeap(),
                                          HEAP_ZERO_MEMORY,
                                          LinkTarget.MaximumLength);
            if (!LinkTarget.Buffer)
            {
                DPRINT("Unable to alloc buffer\n");
                if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                HeapFree(GetProcessHeap(), 0, Expanded);
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            /* Do a real query now */
            Status = NtQuerySymbolicLinkObject(SymbolicLinkHandle, &LinkTarget, &BufferSize);
            if (NT_SUCCESS(Status))
            {
                DPRINT("LinkTarget: %wZ\n", &LinkTarget);

                ExpandedLen = LinkTarget.Length / sizeof(WCHAR);
                if ((ServiceNameLen > ExpandedLen) &&
                    !_wcsnicmp(LinkTarget.Buffer, CanonName, ExpandedLen))
                {
                    *RelativeName = HeapAlloc(GetProcessHeap(),
                                              HEAP_ZERO_MEMORY,
                                              (ServiceNameLen - ExpandedLen) * sizeof(WCHAR) + 13*sizeof(WCHAR));

                    if (*RelativeName == NULL)
                    {
                        DPRINT("Unable to alloc buffer\n");
                        if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                        HeapFree(GetProcessHeap(), 0, Expanded);
                        RtlFreeUnicodeString(&NtPathName);
                        return ERROR_NOT_ENOUGH_MEMORY;
                    }

                    /* Copy it over, substituting the first part
                       with SystemRoot */
                    wcscpy(*RelativeName, L"\\SystemRoot\\");
                    wcscat(*RelativeName, CanonName+ExpandedLen+1);

                    /* Cleanup */
                    if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                    HeapFree(GetProcessHeap(), 0, Expanded);
                    RtlFreeUnicodeString(&NtPathName);

                    /* Return success */
                    return ERROR_SUCCESS;
                }
                else
                {
                    if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                    HeapFree(GetProcessHeap(), 0, Expanded);
                    RtlFreeUnicodeString(&NtPathName);
                    return ERROR_INVALID_PARAMETER;
                }
            }
            else
            {
                DPRINT("Error, Status = %08X\n", Status);
                if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
                HeapFree(GetProcessHeap(), 0, Expanded);
                RtlFreeUnicodeString(&NtPathName);
                return ERROR_INVALID_PARAMETER;
            }
        }
        else
        {
            DPRINT("Error, Status = %08X\n", Status);
            if (SymbolicLinkHandle) NtClose(SymbolicLinkHandle);
            HeapFree(GetProcessHeap(), 0, Expanded);
            RtlFreeUnicodeString(&NtPathName);
            return ERROR_INVALID_PARAMETER;
        }
    }
    else
    {
        /* Failure */
        DPRINT("Error, Status = %08X\n", Status);
        HeapFree(GetProcessHeap(), 0, Expanded);
        return ERROR_INVALID_PARAMETER;
    }
}


DWORD
ScmCanonDriverImagePath(DWORD dwStartType,
                        const wchar_t *lpServiceName,
                        wchar_t **lpCanonName)
{
    DWORD Result;
    SIZE_T ServiceNameLen;
    UNICODE_STRING NtServiceName;
    WCHAR *RelativeName;
    const WCHAR *SourceName = lpServiceName;

    /* Calculate the length of the service's name */
    ServiceNameLen = wcslen(lpServiceName);

    /* 12 is wcslen(L"\\SystemRoot\\") */
    if (ServiceNameLen > 12 &&
        !_wcsnicmp(L"\\SystemRoot\\", lpServiceName, 12))
    {
        /* SystemRoot prefix is already included */
        *lpCanonName = HeapAlloc(GetProcessHeap(),
                                 HEAP_ZERO_MEMORY,
                                 (ServiceNameLen + 1) * sizeof(WCHAR));

        if (*lpCanonName == NULL)
        {
            DPRINT("Error allocating memory for canonized service name!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        /* If it's a boot-time driver, it must be systemroot relative */
        if (dwStartType == SERVICE_BOOT_START)
            SourceName += 12;

        /* Copy it */
        wcscpy(*lpCanonName, SourceName);

        DPRINT("Canonicalized name %S\n", *lpCanonName);
        return NO_ERROR;
    }

    /* Check if it has %SystemRoot% (len=13) */
    if (ServiceNameLen > 13 &&
        !_wcsnicmp(L"%SystemRoot%\\", lpServiceName, 13))
    {
        /* Substitute %SystemRoot% with \\SystemRoot\\ */
        *lpCanonName = HeapAlloc(GetProcessHeap(),
                                 HEAP_ZERO_MEMORY,
                                 (ServiceNameLen + 1) * sizeof(WCHAR));

        if (*lpCanonName == NULL)
        {
            DPRINT("Error allocating memory for canonized service name!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        /* If it's a boot-time driver, it must be systemroot relative */
        if (dwStartType == SERVICE_BOOT_START)
            wcscpy(*lpCanonName, L"\\SystemRoot\\");

        wcscat(*lpCanonName, lpServiceName + 13);

        DPRINT("Canonicalized name %S\n", *lpCanonName);
        return NO_ERROR;
    }

    /* Check if it's a relative path name */
    if (lpServiceName[0] != L'\\' && lpServiceName[1] != L':')
    {
        *lpCanonName = HeapAlloc(GetProcessHeap(),
                                 HEAP_ZERO_MEMORY,
                                 (ServiceNameLen + 1) * sizeof(WCHAR));

        if (*lpCanonName == NULL)
        {
            DPRINT("Error allocating memory for canonized service name!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }

        /* Just copy it over without changing */
        wcscpy(*lpCanonName, lpServiceName);

        return NO_ERROR;
    }

    /* It seems to be a DOS path, convert it */
    if (!RtlDosPathNameToNtPathName_U(lpServiceName, &NtServiceName, NULL, NULL))
    {
        DPRINT("RtlDosPathNameToNtPathName_U() failed!\n");
        return ERROR_INVALID_PARAMETER;
    }

    *lpCanonName = HeapAlloc(GetProcessHeap(),
                             HEAP_ZERO_MEMORY,
                             NtServiceName.Length + sizeof(WCHAR));

    if (*lpCanonName == NULL)
    {
        DPRINT("Error allocating memory for canonized service name!\n");
        RtlFreeUnicodeString(&NtServiceName);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    /* Copy the string */
    wcsncpy(*lpCanonName, NtServiceName.Buffer, NtServiceName.Length / sizeof(WCHAR));

    /* The unicode string is not needed anymore */
    RtlFreeUnicodeString(&NtServiceName);

    if (dwStartType != SERVICE_BOOT_START)
    {
        DPRINT("Canonicalized name %S\n", *lpCanonName);
        return NO_ERROR;
    }

    /* The service is boot-started, so must be relative */
    Result = ScmConvertToBootPathName(*lpCanonName, &RelativeName);
    if (Result)
    {
        /* There is a problem, free name and return */
        HeapFree(GetProcessHeap(), 0, *lpCanonName);
        DPRINT("Error converting named!\n");
        return Result;
    }

    ASSERT(RelativeName);

    /* Copy that string */
    wcscpy(*lpCanonName, RelativeName + 12);

    /* Free the allocated buffer */
    HeapFree(GetProcessHeap(), 0, RelativeName);

    DPRINT("Canonicalized name %S\n", *lpCanonName);

    /* Success */
    return NO_ERROR;
}


/* Internal recursive function */
/* Need to search for every dependency on every service */
static DWORD
Int_EnumDependentServicesW(HKEY hServicesKey,
                           PSERVICE lpService,
                           DWORD dwServiceState,
                           PSERVICE *lpServices,
                           LPDWORD pcbBytesNeeded,
                           LPDWORD lpServicesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    WCHAR szNameBuf[MAX_PATH];
    WCHAR szValueBuf[MAX_PATH];
    WCHAR *lpszNameBuf = szNameBuf;
    WCHAR *lpszValueBuf = szValueBuf;
    DWORD dwSize;
    DWORD dwNumSubKeys;
    DWORD dwIteration;
    PSERVICE lpCurrentService;
    HKEY hServiceEnumKey;
    DWORD dwCurrentServiceState = SERVICE_ACTIVE;
    DWORD dwDependServiceStrPtr = 0;
    DWORD dwRequiredSize = 0;

    /* Get the number of service keys */
    dwError = RegQueryInfoKeyW(hServicesKey,
                               NULL,
                               NULL,
                               NULL,
                               &dwNumSubKeys,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    if (dwError != ERROR_SUCCESS)
    {
        DPRINT("ERROR! Unable to get number of services keys.\n");
        return dwError;
    }

    /* Iterate the service keys to see if another service depends on the this service */
    for (dwIteration = 0; dwIteration < dwNumSubKeys; dwIteration++)
    {
        dwSize = MAX_PATH;
        dwError = RegEnumKeyExW(hServicesKey,
                                dwIteration,
                                lpszNameBuf,
                                &dwSize,
                                NULL,
                                NULL,
                                NULL,
                                NULL);
        if (dwError != ERROR_SUCCESS)
            return dwError;

        /* Open the Service key */
        dwError = RegOpenKeyExW(hServicesKey,
                                lpszNameBuf,
                                0,
                                KEY_READ,
                                &hServiceEnumKey);
        if (dwError != ERROR_SUCCESS)
            return dwError;

        dwSize = MAX_PATH;

        /* Check for the DependOnService Value */
        dwError = RegQueryValueExW(hServiceEnumKey,
                                   L"DependOnService",
                                   NULL,
                                   NULL,
                                   (LPBYTE)lpszValueBuf,
                                   &dwSize);

        /* FIXME: Handle load order. */

        /* If the service found has a DependOnService value */
        if (dwError == ERROR_SUCCESS)
        {
            dwDependServiceStrPtr = 0;

            /* Can be more than one Dependencies in the DependOnService string */
            while (wcslen(lpszValueBuf + dwDependServiceStrPtr) > 0)
            {
                if (_wcsicmp(lpszValueBuf + dwDependServiceStrPtr, lpService->lpServiceName) == 0)
                {
                    /* Get the current enumed service pointer */
                    lpCurrentService = ScmGetServiceEntryByName(lpszNameBuf);

                    /* Check for valid Service */
                    if (!lpCurrentService)
                    {
                        /* This should never happen! */
                        DPRINT("This should not happen at this point, report to Developer\n");
                        return ERROR_NOT_FOUND;
                    }

                    /* Determine state the service is in */
                    if (lpCurrentService->Status.dwCurrentState == SERVICE_STOPPED)
                        dwCurrentServiceState = SERVICE_INACTIVE;

                    /* If the ServiceState matches that requested or searching for SERVICE_STATE_ALL */
                    if ((dwCurrentServiceState == dwServiceState) ||
                        (dwServiceState == SERVICE_STATE_ALL))
                    {
                        /* Calculate the required size */
                        dwRequiredSize += sizeof(SERVICE_STATUS);
                        dwRequiredSize += (DWORD)((wcslen(lpCurrentService->lpServiceName) + 1) * sizeof(WCHAR));
                        dwRequiredSize += (DWORD)((wcslen(lpCurrentService->lpDisplayName) + 1) * sizeof(WCHAR));

                        /* Add the size for service name and display name pointers */
                        dwRequiredSize += (2 * sizeof(PVOID));

                        /* increase the BytesNeeded size */
                        *pcbBytesNeeded = *pcbBytesNeeded + dwRequiredSize;

                        /* Don't fill callers buffer yet, as MSDN read that the last service with dependency
                           comes first */

                        /* Recursive call to check for its dependencies */
                        Int_EnumDependentServicesW(hServicesKey,
                                                   lpCurrentService,
                                                   dwServiceState,
                                                   lpServices,
                                                   pcbBytesNeeded,
                                                   lpServicesReturned);

                        /* If the lpServices is valid set the service pointer */
                        if (lpServices)
                            lpServices[*lpServicesReturned] = lpCurrentService;

                        *lpServicesReturned = *lpServicesReturned + 1;
                    }
                }

                dwDependServiceStrPtr += (DWORD)(wcslen(lpszValueBuf + dwDependServiceStrPtr) + 1);
            }
        }
        else if (*pcbBytesNeeded)
        {
            dwError = ERROR_SUCCESS;
        }

        RegCloseKey(hServiceEnumKey);
    }

    return dwError;
}


/* Function 0 */
DWORD RCloseServiceHandle(
    LPSC_RPC_HANDLE hSCObject)
{
    PMANAGER_HANDLE hManager;
    PSERVICE_HANDLE hService;
    PSERVICE lpService;
    HKEY hServicesKey;
    DWORD dwError;
    DWORD pcbBytesNeeded = 0;
    DWORD dwServicesReturned = 0;

    DPRINT("RCloseServiceHandle() called\n");

    DPRINT("hSCObject = %p\n", *hSCObject);

    if (*hSCObject == 0)
        return ERROR_INVALID_HANDLE;

    hManager = ScmGetServiceManagerFromHandle(*hSCObject);
    hService = ScmGetServiceFromHandle(*hSCObject);

    if (hManager != NULL)
    {
        DPRINT("Found manager handle\n");

        /* FIXME: add handle cleanup code */

        HeapFree(GetProcessHeap(), 0, hManager);
        hManager = NULL;

        *hSCObject = NULL;

        DPRINT("RCloseServiceHandle() done\n");
        return ERROR_SUCCESS;
    }
    else if (hService != NULL)
    {
        DPRINT("Found service handle\n");

        /* Lock the service database exlusively */
        ScmLockDatabaseExclusive();

        /* Get the pointer to the service record */
        lpService = hService->ServiceEntry;

        /* FIXME: add handle cleanup code */

        /* Free the handle */
        HeapFree(GetProcessHeap(), 0, hService);
        hService = NULL;

        ASSERT(lpService->dwRefCount > 0);

        lpService->dwRefCount--;
        DPRINT("CloseServiceHandle - lpService->dwRefCount %u\n",
               lpService->dwRefCount);

        if (lpService->dwRefCount == 0)
        {
            /* If this service has been marked for deletion */
            if (lpService->bDeleted)
            {
                /* Open the Services Reg key */
                dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                        L"System\\CurrentControlSet\\Services",
                                        0,
                                        KEY_SET_VALUE | KEY_READ,
                                        &hServicesKey);
                if (dwError != ERROR_SUCCESS)
                {
                    DPRINT("Failed to open services key\n");
                    ScmUnlockDatabase();
                    return dwError;
                }

                /* Call the internal function with NULL, just to get bytes we need */
                Int_EnumDependentServicesW(hServicesKey,
                                           lpService,
                                           SERVICE_ACTIVE,
                                           NULL,
                                           &pcbBytesNeeded,
                                           &dwServicesReturned);

                /* if pcbBytesNeeded returned a value then there are services running that are dependent on this service */
                if (pcbBytesNeeded)
                {
                    DPRINT("Deletion failed due to running dependencies.\n");
                    RegCloseKey(hServicesKey);
                    ScmUnlockDatabase();
                    return ERROR_SUCCESS;
                }

                /* There are no references and no runnning dependencies,
                   it is now safe to delete the service */

                /* Delete the Service Key */
                dwError = RegDeleteKeyW(hServicesKey,
                                        lpService->lpServiceName);

                RegCloseKey(hServicesKey);

                if (dwError != ERROR_SUCCESS)
                {
                    DPRINT("Failed to Delete the Service Registry key\n");
                    ScmUnlockDatabase();
                    return dwError;
                }

                /* Delete the Service */
                ScmDeleteServiceRecord(lpService);
            }
        }

        ScmUnlockDatabase();

        *hSCObject = NULL;

        DPRINT("RCloseServiceHandle() done\n");
        return ERROR_SUCCESS;
    }

    DPRINT("Invalid handle tag (Tag %lx)\n", hManager->Handle.Tag);

    return ERROR_INVALID_HANDLE;
}


/* Function 1 */
DWORD RControlService(
    SC_RPC_HANDLE hService,
    DWORD dwControl,
    LPSERVICE_STATUS lpServiceStatus)
{
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;
    ACCESS_MASK DesiredAccess;
    DWORD dwError = ERROR_SUCCESS;
    DWORD pcbBytesNeeded = 0;
    DWORD dwServicesReturned = 0;
    DWORD dwControlsAccepted;
    DWORD dwCurrentState;
    HKEY hServicesKey = NULL;

    DPRINT("RControlService() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    /* Check the service handle */
    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Check the service entry point */
    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT1("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Check access rights */
    switch (dwControl)
    {
        case SERVICE_CONTROL_STOP:
            DesiredAccess = SERVICE_STOP;
            break;

        case SERVICE_CONTROL_PAUSE:
        case SERVICE_CONTROL_CONTINUE:
            DesiredAccess = SERVICE_PAUSE_CONTINUE;
            break;

        case SERVICE_CONTROL_INTERROGATE:
            DesiredAccess = SERVICE_INTERROGATE;
            break;

        default:
            if (dwControl >= 128 && dwControl <= 255)
                DesiredAccess = SERVICE_USER_DEFINED_CONTROL;
            else
                return ERROR_INVALID_PARAMETER;
            break;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  DesiredAccess))
        return ERROR_ACCESS_DENIED;

    /* Return the current service status information */
    RtlCopyMemory(lpServiceStatus,
                  &lpService->Status,
                  sizeof(SERVICE_STATUS));

    if (dwControl == SERVICE_CONTROL_STOP)
    {
        /* Check if the service has dependencies running as windows
           doesn't stop a service that does */

        /* Open the Services Reg key */
        dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                L"System\\CurrentControlSet\\Services",
                                0,
                                KEY_READ,
                                &hServicesKey);
        if (dwError != ERROR_SUCCESS)
        {
            DPRINT("Failed to open services key\n");
            return dwError;
        }

        /* Call the internal function with NULL, just to get bytes we need */
        Int_EnumDependentServicesW(hServicesKey,
                                   lpService,
                                   SERVICE_ACTIVE,
                                   NULL,
                                   &pcbBytesNeeded,
                                   &dwServicesReturned);

        RegCloseKey(hServicesKey);

        /* If pcbBytesNeeded is not zero then there are services running that
           are dependent on this service */
        if (pcbBytesNeeded != 0)
        {
            DPRINT("Service has running dependencies. Failed to stop service.\n");
            return ERROR_DEPENDENT_SERVICES_RUNNING;
        }
    }

    if (lpService->Status.dwServiceType & SERVICE_DRIVER)
    {
        /* Send control code to the driver */
        dwError = ScmControlDriver(lpService,
                                   dwControl,
                                   lpServiceStatus);
    }
    else
    {
        dwControlsAccepted = lpService->Status.dwControlsAccepted;
        dwCurrentState = lpService->Status.dwCurrentState;

        /* Return ERROR_SERVICE_NOT_ACTIVE if the service has not been started */
        if (lpService->lpImage == NULL || dwCurrentState == SERVICE_STOPPED)
            return ERROR_SERVICE_NOT_ACTIVE;

        /* Check the current state before sending a control request */
        switch (dwCurrentState)
        {
            case SERVICE_STOP_PENDING:
            case SERVICE_STOPPED:
                return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;

            case SERVICE_START_PENDING:
                switch (dwControl)
                {
                    case SERVICE_CONTROL_STOP:
                        break;

                    case SERVICE_CONTROL_INTERROGATE:
                        RtlCopyMemory(lpServiceStatus,
                                      &lpService->Status,
                                      sizeof(SERVICE_STATUS));
                        return ERROR_SUCCESS;

                    default:
                        return ERROR_SERVICE_CANNOT_ACCEPT_CTRL;
                }
                break;
        }

        /* Check if the control code is acceptable to the service */
        switch (dwControl)
        {
            case SERVICE_CONTROL_STOP:
                if ((dwControlsAccepted & SERVICE_ACCEPT_STOP) == 0)
                    return ERROR_INVALID_SERVICE_CONTROL;
                break;

            case SERVICE_CONTROL_PAUSE:
            case SERVICE_CONTROL_CONTINUE:
                if ((dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) == 0)
                    return ERROR_INVALID_SERVICE_CONTROL;
                break;
        }

        /* Send control code to the service */
        dwError = ScmControlService(lpService,
                                    dwControl);

        /* Return service status information */
        RtlCopyMemory(lpServiceStatus,
                      &lpService->Status,
                      sizeof(SERVICE_STATUS));
    }

    return dwError;
}


/* Function 2 */
DWORD RDeleteService(
    SC_RPC_HANDLE hService)
{
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;
    DWORD dwError;

    DPRINT("RDeleteService() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  DELETE))
        return ERROR_ACCESS_DENIED;

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    if (lpService->bDeleted)
    {
        DPRINT("The service has already been marked for delete!\n");
        dwError = ERROR_SERVICE_MARKED_FOR_DELETE;
        goto Done;
    }

    /* Mark service for delete */
    lpService->bDeleted = TRUE;

    dwError = ScmMarkServiceForDelete(lpService);

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("RDeleteService() done\n");

    return dwError;
}


/* Function 3 */
DWORD RLockServiceDatabase(
    SC_RPC_HANDLE hSCManager,
    LPSC_RPC_LOCK lpLock)
{
    PMANAGER_HANDLE hMgr;

    DPRINT("RLockServiceDatabase() called\n");

    *lpLock = NULL;

    hMgr = ScmGetServiceManagerFromHandle(hSCManager);
    if (hMgr == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hMgr->Handle.DesiredAccess,
                                  SC_MANAGER_LOCK))
        return ERROR_ACCESS_DENIED;

    return ScmAcquireServiceStartLock(FALSE, lpLock);
}


/* Function 4 */
DWORD RQueryServiceObjectSecurity(
    SC_RPC_HANDLE hService,
    SECURITY_INFORMATION dwSecurityInformation,
    LPBYTE lpSecurityDescriptor,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded)
{
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;
    ULONG DesiredAccess = 0;
    NTSTATUS Status;
    DWORD dwBytesNeeded;
    DWORD dwError;


    SECURITY_DESCRIPTOR ObjectDescriptor;

    DPRINT("RQueryServiceObjectSecurity() called\n");

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (dwSecurityInformation & (DACL_SECURITY_INFORMATION |
                                 GROUP_SECURITY_INFORMATION |
                                 OWNER_SECURITY_INFORMATION))
        DesiredAccess |= READ_CONTROL;

    if (dwSecurityInformation & SACL_SECURITY_INFORMATION)
        DesiredAccess |= ACCESS_SYSTEM_SECURITY;

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  DesiredAccess))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database */
    ScmLockDatabaseShared();


    /* hack */
    Status = RtlCreateSecurityDescriptor(&ObjectDescriptor, SECURITY_DESCRIPTOR_REVISION);

    Status = RtlQuerySecurityObject(&ObjectDescriptor  /* lpService->lpSecurityDescriptor */,
                                    dwSecurityInformation,
                                    (PSECURITY_DESCRIPTOR)lpSecurityDescriptor,
                                    cbBufSize,
                                    &dwBytesNeeded);

    /* Unlock the service database */
    ScmUnlockDatabase();

    if (NT_SUCCESS(Status))
    {
        *pcbBytesNeeded = dwBytesNeeded;
        dwError = STATUS_SUCCESS;
    }
    else if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        *pcbBytesNeeded = dwBytesNeeded;
        dwError = ERROR_INSUFFICIENT_BUFFER;
    }
    else if (Status == STATUS_BAD_DESCRIPTOR_FORMAT)
    {
        dwError = ERROR_GEN_FAILURE;
    }
    else
    {
        dwError = RtlNtStatusToDosError(Status);
    }

    return dwError;
}


/* Function 5 */
DWORD RSetServiceObjectSecurity(
    SC_RPC_HANDLE hService,
    DWORD dwSecurityInformation,
    LPBYTE lpSecurityDescriptor,
    DWORD dwSecuityDescriptorSize)
{
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;
    ULONG DesiredAccess = 0;
    /* HANDLE hToken = NULL; */
    HKEY hServiceKey;
    /* NTSTATUS Status; */
    DWORD dwError;

    DPRINT("RSetServiceObjectSecurity() called\n");

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (dwSecurityInformation == 0 ||
        dwSecurityInformation & ~(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION
        | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION))
        return ERROR_INVALID_PARAMETER;

    if (!RtlValidSecurityDescriptor((PSECURITY_DESCRIPTOR)lpSecurityDescriptor))
        return ERROR_INVALID_PARAMETER;

    if (dwSecurityInformation & SACL_SECURITY_INFORMATION)
        DesiredAccess |= ACCESS_SYSTEM_SECURITY;

    if (dwSecurityInformation & DACL_SECURITY_INFORMATION)
        DesiredAccess |= WRITE_DAC;

    if (dwSecurityInformation & (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION))
        DesiredAccess |= WRITE_OWNER;

    if ((dwSecurityInformation & OWNER_SECURITY_INFORMATION) &&
        (((PISECURITY_DESCRIPTOR)lpSecurityDescriptor)->Owner == NULL))
        return ERROR_INVALID_PARAMETER;

    if ((dwSecurityInformation & GROUP_SECURITY_INFORMATION) &&
        (((PISECURITY_DESCRIPTOR)lpSecurityDescriptor)->Group == NULL))
        return ERROR_INVALID_PARAMETER;

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  DesiredAccess))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (lpService->bDeleted)
        return ERROR_SERVICE_MARKED_FOR_DELETE;

#if 0
    RpcImpersonateClient(NULL);

    Status = NtOpenThreadToken(NtCurrentThread(),
                               8,
                               TRUE,
                               &hToken);
    if (!NT_SUCCESS(Status))
        return RtlNtStatusToDosError(Status);

    RpcRevertToSelf();
#endif

    /* Lock the service database exclusive */
    ScmLockDatabaseExclusive();

#if 0
    Status = RtlSetSecurityObject(dwSecurityInformation,
                                  (PSECURITY_DESCRIPTOR)lpSecurityDescriptor,
                                  &lpService->lpSecurityDescriptor,
                                  &ScmServiceMapping,
                                  hToken);
    if (!NT_SUCCESS(Status))
    {
        dwError = RtlNtStatusToDosError(Status);
        goto Done;
    }
#endif

    dwError = ScmOpenServiceKey(lpService->lpServiceName,
                                READ_CONTROL | KEY_CREATE_SUB_KEY | KEY_SET_VALUE,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    UNIMPLEMENTED;
    dwError = ERROR_SUCCESS;
//    dwError = ScmWriteSecurityDescriptor(hServiceKey,
//                                         lpService->lpSecurityDescriptor);

    RegFlushKey(hServiceKey);
    RegCloseKey(hServiceKey);

Done:

#if 0
    if (hToken != NULL)
        NtClose(hToken);
#endif

    /* Unlock service database */
    ScmUnlockDatabase();

    DPRINT("RSetServiceObjectSecurity() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 6 */
DWORD RQueryServiceStatus(
    SC_RPC_HANDLE hService,
    LPSERVICE_STATUS lpServiceStatus)
{
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;

    DPRINT("RQueryServiceStatus() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_STATUS))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    /* Return service status information */
    RtlCopyMemory(lpServiceStatus,
                  &lpService->Status,
                  sizeof(SERVICE_STATUS));

    /* Unlock the service database */
    ScmUnlockDatabase();

    return ERROR_SUCCESS;
}


static BOOL
ScmIsValidServiceState(DWORD dwCurrentState)
{
    switch (dwCurrentState)
    {
        case SERVICE_STOPPED:
        case SERVICE_START_PENDING:
        case SERVICE_STOP_PENDING:
        case SERVICE_RUNNING:
        case SERVICE_CONTINUE_PENDING:
        case SERVICE_PAUSE_PENDING:
        case SERVICE_PAUSED:
            return TRUE;

        default:
            return FALSE;
    }
}


/* Function 7 */
DWORD RSetServiceStatus(
    RPC_SERVICE_STATUS_HANDLE hServiceStatus,
    LPSERVICE_STATUS lpServiceStatus)
{
    PSERVICE lpService;
    DWORD dwPreviousState;
    LPCWSTR lpErrorStrings[2];
    WCHAR szErrorBuffer[32];

    DPRINT("RSetServiceStatus() called\n");
    DPRINT("hServiceStatus = %lu\n", hServiceStatus);
    DPRINT("dwServiceType = %lu\n", lpServiceStatus->dwServiceType);
    DPRINT("dwCurrentState = %lu\n", lpServiceStatus->dwCurrentState);
    DPRINT("dwControlsAccepted = %lu\n", lpServiceStatus->dwControlsAccepted);
    DPRINT("dwWin32ExitCode = %lu\n", lpServiceStatus->dwWin32ExitCode);
    DPRINT("dwServiceSpecificExitCode = %lu\n", lpServiceStatus->dwServiceSpecificExitCode);
    DPRINT("dwCheckPoint = %lu\n", lpServiceStatus->dwCheckPoint);
    DPRINT("dwWaitHint = %lu\n", lpServiceStatus->dwWaitHint);

    if (hServiceStatus == 0)
    {
        DPRINT("hServiceStatus == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    lpService = (PSERVICE)hServiceStatus;

    /* Check current state */
    if (!ScmIsValidServiceState(lpServiceStatus->dwCurrentState))
    {
        DPRINT("Invalid service state!\n");
        return ERROR_INVALID_DATA;
    }

    /* Check service type */
    if (!(lpServiceStatus->dwServiceType & SERVICE_WIN32) &&
         (lpServiceStatus->dwServiceType & SERVICE_DRIVER))
    {
        DPRINT("Invalid service type!\n");
        return ERROR_INVALID_DATA;
    }

    /* Check accepted controls */
    if (lpServiceStatus->dwControlsAccepted & ~0xFF)
    {
        DPRINT("Invalid controls accepted!\n");
        return ERROR_INVALID_DATA;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    /* Save the current service state */
    dwPreviousState = lpService->Status.dwCurrentState;

    RtlCopyMemory(&lpService->Status,
                  lpServiceStatus,
                  sizeof(SERVICE_STATUS));

    /* Unlock the service database */
    ScmUnlockDatabase();

    /* Log a failed service stop */
    if ((lpServiceStatus->dwCurrentState == SERVICE_STOPPED) &&
        (dwPreviousState != SERVICE_STOPPED))
    {
        if (lpServiceStatus->dwWin32ExitCode != ERROR_SUCCESS)
        {
            swprintf(szErrorBuffer, L"%lu", lpServiceStatus->dwWin32ExitCode);
            lpErrorStrings[0] = lpService->lpDisplayName;
            lpErrorStrings[1] = szErrorBuffer;

            ScmLogError(EVENT_SERVICE_EXIT_FAILED,
                        2,
                        lpErrorStrings);
        }
    }

    DPRINT("Set %S to %lu\n", lpService->lpDisplayName, lpService->Status.dwCurrentState);
    DPRINT("RSetServiceStatus() done\n");

    return ERROR_SUCCESS;
}


/* Function 8 */
DWORD RUnlockServiceDatabase(
    LPSC_RPC_LOCK Lock)
{
    DPRINT("RUnlockServiceDatabase(%p)\n", Lock);
    return ScmReleaseServiceStartLock(Lock);
}


/* Function 9 */
DWORD RNotifyBootConfigStatus(
    SVCCTL_HANDLEW lpMachineName,
    DWORD BootAcceptable)
{
    DPRINT1("RNotifyBootConfigStatus(%p %lu) called\n", lpMachineName, BootAcceptable);
    return ERROR_SUCCESS;

//    UNIMPLEMENTED;
//    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 10 */
DWORD RI_ScSetServiceBitsW(
    RPC_SERVICE_STATUS_HANDLE hServiceStatus,
    DWORD dwServiceBits,
    int bSetBitsOn,
    int bUpdateImmediately,
    wchar_t *lpString)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 11 */
DWORD RChangeServiceConfigW(
    SC_RPC_HANDLE hService,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPWSTR lpBinaryPathName,
    LPWSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPWSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPWSTR lpDisplayName)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    LPWSTR lpDisplayNameW = NULL;
    LPWSTR lpImagePathW = NULL;

    DPRINT("RChangeServiceConfigW() called\n");
    DPRINT("dwServiceType = %lu\n", dwServiceType);
    DPRINT("dwStartType = %lu\n", dwStartType);
    DPRINT("dwErrorControl = %lu\n", dwErrorControl);
    DPRINT("lpBinaryPathName = %S\n", lpBinaryPathName);
    DPRINT("lpLoadOrderGroup = %S\n", lpLoadOrderGroup);
    DPRINT("lpDisplayName = %S\n", lpDisplayName);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_CHANGE_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    if (lpService->bDeleted)
    {
        DPRINT("The service has already been marked for delete!\n");
        dwError = ERROR_SERVICE_MARKED_FOR_DELETE;
        goto done;
    }

    /* Open the service key */
    dwError = ScmOpenServiceKey(lpService->szServiceName,
                                KEY_SET_VALUE,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Write service data to the registry */
    /* Set the display name */
    if (lpDisplayName != NULL && *lpDisplayName != 0)
    {
        RegSetValueExW(hServiceKey,
                       L"DisplayName",
                       0,
                       REG_SZ,
                       (LPBYTE)lpDisplayName,
                       (DWORD)((wcslen(lpDisplayName) + 1) * sizeof(WCHAR)));

        /* Update the display name */
        lpDisplayNameW = HeapAlloc(GetProcessHeap(),
                                   HEAP_ZERO_MEMORY,
                                   (wcslen(lpDisplayName) + 1) * sizeof(WCHAR));
        if (lpDisplayNameW == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }

        if (lpService->lpDisplayName != lpService->lpServiceName)
            HeapFree(GetProcessHeap(), 0, lpService->lpDisplayName);

        lpService->lpDisplayName = lpDisplayNameW;
    }

    if (dwServiceType != SERVICE_NO_CHANGE)
    {
        /* Set the service type */
        dwError = RegSetValueExW(hServiceKey,
                                 L"Type",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwServiceType,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->Status.dwServiceType = dwServiceType;
    }

    if (dwStartType != SERVICE_NO_CHANGE)
    {
        /* Set the start value */
        dwError = RegSetValueExW(hServiceKey,
                                 L"Start",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwStartType,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->dwStartType = dwStartType;
    }

    if (dwErrorControl != SERVICE_NO_CHANGE)
    {
        /* Set the error control value */
        dwError = RegSetValueExW(hServiceKey,
                                 L"ErrorControl",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwErrorControl,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->dwErrorControl = dwErrorControl;
    }

    if (lpBinaryPathName != NULL && *lpBinaryPathName != 0)
    {
        /* Set the image path */
        lpImagePathW = lpBinaryPathName;

        if (lpService->Status.dwServiceType & SERVICE_DRIVER)
        {
            dwError = ScmCanonDriverImagePath(lpService->dwStartType,
                                              lpBinaryPathName,
                                              &lpImagePathW);

            if (dwError != ERROR_SUCCESS)
                goto done;
        }

        dwError = RegSetValueExW(hServiceKey,
                                 L"ImagePath",
                                 0,
                                 REG_EXPAND_SZ,
                                 (LPBYTE)lpImagePathW,
                                 (DWORD)((wcslen(lpImagePathW) + 1) * sizeof(WCHAR)));

        if (lpImagePathW != lpBinaryPathName)
            HeapFree(GetProcessHeap(), 0, lpImagePathW);

        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Set the group name */
    if (lpLoadOrderGroup != NULL && *lpLoadOrderGroup != 0)
    {
        dwError = RegSetValueExW(hServiceKey,
                                 L"Group",
                                 0,
                                 REG_SZ,
                                 (LPBYTE)lpLoadOrderGroup,
                                 (DWORD)((wcslen(lpLoadOrderGroup) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
            goto done;

        dwError = ScmSetServiceGroup(lpService,
                                     lpLoadOrderGroup);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    if (lpdwTagId != NULL)
    {
        dwError = ScmAssignNewTag(lpService);
        if (dwError != ERROR_SUCCESS)
            goto done;

        dwError = RegSetValueExW(hServiceKey,
                                 L"Tag",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&lpService->dwTag,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        *lpdwTagId = lpService->dwTag;
    }

    /* Write dependencies */
    if (lpDependencies != NULL && *lpDependencies != 0)
    {
        dwError = ScmWriteDependencies(hServiceKey,
                                       (LPWSTR)lpDependencies,
                                       dwDependSize);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    if (lpPassword != NULL)
    {
        /* FIXME: Decrypt and write password */
    }

done:
    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("RChangeServiceConfigW() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 12 */
DWORD RCreateServiceW(
    SC_RPC_HANDLE hSCManager,
    LPCWSTR lpServiceName,
    LPCWSTR lpDisplayName,
    DWORD dwDesiredAccess,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPCWSTR lpBinaryPathName,
    LPCWSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPCWSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    PMANAGER_HANDLE hManager;
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE lpService = NULL;
    SC_HANDLE hServiceHandle = NULL;
    LPWSTR lpImagePath = NULL;
    HKEY hServiceKey = NULL;
    LPWSTR lpObjectName;

    DPRINT("RCreateServiceW() called\n");
    DPRINT("lpServiceName = %S\n", lpServiceName);
    DPRINT("lpDisplayName = %S\n", lpDisplayName);
    DPRINT("dwDesiredAccess = %lx\n", dwDesiredAccess);
    DPRINT("dwServiceType = %lu\n", dwServiceType);
    DPRINT("dwStartType = %lu\n", dwStartType);
    DPRINT("dwErrorControl = %lu\n", dwErrorControl);
    DPRINT("lpBinaryPathName = %S\n", lpBinaryPathName);
    DPRINT("lpLoadOrderGroup = %S\n", lpLoadOrderGroup);
    DPRINT("lpdwTagId = %p\n", lpdwTagId);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hManager = ScmGetServiceManagerFromHandle(hSCManager);
    if (hManager == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Check access rights */
    if (!RtlAreAllAccessesGranted(hManager->Handle.DesiredAccess,
                                  SC_MANAGER_CREATE_SERVICE))
    {
        DPRINT("Insufficient access rights! 0x%lx\n",
               hManager->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    if (wcslen(lpServiceName) == 0)
    {
        return ERROR_INVALID_NAME;
    }

    if (wcslen(lpBinaryPathName) == 0)
    {
        return ERROR_INVALID_PARAMETER;
    }

    /* Check for invalid service type value */
    if ((dwServiceType != SERVICE_KERNEL_DRIVER) &&
        (dwServiceType != SERVICE_FILE_SYSTEM_DRIVER) &&
        ((dwServiceType & ~SERVICE_INTERACTIVE_PROCESS) != SERVICE_WIN32_OWN_PROCESS) &&
        ((dwServiceType & ~SERVICE_INTERACTIVE_PROCESS) != SERVICE_WIN32_SHARE_PROCESS))
            return ERROR_INVALID_PARAMETER;

    /* Check for invalid start type value */
    if ((dwStartType != SERVICE_BOOT_START) &&
        (dwStartType != SERVICE_SYSTEM_START) &&
        (dwStartType != SERVICE_AUTO_START) &&
        (dwStartType != SERVICE_DEMAND_START) &&
        (dwStartType != SERVICE_DISABLED))
        return ERROR_INVALID_PARAMETER;

    /* Only drivers can be boot start or system start services */
    if ((dwStartType == SERVICE_BOOT_START) ||
        (dwStartType == SERVICE_SYSTEM_START))
    {
        if ((dwServiceType != SERVICE_KERNEL_DRIVER) &&
            (dwServiceType != SERVICE_FILE_SYSTEM_DRIVER))
            return ERROR_INVALID_PARAMETER;
    }

    /* Check for invalid error control value */
    if ((dwErrorControl != SERVICE_ERROR_IGNORE) &&
        (dwErrorControl != SERVICE_ERROR_NORMAL) &&
        (dwErrorControl != SERVICE_ERROR_SEVERE) &&
        (dwErrorControl != SERVICE_ERROR_CRITICAL))
        return ERROR_INVALID_PARAMETER;

    if ((dwServiceType == (SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS)) &&
        (lpServiceStartName))
    {
        return ERROR_INVALID_PARAMETER;
    }

    if (lpdwTagId && (!lpLoadOrderGroup || !*lpLoadOrderGroup))
    {
        return ERROR_INVALID_PARAMETER;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    lpService = ScmGetServiceEntryByName(lpServiceName);
    if (lpService)
    {
        /* Unlock the service database */
        ScmUnlockDatabase();

        /* Check if it is marked for deletion */
        if (lpService->bDeleted)
            return ERROR_SERVICE_MARKED_FOR_DELETE;

        /* Return Error exist */
        return ERROR_SERVICE_EXISTS;
    }

    if (lpDisplayName != NULL &&
        ScmGetServiceEntryByDisplayName(lpDisplayName) != NULL)
    {
        /* Unlock the service database */
        ScmUnlockDatabase();

        return ERROR_DUPLICATE_SERVICE_NAME;
    }

    if (dwServiceType & SERVICE_DRIVER)
    {
        dwError = ScmCanonDriverImagePath(dwStartType,
                                          lpBinaryPathName,
                                          &lpImagePath);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }
    else
    {
        if (dwStartType == SERVICE_BOOT_START ||
            dwStartType == SERVICE_SYSTEM_START)
        {
            /* Unlock the service database */
            ScmUnlockDatabase();

            return ERROR_INVALID_PARAMETER;
        }
    }

    /* Allocate a new service entry */
    dwError = ScmCreateNewServiceRecord(lpServiceName,
                                        &lpService);
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Fill the new service entry */
    lpService->Status.dwServiceType = dwServiceType;
    lpService->dwStartType = dwStartType;
    lpService->dwErrorControl = dwErrorControl;

    /* Fill the display name */
    if (lpDisplayName != NULL &&
        *lpDisplayName != 0 &&
        _wcsicmp(lpService->lpDisplayName, lpDisplayName) != 0)
    {
        lpService->lpDisplayName = HeapAlloc(GetProcessHeap(),
                                             HEAP_ZERO_MEMORY,
                                             (wcslen(lpDisplayName) + 1) * sizeof(WCHAR));
        if (lpService->lpDisplayName == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }
        wcscpy(lpService->lpDisplayName, lpDisplayName);
    }

    /* Assign the service to a group */
    if (lpLoadOrderGroup != NULL && *lpLoadOrderGroup != 0)
    {
        dwError = ScmSetServiceGroup(lpService,
                                     lpLoadOrderGroup);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Assign a new tag */
    if (lpdwTagId != NULL)
    {
        dwError = ScmAssignNewTag(lpService);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Write service data to the registry */
    /* Create the service key */
    dwError = ScmCreateServiceKey(lpServiceName,
                                  KEY_WRITE,
                                  &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Set the display name */
    if (lpDisplayName != NULL && *lpDisplayName != 0)
    {
        RegSetValueExW(hServiceKey,
                       L"DisplayName",
                       0,
                       REG_SZ,
                       (LPBYTE)lpDisplayName,
                       (DWORD)((wcslen(lpDisplayName) + 1) * sizeof(WCHAR)));
    }

    /* Set the service type */
    dwError = RegSetValueExW(hServiceKey,
                             L"Type",
                             0,
                             REG_DWORD,
                             (LPBYTE)&dwServiceType,
                             sizeof(DWORD));
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Set the start value */
    dwError = RegSetValueExW(hServiceKey,
                             L"Start",
                             0,
                             REG_DWORD,
                             (LPBYTE)&dwStartType,
                             sizeof(DWORD));
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Set the error control value */
    dwError = RegSetValueExW(hServiceKey,
                             L"ErrorControl",
                             0,
                             REG_DWORD,
                             (LPBYTE)&dwErrorControl,
                             sizeof(DWORD));
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Set the image path */
    if (dwServiceType & SERVICE_WIN32)
    {
        dwError = RegSetValueExW(hServiceKey,
                                 L"ImagePath",
                                 0,
                                 REG_EXPAND_SZ,
                                 (LPBYTE)lpBinaryPathName,
                                 (DWORD)((wcslen(lpBinaryPathName) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
            goto done;
    }
    else if (dwServiceType & SERVICE_DRIVER)
    {
        dwError = RegSetValueExW(hServiceKey,
                                 L"ImagePath",
                                 0,
                                 REG_EXPAND_SZ,
                                 (LPBYTE)lpImagePath,
                                 (DWORD)((wcslen(lpImagePath) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Set the group name */
    if (lpLoadOrderGroup != NULL && *lpLoadOrderGroup != 0)
    {
        dwError = RegSetValueExW(hServiceKey,
                                 L"Group",
                                 0,
                                 REG_SZ,
                                 (LPBYTE)lpLoadOrderGroup,
                                 (DWORD)((wcslen(lpLoadOrderGroup) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    if (lpdwTagId != NULL)
    {
        dwError = RegSetValueExW(hServiceKey,
                                 L"Tag",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&lpService->dwTag,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Write dependencies */
    if (lpDependencies != NULL && *lpDependencies != 0)
    {
        dwError = ScmWriteDependencies(hServiceKey,
                                       (LPCWSTR)lpDependencies,
                                       dwDependSize);
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Write service start name */
    if (dwServiceType & SERVICE_WIN32)
    {
        lpObjectName = (lpServiceStartName != NULL) ? (LPWSTR)lpServiceStartName : L"LocalSystem";
        dwError = RegSetValueExW(hServiceKey,
                                 L"ObjectName",
                                 0,
                                 REG_SZ,
                                 (LPBYTE)lpObjectName,
                                 (DWORD)((wcslen(lpObjectName) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    if (lpPassword != NULL)
    {
        /* FIXME: Decrypt and write password */
    }

    dwError = ScmCreateServiceHandle(lpService,
                                     &hServiceHandle);
    if (dwError != ERROR_SUCCESS)
        goto done;

    dwError = ScmCheckAccess(hServiceHandle,
                             dwDesiredAccess);
    if (dwError != ERROR_SUCCESS)
        goto done;

    lpService->dwRefCount = 1;
    DPRINT("CreateService - lpService->dwRefCount %u\n", lpService->dwRefCount);

done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    if (dwError == ERROR_SUCCESS)
    {
        DPRINT("hService %p\n", hServiceHandle);
        *lpServiceHandle = (SC_RPC_HANDLE)hServiceHandle;

        if (lpdwTagId != NULL)
            *lpdwTagId = lpService->dwTag;
    }
    else
    {
        if (lpService != NULL &&
            lpService->lpServiceName != NULL)
        {
            /* Release the display name buffer */
            HeapFree(GetProcessHeap(), 0, lpService->lpDisplayName);
        }

        if (hServiceHandle)
        {
            /* Remove the service handle */
            HeapFree(GetProcessHeap(), 0, hServiceHandle);
        }

        if (lpService != NULL)
        {
            /* FIXME: remove the service entry */
        }
    }

    if (lpImagePath != NULL)
        HeapFree(GetProcessHeap(), 0, lpImagePath);

    DPRINT("RCreateServiceW() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 13 */
DWORD REnumDependentServicesW(
    SC_RPC_HANDLE hService,
    DWORD dwServiceState,
    LPBYTE lpServices,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwServicesReturned = 0;
    DWORD dwServiceCount;
    HKEY hServicesKey = NULL;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    PSERVICE *lpServicesArray = NULL;
    LPENUM_SERVICE_STATUSW lpServicesPtr = NULL;
    LPWSTR lpStr;

    *pcbBytesNeeded = 0;
    *lpServicesReturned = 0;

    DPRINT("REnumDependentServicesW() called\n");

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    lpService = hSvc->ServiceEntry;

    /* Check access rights */
    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SC_MANAGER_ENUMERATE_SERVICE))
    {
        DPRINT("Insufficient access rights! 0x%lx\n",
               hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    /* Open the Services Reg key */
    dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"System\\CurrentControlSet\\Services",
                            0,
                            KEY_READ,
                            &hServicesKey);
    if (dwError != ERROR_SUCCESS)
        return dwError;

    /* First determine the bytes needed and get the number of dependent services */
    dwError = Int_EnumDependentServicesW(hServicesKey,
                                         lpService,
                                         dwServiceState,
                                         NULL,
                                         pcbBytesNeeded,
                                         &dwServicesReturned);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* If buffer size is less than the bytes needed or pointer is null */
    if ((!lpServices) || (cbBufSize < *pcbBytesNeeded))
    {
        dwError = ERROR_MORE_DATA;
        goto Done;
    }

    /* Allocate memory for array of service pointers */
    lpServicesArray = HeapAlloc(GetProcessHeap(),
                                HEAP_ZERO_MEMORY,
                                (dwServicesReturned + 1) * sizeof(PSERVICE));
    if (!lpServicesArray)
    {
        DPRINT1("Could not allocate a buffer!!\n");
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        goto Done;
    }

    dwServicesReturned = 0;
    *pcbBytesNeeded = 0;

    dwError = Int_EnumDependentServicesW(hServicesKey,
                                         lpService,
                                         dwServiceState,
                                         lpServicesArray,
                                         pcbBytesNeeded,
                                         &dwServicesReturned);
    if (dwError != ERROR_SUCCESS)
    {
        goto Done;
    }

    lpServicesPtr = (LPENUM_SERVICE_STATUSW)lpServices;
    lpStr = (LPWSTR)(lpServices + (dwServicesReturned * sizeof(ENUM_SERVICE_STATUSW)));

    /* Copy EnumDepenedentService to Buffer */
    for (dwServiceCount = 0; dwServiceCount < dwServicesReturned; dwServiceCount++)
    {
        lpService = lpServicesArray[dwServiceCount];

        /* Copy status info */
        memcpy(&lpServicesPtr->ServiceStatus,
               &lpService->Status,
               sizeof(SERVICE_STATUS));

        /* Copy display name */
        wcscpy(lpStr, lpService->lpDisplayName);
        lpServicesPtr->lpDisplayName = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServices);
        lpStr += (wcslen(lpService->lpDisplayName) + 1);

        /* Copy service name */
        wcscpy(lpStr, lpService->lpServiceName);
        lpServicesPtr->lpServiceName = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServices);
        lpStr += (wcslen(lpService->lpServiceName) + 1);

        lpServicesPtr++;
    }

    *lpServicesReturned = dwServicesReturned;

Done:
    if (lpServicesArray != NULL)
        HeapFree(GetProcessHeap(), 0, lpServicesArray);

    RegCloseKey(hServicesKey);

    DPRINT("REnumDependentServicesW() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 14 */
DWORD REnumServicesStatusW(
    SC_RPC_HANDLE hSCManager,
    DWORD dwServiceType,
    DWORD dwServiceState,
    LPBYTE lpBuffer,
    DWORD dwBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned,
    LPBOUNDED_DWORD_256K lpResumeHandle)
{
    /* Enumerate all the services, not regarding of their group */
    return REnumServiceGroupW(hSCManager,
                              dwServiceType,
                              dwServiceState,
                              lpBuffer,
                              dwBufSize,
                              pcbBytesNeeded,
                              lpServicesReturned,
                              lpResumeHandle,
                              NULL);
}


/* Function 15 */
DWORD ROpenSCManagerW(
    LPWSTR lpMachineName,
    LPWSTR lpDatabaseName,
    DWORD dwDesiredAccess,
    LPSC_RPC_HANDLE lpScHandle)
{
    DWORD dwError;
    SC_HANDLE hHandle;

    DPRINT("ROpenSCManagerW() called\n");
    DPRINT("lpMachineName = %p\n", lpMachineName);
    DPRINT("lpMachineName: %S\n", lpMachineName);
    DPRINT("lpDataBaseName = %p\n", lpDatabaseName);
    DPRINT("lpDataBaseName: %S\n", lpDatabaseName);
    DPRINT("dwDesiredAccess = %x\n", dwDesiredAccess);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    if (!lpScHandle)
        return ERROR_INVALID_PARAMETER;

    dwError = ScmCreateManagerHandle(lpDatabaseName,
                                     &hHandle);
    if (dwError != ERROR_SUCCESS)
    {
        DPRINT("ScmCreateManagerHandle() failed (Error %lu)\n", dwError);
        return dwError;
    }

    /* Check the desired access */
    dwError = ScmCheckAccess(hHandle,
                             dwDesiredAccess | SC_MANAGER_CONNECT);
    if (dwError != ERROR_SUCCESS)
    {
        DPRINT("ScmCheckAccess() failed (Error %lu)\n", dwError);
        HeapFree(GetProcessHeap(), 0, hHandle);
        return dwError;
    }

    *lpScHandle = (SC_RPC_HANDLE)hHandle;
    DPRINT("*hScm = %p\n", *lpScHandle);

    DPRINT("ROpenSCManagerW() done\n");

    return ERROR_SUCCESS;
}


/* Function 16 */
DWORD ROpenServiceW(
    SC_RPC_HANDLE hSCManager,
    LPWSTR lpServiceName,
    DWORD dwDesiredAccess,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    PSERVICE lpService;
    PMANAGER_HANDLE hManager;
    SC_HANDLE hHandle;
    DWORD dwError = ERROR_SUCCESS;

    DPRINT("ROpenServiceW() called\n");
    DPRINT("hSCManager = %p\n", hSCManager);
    DPRINT("lpServiceName = %p\n", lpServiceName);
    DPRINT("lpServiceName: %S\n", lpServiceName);
    DPRINT("dwDesiredAccess = %x\n", dwDesiredAccess);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hManager = ScmGetServiceManagerFromHandle(hSCManager);
    if (hManager == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!lpServiceHandle)
        return ERROR_INVALID_PARAMETER;

    if (!lpServiceName)
        return ERROR_INVALID_ADDRESS;

    /* Lock the service database exclusive */
    ScmLockDatabaseExclusive();

    /* Get service database entry */
    lpService = ScmGetServiceEntryByName(lpServiceName);
    if (lpService == NULL)
    {
        DPRINT("Could not find a service!\n");
        dwError = ERROR_SERVICE_DOES_NOT_EXIST;
        goto Done;
    }

    /* Create a service handle */
    dwError = ScmCreateServiceHandle(lpService,
                                     &hHandle);
    if (dwError != ERROR_SUCCESS)
    {
        DPRINT("ScmCreateServiceHandle() failed (Error %lu)\n", dwError);
        goto Done;
    }

    /* Check the desired access */
    dwError = ScmCheckAccess(hHandle,
                             dwDesiredAccess);
    if (dwError != ERROR_SUCCESS)
    {
        DPRINT("ScmCheckAccess() failed (Error %lu)\n", dwError);
        HeapFree(GetProcessHeap(), 0, hHandle);
        goto Done;
    }

    lpService->dwRefCount++;
    DPRINT("OpenService - lpService->dwRefCount %u\n",lpService->dwRefCount);

    *lpServiceHandle = (SC_RPC_HANDLE)hHandle;
    DPRINT("*hService = %p\n", *lpServiceHandle);

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("ROpenServiceW() done\n");

    return dwError;
}


/* Function 17 */
DWORD RQueryServiceConfigW(
    SC_RPC_HANDLE hService,
    LPBYTE lpBuf, //LPQUERY_SERVICE_CONFIGW lpServiceConfig,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_8K pcbBytesNeeded)
{
    LPQUERY_SERVICE_CONFIGW lpServiceConfig = (LPQUERY_SERVICE_CONFIGW)lpBuf;
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    LPWSTR lpImagePath = NULL;
    LPWSTR lpServiceStartName = NULL;
    LPWSTR lpDependencies = NULL;
    DWORD dwDependenciesLength = 0;
    DWORD dwRequiredSize;
    WCHAR lpEmptyString[] = {0,0};
    LPWSTR lpStr;

    DPRINT("RQueryServiceConfigW() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    dwError = ScmOpenServiceKey(lpService->lpServiceName,
                                KEY_READ,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* Read the image path */
    dwError = ScmReadString(hServiceKey,
                            L"ImagePath",
                            &lpImagePath);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* Read the service start name */
    ScmReadString(hServiceKey,
                  L"ObjectName",
                  &lpServiceStartName);

    /* Read the dependencies */
    ScmReadDependencies(hServiceKey,
                        &lpDependencies,
                        &dwDependenciesLength);

    dwRequiredSize = sizeof(QUERY_SERVICE_CONFIGW);

    if (lpImagePath != NULL)
        dwRequiredSize += (DWORD)((wcslen(lpImagePath) + 1) * sizeof(WCHAR));
    else
        dwRequiredSize += 2 * sizeof(WCHAR);

    if ((lpService->lpGroup != NULL) && (lpService->lpGroup->lpGroupName != NULL))
        dwRequiredSize += (DWORD)((wcslen(lpService->lpGroup->lpGroupName) + 1) * sizeof(WCHAR));
    else
        dwRequiredSize += 2 * sizeof(WCHAR);

    if (lpDependencies != NULL)
        dwRequiredSize += dwDependenciesLength * sizeof(WCHAR);
    else
        dwRequiredSize += 2 * sizeof(WCHAR);

    if (lpServiceStartName != NULL)
        dwRequiredSize += (DWORD)((wcslen(lpServiceStartName) + 1) * sizeof(WCHAR));
    else
        dwRequiredSize += 2 * sizeof(WCHAR);

    if (lpService->lpDisplayName != NULL)
        dwRequiredSize += (DWORD)((wcslen(lpService->lpDisplayName) + 1) * sizeof(WCHAR));
    else
        dwRequiredSize += 2 * sizeof(WCHAR);

    if (lpServiceConfig == NULL || cbBufSize < dwRequiredSize)
    {
        dwError = ERROR_INSUFFICIENT_BUFFER;
    }
    else
    {
        lpServiceConfig->dwServiceType = lpService->Status.dwServiceType;
        lpServiceConfig->dwStartType = lpService->dwStartType;
        lpServiceConfig->dwErrorControl = lpService->dwErrorControl;
        lpServiceConfig->dwTagId = lpService->dwTag;

        lpStr = (LPWSTR)(lpServiceConfig + 1);

        /* Append the image path */
        if (lpImagePath != NULL)
        {
            wcscpy(lpStr, lpImagePath);
        }
        else
        {
            wcscpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpBinaryPathName = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (wcslen(lpStr) + 1);

        /* Append the group name */
        if (lpService->lpGroup != NULL)
        {
            wcscpy(lpStr, lpService->lpGroup->lpGroupName);
        }
        else
        {
            wcscpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpLoadOrderGroup = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (wcslen(lpStr) + 1);

        /* Append Dependencies */
        if (lpDependencies != NULL)
        {
            memcpy(lpStr,
                   lpDependencies,
                   dwDependenciesLength * sizeof(WCHAR));
        }
        else
        {
            wcscpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpDependencies = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        if (lpDependencies != NULL)
            lpStr += dwDependenciesLength;
        else
            lpStr += (wcslen(lpStr) + 1);

        /* Append the service start name */
        if (lpServiceStartName != NULL)
        {
            wcscpy(lpStr, lpServiceStartName);
        }
        else
        {
            wcscpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpServiceStartName = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (wcslen(lpStr) + 1);

        /* Append the display name */
        if (lpService->lpDisplayName != NULL)
        {
            wcscpy(lpStr, lpService->lpDisplayName);
        }
        else
        {
            wcscpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpDisplayName = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
    }

    if (pcbBytesNeeded != NULL)
        *pcbBytesNeeded = dwRequiredSize;

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (lpImagePath != NULL)
        HeapFree(GetProcessHeap(), 0, lpImagePath);

    if (lpServiceStartName != NULL)
        HeapFree(GetProcessHeap(), 0, lpServiceStartName);

    if (lpDependencies != NULL)
        HeapFree(GetProcessHeap(), 0, lpDependencies);

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    DPRINT("RQueryServiceConfigW() done\n");

    return dwError;
}


/* Function 18 */
DWORD RQueryServiceLockStatusW(
    SC_RPC_HANDLE hSCManager,
    LPBYTE lpBuf, // LPQUERY_SERVICE_LOCK_STATUSW lpLockStatus,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_4K pcbBytesNeeded)
{
    LPQUERY_SERVICE_LOCK_STATUSW lpLockStatus = (LPQUERY_SERVICE_LOCK_STATUSW)lpBuf;
    PMANAGER_HANDLE hMgr;
    DWORD dwRequiredSize;

    if (!lpLockStatus || !pcbBytesNeeded)
        return ERROR_INVALID_PARAMETER;

    hMgr = ScmGetServiceManagerFromHandle(hSCManager);
    if (hMgr == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hMgr->Handle.DesiredAccess,
                                  SC_MANAGER_QUERY_LOCK_STATUS))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hMgr->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    /* FIXME: we need to compute instead the real length of the owner name */
    dwRequiredSize = sizeof(QUERY_SERVICE_LOCK_STATUSW) + sizeof(WCHAR);
    *pcbBytesNeeded = dwRequiredSize;

    if (cbBufSize < dwRequiredSize)
        return ERROR_INSUFFICIENT_BUFFER;

    ScmQueryServiceLockStatusW(lpLockStatus);

    return ERROR_SUCCESS;
}


/* Function 19 */
DWORD RStartServiceW(
    SC_RPC_HANDLE hService,
    DWORD argc,
    LPSTRING_PTRSW argv)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    SC_RPC_LOCK Lock = NULL;

#ifndef NDEBUG
    DWORD i;

    DPRINT("RStartServiceW(%p %lu %p) called\n", hService, argc, argv);
    DPRINT("  argc: %lu\n", argc);
    if (argv != NULL)
    {
        for (i = 0; i < argc; i++)
        {
            DPRINT("  argv[%lu]: %S\n", i, argv[i].StringPtr);
        }
    }
#endif

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_START))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (lpService->dwStartType == SERVICE_DISABLED)
        return ERROR_SERVICE_DISABLED;

    if (lpService->bDeleted)
        return ERROR_SERVICE_MARKED_FOR_DELETE;

    /* Acquire the service start lock until the service has been started */
    dwError = ScmAcquireServiceStartLock(TRUE, &Lock);
    if (dwError != ERROR_SUCCESS)
        return dwError;

    /* Start the service */
    dwError = ScmStartService(lpService, argc, (LPWSTR*)argv);

    /* Release the service start lock */
    ScmReleaseServiceStartLock(&Lock);

    return dwError;
}


/* Function 20 */
DWORD RGetServiceDisplayNameW(
    SC_RPC_HANDLE hSCManager,
    LPCWSTR lpServiceName,
    LPWSTR lpDisplayName,
    DWORD *lpcchBuffer)
{
//    PMANAGER_HANDLE hManager;
    PSERVICE lpService;
    DWORD dwLength;
    DWORD dwError;

    DPRINT("RGetServiceDisplayNameW() called\n");
    DPRINT("hSCManager = %p\n", hSCManager);
    DPRINT("lpServiceName: %S\n", lpServiceName);
    DPRINT("lpDisplayName: %p\n", lpDisplayName);
    DPRINT("*lpcchBuffer: %lu\n", *lpcchBuffer);

//    hManager = (PMANAGER_HANDLE)hSCManager;
//    if (hManager->Handle.Tag != MANAGER_TAG)
//    {
//        DPRINT("Invalid manager handle!\n");
//        return ERROR_INVALID_HANDLE;
//    }

    /* Get service database entry */
    lpService = ScmGetServiceEntryByName(lpServiceName);
    if (lpService == NULL)
    {
        DPRINT("Could not find a service!\n");

        /* If the service could not be found and lpcchBuffer is less than 2, windows
           puts null in lpDisplayName and puts 2 in lpcchBuffer */
        if (*lpcchBuffer < 2)
        {
            *lpcchBuffer = 2;
            if (lpDisplayName != NULL)
            {
                *lpDisplayName = 0;
            }
        }

        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    if (!lpService->lpDisplayName)
    {
        dwLength = (DWORD)wcslen(lpService->lpServiceName);

        if (lpDisplayName != NULL &&
            *lpcchBuffer > dwLength)
        {
            wcscpy(lpDisplayName, lpService->lpServiceName);
        }
    }
    else
    {
        dwLength = (DWORD)wcslen(lpService->lpDisplayName);

        if (lpDisplayName != NULL &&
            *lpcchBuffer > dwLength)
        {
            wcscpy(lpDisplayName, lpService->lpDisplayName);
        }
    }

    dwError = (*lpcchBuffer > dwLength) ? ERROR_SUCCESS : ERROR_INSUFFICIENT_BUFFER;

    *lpcchBuffer = dwLength;

    return dwError;
}


/* Function 21 */
DWORD RGetServiceKeyNameW(
    SC_RPC_HANDLE hSCManager,
    LPCWSTR lpDisplayName,
    LPWSTR lpServiceName,
    DWORD *lpcchBuffer)
{
//    PMANAGER_HANDLE hManager;
    PSERVICE lpService;
    DWORD dwLength;
    DWORD dwError;

    DPRINT("RGetServiceKeyNameW() called\n");
    DPRINT("hSCManager = %p\n", hSCManager);
    DPRINT("lpDisplayName: %S\n", lpDisplayName);
    DPRINT("lpServiceName: %p\n", lpServiceName);
    DPRINT("*lpcchBuffer: %lu\n", *lpcchBuffer);

//    hManager = (PMANAGER_HANDLE)hSCManager;
//    if (hManager->Handle.Tag != MANAGER_TAG)
//    {
//        DPRINT("Invalid manager handle!\n");
//        return ERROR_INVALID_HANDLE;
//    }

    /* Get service database entry */
    lpService = ScmGetServiceEntryByDisplayName(lpDisplayName);
    if (lpService == NULL)
    {
        DPRINT("Could not find a service!\n");

        /* If the service could not be found and lpcchBuffer is less than 2, windows
           puts null in lpDisplayName and puts 2 in lpcchBuffer */
        if (*lpcchBuffer < 2)
        {
            *lpcchBuffer = 2;
            if (lpServiceName != NULL)
            {
                *lpServiceName = 0;
            }
        }

        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    dwLength = (DWORD)wcslen(lpService->lpServiceName);

    if (lpServiceName != NULL &&
        *lpcchBuffer > dwLength)
    {
        wcscpy(lpServiceName, lpService->lpServiceName);
        *lpcchBuffer = dwLength;
        return ERROR_SUCCESS;
    }

    dwError = (*lpcchBuffer > dwLength) ? ERROR_SUCCESS : ERROR_INSUFFICIENT_BUFFER;

    *lpcchBuffer = dwLength;

    return dwError;
}


/* Function 22 */
DWORD RI_ScSetServiceBitsA(
    RPC_SERVICE_STATUS_HANDLE hServiceStatus,
    DWORD dwServiceBits,
    int bSetBitsOn,
    int bUpdateImmediately,
    char *lpString)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 23 */
DWORD RChangeServiceConfigA(
    SC_RPC_HANDLE hService,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPSTR lpBinaryPathName,
    LPSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPSTR lpDisplayName)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    LPWSTR lpDisplayNameW = NULL;
    LPWSTR lpBinaryPathNameW = NULL;
    LPWSTR lpCanonicalImagePathW = NULL;
    LPWSTR lpLoadOrderGroupW = NULL;
    LPWSTR lpDependenciesW = NULL;

    DPRINT("RChangeServiceConfigA() called\n");
    DPRINT("dwServiceType = %lu\n", dwServiceType);
    DPRINT("dwStartType = %lu\n", dwStartType);
    DPRINT("dwErrorControl = %lu\n", dwErrorControl);
    DPRINT("lpBinaryPathName = %s\n", lpBinaryPathName);
    DPRINT("lpLoadOrderGroup = %s\n", lpLoadOrderGroup);
    DPRINT("lpDisplayName = %s\n", lpDisplayName);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_CHANGE_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    if (lpService->bDeleted)
    {
        DPRINT("The service has already been marked for delete!\n");
        dwError = ERROR_SERVICE_MARKED_FOR_DELETE;
        goto done;
    }

    /* Open the service key */
    dwError = ScmOpenServiceKey(lpService->szServiceName,
                                KEY_SET_VALUE,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Write service data to the registry */

    if (lpDisplayName != NULL && *lpDisplayName != 0)
    {
        /* Set the display name */
        lpDisplayNameW = HeapAlloc(GetProcessHeap(),
                                   HEAP_ZERO_MEMORY,
                                   (strlen(lpDisplayName) + 1) * sizeof(WCHAR));
        if (lpDisplayNameW == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }

        MultiByteToWideChar(CP_ACP,
                            0,
                            lpDisplayName,
                            -1,
                            lpDisplayNameW,
                            (int)(strlen(lpDisplayName) + 1));

        RegSetValueExW(hServiceKey,
                       L"DisplayName",
                       0,
                       REG_SZ,
                       (LPBYTE)lpDisplayNameW,
                       (DWORD)((wcslen(lpDisplayNameW) + 1) * sizeof(WCHAR)));

        /* Update lpService->lpDisplayName */
        if (lpService->lpDisplayName)
            HeapFree(GetProcessHeap(), 0, lpService->lpDisplayName);

        lpService->lpDisplayName = lpDisplayNameW;
    }

    if (dwServiceType != SERVICE_NO_CHANGE)
    {
        /* Set the service type */
        dwError = RegSetValueExW(hServiceKey,
                                 L"Type",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwServiceType,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->Status.dwServiceType = dwServiceType;
    }

    if (dwStartType != SERVICE_NO_CHANGE)
    {
        /* Set the start value */
        dwError = RegSetValueExW(hServiceKey,
                                 L"Start",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwStartType,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->dwStartType = dwStartType;
    }

    if (dwErrorControl != SERVICE_NO_CHANGE)
    {
        /* Set the error control value */
        dwError = RegSetValueExW(hServiceKey,
                                 L"ErrorControl",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&dwErrorControl,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        lpService->dwErrorControl = dwErrorControl;
    }

    if (lpBinaryPathName != NULL && *lpBinaryPathName != 0)
    {
        /* Set the image path */
        lpBinaryPathNameW = HeapAlloc(GetProcessHeap(),
                                      HEAP_ZERO_MEMORY,
                                      (strlen(lpBinaryPathName) + 1) * sizeof(WCHAR));
        if (lpBinaryPathNameW == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }

        MultiByteToWideChar(CP_ACP,
                            0,
                            lpBinaryPathName,
                            -1,
                            lpBinaryPathNameW,
                            (int)(strlen(lpBinaryPathName) + 1));

        if (lpService->Status.dwServiceType & SERVICE_DRIVER)
        {
            dwError = ScmCanonDriverImagePath(lpService->dwStartType,
                                              lpBinaryPathNameW,
                                              &lpCanonicalImagePathW);

            HeapFree(GetProcessHeap(), 0, lpBinaryPathNameW);

            if (dwError != ERROR_SUCCESS)
                goto done;

            lpBinaryPathNameW = lpCanonicalImagePathW;
        }

        dwError = RegSetValueExW(hServiceKey,
                                 L"ImagePath",
                                 0,
                                 REG_EXPAND_SZ,
                                 (LPBYTE)lpBinaryPathNameW,
                                 (DWORD)((wcslen(lpBinaryPathNameW) + 1) * sizeof(WCHAR)));

        HeapFree(GetProcessHeap(), 0, lpBinaryPathNameW);

        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    /* Set the group name */
    if (lpLoadOrderGroup != NULL && *lpLoadOrderGroup != 0)
    {
        lpLoadOrderGroupW = HeapAlloc(GetProcessHeap(),
                                      HEAP_ZERO_MEMORY,
                                      (strlen(lpLoadOrderGroup) + 1) * sizeof(WCHAR));
        if (lpLoadOrderGroupW == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }

        MultiByteToWideChar(CP_ACP,
                            0,
                            lpLoadOrderGroup,
                            -1,
                            lpLoadOrderGroupW,
                            (int)(strlen(lpLoadOrderGroup) + 1));

        dwError = RegSetValueExW(hServiceKey,
                                 L"Group",
                                 0,
                                 REG_SZ,
                                 (LPBYTE)lpLoadOrderGroupW,
                                 (DWORD)((wcslen(lpLoadOrderGroupW) + 1) * sizeof(WCHAR)));
        if (dwError != ERROR_SUCCESS)
        {
            HeapFree(GetProcessHeap(), 0, lpLoadOrderGroupW);
            goto done;
        }

        dwError = ScmSetServiceGroup(lpService,
                                     lpLoadOrderGroupW);

        HeapFree(GetProcessHeap(), 0, lpLoadOrderGroupW);

        if (dwError != ERROR_SUCCESS)
            goto done;
    }

    if (lpdwTagId != NULL)
    {
        dwError = ScmAssignNewTag(lpService);
        if (dwError != ERROR_SUCCESS)
            goto done;

        dwError = RegSetValueExW(hServiceKey,
                                 L"Tag",
                                 0,
                                 REG_DWORD,
                                 (LPBYTE)&lpService->dwTag,
                                 sizeof(DWORD));
        if (dwError != ERROR_SUCCESS)
            goto done;

        *lpdwTagId = lpService->dwTag;
    }

    /* Write dependencies */
    if (lpDependencies != NULL && *lpDependencies != 0)
    {
        lpDependenciesW = HeapAlloc(GetProcessHeap(),
                                    HEAP_ZERO_MEMORY,
                                    (strlen((LPSTR)lpDependencies) + 1) * sizeof(WCHAR));
        if (lpDependenciesW == NULL)
        {
            dwError = ERROR_NOT_ENOUGH_MEMORY;
            goto done;
        }

        MultiByteToWideChar(CP_ACP,
                            0,
                            (LPSTR)lpDependencies,
                            dwDependSize,
                            lpDependenciesW,
                            (int)(strlen((LPSTR)lpDependencies) + 1));

        dwError = ScmWriteDependencies(hServiceKey,
                                       (LPWSTR)lpDependenciesW,
                                       dwDependSize);

        HeapFree(GetProcessHeap(), 0, lpDependenciesW);
    }

    if (lpPassword != NULL)
    {
        /* FIXME: Decrypt and write password */
    }

done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    DPRINT("RChangeServiceConfigA() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 24 */
DWORD RCreateServiceA(
    SC_RPC_HANDLE hSCManager,
    LPSTR lpServiceName,
    LPSTR lpDisplayName,
    DWORD dwDesiredAccess,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPSTR lpBinaryPathName,
    LPSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    DWORD dwError = ERROR_SUCCESS;
    LPWSTR lpServiceNameW = NULL;
    LPWSTR lpDisplayNameW = NULL;
    LPWSTR lpBinaryPathNameW = NULL;
    LPWSTR lpLoadOrderGroupW = NULL;
    LPWSTR lpDependenciesW = NULL;
    LPWSTR lpServiceStartNameW = NULL;
    DWORD dwDependenciesLength = 0;
    SIZE_T cchLength;
    int len;
    LPCSTR lpStr;

    if (lpServiceName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpServiceName, -1, NULL, 0);
        lpServiceNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len * sizeof(WCHAR));
        if (!lpServiceNameW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, lpServiceName, -1, lpServiceNameW, len);
    }

    if (lpDisplayName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpDisplayName, -1, NULL, 0);
        lpDisplayNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len * sizeof(WCHAR));
        if (!lpDisplayNameW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, lpDisplayName, -1, lpDisplayNameW, len);
    }

    if (lpBinaryPathName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpBinaryPathName, -1, NULL, 0);
        lpBinaryPathNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len * sizeof(WCHAR));
        if (!lpBinaryPathNameW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, lpBinaryPathName, -1, lpBinaryPathNameW, len);
    }

    if (lpLoadOrderGroup)
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpLoadOrderGroup, -1, NULL, 0);
        lpLoadOrderGroupW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len * sizeof(WCHAR));
        if (!lpLoadOrderGroupW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, lpLoadOrderGroup, -1, lpLoadOrderGroupW, len);
    }

    if (lpDependencies)
    {
        lpStr = (LPCSTR)lpDependencies;
        while (*lpStr)
        {
            cchLength = strlen(lpStr) + 1;
            dwDependenciesLength += (DWORD)cchLength;
            lpStr = lpStr + cchLength;
        }
        dwDependenciesLength++;

        lpDependenciesW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwDependenciesLength * sizeof(WCHAR));
        if (!lpDependenciesW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)lpDependencies, dwDependenciesLength, lpDependenciesW, dwDependenciesLength);
    }

    if (lpServiceStartName)
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpServiceStartName, -1, NULL, 0);
        lpServiceStartNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len * sizeof(WCHAR));
        if (!lpServiceStartNameW)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            goto cleanup;
        }
        MultiByteToWideChar(CP_ACP, 0, lpServiceStartName, -1, lpServiceStartNameW, len);
    }

    dwError = RCreateServiceW(hSCManager,
                              lpServiceNameW,
                              lpDisplayNameW,
                              dwDesiredAccess,
                              dwServiceType,
                              dwStartType,
                              dwErrorControl,
                              lpBinaryPathNameW,
                              lpLoadOrderGroupW,
                              lpdwTagId,
                              (LPBYTE)lpDependenciesW,
                              dwDependenciesLength,
                              lpServiceStartNameW,
                              lpPassword,
                              dwPwSize,
                              lpServiceHandle);

cleanup:
    if (lpServiceNameW !=NULL)
        HeapFree(GetProcessHeap(), 0, lpServiceNameW);

    if (lpDisplayNameW != NULL)
        HeapFree(GetProcessHeap(), 0, lpDisplayNameW);

    if (lpBinaryPathNameW != NULL)
        HeapFree(GetProcessHeap(), 0, lpBinaryPathNameW);

    if (lpLoadOrderGroupW != NULL)
        HeapFree(GetProcessHeap(), 0, lpLoadOrderGroupW);

    if (lpDependenciesW != NULL)
        HeapFree(GetProcessHeap(), 0, lpDependenciesW);

    if (lpServiceStartNameW != NULL)
        HeapFree(GetProcessHeap(), 0, lpServiceStartNameW);

    return dwError;
}


/* Function 25 */
DWORD REnumDependentServicesA(
    SC_RPC_HANDLE hService,
    DWORD dwServiceState,
    LPBYTE lpServices,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwServicesReturned = 0;
    DWORD dwServiceCount;
    HKEY hServicesKey = NULL;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    PSERVICE *lpServicesArray = NULL;
    LPENUM_SERVICE_STATUSA lpServicesPtr = NULL;
    LPSTR lpStr;

    *pcbBytesNeeded = 0;
    *lpServicesReturned = 0;

    DPRINT("REnumDependentServicesA() called\n");

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    lpService = hSvc->ServiceEntry;

    /* Check access rights */
    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SC_MANAGER_ENUMERATE_SERVICE))
    {
        DPRINT("Insufficient access rights! 0x%lx\n",
               hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    /* Open the Services Reg key */
    dwError = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                            L"System\\CurrentControlSet\\Services",
                            0,
                            KEY_READ,
                            &hServicesKey);

    if (dwError != ERROR_SUCCESS)
        return dwError;

    /* NOTE: Windows calculates the pcbBytesNeeded based on WCHAR strings for
             both EnumDependentServicesA and EnumDependentServicesW. So returned pcbBytesNeeded
             are the same for both. Verified in WINXP. */

    /* First determine the bytes needed and get the number of dependent services*/
    dwError = Int_EnumDependentServicesW(hServicesKey,
                                         lpService,
                                         dwServiceState,
                                         NULL,
                                         pcbBytesNeeded,
                                         &dwServicesReturned);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* If buffer size is less than the bytes needed or pointer is null*/
    if ((!lpServices) || (cbBufSize < *pcbBytesNeeded))
    {
        dwError = ERROR_MORE_DATA;
        goto Done;
    }

    /* Allocate memory for array of service pointers */
    lpServicesArray = HeapAlloc(GetProcessHeap(),
                                HEAP_ZERO_MEMORY,
                                (dwServicesReturned + 1) * sizeof(PSERVICE));
    if (!lpServicesArray)
    {
        DPRINT("Could not allocate a buffer!!\n");
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        goto Done;
    }

    dwServicesReturned = 0;
    *pcbBytesNeeded = 0;

    dwError = Int_EnumDependentServicesW(hServicesKey,
                                         lpService,
                                         dwServiceState,
                                         lpServicesArray,
                                         pcbBytesNeeded,
                                         &dwServicesReturned);
    if (dwError != ERROR_SUCCESS)
    {
        goto Done;
    }

    lpServicesPtr = (LPENUM_SERVICE_STATUSA)lpServices;
    lpStr = (LPSTR)(lpServices + (dwServicesReturned * sizeof(ENUM_SERVICE_STATUSA)));

    /* Copy EnumDepenedentService to Buffer */
    for (dwServiceCount = 0; dwServiceCount < dwServicesReturned; dwServiceCount++)
    {
        lpService = lpServicesArray[dwServiceCount];

        /* Copy the status info */
        memcpy(&lpServicesPtr->ServiceStatus,
               &lpService->Status,
               sizeof(SERVICE_STATUS));

        /* Copy display name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpService->lpDisplayName,
                            -1,
                            lpStr,
                            (int)wcslen(lpService->lpDisplayName),
                            0,
                            0);
        lpServicesPtr->lpDisplayName = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServices);
        lpStr += strlen(lpStr) + 1;

        /* Copy service name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpService->lpServiceName,
                            -1,
                            lpStr,
                            (int)wcslen(lpService->lpServiceName),
                            0,
                            0);
        lpServicesPtr->lpServiceName = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServices);
        lpStr += strlen(lpStr) + 1;

        lpServicesPtr++;
    }

    *lpServicesReturned = dwServicesReturned;

Done:
    if (lpServicesArray)
        HeapFree(GetProcessHeap(), 0, lpServicesArray);

    RegCloseKey(hServicesKey);

    DPRINT("REnumDependentServicesA() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 26 */
DWORD REnumServicesStatusA(
    SC_RPC_HANDLE hSCManager,
    DWORD dwServiceType,
    DWORD dwServiceState,
    LPBYTE lpBuffer,
    DWORD dwBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned,
    LPBOUNDED_DWORD_256K lpResumeHandle)
{
    LPENUM_SERVICE_STATUSW lpStatusPtrW = NULL;
    LPENUM_SERVICE_STATUSW lpStatusPtrIncrW;
    LPENUM_SERVICE_STATUSA lpStatusPtrA = NULL;
    LPWSTR lpStringPtrW;
    LPSTR lpStringPtrA;
    DWORD dwError;
    DWORD dwServiceCount;

    DPRINT("REnumServicesStatusA() called\n");

    if ((dwBufSize > 0) && (lpBuffer))
    {
        lpStatusPtrW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBufSize);
        if (!lpStatusPtrW)
        {
            DPRINT("Failed to allocate buffer!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    dwError = REnumServicesStatusW(hSCManager,
                                   dwServiceType,
                                   dwServiceState,
                                   (LPBYTE)lpStatusPtrW,
                                   dwBufSize,
                                   pcbBytesNeeded,
                                   lpServicesReturned,
                                   lpResumeHandle);

    /* if no services were returned then we are Done */
    if (*lpServicesReturned == 0)
        goto Done;

    lpStatusPtrIncrW = lpStatusPtrW;
    lpStatusPtrA = (LPENUM_SERVICE_STATUSA)lpBuffer;
    lpStringPtrA = (LPSTR)((ULONG_PTR)lpBuffer +
                  *lpServicesReturned * sizeof(ENUM_SERVICE_STATUSA));
    lpStringPtrW = (LPWSTR)((ULONG_PTR)lpStatusPtrW +
                  *lpServicesReturned * sizeof(ENUM_SERVICE_STATUSW));

    for (dwServiceCount = 0; dwServiceCount < *lpServicesReturned; dwServiceCount++)
    {
        /* Copy the service name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpStringPtrW,
                            -1,
                            lpStringPtrA,
                            (int)wcslen(lpStringPtrW),
                            0,
                            0);

        lpStatusPtrA->lpServiceName = (LPSTR)((ULONG_PTR)lpStringPtrA - (ULONG_PTR)lpBuffer);
        lpStringPtrA += wcslen(lpStringPtrW) + 1;
        lpStringPtrW += wcslen(lpStringPtrW) + 1;

        /* Copy the display name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpStringPtrW,
                            -1,
                            lpStringPtrA,
                            (int)wcslen(lpStringPtrW),
                            0,
                            0);

        lpStatusPtrA->lpDisplayName = (LPSTR)((ULONG_PTR)lpStringPtrA - (ULONG_PTR)lpBuffer);
        lpStringPtrA += wcslen(lpStringPtrW) + 1;
        lpStringPtrW += wcslen(lpStringPtrW) + 1;

        /* Copy the status information */
        memcpy(&lpStatusPtrA->ServiceStatus,
               &lpStatusPtrIncrW->ServiceStatus,
               sizeof(SERVICE_STATUS));

        lpStatusPtrIncrW++;
        lpStatusPtrA++;
    }

Done:
    if (lpStatusPtrW)
        HeapFree(GetProcessHeap(), 0, lpStatusPtrW);

    DPRINT("REnumServicesStatusA() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 27 */
DWORD ROpenSCManagerA(
    LPSTR lpMachineName,
    LPSTR lpDatabaseName,
    DWORD dwDesiredAccess,
    LPSC_RPC_HANDLE lpScHandle)
{
    UNICODE_STRING MachineName;
    UNICODE_STRING DatabaseName;
    DWORD dwError;

    DPRINT("ROpenSCManagerA() called\n");

    if (lpMachineName)
        RtlCreateUnicodeStringFromAsciiz(&MachineName,
                                         lpMachineName);

    if (lpDatabaseName)
        RtlCreateUnicodeStringFromAsciiz(&DatabaseName,
                                         lpDatabaseName);

    dwError = ROpenSCManagerW(lpMachineName ? MachineName.Buffer : NULL,
                              lpDatabaseName ? DatabaseName.Buffer : NULL,
                              dwDesiredAccess,
                              lpScHandle);

    if (lpMachineName)
        RtlFreeUnicodeString(&MachineName);

    if (lpDatabaseName)
        RtlFreeUnicodeString(&DatabaseName);

    return dwError;
}


/* Function 28 */
DWORD ROpenServiceA(
    SC_RPC_HANDLE hSCManager,
    LPSTR lpServiceName,
    DWORD dwDesiredAccess,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    UNICODE_STRING ServiceName;
    DWORD dwError;

    DPRINT("ROpenServiceA() called\n");

    if (lpServiceName)
        RtlCreateUnicodeStringFromAsciiz(&ServiceName,
                                         lpServiceName);

    dwError = ROpenServiceW(hSCManager,
                            lpServiceName ? ServiceName.Buffer : NULL,
                            dwDesiredAccess,
                            lpServiceHandle);

    if (lpServiceName)
        RtlFreeUnicodeString(&ServiceName);

    return dwError;
}


/* Function 29 */
DWORD RQueryServiceConfigA(
    SC_RPC_HANDLE hService,
    LPBYTE lpBuf, //LPQUERY_SERVICE_CONFIGA lpServiceConfig,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_8K pcbBytesNeeded)
{
    LPQUERY_SERVICE_CONFIGA lpServiceConfig = (LPQUERY_SERVICE_CONFIGA)lpBuf;
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    LPWSTR lpImagePath = NULL;
    LPWSTR lpServiceStartName = NULL;
    LPWSTR lpDependencies = NULL;
    DWORD dwDependenciesLength = 0;
    DWORD dwRequiredSize;
    CHAR lpEmptyString[]={0,0};
    LPSTR lpStr;

    DPRINT("RQueryServiceConfigA() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    dwError = ScmOpenServiceKey(lpService->lpServiceName,
                                KEY_READ,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* Read the image path */
    dwError = ScmReadString(hServiceKey,
                            L"ImagePath",
                            &lpImagePath);
    if (dwError != ERROR_SUCCESS)
        goto Done;

    /* Read the service start name */
    ScmReadString(hServiceKey,
                  L"ObjectName",
                  &lpServiceStartName);

    /* Read the dependencies */
    ScmReadDependencies(hServiceKey,
                        &lpDependencies,
                        &dwDependenciesLength);

    dwRequiredSize = sizeof(QUERY_SERVICE_CONFIGA);

    if (lpImagePath != NULL)
        dwRequiredSize += (DWORD)(wcslen(lpImagePath) + 1);
    else
        dwRequiredSize += 2;

    if ((lpService->lpGroup != NULL) && (lpService->lpGroup->lpGroupName != NULL))
        dwRequiredSize += (DWORD)(wcslen(lpService->lpGroup->lpGroupName) + 1);
    else
        dwRequiredSize += 2;

    /* Add Dependencies length */
    if (lpDependencies != NULL)
        dwRequiredSize += dwDependenciesLength;
    else
        dwRequiredSize += 2;

    if (lpServiceStartName != NULL)
        dwRequiredSize += (DWORD)(wcslen(lpServiceStartName) + 1);
    else
        dwRequiredSize += 2;

    if (lpService->lpDisplayName != NULL)
        dwRequiredSize += (DWORD)(wcslen(lpService->lpDisplayName) + 1);
    else
        dwRequiredSize += 2;

    if (lpServiceConfig == NULL || cbBufSize < dwRequiredSize)
    {
        dwError = ERROR_INSUFFICIENT_BUFFER;
    }
    else
    {
        lpServiceConfig->dwServiceType = lpService->Status.dwServiceType;
        lpServiceConfig->dwStartType = lpService->dwStartType;
        lpServiceConfig->dwErrorControl = lpService->dwErrorControl;
        lpServiceConfig->dwTagId = lpService->dwTag;

        lpStr = (LPSTR)(lpServiceConfig + 1);

        /* NOTE: Strings that are NULL for QUERY_SERVICE_CONFIG are pointers to empty strings.
           Verified in WINXP */

        if (lpImagePath)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpImagePath,
                                -1,
                                lpStr,
                                (int)(wcslen(lpImagePath) + 1),
                                0,
                                0);
        }
        else
        {
            strcpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpBinaryPathName = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (strlen((LPSTR)lpStr) + 1);

        if (lpService->lpGroup && lpService->lpGroup->lpGroupName)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpService->lpGroup->lpGroupName,
                                -1,
                                lpStr,
                                (int)(wcslen(lpService->lpGroup->lpGroupName) + 1),
                                0,
                                0);
        }
        else
        {
            strcpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpLoadOrderGroup = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (strlen(lpStr) + 1);

        /* Append Dependencies */
        if (lpDependencies)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpDependencies,
                                dwDependenciesLength,
                                lpStr,
                                dwDependenciesLength,
                                0,
                                0);
        }
        else
        {
            strcpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpDependencies = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        if (lpDependencies)
            lpStr += dwDependenciesLength;
        else
            lpStr += (strlen(lpStr) + 1);

        if (lpServiceStartName)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpServiceStartName,
                                -1,
                                lpStr,
                                (int)(wcslen(lpServiceStartName) + 1),
                                0,
                                0);
        }
        else
        {
            strcpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpServiceStartName = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
        lpStr += (strlen(lpStr) + 1);

        if (lpService->lpDisplayName)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpService->lpDisplayName,
                                -1,
                                lpStr,
                                (int)(wcslen(lpService->lpDisplayName) + 1),
                                0,
                                0);
        }
        else
        {
            strcpy(lpStr, lpEmptyString);
        }

        lpServiceConfig->lpDisplayName = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceConfig);
    }

    if (pcbBytesNeeded != NULL)
        *pcbBytesNeeded = dwRequiredSize;

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (lpImagePath != NULL)
        HeapFree(GetProcessHeap(), 0, lpImagePath);

    if (lpServiceStartName != NULL)
        HeapFree(GetProcessHeap(), 0, lpServiceStartName);

    if (lpDependencies != NULL)
        HeapFree(GetProcessHeap(), 0, lpDependencies);

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    DPRINT("RQueryServiceConfigA() done\n");

    return dwError;
}


/* Function 30 */
DWORD RQueryServiceLockStatusA(
    SC_RPC_HANDLE hSCManager,
    LPBYTE lpBuf, // LPQUERY_SERVICE_LOCK_STATUSA lpLockStatus,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_4K pcbBytesNeeded)
{
    LPQUERY_SERVICE_LOCK_STATUSA lpLockStatus = (LPQUERY_SERVICE_LOCK_STATUSA)lpBuf;
    PMANAGER_HANDLE hMgr;
    DWORD dwRequiredSize;

    if (!lpLockStatus || !pcbBytesNeeded)
        return ERROR_INVALID_PARAMETER;

    hMgr = ScmGetServiceManagerFromHandle(hSCManager);
    if (hMgr == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hMgr->Handle.DesiredAccess,
                                  SC_MANAGER_QUERY_LOCK_STATUS))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hMgr->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    /* FIXME: we need to compute instead the real length of the owner name */
    dwRequiredSize = sizeof(QUERY_SERVICE_LOCK_STATUSA) + sizeof(CHAR);
    *pcbBytesNeeded = dwRequiredSize;

    if (cbBufSize < dwRequiredSize)
        return ERROR_INSUFFICIENT_BUFFER;

    ScmQueryServiceLockStatusA(lpLockStatus);

    return ERROR_SUCCESS;
}


/* Function 31 */
DWORD RStartServiceA(
    SC_RPC_HANDLE hService,
    DWORD argc,
    LPSTRING_PTRSA argv)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    SC_RPC_LOCK Lock = NULL;
    LPWSTR *lpVector = NULL;
    DWORD i;
    DWORD dwLength;

    DPRINT("RStartServiceA() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_START))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (lpService->dwStartType == SERVICE_DISABLED)
        return ERROR_SERVICE_DISABLED;

    if (lpService->bDeleted)
        return ERROR_SERVICE_MARKED_FOR_DELETE;

    /* Build a Unicode argument vector */
    if (argc > 0)
    {
        lpVector = HeapAlloc(GetProcessHeap(),
                             HEAP_ZERO_MEMORY,
                             argc * sizeof(LPWSTR));
        if (lpVector == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        for (i = 0; i < argc; i++)
        {
            dwLength = MultiByteToWideChar(CP_ACP,
                                           0,
                                           ((LPSTR*)argv)[i],
                                           -1,
                                           NULL,
                                           0);

            lpVector[i] = HeapAlloc(GetProcessHeap(),
                                    HEAP_ZERO_MEMORY,
                                    dwLength * sizeof(WCHAR));
            if (lpVector[i] == NULL)
            {
                dwError = ERROR_NOT_ENOUGH_MEMORY;
                goto done;
            }

            MultiByteToWideChar(CP_ACP,
                                0,
                                ((LPSTR*)argv)[i],
                                -1,
                                lpVector[i],
                                dwLength);
        }
    }

    /* Acquire the service start lock until the service has been started */
    dwError = ScmAcquireServiceStartLock(TRUE, &Lock);
    if (dwError != ERROR_SUCCESS)
        goto done;

    /* Start the service */
    dwError = ScmStartService(lpService, argc, lpVector);

     /* Release the service start lock */
     ScmReleaseServiceStartLock(&Lock);

done:
    /* Free the Unicode argument vector */
    if (lpVector != NULL)
    {
        for (i = 0; i < argc; i++)
        {
            if (lpVector[i] != NULL)
                HeapFree(GetProcessHeap(), 0, lpVector[i]);
        }
        HeapFree(GetProcessHeap(), 0, lpVector);
    }

    return dwError;
}


/* Function 32 */
DWORD RGetServiceDisplayNameA(
    SC_RPC_HANDLE hSCManager,
    LPCSTR lpServiceName,
    LPSTR lpDisplayName,
    LPBOUNDED_DWORD_4K lpcchBuffer)
{
//    PMANAGER_HANDLE hManager;
    PSERVICE lpService = NULL;
    DWORD dwLength;
    DWORD dwError;
    LPWSTR lpServiceNameW;

    DPRINT("RGetServiceDisplayNameA() called\n");
    DPRINT("hSCManager = %p\n", hSCManager);
    DPRINT("lpServiceName: %s\n", lpServiceName);
    DPRINT("lpDisplayName: %p\n", lpDisplayName);
    DPRINT("*lpcchBuffer: %lu\n", *lpcchBuffer);

//    hManager = (PMANAGER_HANDLE)hSCManager;
//    if (hManager->Handle.Tag != MANAGER_TAG)
//    {
//        DPRINT("Invalid manager handle!\n");
//        return ERROR_INVALID_HANDLE;
//    }

    if (lpServiceName != NULL)
    {
        dwLength = (DWORD)(strlen(lpServiceName) + 1);
        lpServiceNameW = HeapAlloc(GetProcessHeap(),
                                   HEAP_ZERO_MEMORY,
                                   dwLength * sizeof(WCHAR));
        if (!lpServiceNameW)
            return ERROR_NOT_ENOUGH_MEMORY;

        MultiByteToWideChar(CP_ACP,
                            0,
                            lpServiceName,
                            -1,
                            lpServiceNameW,
                            dwLength);

        lpService = ScmGetServiceEntryByName(lpServiceNameW);

        HeapFree(GetProcessHeap(), 0, lpServiceNameW);
    }

    if (lpService == NULL)
    {
        DPRINT("Could not find a service!\n");

        /* If the service could not be found and lpcchBuffer is 0, windows
           puts null in lpDisplayName and puts 1 in lpcchBuffer */
        if (*lpcchBuffer == 0)
        {
            *lpcchBuffer = 1;
            if (lpDisplayName != NULL)
            {
                *lpDisplayName = 0;
            }
        }
        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    if (!lpService->lpDisplayName)
    {
        dwLength = (DWORD)wcslen(lpService->lpServiceName);
        if (lpDisplayName != NULL &&
            *lpcchBuffer > dwLength)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpService->lpServiceName,
                                (int)wcslen(lpService->lpServiceName),
                                lpDisplayName,
                                dwLength + 1,
                                NULL,
                                NULL);
            return ERROR_SUCCESS;
        }
    }
    else
    {
        dwLength = (DWORD)wcslen(lpService->lpDisplayName);
        if (lpDisplayName != NULL &&
            *lpcchBuffer > dwLength)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpService->lpDisplayName,
                                (int)wcslen(lpService->lpDisplayName),
                                lpDisplayName,
                                dwLength + 1,
                                NULL,
                                NULL);
            return ERROR_SUCCESS;
        }
    }

    dwError = (*lpcchBuffer > dwLength) ? ERROR_SUCCESS : ERROR_INSUFFICIENT_BUFFER;

    *lpcchBuffer = dwLength * 2;

    return dwError;
}


/* Function 33 */
DWORD RGetServiceKeyNameA(
    SC_RPC_HANDLE hSCManager,
    LPCSTR lpDisplayName,
    LPSTR lpServiceName,
    LPBOUNDED_DWORD_4K lpcchBuffer)
{
    PSERVICE lpService;
    DWORD dwLength;
    DWORD dwError;
    LPWSTR lpDisplayNameW;

    DPRINT("RGetServiceKeyNameA() called\n");
    DPRINT("hSCManager = %p\n", hSCManager);
    DPRINT("lpDisplayName: %s\n", lpDisplayName);
    DPRINT("lpServiceName: %p\n", lpServiceName);
    DPRINT("*lpcchBuffer: %lu\n", *lpcchBuffer);

    dwLength = (DWORD)(strlen(lpDisplayName) + 1);
    lpDisplayNameW = HeapAlloc(GetProcessHeap(),
                               HEAP_ZERO_MEMORY,
                               dwLength * sizeof(WCHAR));
    if (!lpDisplayNameW)
        return ERROR_NOT_ENOUGH_MEMORY;

    MultiByteToWideChar(CP_ACP,
                        0,
                        lpDisplayName,
                        -1,
                        lpDisplayNameW,
                        dwLength);

    lpService = ScmGetServiceEntryByDisplayName(lpDisplayNameW);

    HeapFree(GetProcessHeap(), 0, lpDisplayNameW);

    if (lpService == NULL)
    {
        DPRINT("Could not find the service!\n");

        /* If the service could not be found and lpcchBuffer is 0,
           put null in lpDisplayName and puts 1 in lpcchBuffer, verified WINXP. */
        if (*lpcchBuffer == 0)
        {
            *lpcchBuffer = 1;
            if (lpServiceName != NULL)
            {
                *lpServiceName = 0;
            }
        }

        return ERROR_SERVICE_DOES_NOT_EXIST;
    }

    dwLength = (DWORD)wcslen(lpService->lpServiceName);
    if (lpServiceName != NULL &&
        *lpcchBuffer > dwLength)
    {
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpService->lpServiceName,
                            (int)wcslen(lpService->lpServiceName),
                            lpServiceName,
                            dwLength + 1,
                            NULL,
                            NULL);
        return ERROR_SUCCESS;
    }

    dwError = (*lpcchBuffer > dwLength) ? ERROR_SUCCESS : ERROR_INSUFFICIENT_BUFFER;

    *lpcchBuffer = dwLength * 2;

    return dwError;
}


/* Function 34 */
DWORD RI_ScGetCurrentGroupStateW(
    SC_RPC_HANDLE hSCManager,
    LPWSTR lpLoadOrderGroup,
    LPDWORD lpState)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 35 */
DWORD REnumServiceGroupW(
    SC_RPC_HANDLE hSCManager,
    DWORD dwServiceType,
    DWORD dwServiceState,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned,
    LPBOUNDED_DWORD_256K lpResumeIndex,
    LPCWSTR pszGroupName)
{
    PMANAGER_HANDLE hManager;
    PSERVICE lpService;
    DWORD dwError = ERROR_SUCCESS;
    PLIST_ENTRY ServiceEntry;
    PSERVICE CurrentService;
    DWORD dwState;
    DWORD dwRequiredSize;
    DWORD dwServiceCount;
    DWORD dwSize;
    DWORD dwLastResumeCount = 0;
    LPENUM_SERVICE_STATUSW lpStatusPtr;
    LPWSTR lpStringPtr;

    DPRINT("REnumServiceGroupW() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hManager = ScmGetServiceManagerFromHandle(hSCManager);
    if (hManager == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    *pcbBytesNeeded = 0;
    *lpServicesReturned = 0;

    if ((dwServiceType == 0) ||
        ((dwServiceType & ~SERVICE_TYPE_ALL) != 0))
    {
        DPRINT("Not a valid Service Type!\n");
        return ERROR_INVALID_PARAMETER;
    }

    if ((dwServiceState == 0) ||
        ((dwServiceState & ~SERVICE_STATE_ALL) != 0))
    {
        DPRINT("Not a valid Service State!\n");
        return ERROR_INVALID_PARAMETER;
    }

    /* Check access rights */
    if (!RtlAreAllAccessesGranted(hManager->Handle.DesiredAccess,
                                  SC_MANAGER_ENUMERATE_SERVICE))
    {
        DPRINT("Insufficient access rights! 0x%lx\n",
                hManager->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    if (lpResumeIndex)
        dwLastResumeCount = *lpResumeIndex;

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    lpService = ScmGetServiceEntryByResumeCount(dwLastResumeCount);
    if (lpService == NULL)
    {
        dwError = ERROR_SUCCESS;
        goto Done;
    }

    dwRequiredSize = 0;
    dwServiceCount = 0;

    for (ServiceEntry = &lpService->ServiceListEntry;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwSize = sizeof(ENUM_SERVICE_STATUSW) +
                 (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                 (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR));

        if (dwRequiredSize + dwSize > cbBufSize)
        {
            DPRINT("Service name: %S  no fit\n", CurrentService->lpServiceName);
            break;
        }

        DPRINT("Service name: %S  fit\n", CurrentService->lpServiceName);
        dwRequiredSize += dwSize;
        dwServiceCount++;
        dwLastResumeCount = CurrentService->dwResumeCount;
    }

    DPRINT("dwRequiredSize: %lu\n", dwRequiredSize);
    DPRINT("dwServiceCount: %lu\n", dwServiceCount);

    for (;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwRequiredSize += (sizeof(ENUM_SERVICE_STATUSW) +
                           (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                           (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR)));

        dwError = ERROR_MORE_DATA;
    }

    DPRINT("*pcbBytesNeeded: %lu\n", dwRequiredSize);

    if (lpResumeIndex)
        *lpResumeIndex = dwLastResumeCount;

    *lpServicesReturned = dwServiceCount;
    *pcbBytesNeeded = dwRequiredSize;

    lpStatusPtr = (LPENUM_SERVICE_STATUSW)lpBuffer;
    lpStringPtr = (LPWSTR)((ULONG_PTR)lpBuffer +
                           dwServiceCount * sizeof(ENUM_SERVICE_STATUSW));

    dwRequiredSize = 0;
    for (ServiceEntry = &lpService->ServiceListEntry;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwSize = sizeof(ENUM_SERVICE_STATUSW) +
                 (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                 (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR));

        if (dwRequiredSize + dwSize > cbBufSize)
            break;

        /* Copy the service name */
        wcscpy(lpStringPtr, CurrentService->lpServiceName);
        lpStatusPtr->lpServiceName = (LPWSTR)((ULONG_PTR)lpStringPtr - (ULONG_PTR)lpBuffer);
        lpStringPtr += (wcslen(CurrentService->lpServiceName) + 1);

        /* Copy the display name */
        wcscpy(lpStringPtr, CurrentService->lpDisplayName);
        lpStatusPtr->lpDisplayName = (LPWSTR)((ULONG_PTR)lpStringPtr - (ULONG_PTR)lpBuffer);
        lpStringPtr += (wcslen(CurrentService->lpDisplayName) + 1);

        /* Copy the status information */
        memcpy(&lpStatusPtr->ServiceStatus,
               &CurrentService->Status,
               sizeof(SERVICE_STATUS));

        lpStatusPtr++;
        dwRequiredSize += dwSize;
    }

    if (dwError == ERROR_SUCCESS)
    {
        *pcbBytesNeeded = 0;
        if (lpResumeIndex) *lpResumeIndex = 0;
    }

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("REnumServiceGroupW() done (Error %lu)\n", dwError);

    return dwError;
}


//
// WARNING: This function is untested
//
/* Function 36 */
DWORD RChangeServiceConfig2A(
    SC_RPC_HANDLE hService,
    SC_RPC_CONFIG_INFOA Info)
{
    SC_RPC_CONFIG_INFOW InfoW;
    DWORD dwRet, dwLength;
    PVOID ptr = NULL;

    DPRINT("RChangeServiceConfig2A() called\n");
    DPRINT("dwInfoLevel = %lu\n", Info.dwInfoLevel);

    InfoW.dwInfoLevel = Info.dwInfoLevel;

    if (InfoW.dwInfoLevel == SERVICE_CONFIG_DESCRIPTION)
    {
        LPSERVICE_DESCRIPTIONW lpServiceDescriptonW;
        //LPSERVICE_DESCRIPTIONA lpServiceDescriptonA;

        //lpServiceDescriptonA = Info.psd;

        ///if (lpServiceDescriptonA &&
        ///lpServiceDescriptonA->lpDescription)
        ///{
            dwLength = (DWORD)((strlen(Info.lpDescription) + 1) * sizeof(WCHAR));

            lpServiceDescriptonW = HeapAlloc(GetProcessHeap(),
                                             HEAP_ZERO_MEMORY,
                                             dwLength + sizeof(SERVICE_DESCRIPTIONW));
            if (!lpServiceDescriptonW)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            lpServiceDescriptonW->lpDescription = (LPWSTR)(lpServiceDescriptonW + 1);

            MultiByteToWideChar(CP_ACP,
                                0,
                                Info.lpDescription,
                                -1,
                                lpServiceDescriptonW->lpDescription,
                                dwLength);

            ptr = lpServiceDescriptonW;
            InfoW.psd = lpServiceDescriptonW;
        ///}
    }
    else if (Info.dwInfoLevel == SERVICE_CONFIG_FAILURE_ACTIONS)
    {
        LPSERVICE_FAILURE_ACTIONSW lpServiceFailureActionsW;
        LPSERVICE_FAILURE_ACTIONSA lpServiceFailureActionsA;
        DWORD dwRebootLen = 0;
        DWORD dwCommandLen = 0;

        lpServiceFailureActionsA = Info.psfa;

        if (lpServiceFailureActionsA)
        {
            if (lpServiceFailureActionsA->lpRebootMsg)
            {
                dwRebootLen = (DWORD)((strlen(lpServiceFailureActionsA->lpRebootMsg) + 1) * sizeof(WCHAR));
            }
            if (lpServiceFailureActionsA->lpCommand)
            {
                dwCommandLen = (DWORD)((strlen(lpServiceFailureActionsA->lpCommand) + 1) * sizeof(WCHAR));
            }
            dwLength = dwRebootLen + dwCommandLen + sizeof(SERVICE_FAILURE_ACTIONSW);

            lpServiceFailureActionsW = HeapAlloc(GetProcessHeap(),
                                                 HEAP_ZERO_MEMORY,
                                                 dwLength);
            if (!lpServiceFailureActionsW)
            {
                return ERROR_NOT_ENOUGH_MEMORY;
            }

            lpServiceFailureActionsW->cActions = lpServiceFailureActionsA->cActions;
            lpServiceFailureActionsW->dwResetPeriod = lpServiceFailureActionsA->dwResetPeriod;
            CopyMemory(lpServiceFailureActionsW->lpsaActions, lpServiceFailureActionsA->lpsaActions, sizeof(SC_ACTION));

            if (lpServiceFailureActionsA->lpRebootMsg)
            {
                MultiByteToWideChar(CP_ACP,
                                    0,
                                    lpServiceFailureActionsA->lpRebootMsg,
                                    -1,
                                    lpServiceFailureActionsW->lpRebootMsg,
                                    dwRebootLen);
            }

            if (lpServiceFailureActionsA->lpCommand)
            {
                MultiByteToWideChar(CP_ACP,
                                    0,
                                    lpServiceFailureActionsA->lpCommand,
                                    -1,
                                    lpServiceFailureActionsW->lpCommand,
                                    dwCommandLen);
            }

            ptr = lpServiceFailureActionsW;
        }
    }

    dwRet = RChangeServiceConfig2W(hService, InfoW);

    HeapFree(GetProcessHeap(), 0, ptr);

    return dwRet;
}


static DWORD
ScmSetFailureActions(PSERVICE_HANDLE hSvc,
                     PSERVICE lpService,
                     HKEY hServiceKey,
                     LPSERVICE_FAILURE_ACTIONSW lpFailureActions)
{
    LPSERVICE_FAILURE_ACTIONSW lpReadBuffer = NULL;
    LPSERVICE_FAILURE_ACTIONSW lpWriteBuffer = NULL;
    BOOL bIsActionRebootSet = FALSE;
    DWORD dwDesiredAccess = SERVICE_CHANGE_CONFIG;
    DWORD dwRequiredSize = 0;
    DWORD dwType = 0;
    DWORD i = 0;
    DWORD dwError;

    /* There is nothing to be done if we have no failure actions */
    if (lpFailureActions == NULL)
        return ERROR_SUCCESS;

    /*
     * 1- Check whether or not we can set
     *    failure actions for this service.
     */

    /* Failure actions can only be set for Win32 services, not for drivers */
    if (lpService->Status.dwServiceType & SERVICE_DRIVER)
        return ERROR_CANNOT_DETECT_DRIVER_FAILURE;

    /*
     * If the service controller handles the SC_ACTION_RESTART action,
     * hService must have the SERVICE_START access right.
     *
     * If you specify SC_ACTION_REBOOT, the caller must have the
     * SE_SHUTDOWN_NAME privilege.
     */
    if (lpFailureActions->cActions > 0 &&
        lpFailureActions->lpsaActions != NULL)
    {
        for (i = 0; i < lpFailureActions->cActions; ++i)
        {
           if (lpFailureActions->lpsaActions[i].Type == SC_ACTION_RESTART)
               dwDesiredAccess |= SERVICE_START;
           else if (lpFailureActions->lpsaActions[i].Type == SC_ACTION_REBOOT)
               bIsActionRebootSet = TRUE;
        }
    }

    /* Re-check the access rights */
    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  dwDesiredAccess))
    {
        DPRINT1("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    /* FIXME: Check if the caller has the SE_SHUTDOWN_NAME privilege */
    if (bIsActionRebootSet)
    {
    }

    /*
     * 2- Retrieve the original value of FailureActions.
     */

    /* Query value length */
    dwError = RegQueryValueExW(hServiceKey,
                               L"FailureActions",
                               NULL,
                               &dwType,
                               NULL,
                              &dwRequiredSize);
    if (dwError != ERROR_SUCCESS &&
        dwError != ERROR_MORE_DATA &&
        dwError != ERROR_FILE_NOT_FOUND)
        return dwError;

    dwRequiredSize = (dwType == REG_BINARY) ? max(sizeof(SERVICE_FAILURE_ACTIONSW), dwRequiredSize)
                                            : sizeof(SERVICE_FAILURE_ACTIONSW);

    /* Initialize the read buffer */
    lpReadBuffer = HeapAlloc(GetProcessHeap(),
                             HEAP_ZERO_MEMORY,
                             dwRequiredSize);
    if (lpReadBuffer == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    /* Now we can fill the read buffer */
    if (dwError != ERROR_FILE_NOT_FOUND &&
        dwType == REG_BINARY)
    {
        dwError = RegQueryValueExW(hServiceKey,
                                   L"FailureActions",
                                   NULL,
                                   NULL,
                                   (LPBYTE)lpReadBuffer,
                                   &dwRequiredSize);
        if (dwError != ERROR_SUCCESS &&
            dwError != ERROR_FILE_NOT_FOUND)
            goto done;

        if (dwRequiredSize < sizeof(SERVICE_FAILURE_ACTIONSW))
            dwRequiredSize = sizeof(SERVICE_FAILURE_ACTIONSW);
    }
    else
    {
        /*
         * The value of the error doesn't really matter, the only
         * important thing is that it must be != ERROR_SUCCESS.
         */
        dwError = ERROR_INVALID_DATA;
    }

    if (dwError == ERROR_SUCCESS)
    {
        lpReadBuffer->cActions = min(lpReadBuffer->cActions, (dwRequiredSize - sizeof(SERVICE_FAILURE_ACTIONSW)) / sizeof(SC_ACTION));
        lpReadBuffer->lpsaActions = (lpReadBuffer->cActions > 0 ? (LPSC_ACTION)(lpReadBuffer + 1) : NULL);
    }
    else
    {
        lpReadBuffer->dwResetPeriod = 0;
        lpReadBuffer->cActions = 0;
        lpReadBuffer->lpsaActions = NULL;
    }

    lpReadBuffer->lpRebootMsg = NULL;
    lpReadBuffer->lpCommand = NULL;

    /*
     * 3- Initialize the new value to set.
     */

    dwRequiredSize = sizeof(SERVICE_FAILURE_ACTIONSW);

    if (lpFailureActions->lpsaActions == NULL)
    {
        /*
         * lpFailureActions->cActions is ignored.
         * Therefore we use the original values
         * of cActions and lpsaActions.
         */
        dwRequiredSize += lpReadBuffer->cActions * sizeof(SC_ACTION);
    }
    else
    {
        /*
         * The reset period and array of failure actions
         * are deleted if lpFailureActions->cActions == 0 .
         */
        dwRequiredSize += lpFailureActions->cActions * sizeof(SC_ACTION);
    }

    lpWriteBuffer = HeapAlloc(GetProcessHeap(),
                              HEAP_ZERO_MEMORY,
                              dwRequiredSize);
    if (lpWriteBuffer == NULL)
    {
        dwError = ERROR_NOT_ENOUGH_MEMORY;
        goto done;
    }

    /* Clean the pointers as they have no meaning when the structure is stored in the registry */
    lpWriteBuffer->lpRebootMsg = NULL;
    lpWriteBuffer->lpCommand = NULL;
    lpWriteBuffer->lpsaActions = NULL;

    /* Set the members */
    if (lpFailureActions->lpsaActions == NULL)
    {
        /*
         * lpFailureActions->dwResetPeriod and lpFailureActions->cActions are ignored.
         * Therefore we use the original values of dwResetPeriod, cActions and lpsaActions.
         */
        lpWriteBuffer->dwResetPeriod = lpReadBuffer->dwResetPeriod;
        lpWriteBuffer->cActions = lpReadBuffer->cActions;

        if (lpReadBuffer->lpsaActions != NULL)
        {
            memmove(lpWriteBuffer + 1,
                    lpReadBuffer->lpsaActions,
                    lpReadBuffer->cActions * sizeof(SC_ACTION));
        }
    }
    else
    {
        if (lpFailureActions->cActions > 0)
        {
            lpWriteBuffer->dwResetPeriod = lpFailureActions->dwResetPeriod;
            lpWriteBuffer->cActions = lpFailureActions->cActions;

            memmove(lpWriteBuffer + 1,
                    lpFailureActions->lpsaActions,
                    lpFailureActions->cActions * sizeof(SC_ACTION));
        }
        else
        {
            /* The reset period and array of failure actions are deleted */
            lpWriteBuffer->dwResetPeriod = 0;
            lpWriteBuffer->cActions = 0;
        }
    }

    /* Save the new failure actions into the registry */
    dwError = RegSetValueExW(hServiceKey,
                             L"FailureActions",
                             0,
                             REG_BINARY,
                             (LPBYTE)lpWriteBuffer,
                             dwRequiredSize);

    /* We modify the strings only in case of success.*/
    if (dwError == ERROR_SUCCESS)
    {
        /* Modify the Reboot Message value, if specified */
        if (lpFailureActions->lpRebootMsg != NULL)
        {
            /* If the Reboot Message is "" then we delete it */
            if (*lpFailureActions->lpRebootMsg == 0)
            {
                DPRINT("Delete Reboot Message value\n");
                RegDeleteValueW(hServiceKey, L"RebootMessage");
            }
            else
            {
                DPRINT("Setting Reboot Message value %S\n", lpFailureActions->lpRebootMsg);
                RegSetValueExW(hServiceKey,
                               L"RebootMessage",
                               0,
                               REG_SZ,
                               (LPBYTE)lpFailureActions->lpRebootMsg,
                               (DWORD)((wcslen(lpFailureActions->lpRebootMsg) + 1) * sizeof(WCHAR)));
            }
        }

        /* Modify the Failure Command value, if specified */
        if (lpFailureActions->lpCommand != NULL)
        {
            /* If the FailureCommand string is an empty string, delete the value */
            if (*lpFailureActions->lpCommand == 0)
            {
                DPRINT("Delete Failure Command value\n");
                RegDeleteValueW(hServiceKey, L"FailureCommand");
            }
            else
            {
                DPRINT("Setting Failure Command value %S\n", lpFailureActions->lpCommand);
                RegSetValueExW(hServiceKey,
                               L"FailureCommand",
                               0,
                               REG_SZ,
                               (LPBYTE)lpFailureActions->lpCommand,
                               (DWORD)((wcslen(lpFailureActions->lpCommand) + 1) * sizeof(WCHAR)));
            }
        }
    }

done:
    if (lpWriteBuffer != NULL)
        HeapFree(GetProcessHeap(), 0, lpWriteBuffer);

    if (lpReadBuffer != NULL)
        HeapFree(GetProcessHeap(), 0, lpReadBuffer);

    return dwError;
}


/* Function 37 */
DWORD RChangeServiceConfig2W(
    SC_RPC_HANDLE hService,
    SC_RPC_CONFIG_INFOW Info)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;

    DPRINT("RChangeServiceConfig2W() called\n");
    DPRINT("dwInfoLevel = %lu\n", Info.dwInfoLevel);

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_CHANGE_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database exclusively */
    ScmLockDatabaseExclusive();

    if (lpService->bDeleted)
    {
        DPRINT("The service has already been marked for delete!\n");
        dwError = ERROR_SERVICE_MARKED_FOR_DELETE;
        goto done;
    }

    /* Open the service key */
    dwError = ScmOpenServiceKey(lpService->szServiceName,
                                KEY_READ | KEY_SET_VALUE,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    if (Info.dwInfoLevel == SERVICE_CONFIG_DESCRIPTION)
    {
        LPSERVICE_DESCRIPTIONW lpServiceDescription = (LPSERVICE_DESCRIPTIONW)Info.psd;

        /* Modify the service description, if specified */
        if (lpServiceDescription != NULL &&
            lpServiceDescription->lpDescription != NULL)
        {
            /* If the description is "" then we delete it */
            if (*lpServiceDescription->lpDescription == 0)
            {
                DPRINT("Delete service description\n");
                dwError = RegDeleteValueW(hServiceKey, L"Description");

                if (dwError == ERROR_FILE_NOT_FOUND)
                    dwError = ERROR_SUCCESS;
            }
            else
            {
                DPRINT("Setting service description value %S\n", lpServiceDescription->lpDescription);
                dwError = RegSetValueExW(hServiceKey,
                                         L"Description",
                                         0,
                                         REG_SZ,
                                         (LPBYTE)lpServiceDescription->lpDescription,
                                         (DWORD)((wcslen(lpServiceDescription->lpDescription) + 1) * sizeof(WCHAR)));
            }
        }
        else
        {
            dwError = ERROR_SUCCESS;
        }
    }
    else if (Info.dwInfoLevel == SERVICE_CONFIG_FAILURE_ACTIONS)
    {
        dwError = ScmSetFailureActions(hSvc,
                                       lpService,
                                       hServiceKey,
                                       (LPSERVICE_FAILURE_ACTIONSW)Info.psfa);
    }

done:
    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("RChangeServiceConfig2W() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 38 */
DWORD RQueryServiceConfig2A(
    SC_RPC_HANDLE hService,
    DWORD dwInfoLevel,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_8K pcbBytesNeeded)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    DWORD dwRequiredSize = 0;
    DWORD dwType = 0;
    LPWSTR lpDescriptionW = NULL;
    LPWSTR lpRebootMessageW = NULL;
    LPWSTR lpFailureCommandW = NULL;

    DPRINT("RQueryServiceConfig2A() called hService %p dwInfoLevel %u, lpBuffer %p cbBufSize %u pcbBytesNeeded %p\n",
           hService, dwInfoLevel, lpBuffer, cbBufSize, pcbBytesNeeded);

    if (!lpBuffer)
        return ERROR_INVALID_ADDRESS;

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    dwError = ScmOpenServiceKey(lpService->lpServiceName,
                                KEY_READ,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    if (dwInfoLevel == SERVICE_CONFIG_DESCRIPTION)
    {
        LPSERVICE_DESCRIPTIONA lpServiceDescription = (LPSERVICE_DESCRIPTIONA)lpBuffer;
        LPSTR lpStr;

        dwError = ScmReadString(hServiceKey,
                                L"Description",
                                &lpDescriptionW);
        if (dwError != ERROR_SUCCESS && dwError != ERROR_FILE_NOT_FOUND)
            goto done;

        *pcbBytesNeeded = sizeof(SERVICE_DESCRIPTIONA);
        if (dwError == ERROR_SUCCESS)
            *pcbBytesNeeded += (DWORD)((wcslen(lpDescriptionW) + 1) * sizeof(WCHAR));

        if (cbBufSize < *pcbBytesNeeded)
        {
            dwError = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        if (dwError == ERROR_SUCCESS)
        {
            lpStr = (LPSTR)(lpServiceDescription + 1);

            WideCharToMultiByte(CP_ACP,
                                0,
                                lpDescriptionW,
                                -1,
                                lpStr,
                                (int)wcslen(lpDescriptionW),
                                NULL,
                                NULL);
            lpServiceDescription->lpDescription = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceDescription);
        }
        else
        {
            lpServiceDescription->lpDescription = NULL;
            dwError = ERROR_SUCCESS;
        }
    }
    else if (dwInfoLevel == SERVICE_CONFIG_FAILURE_ACTIONS)
    {
        LPSERVICE_FAILURE_ACTIONSA lpFailureActions = (LPSERVICE_FAILURE_ACTIONSA)lpBuffer;
        LPSTR lpStr = NULL;

        /* Query value length */
        dwError = RegQueryValueExW(hServiceKey,
                                   L"FailureActions",
                                   NULL,
                                   &dwType,
                                   NULL,
                                   &dwRequiredSize);
        if (dwError != ERROR_SUCCESS &&
            dwError != ERROR_MORE_DATA &&
            dwError != ERROR_FILE_NOT_FOUND)
            goto done;

        dwRequiredSize = (dwType == REG_BINARY) ? max(sizeof(SERVICE_FAILURE_ACTIONSA), dwRequiredSize)
                                                : sizeof(SERVICE_FAILURE_ACTIONSA);

        /* Get the strings */
        ScmReadString(hServiceKey,
                      L"FailureCommand",
                      &lpFailureCommandW);

        ScmReadString(hServiceKey,
                      L"RebootMessage",
                      &lpRebootMessageW);

        if (lpRebootMessageW)
            dwRequiredSize += (DWORD)((wcslen(lpRebootMessageW) + 1) * sizeof(WCHAR));

        if (lpFailureCommandW)
            dwRequiredSize += (DWORD)((wcslen(lpFailureCommandW) + 1) * sizeof(WCHAR));

        if (cbBufSize < dwRequiredSize)
        {
            *pcbBytesNeeded = dwRequiredSize;
            dwError = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        /* Now we can fill the buffer */
        if (dwError != ERROR_FILE_NOT_FOUND && dwType == REG_BINARY)
        {
            dwError = RegQueryValueExW(hServiceKey,
                                       L"FailureActions",
                                       NULL,
                                       NULL,
                                       (LPBYTE)lpFailureActions,
                                       &dwRequiredSize);
            if (dwError != ERROR_SUCCESS && dwError != ERROR_FILE_NOT_FOUND)
                goto done;

            if (dwRequiredSize < sizeof(SERVICE_FAILURE_ACTIONSA))
                dwRequiredSize = sizeof(SERVICE_FAILURE_ACTIONSA);
        }
        else
        {
            /*
             * The value of the error doesn't really matter, the only
             * important thing is that it must be != ERROR_SUCCESS .
             */
            dwError = ERROR_INVALID_DATA;
        }

        if (dwError == ERROR_SUCCESS)
        {
            lpFailureActions->cActions = min(lpFailureActions->cActions, (dwRequiredSize - sizeof(SERVICE_FAILURE_ACTIONSA)) / sizeof(SC_ACTION));

            /* Here lpFailureActions->lpsaActions contains an offset. The conversion is done by the caller. */
            lpFailureActions->lpsaActions = (lpFailureActions->cActions > 0 ? (LPSC_ACTION)(ULONG_PTR)sizeof(SERVICE_FAILURE_ACTIONSA) : NULL);

            lpStr = (LPSTR)((ULONG_PTR)(lpFailureActions + 1) + lpFailureActions->cActions * sizeof(SC_ACTION));
        }
        else
        {
            lpFailureActions->dwResetPeriod = 0;
            lpFailureActions->cActions = 0;
            lpFailureActions->lpsaActions = NULL;
            lpStr = (LPSTR)(lpFailureActions + 1);
        }

        lpFailureActions->lpRebootMsg = NULL;
        lpFailureActions->lpCommand = NULL;

        if (lpRebootMessageW)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpRebootMessageW,
                                -1,
                                lpStr,
                                (int)wcslen(lpRebootMessageW),
                                NULL,
                                NULL);
            lpFailureActions->lpRebootMsg = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpFailureActions);
            lpStr += strlen(lpStr) + 1;
        }

        if (lpFailureCommandW)
        {
            WideCharToMultiByte(CP_ACP,
                                0,
                                lpFailureCommandW,
                                -1,
                                lpStr,
                                (int)wcslen(lpFailureCommandW),
                                NULL,
                                NULL);
            lpFailureActions->lpCommand = (LPSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpFailureActions);
            /* lpStr += strlen(lpStr) + 1; */
        }

        dwError = ERROR_SUCCESS;
    }

done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (lpDescriptionW != NULL)
        HeapFree(GetProcessHeap(), 0, lpDescriptionW);

    if (lpRebootMessageW != NULL)
        HeapFree(GetProcessHeap(), 0, lpRebootMessageW);

    if (lpFailureCommandW != NULL)
        HeapFree(GetProcessHeap(), 0, lpFailureCommandW);

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    DPRINT("RQueryServiceConfig2A() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 39 */
DWORD RQueryServiceConfig2W(
    SC_RPC_HANDLE hService,
    DWORD dwInfoLevel,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_8K pcbBytesNeeded)
{
    DWORD dwError = ERROR_SUCCESS;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService = NULL;
    HKEY hServiceKey = NULL;
    DWORD dwRequiredSize = 0;
    DWORD dwType = 0;
    LPWSTR lpDescription = NULL;
    LPWSTR lpRebootMessage = NULL;
    LPWSTR lpFailureCommand = NULL;

    DPRINT("RQueryServiceConfig2W() called\n");

    if (!lpBuffer)
        return ERROR_INVALID_ADDRESS;

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_CONFIG))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    dwError = ScmOpenServiceKey(lpService->lpServiceName,
                                KEY_READ,
                                &hServiceKey);
    if (dwError != ERROR_SUCCESS)
        goto done;

    if (dwInfoLevel == SERVICE_CONFIG_DESCRIPTION)
    {
        LPSERVICE_DESCRIPTIONW lpServiceDescription = (LPSERVICE_DESCRIPTIONW)lpBuffer;
        LPWSTR lpStr;

        dwError = ScmReadString(hServiceKey,
                                L"Description",
                                &lpDescription);
        if (dwError != ERROR_SUCCESS && dwError != ERROR_FILE_NOT_FOUND)
            goto done;

        *pcbBytesNeeded = sizeof(SERVICE_DESCRIPTIONW);
        if (dwError == ERROR_SUCCESS)
            *pcbBytesNeeded += (DWORD)((wcslen(lpDescription) + 1) * sizeof(WCHAR));

        if (cbBufSize < *pcbBytesNeeded)
        {
            dwError = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        if (dwError == ERROR_SUCCESS)
        {
            lpStr = (LPWSTR)(lpServiceDescription + 1);
            wcscpy(lpStr, lpDescription);
            lpServiceDescription->lpDescription = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpServiceDescription);
        }
        else
        {
            lpServiceDescription->lpDescription = NULL;
            dwError = ERROR_SUCCESS;
        }
    }
    else if (dwInfoLevel == SERVICE_CONFIG_FAILURE_ACTIONS)
    {
        LPSERVICE_FAILURE_ACTIONSW lpFailureActions = (LPSERVICE_FAILURE_ACTIONSW)lpBuffer;
        LPWSTR lpStr = NULL;

        /* Query value length */
        dwError = RegQueryValueExW(hServiceKey,
                                   L"FailureActions",
                                   NULL,
                                   &dwType,
                                   NULL,
                                   &dwRequiredSize);
        if (dwError != ERROR_SUCCESS &&
            dwError != ERROR_MORE_DATA &&
            dwError != ERROR_FILE_NOT_FOUND)
            goto done;

        dwRequiredSize = (dwType == REG_BINARY) ? max(sizeof(SERVICE_FAILURE_ACTIONSW), dwRequiredSize)
                                                : sizeof(SERVICE_FAILURE_ACTIONSW);

        /* Get the strings */
        ScmReadString(hServiceKey,
                      L"FailureCommand",
                      &lpFailureCommand);

        ScmReadString(hServiceKey,
                      L"RebootMessage",
                      &lpRebootMessage);

        if (lpRebootMessage)
            dwRequiredSize += (DWORD)((wcslen(lpRebootMessage) + 1) * sizeof(WCHAR));

        if (lpFailureCommand)
            dwRequiredSize += (DWORD)((wcslen(lpFailureCommand) + 1) * sizeof(WCHAR));

        if (cbBufSize < dwRequiredSize)
        {
            *pcbBytesNeeded = dwRequiredSize;
            dwError = ERROR_INSUFFICIENT_BUFFER;
            goto done;
        }

        /* Now we can fill the buffer */
        if (dwError != ERROR_FILE_NOT_FOUND && dwType == REG_BINARY)
        {
            dwError = RegQueryValueExW(hServiceKey,
                                       L"FailureActions",
                                       NULL,
                                       NULL,
                                       (LPBYTE)lpFailureActions,
                                       &dwRequiredSize);
            if (dwError != ERROR_SUCCESS && dwError != ERROR_FILE_NOT_FOUND)
                goto done;

            if (dwRequiredSize < sizeof(SERVICE_FAILURE_ACTIONSW))
                dwRequiredSize = sizeof(SERVICE_FAILURE_ACTIONSW);
        }
        else
        {
            /*
             * The value of the error doesn't really matter, the only
             * important thing is that it must be != ERROR_SUCCESS .
             */
            dwError = ERROR_INVALID_DATA;
        }

        if (dwError == ERROR_SUCCESS)
        {
            lpFailureActions->cActions = min(lpFailureActions->cActions, (dwRequiredSize - sizeof(SERVICE_FAILURE_ACTIONSW)) / sizeof(SC_ACTION));

            /* Here lpFailureActions->lpsaActions contains an offset. The conversion is done by the caller. */
            lpFailureActions->lpsaActions = (lpFailureActions->cActions > 0 ? (LPSC_ACTION)(ULONG_PTR)sizeof(SERVICE_FAILURE_ACTIONSW) : NULL);

            lpStr = (LPWSTR)((ULONG_PTR)(lpFailureActions + 1) + lpFailureActions->cActions * sizeof(SC_ACTION));
        }
        else
        {
            lpFailureActions->dwResetPeriod = 0;
            lpFailureActions->cActions = 0;
            lpFailureActions->lpsaActions = NULL;
            lpStr = (LPWSTR)(lpFailureActions + 1);
        }

        lpFailureActions->lpRebootMsg = NULL;
        lpFailureActions->lpCommand   = NULL;

        if (lpRebootMessage)
        {
            wcscpy(lpStr, lpRebootMessage);
            lpFailureActions->lpRebootMsg = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpFailureActions);
            lpStr += wcslen(lpStr) + 1;
        }

        if (lpFailureCommand)
        {
            wcscpy(lpStr, lpFailureCommand);
            lpFailureActions->lpCommand = (LPWSTR)((ULONG_PTR)lpStr - (ULONG_PTR)lpFailureActions);
            /* lpStr += wcslen(lpStr) + 1; */
        }

        dwError = ERROR_SUCCESS;
    }

done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    if (lpDescription != NULL)
        HeapFree(GetProcessHeap(), 0, lpDescription);

    if (lpRebootMessage != NULL)
        HeapFree(GetProcessHeap(), 0, lpRebootMessage);

    if (lpFailureCommand != NULL)
        HeapFree(GetProcessHeap(), 0, lpFailureCommand);

    if (hServiceKey != NULL)
        RegCloseKey(hServiceKey);

    DPRINT("RQueryServiceConfig2W() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 40 */
DWORD RQueryServiceStatusEx(
    SC_RPC_HANDLE hService,
    SC_STATUS_TYPE InfoLevel,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_8K pcbBytesNeeded)
{
    LPSERVICE_STATUS_PROCESS lpStatus;
    PSERVICE_HANDLE hSvc;
    PSERVICE lpService;

    DPRINT("RQueryServiceStatusEx() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    if (InfoLevel != SC_STATUS_PROCESS_INFO)
        return ERROR_INVALID_LEVEL;

    *pcbBytesNeeded = sizeof(SERVICE_STATUS_PROCESS);

    if (cbBufSize < sizeof(SERVICE_STATUS_PROCESS))
        return ERROR_INSUFFICIENT_BUFFER;

    hSvc = ScmGetServiceFromHandle(hService);
    if (hSvc == NULL)
    {
        DPRINT1("Invalid service handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    if (!RtlAreAllAccessesGranted(hSvc->Handle.DesiredAccess,
                                  SERVICE_QUERY_STATUS))
    {
        DPRINT("Insufficient access rights! 0x%lx\n", hSvc->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    lpService = hSvc->ServiceEntry;
    if (lpService == NULL)
    {
        DPRINT("lpService == NULL!\n");
        return ERROR_INVALID_HANDLE;
    }

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    lpStatus = (LPSERVICE_STATUS_PROCESS)lpBuffer;

    /* Return service status information */
    RtlCopyMemory(lpStatus,
                  &lpService->Status,
                  sizeof(SERVICE_STATUS));

    lpStatus->dwProcessId = (lpService->lpImage != NULL) ? lpService->lpImage->dwProcessId : 0; /* FIXME */
    lpStatus->dwServiceFlags = 0;			/* FIXME */

    /* Unlock the service database */
    ScmUnlockDatabase();

    return ERROR_SUCCESS;
}


/* Function 41 */
DWORD REnumServicesStatusExA(
    SC_RPC_HANDLE hSCManager,
    SC_ENUM_TYPE InfoLevel,
    DWORD dwServiceType,
    DWORD dwServiceState,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned,
    LPBOUNDED_DWORD_256K lpResumeIndex,
    LPCSTR pszGroupName)
{
    LPENUM_SERVICE_STATUS_PROCESSW lpStatusPtrW = NULL;
    LPENUM_SERVICE_STATUS_PROCESSW lpStatusPtrIncrW;
    LPENUM_SERVICE_STATUS_PROCESSA lpStatusPtrA = NULL;
    LPWSTR lpStringPtrW;
    LPSTR lpStringPtrA;
    LPWSTR pszGroupNameW = NULL;
    DWORD dwError;
    DWORD dwServiceCount;

    DPRINT("REnumServicesStatusExA() called\n");

    if (pszGroupName)
    {
        pszGroupNameW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (strlen(pszGroupName) + 1) * sizeof(WCHAR));
        if (!pszGroupNameW)
        {
             DPRINT("Failed to allocate buffer!\n");
             return ERROR_NOT_ENOUGH_MEMORY;
        }

        MultiByteToWideChar(CP_ACP,
                            0,
                            pszGroupName,
                            -1,
                            pszGroupNameW,
                            (int)(strlen(pszGroupName) + 1));
    }

    if ((cbBufSize > 0) && (lpBuffer))
    {
        lpStatusPtrW = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbBufSize);
        if (!lpStatusPtrW)
        {
            DPRINT("Failed to allocate buffer!\n");
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    dwError = REnumServicesStatusExW(hSCManager,
                                     InfoLevel,
                                     dwServiceType,
                                     dwServiceState,
                                     (LPBYTE)lpStatusPtrW,
                                     cbBufSize,
                                     pcbBytesNeeded,
                                     lpServicesReturned,
                                     lpResumeIndex,
                                     pszGroupNameW);

    /* if no services were returned then we are Done */
    if (*lpServicesReturned == 0)
        goto Done;

    lpStatusPtrIncrW = lpStatusPtrW;
    lpStatusPtrA = (LPENUM_SERVICE_STATUS_PROCESSA)lpBuffer;
    lpStringPtrA = (LPSTR)((ULONG_PTR)lpBuffer +
                  *lpServicesReturned * sizeof(ENUM_SERVICE_STATUS_PROCESSA));
    lpStringPtrW = (LPWSTR)((ULONG_PTR)lpStatusPtrW +
                  *lpServicesReturned * sizeof(ENUM_SERVICE_STATUS_PROCESSW));

    for (dwServiceCount = 0; dwServiceCount < *lpServicesReturned; dwServiceCount++)
    {
        /* Copy the service name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpStringPtrW,
                            -1,
                            lpStringPtrA,
                            (int)wcslen(lpStringPtrW),
                            0,
                            0);

        lpStatusPtrA->lpServiceName = (LPSTR)((ULONG_PTR)lpStringPtrA - (ULONG_PTR)lpBuffer);
        lpStringPtrA += wcslen(lpStringPtrW) + 1;
        lpStringPtrW += wcslen(lpStringPtrW) + 1;

        /* Copy the display name */
        WideCharToMultiByte(CP_ACP,
                            0,
                            lpStringPtrW,
                            -1,
                            lpStringPtrA,
                            (int)wcslen(lpStringPtrW),
                            0,
                            0);

        lpStatusPtrA->lpDisplayName = (LPSTR)((ULONG_PTR)lpStringPtrA - (ULONG_PTR)lpBuffer);
        lpStringPtrA += wcslen(lpStringPtrW) + 1;
        lpStringPtrW += wcslen(lpStringPtrW) + 1;

        /* Copy the status information */
        memcpy(&lpStatusPtrA->ServiceStatusProcess,
               &lpStatusPtrIncrW->ServiceStatusProcess,
               sizeof(SERVICE_STATUS));

        lpStatusPtrA->ServiceStatusProcess.dwProcessId = lpStatusPtrIncrW->ServiceStatusProcess.dwProcessId; /* FIXME */
        lpStatusPtrA->ServiceStatusProcess.dwServiceFlags = 0; /* FIXME */

        lpStatusPtrIncrW++;
        lpStatusPtrA++;
    }

Done:
    if (pszGroupNameW)
        HeapFree(GetProcessHeap(), 0, pszGroupNameW);

    if (lpStatusPtrW)
        HeapFree(GetProcessHeap(), 0, lpStatusPtrW);

    DPRINT("REnumServicesStatusExA() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 42 */
DWORD REnumServicesStatusExW(
    SC_RPC_HANDLE hSCManager,
    SC_ENUM_TYPE InfoLevel,
    DWORD dwServiceType,
    DWORD dwServiceState,
    LPBYTE lpBuffer,
    DWORD cbBufSize,
    LPBOUNDED_DWORD_256K pcbBytesNeeded,
    LPBOUNDED_DWORD_256K lpServicesReturned,
    LPBOUNDED_DWORD_256K lpResumeIndex,
    LPCWSTR pszGroupName)
{
    PMANAGER_HANDLE hManager;
    PSERVICE lpService;
    DWORD dwError = ERROR_SUCCESS;
    PLIST_ENTRY ServiceEntry;
    PSERVICE CurrentService;
    DWORD dwState;
    DWORD dwRequiredSize;
    DWORD dwServiceCount;
    DWORD dwSize;
    DWORD dwLastResumeCount = 0;
    LPENUM_SERVICE_STATUS_PROCESSW lpStatusPtr;
    LPWSTR lpStringPtr;

    DPRINT("REnumServicesStatusExW() called\n");

    if (ScmShutdown)
        return ERROR_SHUTDOWN_IN_PROGRESS;

    if (InfoLevel != SC_ENUM_PROCESS_INFO)
        return ERROR_INVALID_LEVEL;

    hManager = ScmGetServiceManagerFromHandle(hSCManager);
    if (hManager == NULL)
    {
        DPRINT1("Invalid service manager handle!\n");
        return ERROR_INVALID_HANDLE;
    }

    *pcbBytesNeeded = 0;
    *lpServicesReturned = 0;

    if ((dwServiceType == 0) ||
        ((dwServiceType & ~SERVICE_TYPE_ALL) != 0))
    {
        DPRINT("Not a valid Service Type!\n");
        return ERROR_INVALID_PARAMETER;
    }

    if ((dwServiceState == 0) ||
        ((dwServiceState & ~SERVICE_STATE_ALL) != 0))
    {
        DPRINT("Not a valid Service State!\n");
        return ERROR_INVALID_PARAMETER;
    }

    /* Check access rights */
    if (!RtlAreAllAccessesGranted(hManager->Handle.DesiredAccess,
                                  SC_MANAGER_ENUMERATE_SERVICE))
    {
        DPRINT("Insufficient access rights! 0x%lx\n",
               hManager->Handle.DesiredAccess);
        return ERROR_ACCESS_DENIED;
    }

    if (lpResumeIndex)
        dwLastResumeCount = *lpResumeIndex;

    /* Lock the service database shared */
    ScmLockDatabaseShared();

    lpService = ScmGetServiceEntryByResumeCount(dwLastResumeCount);
    if (lpService == NULL)
    {
        dwError = ERROR_SUCCESS;
        goto Done;
    }

    dwRequiredSize = 0;
    dwServiceCount = 0;

    for (ServiceEntry = &lpService->ServiceListEntry;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwSize = sizeof(ENUM_SERVICE_STATUS_PROCESSW) +
                 (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                 (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR));

        if (dwRequiredSize + dwSize <= cbBufSize)
        {
            DPRINT("Service name: %S  fit\n", CurrentService->lpServiceName);
            dwRequiredSize += dwSize;
            dwServiceCount++;
            dwLastResumeCount = CurrentService->dwResumeCount;
        }
        else
        {
            DPRINT("Service name: %S  no fit\n", CurrentService->lpServiceName);
            break;
        }

    }

    DPRINT("dwRequiredSize: %lu\n", dwRequiredSize);
    DPRINT("dwServiceCount: %lu\n", dwServiceCount);

    for (;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwRequiredSize += (sizeof(ENUM_SERVICE_STATUS_PROCESSW) +
                           (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                           (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR)));

        dwError = ERROR_MORE_DATA;
    }

    DPRINT("*pcbBytesNeeded: %lu\n", dwRequiredSize);

    if (lpResumeIndex)
        *lpResumeIndex = dwLastResumeCount;

    *lpServicesReturned = dwServiceCount;
    *pcbBytesNeeded = dwRequiredSize;

    /* If there was no services that matched */
    if ((!dwServiceCount) && (dwError != ERROR_MORE_DATA))
    {
        dwError = ERROR_SERVICE_DOES_NOT_EXIST;
        goto Done;
    }

    lpStatusPtr = (LPENUM_SERVICE_STATUS_PROCESSW)lpBuffer;
    lpStringPtr = (LPWSTR)((ULONG_PTR)lpBuffer +
                           dwServiceCount * sizeof(ENUM_SERVICE_STATUS_PROCESSW));

    dwRequiredSize = 0;
    for (ServiceEntry = &lpService->ServiceListEntry;
         ServiceEntry != &ServiceListHead;
         ServiceEntry = ServiceEntry->Flink)
    {
        CurrentService = CONTAINING_RECORD(ServiceEntry,
                                           SERVICE,
                                           ServiceListEntry);

        if ((CurrentService->Status.dwServiceType & dwServiceType) == 0)
            continue;

        dwState = SERVICE_ACTIVE;
        if (CurrentService->Status.dwCurrentState == SERVICE_STOPPED)
            dwState = SERVICE_INACTIVE;

        if ((dwState & dwServiceState) == 0)
            continue;

        if (pszGroupName)
        {
            if (*pszGroupName == 0)
            {
                if (CurrentService->lpGroup != NULL)
                    continue;
            }
            else
            {
                if ((CurrentService->lpGroup == NULL) ||
                    _wcsicmp(pszGroupName, CurrentService->lpGroup->lpGroupName) != 0)
                    continue;
            }
        }

        dwSize = sizeof(ENUM_SERVICE_STATUS_PROCESSW) +
                 (DWORD)((wcslen(CurrentService->lpServiceName) + 1) * sizeof(WCHAR)) +
                 (DWORD)((wcslen(CurrentService->lpDisplayName) + 1) * sizeof(WCHAR));

        if (dwRequiredSize + dwSize <= cbBufSize)
        {
            /* Copy the service name */
            wcscpy(lpStringPtr,
                   CurrentService->lpServiceName);
            lpStatusPtr->lpServiceName = (LPWSTR)((ULONG_PTR)lpStringPtr - (ULONG_PTR)lpBuffer);
            lpStringPtr += (wcslen(CurrentService->lpServiceName) + 1);

            /* Copy the display name */
            wcscpy(lpStringPtr,
                   CurrentService->lpDisplayName);
            lpStatusPtr->lpDisplayName = (LPWSTR)((ULONG_PTR)lpStringPtr - (ULONG_PTR)lpBuffer);
            lpStringPtr += (wcslen(CurrentService->lpDisplayName) + 1);

            /* Copy the status information */
            memcpy(&lpStatusPtr->ServiceStatusProcess,
                   &CurrentService->Status,
                   sizeof(SERVICE_STATUS));
            lpStatusPtr->ServiceStatusProcess.dwProcessId =
                (CurrentService->lpImage != NULL) ? CurrentService->lpImage->dwProcessId : 0; /* FIXME */
            lpStatusPtr->ServiceStatusProcess.dwServiceFlags = 0; /* FIXME */

            lpStatusPtr++;
            dwRequiredSize += dwSize;
        }
        else
        {
            break;
        }
    }

    if (dwError == 0)
    {
        *pcbBytesNeeded = 0;
        if (lpResumeIndex)
            *lpResumeIndex = 0;
    }

Done:
    /* Unlock the service database */
    ScmUnlockDatabase();

    DPRINT("REnumServicesStatusExW() done (Error %lu)\n", dwError);

    return dwError;
}


/* Function 43 */
DWORD RSendTSMessage(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 44 */
DWORD RCreateServiceWOW64A(
    handle_t BindingHandle,
    LPSTR lpServiceName,
    LPSTR lpDisplayName,
    DWORD dwDesiredAccess,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPSTR lpBinaryPathName,
    LPSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 45 */
DWORD RCreateServiceWOW64W(
    handle_t BindingHandle,
    LPWSTR lpServiceName,
    LPWSTR lpDisplayName,
    DWORD dwDesiredAccess,
    DWORD dwServiceType,
    DWORD dwStartType,
    DWORD dwErrorControl,
    LPWSTR lpBinaryPathName,
    LPWSTR lpLoadOrderGroup,
    LPDWORD lpdwTagId,
    LPBYTE lpDependencies,
    DWORD dwDependSize,
    LPWSTR lpServiceStartName,
    LPBYTE lpPassword,
    DWORD dwPwSize,
    LPSC_RPC_HANDLE lpServiceHandle)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 46 */
DWORD RQueryServiceTagInfo(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 47 */
DWORD RNotifyServiceStatusChange(
    SC_RPC_HANDLE hService,
    SC_RPC_NOTIFY_PARAMS NotifyParams,
    GUID *pClientProcessGuid,
    GUID *pSCMProcessGuid,
    PBOOL pfCreateRemoteQueue,
    LPSC_NOTIFY_RPC_HANDLE phNotify)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 48 */
DWORD RGetNotifyResults(
    SC_NOTIFY_RPC_HANDLE hNotify,
    PSC_RPC_NOTIFY_PARAMS_LIST *ppNotifyParams)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 49 */
DWORD RCloseNotifyHandle(
    LPSC_NOTIFY_RPC_HANDLE phNotify,
    PBOOL pfApcFired)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 50 */
DWORD RControlServiceExA(
    SC_RPC_HANDLE hService,
    DWORD dwControl,
    DWORD dwInfoLevel)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 51 */
DWORD RControlServiceExW(
    SC_RPC_HANDLE hService,
    DWORD dwControl,
    DWORD dwInfoLevel)
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 52 */
DWORD RSendPnPMessage(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 53 */
DWORD RValidatePnPService(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 54 */
DWORD ROpenServiceStatusHandle(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/* Function 55 */
DWORD RFunction55(
    handle_t BindingHandle)  /* FIXME */
{
    UNIMPLEMENTED;
    return ERROR_CALL_NOT_IMPLEMENTED;
}


void __RPC_FAR * __RPC_USER midl_user_allocate(SIZE_T len)
{
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
}


void __RPC_USER midl_user_free(void __RPC_FAR * ptr)
{
    HeapFree(GetProcessHeap(), 0, ptr);
}


void __RPC_USER SC_RPC_HANDLE_rundown(SC_RPC_HANDLE hSCObject)
{
    /* Close the handle */
    RCloseServiceHandle(&hSCObject);
}


void __RPC_USER SC_RPC_LOCK_rundown(SC_RPC_LOCK Lock)
{
    /* Unlock the database */
    RUnlockServiceDatabase(&Lock);
}


void __RPC_USER SC_NOTIFY_RPC_HANDLE_rundown(SC_NOTIFY_RPC_HANDLE hNotify)
{
}

/* EOF */
