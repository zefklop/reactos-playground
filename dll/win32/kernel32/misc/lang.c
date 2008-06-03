/* $Id$
 *
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS system libraries
 * FILE:            dll/win32/kernel32/file/lang.c
 * PURPOSE:         National Laguage Support related funcs
 * PROGRAMMER:      Thomas Weidenmueller
 *                  Gunnar Andre Dalsnes
 *                  Aleksey Bragin
 *                  Eric Kohl
 *                  Alex Ionescu
 *                  Richard Campbell
 *                  James Tabor
 * UPDATE HISTORY:
 *                  Created 21/09/2003
 */

#include <k32.h>

#define NDEBUG
#include <debug.h>

#include "lcformat_private.h"

/* FIXME:  these are included in winnls.h, however including this file causes alot of
           conflicting type errors. */

#define LOCALE_RETURN_NUMBER 0x20000000
#define LOCALE_USE_CP_ACP 0x40000000
#define LOCALE_LOCALEINFOFLAGSMASK (LOCALE_NOUSEROVERRIDE|LOCALE_USE_CP_ACP|LOCALE_RETURN_NUMBER)
#define CALINFO_MAX_YEAR 2029

//static LCID SystemLocale = MAKELCID(LANG_ENGLISH, SORT_DEFAULT);

//static RTL_CRITICAL_SECTION LocalesListLock;

typedef struct
{
    union
    {
        UILANGUAGE_ENUMPROCA procA;
        UILANGUAGE_ENUMPROCW procW;
    } u;
    DWORD flags;
    LONG_PTR param;
} ENUM_UILANG_CALLBACK;

static const WCHAR szLocaleKeyName[] = 

{
    'M','a','c','h','i','n','e','\\','S','y','s','t','e','m','\\',
    'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
    'C','o','n','t','r','o','l','\\','N','l','s','\\','L','o','c','a','l','e',0
};

static const WCHAR szLangGroupsKeyName[] = {
    'M','a','c','h','i','n','e','\\','S','y','s','t','e','m','\\',
    'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
    'C','o','n','t','r','o','l','\\','N','l','s','\\',
    'L','a','n','g','u','a','g','e',' ','G','r','o','u','p','s',0
};

/******************************************************************************
 * @implemented
 * RIPPED FROM WINE's dlls\kernel\locale.c rev 1.42
 *
 *    ConvertDefaultLocale (KERNEL32.@)
 *
 * Convert a default locale identifier into a real identifier.
 *
 * PARAMS
 *  lcid [I] LCID identifier of the locale to convert
 *
 * RETURNS
 *  lcid unchanged, if not a default locale or its sublanguage is
 *   not SUBLANG_NEUTRAL.
 *  GetSystemDefaultLCID(), if lcid == LOCALE_SYSTEM_DEFAULT.
 *  GetUserDefaultLCID(), if lcid == LOCALE_USER_DEFAULT or LOCALE_NEUTRAL.
 *  Otherwise, lcid with sublanguage changed to SUBLANG_DEFAULT.
 */
LCID WINAPI
ConvertDefaultLocale(LCID lcid)
{
  LANGID langid;

  switch (lcid)
  {
    case LOCALE_SYSTEM_DEFAULT:
      lcid = GetSystemDefaultLCID();
      break;

    case LOCALE_USER_DEFAULT:
    case LOCALE_NEUTRAL:
      lcid = GetUserDefaultLCID();
      break;

    default:
      /* Replace SUBLANG_NEUTRAL with SUBLANG_DEFAULT */
      langid = LANGIDFROMLCID(lcid);
      if (SUBLANGID(langid) == SUBLANG_NEUTRAL)
      {
        langid = MAKELANGID(PRIMARYLANGID(langid), SUBLANG_DEFAULT);
        lcid = MAKELCID(langid, SORTIDFROMLCID(lcid));
      }
  }

  return lcid;
}


/**************************************************************************
 *              EnumDateFormatsExA    (KERNEL32.@)
 *
 * FIXME: MSDN mentions only LOCALE_USE_CP_ACP, should we handle
 * LOCALE_NOUSEROVERRIDE here as well?
 */
BOOL
STDCALL
EnumDateFormatsExA(
    DATEFMT_ENUMPROCEXA lpDateFmtEnumProcEx,
    LCID                Locale,
    DWORD               dwFlags)
{
    CALID cal_id;
    char szBuf[256];

    if (!lpDateFmtEnumProcEx)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!GetLocaleInfoW(Locale,
                        LOCALE_ICALENDARTYPE|LOCALE_RETURN_NUMBER,
                        (LPWSTR)&cal_id,
                        sizeof(cal_id)/sizeof(WCHAR)))
    {
        return FALSE;
    }

    switch (dwFlags & ~LOCALE_USE_CP_ACP)
    {
        case 0:
        case DATE_SHORTDATE:
            if (GetLocaleInfoA(Locale,
                LOCALE_SSHORTDATE | (dwFlags & LOCALE_USE_CP_ACP),
                szBuf, 256))
            {
                lpDateFmtEnumProcEx(szBuf, cal_id);
            }
            break;

        case DATE_LONGDATE:
            if (GetLocaleInfoA(Locale,
                LOCALE_SLONGDATE | (dwFlags & LOCALE_USE_CP_ACP),
                szBuf, 256))
            {
                lpDateFmtEnumProcEx(szBuf, cal_id);
            }
            break;

        case DATE_YEARMONTH:
            if (GetLocaleInfoA(Locale,
                LOCALE_SYEARMONTH | (dwFlags & LOCALE_USE_CP_ACP),
                szBuf, 256))
            {
                lpDateFmtEnumProcEx(szBuf, cal_id);
            }
            break;

        default:
            // FIXME: Unknown date format
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }
    return TRUE;
}


/**************************************************************************
 *              EnumDateFormatsExW    (KERNEL32.@)
 */
BOOL
STDCALL
EnumDateFormatsExW(
    DATEFMT_ENUMPROCEXW lpDateFmtEnumProcEx,
    LCID                Locale,
    DWORD               dwFlags)
{
    CALID cal_id;
    WCHAR wbuf[256]; // FIXME

    if (!lpDateFmtEnumProcEx)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!GetLocaleInfoW(Locale,
                        LOCALE_ICALENDARTYPE | LOCALE_RETURN_NUMBER,
                        (LPWSTR)&cal_id,
                        sizeof(cal_id)/sizeof(WCHAR)))
    {
        return FALSE;
    }

    switch (dwFlags & ~LOCALE_USE_CP_ACP)
    {
        case 0:
        case DATE_SHORTDATE:
            if (GetLocaleInfoW(Locale,
                               LOCALE_SSHORTDATE | (dwFlags & LOCALE_USE_CP_ACP),
                               wbuf,
                               256))
            {
                lpDateFmtEnumProcEx(wbuf, cal_id);
            }
            break;

        case DATE_LONGDATE:
            if (GetLocaleInfoW(Locale,
                               LOCALE_SLONGDATE | (dwFlags & LOCALE_USE_CP_ACP),
                               wbuf,
                               256))
            {
                lpDateFmtEnumProcEx(wbuf, cal_id);
            }
            break;

        case DATE_YEARMONTH:
            if (GetLocaleInfoW(Locale,
                               LOCALE_SYEARMONTH | (dwFlags & LOCALE_USE_CP_ACP),
                               wbuf,
                               256))
            {
                lpDateFmtEnumProcEx(wbuf, cal_id);
            }
            break;

        default:
            // FIXME: Unknown date format
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
    }
    return TRUE;
}


static BOOL NLS_RegEnumValue(HANDLE hKey, UINT ulIndex,
                             LPWSTR szValueName, ULONG valueNameSize,
                             LPWSTR szValueData, ULONG valueDataSize)
{
    BYTE buffer[80];
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;
    DWORD dwLen;

    if (NtEnumerateValueKey( hKey, ulIndex, KeyValueFullInformation,
        buffer, sizeof(buffer), &dwLen ) != STATUS_SUCCESS ||
        info->NameLength > valueNameSize ||
        info->DataLength > valueDataSize)
    {
        return FALSE;
    }

    DPRINT("info->Name %s info->DataLength %d\n", info->Name, info->DataLength);

    memcpy( szValueName, info->Name, info->NameLength);
    szValueName[info->NameLength / sizeof(WCHAR)] = '\0';
    memcpy( szValueData, buffer + info->DataOffset, info->DataLength );
    szValueData[info->DataLength / sizeof(WCHAR)] = '\0';

    DPRINT("returning %s %s\n", szValueName, szValueData);
    return TRUE;
}


static HANDLE NLS_RegOpenKey(HANDLE hRootKey, LPCWSTR szKeyName)
{
    UNICODE_STRING keyName;
    OBJECT_ATTRIBUTES attr;
    HANDLE hkey;

    RtlInitUnicodeString( &keyName, szKeyName );
    InitializeObjectAttributes(&attr, &keyName, 0, hRootKey, NULL);

    if (NtOpenKey( &hkey, KEY_ALL_ACCESS, &attr ) != STATUS_SUCCESS)
        hkey = 0;

    return hkey;
}

static BOOL NLS_RegEnumSubKey(HANDLE hKey, UINT ulIndex, LPWSTR szKeyName,
                              ULONG keyNameSize)
{
    BYTE buffer[80];
    KEY_BASIC_INFORMATION *info = (KEY_BASIC_INFORMATION *)buffer;
    DWORD dwLen;

    if (NtEnumerateKey( hKey, ulIndex, KeyBasicInformation, buffer,
                        sizeof(buffer), &dwLen) != STATUS_SUCCESS ||
        info->NameLength > keyNameSize)
    {
        return FALSE;
    }

    DPRINT("info->Name %s info->NameLength %d\n", info->Name, info->NameLength);

    memcpy( szKeyName, info->Name, info->NameLength);
    szKeyName[info->NameLength / sizeof(WCHAR)] = '\0';

    DPRINT("returning %s\n", szKeyName);
    return TRUE;
}

static BOOL NLS_RegGetDword(HANDLE hKey, LPCWSTR szValueName, DWORD *lpVal)
{
    BYTE buffer[128];
    const KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    DWORD dwSize = sizeof(buffer);
    UNICODE_STRING valueName;

    RtlInitUnicodeString( &valueName, szValueName );

    DPRINT("%p, %s\n", hKey, szValueName);
    if (NtQueryValueKey( hKey, &valueName, KeyValuePartialInformation,
                         buffer, dwSize, &dwSize ) == STATUS_SUCCESS &&
        info->DataLength == sizeof(DWORD))
    {
        memcpy(lpVal, info->Data, sizeof(DWORD));
        return TRUE;
    }

    return FALSE;
}

static BOOL NLS_GetLanguageGroupName(LGRPID lgrpid, LPWSTR szName, ULONG nameSize)
{
    LANGID  langId;
    LPCWSTR szResourceName = MAKEINTRESOURCEW(((lgrpid + 0x2000) >> 4) + 1);
    HRSRC   hResource;
    BOOL    bRet = FALSE;

    /* FIXME: Is it correct to use the system default langid? */
    langId = GetSystemDefaultLangID();

    if (SUBLANGID(langId) == SUBLANG_NEUTRAL)
        langId = MAKELANGID( PRIMARYLANGID(langId), SUBLANG_DEFAULT );

    hResource = FindResourceExW( hCurrentModule, (LPWSTR)RT_STRING, szResourceName, langId );

    if (hResource)
    {
        HGLOBAL hResDir = LoadResource( hCurrentModule, hResource );

        if (hResDir)
        {
            ULONG   iResourceIndex = lgrpid & 0xf;
            LPCWSTR lpResEntry = LockResource( hResDir );
            ULONG   i;

            for (i = 0; i < iResourceIndex; i++)
                lpResEntry += *lpResEntry + 1;

            if (*lpResEntry < nameSize)
            {
                memcpy( szName, lpResEntry + 1, *lpResEntry * sizeof(WCHAR) );
                szName[*lpResEntry] = '\0';
                bRet = TRUE;
            }

        }
        FreeResource( hResource );
    }
    return bRet;
}


/* Callback function ptrs for EnumLanguageGrouplocalesA/W */
typedef struct
{
  LANGGROUPLOCALE_ENUMPROCA procA;
  LANGGROUPLOCALE_ENUMPROCW procW;
  DWORD    dwFlags;
  LGRPID   lgrpid;
  LONG_PTR lParam;
} ENUMLANGUAGEGROUPLOCALE_CALLBACKS;

/* Internal implementation of EnumLanguageGrouplocalesA/W */
static BOOL NLS_EnumLanguageGroupLocales(ENUMLANGUAGEGROUPLOCALE_CALLBACKS *lpProcs)
{
    static const WCHAR szAlternateSortsKeyName[] = {
      'A','l','t','e','r','n','a','t','e',' ','S','o','r','t','s','\0'
    };
    WCHAR szNumber[10], szValue[4];
    HANDLE hKey;
    BOOL bContinue = TRUE, bAlternate = FALSE;
    LGRPID lgrpid;
    ULONG ulIndex = 1;  /* Ignore default entry of 1st key */

    if (!lpProcs || !lpProcs->lgrpid || lpProcs->lgrpid > LGRPID_ARMENIAN)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (lpProcs->dwFlags)
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return FALSE;
    }

    hKey = NLS_RegOpenKey( 0, szLocaleKeyName );

    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (bContinue)
    {
        if (NLS_RegEnumValue( hKey, ulIndex, szNumber, sizeof(szNumber),
                              szValue, sizeof(szValue) ))
        {
            lgrpid = wcstoul( szValue, NULL, 16 );

            DPRINT("lcid %s, grpid %d (%smatched)\n", szNumber,
                   lgrpid, lgrpid == lpProcs->lgrpid ? "" : "not ");

            if (lgrpid == lpProcs->lgrpid)
            {
                LCID lcid;

                lcid = wcstoul( szNumber, NULL, 16 );

                /* FIXME: native returns extra text for a few (17/150) locales, e.g:
                 * '00000437          ;Georgian'
                 * At present we only pass the LCID string.
                 */

                if (lpProcs->procW)
                    bContinue = lpProcs->procW( lgrpid, lcid, szNumber, lpProcs->lParam );
                else
                {
                    char szNumberA[sizeof(szNumber)/sizeof(WCHAR)];

                    WideCharToMultiByte(CP_ACP, 0, szNumber, -1, szNumberA, sizeof(szNumberA), 0, 0);

                    bContinue = lpProcs->procA( lgrpid, lcid, szNumberA, lpProcs->lParam );
                }
            }

            ulIndex++;
        }
        else
        {
            /* Finished enumerating this key */
            if (!bAlternate)
            {
                /* Enumerate alternate sorts also */
                hKey = NLS_RegOpenKey( hKey, szAlternateSortsKeyName );
                bAlternate = TRUE;
                ulIndex = 0;
            }
            else
                bContinue = FALSE; /* Finished both keys */
        }

        if (!bContinue)
            break;
    }

    if (hKey)
        NtClose( hKey );

    return TRUE;
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumLanguageGroupLocalesA(
    LANGGROUPLOCALE_ENUMPROCA lpLangGroupLocaleEnumProc,
    LGRPID                    LanguageGroup,
    DWORD                     dwFlags,
    LONG_PTR                  lParam)
{
    ENUMLANGUAGEGROUPLOCALE_CALLBACKS callbacks;

    DPRINT("(%p,0x%08X,0x%08X,0x%08lX)\n", lpLangGroupLocaleEnumProc, LanguageGroup, dwFlags, lParam);

    callbacks.procA   = lpLangGroupLocaleEnumProc;
    callbacks.procW   = NULL;
    callbacks.dwFlags = dwFlags;
    callbacks.lgrpid  = LanguageGroup;
    callbacks.lParam  = lParam;

    return NLS_EnumLanguageGroupLocales( lpLangGroupLocaleEnumProc ? &callbacks : NULL );
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumLanguageGroupLocalesW(
    LANGGROUPLOCALE_ENUMPROCW lpLangGroupLocaleEnumProc,
    LGRPID                    LanguageGroup,
    DWORD                     dwFlags,
    LONG_PTR                  lParam)
{
    ENUMLANGUAGEGROUPLOCALE_CALLBACKS callbacks;

    DPRINT("(%p,0x%08X,0x%08X,0x%08lX)\n", lpLangGroupLocaleEnumProc, LanguageGroup, dwFlags, lParam);

    callbacks.procA   = NULL;
    callbacks.procW   = lpLangGroupLocaleEnumProc;
    callbacks.dwFlags = dwFlags;
    callbacks.lgrpid  = LanguageGroup;
    callbacks.lParam  = lParam;

    return NLS_EnumLanguageGroupLocales( lpLangGroupLocaleEnumProc ? &callbacks : NULL );
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemCodePagesW (
    CODEPAGE_ENUMPROCW  lpCodePageEnumProc,
    DWORD               dwFlags
    )
{
    WCHAR szNumber[5 + 1], szValue[MAX_PATH];
    HKEY hKey;
    BOOL bContinue = TRUE;
    ULONG ulIndex = 1;

    DPRINT("(%p,0x%08X)\n", lpCodePageEnumProc, dwFlags);

    if ((!dwFlags & CP_INSTALLED)&&(!dwFlags & CP_SUPPORTED))
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return FALSE;
    }

    if (!lpCodePageEnumProc)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    hKey = NLS_RegOpenKey(0, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\NLS\\CodePage");
    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (bContinue)
    {
        if (NLS_RegEnumValue(hKey, ulIndex, szNumber, sizeof(szNumber),
                             szValue, sizeof(szValue)))
        {
            if ((dwFlags & CP_SUPPORTED)||
                ((dwFlags & CP_INSTALLED)&&(wcslen(szValue) > 2)))
                    if (!lpCodePageEnumProc(szNumber))
                        break;

            ulIndex++;

        } else bContinue = FALSE;

        if (!bContinue)
            break;
    }

    if (hKey)
        NtClose(hKey);

    return TRUE;
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemCodePagesA (
    CODEPAGE_ENUMPROCA lpCodePageEnumProc,
    DWORD              dwFlags
    )
{
    WCHAR szNumber[5 + 1], szValue[MAX_PATH];
    HKEY hKey;
    BOOL bContinue = TRUE;
    ULONG ulIndex = 1;

    DPRINT("(%p,0x%08X)\n", lpCodePageEnumProc, dwFlags);

    if ((!dwFlags & CP_INSTALLED)&&(!dwFlags & CP_SUPPORTED))
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return FALSE;
    }

    if (!lpCodePageEnumProc)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    hKey = NLS_RegOpenKey(0, L"\\Machine\\SYSTEM\\CurrentControlSet\\Control\\NLS\\CodePage");
    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (bContinue)
    {
        if (NLS_RegEnumValue(hKey, ulIndex, szNumber, sizeof(szNumber),
                             szValue, sizeof(szValue)))
        {
            char szNumberA[sizeof(szNumber)/sizeof(WCHAR)];

            WideCharToMultiByte(CP_ACP, 0, szNumber, -1, szNumberA, sizeof(szNumberA), 0, 0);

            if ((dwFlags & CP_SUPPORTED)||
                ((dwFlags & CP_INSTALLED)&&(wcslen(szValue) > 1)))
                    if (!lpCodePageEnumProc(szNumberA))
                        break;

            ulIndex++;

        } else bContinue = FALSE;

        if (!bContinue)
            break;
    }

    if (hKey)
        NtClose(hKey);

    return TRUE;
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemGeoID(
    GEOCLASS        GeoClass,
    GEOID           ParentGeoId, // reserved
    GEO_ENUMPROC    lpGeoEnumProc)
{
    WCHAR szNumber[5 + 1];
    ULONG ulIndex = 0;
    HANDLE hKey;

    DPRINT("(0x%08X,0x%08X,%p)\n", GeoClass, ParentGeoId, lpGeoEnumProc);

    if(!lpGeoEnumProc || GeoClass != GEOCLASS_NATION)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    hKey = NLS_RegOpenKey(0, L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Country List");
    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (NLS_RegEnumSubKey(hKey, ulIndex, szNumber, sizeof(szNumber)))
    {
        BOOL bContinue = TRUE;
        DWORD dwGeoId;
        HANDLE hSubKey = NLS_RegOpenKey(hKey, szNumber);

        if (hSubKey)
        {
            if (NLS_RegGetDword(hSubKey, L"CountryCode", &dwGeoId))
            {
                if (!lpGeoEnumProc(dwGeoId))
                    bContinue = FALSE;
            }

            NtClose(hSubKey);
        }

        if (!bContinue)
            break;

        ulIndex++;
    }

    if (hKey)
        NtClose(hKey);

    return TRUE;
}


/* Callback function ptrs for EnumSystemLanguageGroupsA/W */
typedef struct
{
  LANGUAGEGROUP_ENUMPROCA procA;
  LANGUAGEGROUP_ENUMPROCW procW;
  DWORD    dwFlags;
  LONG_PTR lParam;
} ENUMLANGUAGEGROUP_CALLBACKS;


/* Internal implementation of EnumSystemLanguageGroupsA/W */
static BOOL NLS_EnumSystemLanguageGroups(ENUMLANGUAGEGROUP_CALLBACKS *lpProcs)
{
    WCHAR szNumber[10], szValue[4];
    HANDLE hKey;
    BOOL bContinue = TRUE;
    ULONG ulIndex = 0;

    if (!lpProcs)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    switch (lpProcs->dwFlags)
    {
    case 0:
        /* Default to LGRPID_INSTALLED */
        lpProcs->dwFlags = LGRPID_INSTALLED;
        /* Fall through... */
    case LGRPID_INSTALLED:
    case LGRPID_SUPPORTED:
        break;
    default:
        SetLastError(ERROR_INVALID_FLAGS);
        return FALSE;
    }

    hKey = NLS_RegOpenKey( 0, szLangGroupsKeyName );

    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (bContinue)
    {
        if (NLS_RegEnumValue( hKey, ulIndex, szNumber, sizeof(szNumber),
                              szValue, sizeof(szValue) ))
        {
            BOOL bInstalled = szValue[0] == '1' ? TRUE : FALSE;
            LGRPID lgrpid = wcstoul( szNumber, NULL, 16 );

            DPRINT("grpid %s (%sinstalled)\n", szNumber,
                   bInstalled ? "" : "not ");

            if (lpProcs->dwFlags == LGRPID_SUPPORTED || bInstalled)
            {
                WCHAR szGrpName[48];

                if (!NLS_GetLanguageGroupName( lgrpid, szGrpName, sizeof(szGrpName) / sizeof(WCHAR) ))
                    szGrpName[0] = '\0';

                if (lpProcs->procW)
                    bContinue = lpProcs->procW( lgrpid, szNumber, szGrpName, lpProcs->dwFlags,
                                                lpProcs->lParam );
                else
                {
                    char szNumberA[sizeof(szNumber)/sizeof(WCHAR)];
                    char szGrpNameA[48];

                    /* FIXME: MSDN doesn't say which code page the W->A translation uses,
                     *        or whether the language names are ever localised. Assume CP_ACP.
                     */

                    WideCharToMultiByte(CP_ACP, 0, szNumber, -1, szNumberA, sizeof(szNumberA), 0, 0);
                    WideCharToMultiByte(CP_ACP, 0, szGrpName, -1, szGrpNameA, sizeof(szGrpNameA), 0, 0);

                    bContinue = lpProcs->procA( lgrpid, szNumberA, szGrpNameA, lpProcs->dwFlags,
                                                lpProcs->lParam );
                }
            }

            ulIndex++;
        }
        else
            bContinue = FALSE;

        if (!bContinue)
            break;
    }

    if (hKey)
        NtClose( hKey );

    return TRUE;
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemLanguageGroupsA(
    LANGUAGEGROUP_ENUMPROCA pLangGroupEnumProc,
    DWORD                   dwFlags,
    LONG_PTR                lParam)
{
    ENUMLANGUAGEGROUP_CALLBACKS procs;

    DPRINT("(%p,0x%08X,0x%08lX)\n", pLangGroupEnumProc, dwFlags, lParam);

    procs.procA = pLangGroupEnumProc;
    procs.procW = NULL;
    procs.dwFlags = dwFlags;
    procs.lParam = lParam;

    return NLS_EnumSystemLanguageGroups( pLangGroupEnumProc ? &procs : NULL);
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemLanguageGroupsW(
    LANGUAGEGROUP_ENUMPROCW pLangGroupEnumProc,
    DWORD                   dwFlags,
    LONG_PTR                lParam)
{
    ENUMLANGUAGEGROUP_CALLBACKS procs;

    DPRINT("(%p,0x%08X,0x%08lX)\n", pLangGroupEnumProc, dwFlags, lParam);

    procs.procA = NULL;
    procs.procW = pLangGroupEnumProc;
    procs.dwFlags = dwFlags;
    procs.lParam = lParam;

    return NLS_EnumSystemLanguageGroups( pLangGroupEnumProc ? &procs : NULL);
}


/* Callback function ptrs for EnumSystemLocalesA/W */
typedef struct
{
  LOCALE_ENUMPROCA procA;
  LOCALE_ENUMPROCW procW;
  DWORD            dwFlags;
} ENUMSYSTEMLOCALES_CALLBACKS;


/* Internal implementation of EnumSystemLocalesA/W */
static BOOL NLS_EnumSystemLocales(ENUMSYSTEMLOCALES_CALLBACKS *lpProcs)
{
    WCHAR szNumber[10], szValue[4];
    HANDLE hKey;
    BOOL bContinue = TRUE;
    ULONG ulIndex = 0;

    if (!lpProcs)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    switch (lpProcs->dwFlags)
    {
        case LCID_INSTALLED:
        case LCID_SUPPORTED:
        case LCID_ALTERNATE_SORTS:
            break;
        default:
            SetLastError(ERROR_INVALID_FLAGS);
            return FALSE;
    }

    hKey = NLS_RegOpenKey(0, L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Nls\\Locale");

    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return FALSE;
    }

    while (bContinue)
    {
        if (NLS_RegEnumValue( hKey, ulIndex, szNumber, sizeof(szNumber),
                              szValue, sizeof(szValue)))
        {
            if ((lpProcs->dwFlags == LCID_SUPPORTED)||
                ((lpProcs->dwFlags == LCID_INSTALLED)&&(wcslen(szValue) > 0)))
            {
                if (lpProcs->procW)
                {
                    bContinue = lpProcs->procW(szNumber);
                }
                else
                {
                    char szNumberA[sizeof(szNumber)/sizeof(WCHAR)];

                    WideCharToMultiByte(CP_ACP, 0, szNumber, -1, szNumberA, sizeof(szNumberA), 0, 0);
                    bContinue = lpProcs->procA(szNumberA);
                }
            }

            ulIndex++;
        }
        else
            bContinue = FALSE;

        if (!bContinue)
            break;
    }

    if (hKey)
        NtClose(hKey);

    return TRUE;
}

/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemLocalesA (
    LOCALE_ENUMPROCA lpLocaleEnumProc,
    DWORD            dwFlags
    )
{
    ENUMSYSTEMLOCALES_CALLBACKS procs;

    DPRINT("(%p,0x%08X)\n", lpLocaleEnumProc, dwFlags);

    procs.procA = lpLocaleEnumProc;
    procs.procW = NULL;
    procs.dwFlags = dwFlags;

    return NLS_EnumSystemLocales(lpLocaleEnumProc ? &procs : NULL);
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumSystemLocalesW (
    LOCALE_ENUMPROCW lpLocaleEnumProc,
    DWORD            dwFlags
    )
{
    ENUMSYSTEMLOCALES_CALLBACKS procs;

    DPRINT("(%p,0x%08X)\n", lpLocaleEnumProc, dwFlags);

    procs.procA = NULL;
    procs.procW = lpLocaleEnumProc;
    procs.dwFlags = dwFlags;

    return NLS_EnumSystemLocales(lpLocaleEnumProc ? &procs : NULL);
}


static BOOL CALLBACK enum_uilang_proc_a( HMODULE hModule, LPCSTR type,
                                         LPCSTR name, WORD LangID, LONG_PTR lParam )
{
    ENUM_UILANG_CALLBACK *enum_uilang = (ENUM_UILANG_CALLBACK *)lParam;
    char buf[20];

    sprintf(buf, "%08x", (UINT)LangID);
    return enum_uilang->u.procA( buf, enum_uilang->param );
}


/*
 * @implemented
 */
BOOL
STDCALL
EnumUILanguagesA(
    UILANGUAGE_ENUMPROCA lpUILanguageEnumProc,
    DWORD                dwFlags,
    LONG_PTR             lParam)
{
    ENUM_UILANG_CALLBACK enum_uilang;

    DPRINT("%p, %x, %lx\n", lpUILanguageEnumProc, dwFlags, lParam);

    if(!lpUILanguageEnumProc) {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
    if(dwFlags) {
	SetLastError(ERROR_INVALID_FLAGS);
	return FALSE;
    }

    enum_uilang.u.procA = lpUILanguageEnumProc;
    enum_uilang.flags = dwFlags;
    enum_uilang.param = lParam;

    EnumResourceLanguagesA( hCurrentModule, (LPCSTR)RT_STRING,
                            (LPCSTR)LOCALE_ILANGUAGE, enum_uilang_proc_a,
                            (LONG_PTR)&enum_uilang);
    return TRUE;
}

static BOOL CALLBACK enum_uilang_proc_w( HMODULE hModule, LPCWSTR type,
                                         LPCWSTR name, WORD LangID, LONG_PTR lParam )
{
    static const WCHAR formatW[] = {'%','0','8','x',0};
    ENUM_UILANG_CALLBACK *enum_uilang = (ENUM_UILANG_CALLBACK *)lParam;
    WCHAR buf[20];

    swprintf( buf, formatW, (UINT)LangID );
    return enum_uilang->u.procW( buf, enum_uilang->param );
}

/*
 * @implemented
 */
BOOL
STDCALL
EnumUILanguagesW(
    UILANGUAGE_ENUMPROCW lpUILanguageEnumProc,
    DWORD                dwFlags,
    LONG_PTR             lParam)
{
    ENUM_UILANG_CALLBACK enum_uilang;

    DPRINT("%p, %x, %lx\n", lpUILanguageEnumProc, dwFlags, lParam);


    if(!lpUILanguageEnumProc) {
	SetLastError(ERROR_INVALID_PARAMETER);
	return FALSE;
    }
    if(dwFlags) {
	SetLastError(ERROR_INVALID_FLAGS);
	return FALSE;
    }

    enum_uilang.u.procW = lpUILanguageEnumProc;
    enum_uilang.flags = dwFlags;
    enum_uilang.param = lParam;

    EnumResourceLanguagesW( hCurrentModule, (LPCWSTR)RT_STRING,
                            (LPCWSTR)LOCALE_ILANGUAGE, enum_uilang_proc_w,
                            (LONG_PTR)&enum_uilang);
    return TRUE;
}

/*
 * @implemented
 */
int
STDCALL
GetCalendarInfoA(
	LCID lcid,
	CALID Calendar,
	CALTYPE CalType,
	LPSTR lpCalData,
	int cchData,
	LPDWORD lpValue
	)
{
    int ret;
    LPWSTR lpCalDataW = NULL;

    if (NLS_IsUnicodeOnlyLcid(lcid))
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return 0;
    }

    if (cchData &&
        !(lpCalDataW = HeapAlloc(GetProcessHeap(), 0, cchData*sizeof(WCHAR))))
      return 0;

    ret = GetCalendarInfoW(lcid, Calendar, CalType, lpCalDataW, cchData, lpValue);
    if(ret && lpCalDataW && lpCalData)
      WideCharToMultiByte(CP_ACP, 0, lpCalDataW, cchData, lpCalData, cchData, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, lpCalDataW);

    return ret;
}


/*
 * @unimplemented
 */
int
STDCALL
GetCalendarInfoW(
    LCID     Locale,
    CALID    Calendar,
    CALTYPE  CalType,
    LPWSTR   lpCalData,
    int      cchData,
    LPDWORD  lpValue)
{
    if (CalType & CAL_NOUSEROVERRIDE)
	DPRINT("FIXME: flag CAL_NOUSEROVERRIDE used, not fully implemented\n");
    if (CalType & CAL_USE_CP_ACP)
	DPRINT("FIXME: flag CAL_USE_CP_ACP used, not fully implemented\n");

    if (CalType & CAL_RETURN_NUMBER) {
	if (lpCalData != NULL)
	    DPRINT("WARNING: lpCalData not NULL (%p) when it should!\n", lpCalData);
	if (cchData != 0)
	    DPRINT("WARNING: cchData not 0 (%d) when it should!\n", cchData);
    } else {
	if (lpValue != NULL)
	    DPRINT("WARNING: lpValue not NULL (%p) when it should!\n", lpValue);
    }

    /* FIXME: No verification is made yet wrt Locale
     * for the CALTYPES not requiring GetLocaleInfoA */
    switch (CalType & ~(CAL_NOUSEROVERRIDE|CAL_RETURN_NUMBER|CAL_USE_CP_ACP)) {
	case CAL_ICALINTVALUE:
            DPRINT("FIXME: Unimplemented caltype %d\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SCALNAME:
            DPRINT("FIXME: Unimplemented caltype %d\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_IYEAROFFSETRANGE:
            DPRINT("FIXME: Unimplemented caltype %d\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SERASTRING:
            DPRINT("FIXME: Unimplemented caltype %d\n", CalType & 0xffff);
	    return E_FAIL;
	case CAL_SSHORTDATE:
	    return GetLocaleInfoW(Locale, LOCALE_SSHORTDATE, lpCalData, cchData);
	case CAL_SLONGDATE:
	    return GetLocaleInfoW(Locale, LOCALE_SLONGDATE, lpCalData, cchData);
	case CAL_SDAYNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME1, lpCalData, cchData);
	case CAL_SDAYNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME2, lpCalData, cchData);
	case CAL_SDAYNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME3, lpCalData, cchData);
	case CAL_SDAYNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME4, lpCalData, cchData);
	case CAL_SDAYNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME5, lpCalData, cchData);
	case CAL_SDAYNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME6, lpCalData, cchData);
	case CAL_SDAYNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SDAYNAME7, lpCalData, cchData);
	case CAL_SABBREVDAYNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME1, lpCalData, cchData);
	case CAL_SABBREVDAYNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME2, lpCalData, cchData);
	case CAL_SABBREVDAYNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME3, lpCalData, cchData);
	case CAL_SABBREVDAYNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME4, lpCalData, cchData);
	case CAL_SABBREVDAYNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME5, lpCalData, cchData);
	case CAL_SABBREVDAYNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME6, lpCalData, cchData);
	case CAL_SABBREVDAYNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVDAYNAME7, lpCalData, cchData);
	case CAL_SMONTHNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME1, lpCalData, cchData);
	case CAL_SMONTHNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME2, lpCalData, cchData);
	case CAL_SMONTHNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME3, lpCalData, cchData);
	case CAL_SMONTHNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME4, lpCalData, cchData);
	case CAL_SMONTHNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME5, lpCalData, cchData);
	case CAL_SMONTHNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME6, lpCalData, cchData);
	case CAL_SMONTHNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME7, lpCalData, cchData);
	case CAL_SMONTHNAME8:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME8, lpCalData, cchData);
	case CAL_SMONTHNAME9:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME9, lpCalData, cchData);
	case CAL_SMONTHNAME10:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME10, lpCalData, cchData);
	case CAL_SMONTHNAME11:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME11, lpCalData, cchData);
	case CAL_SMONTHNAME12:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME12, lpCalData, cchData);
	case CAL_SMONTHNAME13:
	    return GetLocaleInfoW(Locale, LOCALE_SMONTHNAME13, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME1:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME1, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME2:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME2, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME3:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME3, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME4:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME4, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME5:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME5, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME6:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME6, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME7:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME7, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME8:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME8, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME9:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME9, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME10:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME10, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME11:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME11, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME12:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME12, lpCalData, cchData);
	case CAL_SABBREVMONTHNAME13:
	    return GetLocaleInfoW(Locale, LOCALE_SABBREVMONTHNAME13, lpCalData, cchData);
	case CAL_SYEARMONTH:
	    return GetLocaleInfoW(Locale, LOCALE_SYEARMONTH, lpCalData, cchData);
	case CAL_ITWODIGITYEARMAX:
	    if (lpValue) *lpValue = CALINFO_MAX_YEAR;
	    break;
	default: DPRINT("Unknown caltype %d\n",CalType & 0xffff);
		 return E_FAIL;
    }
    return 0;
}


/*
 * @unimplemented
 */
BOOL
STDCALL
GetCPInfo (
    UINT        CodePage,
    LPCPINFO    CodePageInfo
    )
{
    unsigned i;

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);

    CodePageInfo->MaxCharSize = 1;
    CodePageInfo->DefaultChar[0] = '?';
    for (i = 1; i < MAX_DEFAULTCHAR; i++)
	{
	CodePageInfo->DefaultChar[i] = 0;
	}
    for (i = 0; i < MAX_LEADBYTES; i++)
	{
	CodePageInfo->LeadByte[i] = 0;
	}

    return TRUE;
}


/*
 * @unimplemented
 */
BOOL
STDCALL
GetCPInfoExW(
    UINT          CodePage,
    DWORD         dwFlags,
    LPCPINFOEXW  lpCPInfoEx)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/*
 * @unimplemented
 */
BOOL
STDCALL
GetCPInfoExA(
    UINT          CodePage,
    DWORD         dwFlags,
    LPCPINFOEXA  lpCPInfoEx)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

static int
NLS_GetGeoFriendlyName(GEOID Location, LPWSTR szFriendlyName, int cchData)
{
    HANDLE hKey;
    WCHAR szPath[MAX_PATH];
    UNICODE_STRING ValueName;
    KEY_VALUE_PARTIAL_INFORMATION *info;
    static const int info_size = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
    DWORD dwSize;
    NTSTATUS Status;
    int Ret;

    swprintf(szPath, L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Telephony\\Country List\\%d", Location);

    hKey = NLS_RegOpenKey(0, szPath);
    if (!hKey)
    {
        DPRINT1("NLS_RegOpenKey() failed\n");
        return 0;
    }

    dwSize = info_size + cchData * sizeof(WCHAR);

    if (!(info = HeapAlloc(GetProcessHeap(), 0, dwSize)))
    {
        NtClose(hKey);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    RtlInitUnicodeString(&ValueName, L"Name");

    Status = NtQueryValueKey(hKey, &ValueName, KeyValuePartialInformation,
                             (LPBYTE)info, dwSize, &dwSize);

    if (!Status)
    {
        Ret = (dwSize - info_size) / sizeof(WCHAR);

        if (!Ret || ((WCHAR *)info->Data)[Ret-1])
        {
            if (Ret < cchData || !szFriendlyName) Ret++;
            else
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                Ret = 0;
            }
        }

        if (Ret && szFriendlyName)
        {
            memcpy(szFriendlyName, info->Data, (Ret-1) * sizeof(WCHAR));
            szFriendlyName[Ret-1] = 0;
        }
    }
    else if (Status == STATUS_BUFFER_OVERFLOW && !szFriendlyName)
    {
        Ret = (dwSize - info_size) / sizeof(WCHAR) + 1;
    }
    else if (Status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        Ret = -1;
    }
    else
    {
        SetLastError(RtlNtStatusToDosError(Status));
        Ret = 0;
    }

    NtClose(hKey);
    HeapFree(GetProcessHeap(), 0, info);

    return Ret;
}

/*
 * @unimplemented
 */
int
STDCALL
GetGeoInfoW(
    GEOID       Location,
    GEOTYPE     GeoType,
    LPWSTR      lpGeoData,
    int         cchData,
    LANGID      LangId)
{
    DPRINT1("%d %d %p %d %d\n", Location, GeoType, lpGeoData, cchData, LangId);

    if ((GeoType == GEO_TIMEZONES)||(GeoType == GEO_OFFICIALLANGUAGES))
    {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    }

    switch (GeoType)
    {
        case GEO_FRIENDLYNAME:
        {
            return NLS_GetGeoFriendlyName(Location, lpGeoData, cchData);
        }
        case GEO_NATION:
        case GEO_LATITUDE:
        case GEO_LONGITUDE:
        case GEO_ISO2:
        case GEO_ISO3:
        case GEO_RFC1766:
        case GEO_LCID:
        case GEO_OFFICIALNAME:
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        break;
    }

    return 0;
}


/*
 * @unimplemented
 */
int
STDCALL
GetGeoInfoA(
    GEOID       Location,
    GEOTYPE     GeoType,
    LPSTR       lpGeoData,
    int         cchData,
    LANGID      LangId)
{
    DPRINT1("%d %d %p %d %d\n", Location, GeoType, lpGeoData, cchData, LangId);

    if ((GeoType == GEO_TIMEZONES)||(GeoType == GEO_OFFICIALLANGUAGES))
    {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    }

    switch (GeoType)
    {
        case GEO_FRIENDLYNAME:
        {
            WCHAR szBuffer[MAX_PATH];
            int Ret;
        
            Ret = NLS_GetGeoFriendlyName(Location, szBuffer, cchData);
            char szBufferA[sizeof(szBuffer)/sizeof(WCHAR)];

            WideCharToMultiByte(CP_ACP, 0, szBuffer, -1, szBufferA, sizeof(szBufferA), 0, 0);
            strcpy(lpGeoData, szBufferA);

            return Ret;
        }
        case GEO_NATION:
        case GEO_LATITUDE:
        case GEO_LONGITUDE:
        case GEO_ISO2:
        case GEO_ISO3:
        case GEO_RFC1766:
        case GEO_LCID:
        case GEO_OFFICIALNAME:
            SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        break;
    }

    return 0;
}

const WCHAR *RosGetLocaleValueName( DWORD lctype )
{
    switch (lctype & ~LOCALE_LOCALEINFOFLAGSMASK)
    {
    /* These values are used by SetLocaleInfo and GetLocaleInfo, and
     * the values are stored in the registry, confirmed under Windows.
     */
    case LOCALE_ICALENDARTYPE:    return L"iCalendarType";
    case LOCALE_ICURRDIGITS:      return L"iCurrDigits";
    case LOCALE_ICURRENCY:        return L"iCurrency";
    case LOCALE_IDIGITS:          return L"iDigits";
    case LOCALE_IFIRSTDAYOFWEEK:  return L"iFirstDayOfWeek";
    case LOCALE_IFIRSTWEEKOFYEAR: return L"iFirstWeekOfYear";
    case LOCALE_ILZERO:           return L"iLZero";
    case LOCALE_IMEASURE:         return L"iMeasure";
    case LOCALE_INEGCURR:         return L"iNegCurr";
    case LOCALE_INEGNUMBER:       return L"iNegNumber";
    case LOCALE_IPAPERSIZE:       return L"iPaperSize";
    case LOCALE_ITIME:            return L"iTime";
    case LOCALE_S1159:            return L"s1159";
    case LOCALE_S2359:            return L"s2359";
    case LOCALE_SCURRENCY:        return L"sCurrency";
    case LOCALE_SDATE:            return L"sDate";
    case LOCALE_SDECIMAL:         return L"sDecimal";
    case LOCALE_SGROUPING:        return L"sGrouping";
    case LOCALE_SLIST:            return L"sList";
    case LOCALE_SLONGDATE:        return L"sLongDate";
    case LOCALE_SMONDECIMALSEP:   return L"sMonDecimalSep";
    case LOCALE_SMONGROUPING:     return L"sMonGrouping";
    case LOCALE_SMONTHOUSANDSEP:  return L"sMonThousandSep";
    case LOCALE_SNEGATIVESIGN:    return L"sNegativeSign";
    case LOCALE_SPOSITIVESIGN:    return L"sPositiveSign";
    case LOCALE_SSHORTDATE:       return L"sShortDate";
    case LOCALE_STHOUSAND:        return L"sThousand";
    case LOCALE_STIME:            return L"sTime";
    case LOCALE_STIMEFORMAT:      return L"sTimeFormat";
    case LOCALE_SYEARMONTH:       return L"sYearMonth";

    /* The following are not listed under MSDN as supported,
     * but seem to be used and also stored in the registry.
     */
    case LOCALE_ICOUNTRY:         return L"iCountry";
    case LOCALE_IDATE:            return L"iDate";
    case LOCALE_ILDATE:           return L"iLDate";
    case LOCALE_ITLZERO:          return L"iTLZero";
    case LOCALE_SCOUNTRY:         return L"sCountry";
    case LOCALE_SLANGUAGE:        return L"sLanguage";
    }
    return NULL;
}

HKEY RosCreateRegistryKey(void)
{
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING nameW;
    HANDLE hKey;

    if (RtlOpenCurrentUser( KEY_ALL_ACCESS, &hKey ) != STATUS_SUCCESS) return 0;

    objAttr.Length = sizeof(objAttr);
    objAttr.RootDirectory = hKey;
    objAttr.ObjectName = &nameW;
    objAttr.Attributes = 0;
    objAttr.SecurityDescriptor = NULL;
    objAttr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, L"Control Panel\\International");

    if (NtCreateKey( &hKey, KEY_ALL_ACCESS, &objAttr, 0, NULL, 0, NULL ) != STATUS_SUCCESS) hKey = 0;
    NtClose( objAttr.RootDirectory );
    return hKey;
}

INT RosGetRegistryLocaleInfo( LPCWSTR lpValue, LPWSTR lpBuffer, INT nLen )
{
    DWORD dwSize;
    HKEY hKey;
	INT nRet;
    NTSTATUS ntStatus;
    UNICODE_STRING usNameW;
    KEY_VALUE_PARTIAL_INFORMATION *kvpiInfo;
    const int nInfoSize = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);

    if (!(hKey = RosCreateRegistryKey())) return -1;

    RtlInitUnicodeString( &usNameW, lpValue );
    dwSize = nInfoSize + nLen * sizeof(WCHAR);

    if (!(kvpiInfo = HeapAlloc( GetProcessHeap(), 0, dwSize )))
    {
        NtClose( hKey );
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }

    ntStatus = NtQueryValueKey( hKey, &usNameW, KeyValuePartialInformation, kvpiInfo, dwSize, &dwSize );
    if (ntStatus == STATUS_BUFFER_OVERFLOW && !lpBuffer) ntStatus = 0;

    if (!ntStatus)
    {
        nRet = (dwSize - nInfoSize) / sizeof(WCHAR);

        if (!nRet || ((WCHAR *)kvpiInfo->Data)[nRet - 1])
        {
            if (nRet < nLen || !lpBuffer) nRet++;
            else
            {
                SetLastError( ERROR_INSUFFICIENT_BUFFER );
                nRet = 0;
            }
        }
        if (nRet && lpBuffer)
        {
            memcpy( lpBuffer, kvpiInfo->Data, (nRet - 1) * sizeof(WCHAR) );
            lpBuffer[nRet - 1] = 0;
        }
    }
    else
    {
        if (ntStatus == STATUS_OBJECT_NAME_NOT_FOUND) nRet = -1;
        else
        {
            SetLastError( RtlNtStatusToDosError(ntStatus) );
            nRet = 0;
        }
    }
    NtClose( hKey );
    HeapFree( GetProcessHeap(), 0, kvpiInfo );
    return nRet;
}

/*
 * @implemented
 */
int
STDCALL
GetLocaleInfoW (
	LCID Locale,
    LCTYPE  LCType,
    LPWSTR  lpLCData,
    int cchData
    )
{
    LANGID liLangID;
    HRSRC hRsrc;
    HGLOBAL hMem;
    HMODULE hModule;
    INT nRet;
    UINT uiFlags;
    const WCHAR *ch;
    int i;

    if (cchData < 0 || (cchData && !lpLCData))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (!cchData) lpLCData = NULL;

    if (Locale == LOCALE_NEUTRAL || Locale == LOCALE_SYSTEM_DEFAULT) Locale = GetSystemDefaultLCID();
    else if (Locale == LOCALE_USER_DEFAULT) Locale = GetUserDefaultLCID();

    uiFlags = LCType & LOCALE_LOCALEINFOFLAGSMASK;
    LCType &= ~LOCALE_LOCALEINFOFLAGSMASK;

    if (!(uiFlags & LOCALE_NOUSEROVERRIDE) && Locale == GetUserDefaultLCID())
    {
        const WCHAR *value = RosGetLocaleValueName(LCType);

        if (value && ((nRet = RosGetRegistryLocaleInfo( value, lpLCData, cchData )) != -1)) return nRet;
    }

    liLangID = LANGIDFROMLCID( Locale );

    if (SUBLANGID(liLangID) == SUBLANG_NEUTRAL)
        liLangID = MAKELANGID(PRIMARYLANGID(liLangID), SUBLANG_DEFAULT);

    hModule = GetModuleHandleW( L"kernel32.dll" );
    if (!(hRsrc = FindResourceExW( hModule, (LPWSTR)RT_STRING, (LPCWSTR)((LCType >> 4) + 1), liLangID )))
    {
        SetLastError( ERROR_INVALID_FLAGS );
        return 0;
    }
    if (!(hMem = LoadResource( hModule, hRsrc )))
        return 0;

    ch = LockResource( hMem );
    for (i = 0; i < (int)(LCType & 0x0f); i++) ch += *ch + 1;

    if (uiFlags & LOCALE_RETURN_NUMBER) nRet = sizeof(UINT) / sizeof(WCHAR);
    else nRet = (LCType == LOCALE_FONTSIGNATURE) ? *ch : *ch + 1;

    if (!lpLCData) return nRet;

    if (nRet > cchData)
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return 0;
    }

    if (uiFlags & LOCALE_RETURN_NUMBER)
    {
        UINT uiNum;
        WCHAR *chEnd, *chTmp = HeapAlloc( GetProcessHeap(), 0, (*ch + 1) * sizeof(WCHAR) );

        if (!chTmp)
			return 0;

        memcpy( chTmp, ch + 1, *ch * sizeof(WCHAR) );
        chTmp[*ch] = L'\0';
        uiNum = wcstol( chTmp, &chEnd, 10 );

        if (!*chEnd)
            memcpy( lpLCData, &uiNum, sizeof(uiNum) );
        else
        {
            SetLastError( ERROR_INVALID_FLAGS );
            nRet = 0;
        }
        HeapFree( GetProcessHeap(), 0, chTmp );
    }
    else
    {
        memcpy( lpLCData, ch + 1, *ch * sizeof(WCHAR) );
        if (LCType != LOCALE_FONTSIGNATURE) lpLCData[nRet-1] = 0;
    }
    return nRet;
}



/***********************************************************************
 *    get_lcid_codepage
 *
 * Retrieve the ANSI codepage for a given locale.
 */
__inline static UINT get_lcid_codepage( LCID lcid )
{
    UINT ret;
    if (!GetLocaleInfoW( lcid, LOCALE_IDEFAULTANSICODEPAGE|LOCALE_RETURN_NUMBER, (WCHAR *)&ret,
                         sizeof(ret)/sizeof(WCHAR) )) ret = 0;
    return ret;
}


/*
 * @implemented
 */
/* Synced to Wine-20102004 */
int
STDCALL
CompareStringA (
    LCID    Locale,
    DWORD   dwCmpFlags,
    LPCSTR  lpString1,
    int cchCount1,
    LPCSTR  lpString2,
    int cchCount2
    )
{
    WCHAR *buf1W = NtCurrentTeb()->StaticUnicodeBuffer;
    WCHAR *buf2W = buf1W + 130;
    LPWSTR str1W, str2W;
    INT len1W, len2W, ret;
    UINT locale_cp;

    if (!lpString1 || !lpString2)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (cchCount1 < 0) cchCount1 = strlen(lpString1);
    if (cchCount2 < 0) cchCount2 = strlen(lpString2);

    locale_cp = get_lcid_codepage(Locale);

    len1W = MultiByteToWideChar(locale_cp, 0, lpString1, cchCount1, buf1W, 130);
    if (len1W)
        str1W = buf1W;
    else
    {
        len1W = MultiByteToWideChar(locale_cp, 0, lpString1, cchCount1, NULL, 0);
        str1W = HeapAlloc(GetProcessHeap(), 0, len1W * sizeof(WCHAR));
        if (!str1W)
        {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0;
        }
        MultiByteToWideChar(locale_cp, 0, lpString1, cchCount1, str1W, len1W);
    }
    len2W = MultiByteToWideChar(locale_cp, 0, lpString2, cchCount2, buf2W, 130);
    if (len2W)
        str2W = buf2W;
    else
    {
        len2W = MultiByteToWideChar(locale_cp, 0, lpString2, cchCount2, NULL, 0);
        str2W = HeapAlloc(GetProcessHeap(), 0, len2W * sizeof(WCHAR));
        if (!str2W)
        {
            if (str1W != buf1W) HeapFree(GetProcessHeap(), 0, str1W);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0;
        }
        MultiByteToWideChar(locale_cp, 0, lpString2, cchCount2, str2W, len2W);
    }

    ret = CompareStringW(Locale, dwCmpFlags, str1W, len1W, str2W, len2W);

    if (str1W != buf1W) HeapFree(GetProcessHeap(), 0, str1W);
    if (str2W != buf2W) HeapFree(GetProcessHeap(), 0, str2W);
    return ret;
}


static int compare_unicode_string( 
    PUNICODE_STRING String1, 
    PUNICODE_STRING String2,
    DWORD Flags
    )
{
    ULONG len1, len2;
    PWCHAR s1, s2;
    WCHAR c1, c2;
    
    if (String1 && String2)
    {
      len1 = String1->Length / sizeof(WCHAR);
      len2 = String2->Length / sizeof(WCHAR);
      s1 = String1->Buffer;
      s2 = String2->Buffer;

      while (len1 > 0 && len2 > 0)
      {
        if (Flags & NORM_IGNORESYMBOLS)
        {
            int skip = 0;
            /* FIXME: not tested */
            if (iswctype(*s1, _SPACE | _PUNCT))
            {
                s1++;
                len1--;
                skip = 1;
            }
            if (iswctype(*s2, _SPACE | _PUNCT))
            {
                s2++;
                len2--;
                skip = 1;
            }
            if (skip) continue;
        }

       /* hyphen and apostrophe are treated differently depending on
        * whether SORT_STRINGSORT specified or not
        */
        if (!(Flags & SORT_STRINGSORT))
        {
            if (*s1 == '-' || *s1 == '\'')
            {
                if (*s2 != '-' && *s2 != '\'')
                {
                    s1++;
                    len1--;
                    continue;
                }
            }
            else if (*s2 == '-' || *s2 == '\'')
            {
                s2++;
                len2--;
                continue;
            }
        }
        if (Flags & NORM_IGNORECASE)
        {
            c1 = len1-- ? RtlUpcaseUnicodeChar(*s1++) : 0;
            c2 = len2-- ? RtlUpcaseUnicodeChar(*s2++) : 0;
            if (!c1 || !c2 || c1 != c2)
               return c1 - c2;
        }
        else
        {
            c1 = len1-- ? *s1++ : 0;
            c2 = len2-- ? *s2++ : 0;
            if (!c1 || !c2 || c1 != c2)
               return c1 - c2;
        }
      }
      return (int) len1 - (int) len2;
    }
    return 0;
}


/*
 * @unimplemented
 */
int
STDCALL
CompareStringW (
    LCID    Locale,
    DWORD   dwCmpFlags,
    LPCWSTR lpString1,
    int cchCount1,
    LPCWSTR lpString2,
    int cchCount2
    )
{
    INT Result;
    UNICODE_STRING String1, String2;

    if (!lpString1 || !lpString2)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (dwCmpFlags & ~(NORM_IGNORECASE | NORM_IGNORENONSPACE |
        NORM_IGNORESYMBOLS | SORT_STRINGSORT | NORM_IGNOREKANATYPE |
        NORM_IGNOREWIDTH | 0x10000000))
    {
        SetLastError(ERROR_INVALID_FLAGS);
        return 0;
    }

    if (cchCount1 < 0) cchCount1 = lstrlenW(lpString1);
    if (cchCount2 < 0) cchCount2 = lstrlenW(lpString2);

    String1.Length = String1.MaximumLength = cchCount1 * sizeof(WCHAR);
    String1.Buffer = (LPWSTR)lpString1;
    String2.Length = String2.MaximumLength = cchCount2 * sizeof(WCHAR);
    String2.Buffer = (LPWSTR)lpString2;


    if (dwCmpFlags & ~NORM_IGNORECASE)
    {
        DPRINT("CompareString: STUB flags - 0x%x\n", dwCmpFlags);
	Result = compare_unicode_string(&String1, &String2, dwCmpFlags);
    }
    else
        Result = RtlCompareUnicodeString(
                      &String1, &String2, (BOOLEAN)(dwCmpFlags & NORM_IGNORECASE));
    
    
    if (Result) /* need to translate result */
        return (Result < 0) ? CSTR_LESS_THAN : CSTR_GREATER_THAN;

    return CSTR_EQUAL;
}




/*
 * @implemented
 *
 * Get information about an aspect of a locale.
 *
 * PARAMS
 *  lcid   [I] LCID of the locale
 *  lctype [I] LCTYPE_ flags from "winnls.h"
 *  buffer [O] Destination for the information
 *  len    [I] Length of buffer in characters
 *
 * RETURNS
 *  Success: The size of the data requested. If buffer is non-NULL, it is filled
 *           with the information.
 *  Failure: 0. Use GetLastError() to determine the cause.
 *
 * NOTES
 *  - LOCALE_NEUTRAL is equal to LOCALE_SYSTEM_DEFAULT
 *  - The string returned is NUL terminated, except for LOCALE_FONTSIGNATURE,
 *    which is a bit string.
 */
INT STDCALL GetLocaleInfoA( LCID lcid, LCTYPE lctype, LPSTR buffer, INT len )
{
    WCHAR *bufferW;
    INT lenW, ret;

    if (len < 0 || (len && !buffer))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (!len) buffer = NULL;

    if (!(lenW = GetLocaleInfoW( lcid, lctype, NULL, 0 ))) return 0;

    if (!(bufferW = HeapAlloc( GetProcessHeap(), 0, lenW * sizeof(WCHAR) )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }
    if ((ret = GetLocaleInfoW( lcid, lctype, bufferW, lenW )))
    {
        if ((lctype & LOCALE_RETURN_NUMBER) ||
            ((lctype & ~LOCALE_LOCALEINFOFLAGSMASK) == LOCALE_FONTSIGNATURE))
        {
            /* it's not an ASCII string, just bytes */
            ret *= sizeof(WCHAR);
            if (buffer)
            {
                if (ret <= len) memcpy( buffer, bufferW, ret );
                else
                {
                    SetLastError( ERROR_INSUFFICIENT_BUFFER );
                    ret = 0;
                }
            }
        }
        else
        {
            UINT codepage = CP_ACP;
            if (!(lctype & LOCALE_USE_CP_ACP)) codepage = get_lcid_codepage( lcid );
            ret = WideCharToMultiByte( codepage, 0, bufferW, ret, buffer, len, NULL, NULL );
        }
    }
    HeapFree( GetProcessHeap(), 0, bufferW );
    return ret;
}


/*
 * @implemented
 */
LANGID STDCALL
GetSystemDefaultLangID(VOID)
{
  return LANGIDFROMLCID(GetSystemDefaultLCID());
}


/*
 * @implemented
 */
LCID STDCALL
GetSystemDefaultLCID(VOID)
{
  LCID lcid;

  NtQueryDefaultLocale(FALSE, &lcid);

  return lcid;
}


/*
 * @implemented
 */
LANGID STDCALL
GetSystemDefaultUILanguage(VOID)
{
  LANGID LanguageId;
  NTSTATUS Status;

  Status = NtQueryInstallUILanguage(&LanguageId);
  if (!NT_SUCCESS(Status))
    {
      SetLastErrorByStatus(Status);
      return 0;
    }

  return LanguageId;
}


/*
 * @implemented
 */
LCID STDCALL
GetThreadLocale(VOID)
{
  return NtCurrentTeb()->CurrentLocale;
}


/*
 * @implemented
 */
LANGID STDCALL
GetUserDefaultLangID(VOID)
{
  return LANGIDFROMLCID(GetUserDefaultLCID());
}


/*
 * @implemented
 */
LCID STDCALL
GetUserDefaultLCID(VOID)
{
  LCID lcid;
  NTSTATUS Status;

  Status = NtQueryDefaultLocale(TRUE, &lcid);
  if (!NT_SUCCESS(Status))
    {
      SetLastErrorByStatus(Status);
      return 0;
    }

  return lcid;
}


/*
 * @implemented
 */
LANGID STDCALL
GetUserDefaultUILanguage(VOID)
{
  LANGID LangId;
  NTSTATUS Status;

  Status = NtQueryDefaultUILanguage(&LangId);
  if (!NT_SUCCESS(Status))
    {
      SetLastErrorByStatus(Status);
      return 0;
    }

  return LangId;
}


/***********************************************************************
 *		create_registry_key
 *
 * Create the Control Panel\\International registry key.
 */
static inline HANDLE create_registry_key(void)
{
    static const WCHAR intlW[] = {'C','o','n','t','r','o','l',' ','P','a','n','e','l','\\',
                                  'I','n','t','e','r','n','a','t','i','o','n','a','l',0};
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HANDLE hkey;

    if (RtlOpenCurrentUser( KEY_ALL_ACCESS, &hkey ) != STATUS_SUCCESS) return 0;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, intlW );

    if (NtCreateKey( &hkey, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ) != STATUS_SUCCESS) hkey = 0;
    NtClose( attr.RootDirectory );
    return hkey;
}


/*
 * @unimplemented
 */
GEOID
STDCALL
GetUserGeoID(
    GEOCLASS    GeoClass)
{
    GEOID ret = GEOID_NOT_AVAILABLE;
    static const WCHAR geoW[] = {'G','e','o',0};
    static const WCHAR nationW[] = {'N','a','t','i','o','n',0};
    WCHAR bufferW[40], *end;
    DWORD count;
    HANDLE hkey, hSubkey = 0;
    UNICODE_STRING keyW;
    const KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)bufferW;
    RtlInitUnicodeString( &keyW, nationW );
    count = sizeof(bufferW);

    if(!(hkey = create_registry_key())) return ret;

    switch( GeoClass ){
    case GEOCLASS_NATION:
        if ((hSubkey = NLS_RegOpenKey(hkey, geoW)))
        {
            if((NtQueryValueKey(hSubkey, &keyW, KeyValuePartialInformation,
                                (LPBYTE)bufferW, count, &count) == STATUS_SUCCESS ) && info->DataLength)
                ret = wcstol((LPCWSTR)info->Data, &end, 10);
        }
        break;
    case GEOCLASS_REGION:
        DPRINT("GEOCLASS_REGION not handled yet\n");
        break;
    }

    NtClose(hkey);
    if (hSubkey) NtClose(hSubkey);
    return ret;
}


/******************************************************************************
 *           IsValidLanguageGroup
 *
 * Determine if a language group is supported and/or installed.
 *
 * PARAMS
 *  LanguageGroup [I] Language Group Id (LGRPID_ values from "winnls.h")
 *  dwFlags [I] LGRPID_SUPPORTED=Supported, LGRPID_INSTALLED=Installed
 *
 * RETURNS
 *  TRUE, if lgrpid is supported and/or installed, according to dwFlags.
 *  FALSE otherwise.
 *
 * @implemented
 */
BOOL
STDCALL
IsValidLanguageGroup(
    LGRPID  LanguageGroup,
    DWORD   dwFlags)
{
    static const WCHAR szFormat[] = { '%','x','\0' };
    UNICODE_STRING szNlsKeyName = 
        RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Nls");
    UNICODE_STRING szLangGroupsKeyName = 
        RTL_CONSTANT_STRING(L"Language Groups");
    const int MAX_VALUE_NAME = 16;
    const int MAX_VALUE_SYMB = 128;

    BOOL bNtQuery;
    PKEY_VALUE_PARTIAL_INFORMATION kvpiInfo;

    WCHAR szValueName[MAX_VALUE_NAME];
    UNICODE_STRING ucsValueName;
    DWORD dwRetSize;
    PWSTR pwszValueData;

    DWORD dwSize = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + MAX_VALUE_SYMB * sizeof(WCHAR);

    OBJECT_ATTRIBUTES oaAttr;
    HANDLE hkey, hRootKey;
    BOOL bSupported = FALSE, bInstalled = FALSE;

    DPRINT("IsValidLanguageGroup() called\n");

    kvpiInfo = RtlAllocateHeap(RtlGetProcessHeap(),
			                  HEAP_ZERO_MEMORY,
			                  dwSize);

    switch (dwFlags)
    {
        case LGRPID_INSTALLED:
        case LGRPID_SUPPORTED:

            InitializeObjectAttributes(&oaAttr, &szNlsKeyName, 0, 0, NULL);
            if(NtOpenKey(&hRootKey, KEY_ALL_ACCESS, &oaAttr) != STATUS_SUCCESS) return FALSE;

            InitializeObjectAttributes(&oaAttr, &szLangGroupsKeyName, 0, hRootKey, NULL);
            if(NtOpenKey(&hkey, KEY_ALL_ACCESS, &oaAttr) != STATUS_SUCCESS) return FALSE;

            if(hRootKey) NtClose(hRootKey);

            swprintf(szValueName, szFormat, (ULONG)LanguageGroup);
            RtlInitUnicodeString(&ucsValueName, szValueName);

            bNtQuery = NtQueryValueKey(hkey,
                                       &ucsValueName,
                                       KeyValuePartialInformation,
                                       kvpiInfo,
                                       dwSize,
                                       &dwRetSize);
            if(hkey) NtClose(hkey);

            if(bNtQuery == STATUS_SUCCESS &&
               kvpiInfo->DataLength == sizeof(DWORD))
            {
                pwszValueData = (PWSTR)&kvpiInfo->Data[0];
                bSupported = TRUE;
                if(pwszValueData[0] == L'1') bInstalled = TRUE;
            }
            else
            {
                DPRINT("NtQueryValueKey() failed (Status %lx)\n", bNtQuery);
                RtlFreeHeap(RtlGetProcessHeap(), 0, kvpiInfo);
                return FALSE;
            }

        break;
    }

    RtlFreeHeap(RtlGetProcessHeap(), 0, kvpiInfo);

    if((dwFlags == LGRPID_SUPPORTED && bSupported) ||
       (dwFlags == LGRPID_INSTALLED && bInstalled))
    {
        DPRINT("Language group is supported and installed\n");
        return TRUE;
    }

    return FALSE;
}


/******************************************************************************
 *           IsValidLocale
 *
 * Determine if a locale is valid.
 *
 * PARAMS
 *  Locale  [I] LCID of the locale to check
 *  dwFlags [I] LCID_SUPPORTED = Valid
 *              LCID_INSTALLED = Valid and installed on the system
 *
 * RETURN
 *  TRUE,  if Locale is valid,
 *  FALSE, otherwise.
 *
 * @implemented
 */
BOOL STDCALL
IsValidLocale(LCID Locale,
	      DWORD dwFlags)
{
  OBJECT_ATTRIBUTES ObjectAttributes;
  PKEY_VALUE_PARTIAL_INFORMATION KeyInfo;
  WCHAR ValueNameBuffer[9];
  UNICODE_STRING KeyName;
  UNICODE_STRING ValueName;
  ULONG KeyInfoSize;
  ULONG ReturnedSize;
  HANDLE KeyHandle;
  PWSTR ValueData;
  NTSTATUS Status;

  DPRINT("IsValidLocale() called\n");

  if ((dwFlags & ~(LCID_SUPPORTED | LCID_INSTALLED)) ||
      (dwFlags == (LCID_SUPPORTED | LCID_INSTALLED)))
    {
      DPRINT("Invalid flags: %lx\n", dwFlags);
      return FALSE;
    }

  if (Locale & 0xFFFF0000)
    {
      RtlInitUnicodeString(&KeyName,
			   L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Nls\\Locale\\Alternate Sorts");
    }
  else
    {
      RtlInitUnicodeString(&KeyName,
			   L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control\\Nls\\Locale");
    }

  InitializeObjectAttributes(&ObjectAttributes,
			     &KeyName,
			     OBJ_CASE_INSENSITIVE,
			     NULL,
			     NULL);

  Status = NtOpenKey(&KeyHandle,
		     KEY_QUERY_VALUE,
		     &ObjectAttributes);
  if (!NT_SUCCESS(Status))
    {
      DPRINT("NtOpenKey() failed (Status %lx)\n", Status);
      return FALSE;
    }

  swprintf(ValueNameBuffer, L"%08lx", (ULONG)Locale);
  RtlInitUnicodeString(&ValueName, ValueNameBuffer);

  KeyInfoSize = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + 4 * sizeof(WCHAR);
  KeyInfo = RtlAllocateHeap(RtlGetProcessHeap(),
			    HEAP_ZERO_MEMORY,
			    KeyInfoSize);
  if (KeyInfo == NULL)
    {
      DPRINT("RtlAllocateHeap() failed (Status %lx)\n", Status);
      NtClose(KeyHandle);
      return FALSE;
    }

  Status = NtQueryValueKey(KeyHandle,
			   &ValueName,
			   KeyValuePartialInformation,
			   KeyInfo,
			   KeyInfoSize,
			   &ReturnedSize);
  NtClose(KeyHandle);

  if (!NT_SUCCESS(Status))
    {
      DPRINT("NtQueryValueKey() failed (Status %lx)\n", Status);
      RtlFreeHeap(RtlGetProcessHeap(), 0, KeyInfo);
      return FALSE;
    }

  if (dwFlags & LCID_SUPPORTED)
    {
      DPRINT("Locale is supported\n");
      RtlFreeHeap(RtlGetProcessHeap(), 0, KeyInfo);
      return TRUE;
    }

  ValueData = (PWSTR)&KeyInfo->Data[0];
  if ((KeyInfo->Type == REG_SZ) &&
      (KeyInfo->DataLength == 2 * sizeof(WCHAR)) &&
      (ValueData[0] == L'1'))
    {
      DPRINT("Locale is supported and installed\n");
      RtlFreeHeap(RtlGetProcessHeap(), 0, KeyInfo);
      return TRUE;
    }

  RtlFreeHeap(RtlGetProcessHeap(), 0, KeyInfo);

  DPRINT("IsValidLocale() called\n");

  return FALSE;
}


/*
 * @unimplemented
 */
int
STDCALL
LCMapStringA (
    LCID    Locale,
    DWORD   dwMapFlags,
    LPCSTR  lpSrcStr,
    int cchSrc,
    LPSTR   lpDestStr,
    int cchDest
    )
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/*
 * @unimplemented
 */
int
STDCALL
LCMapStringW (
    LCID    Locale,
    DWORD   dwMapFlags,
    LPCWSTR lpSrcStr,
    int cchSrc,
    LPWSTR  lpDestStr,
    int cchDest
    )
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/*
 * @unimplemented
 */
BOOL
STDCALL
SetCalendarInfoA(
    LCID     Locale,
    CALID    Calendar,
    CALTYPE  CalType,
    LPCSTR  lpCalData)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

/*
 * @unimplemented
 */
BOOL
STDCALL
SetCalendarInfoW(
    LCID     Locale,
    CALID    Calendar,
    CALTYPE  CalType,
    LPCWSTR  lpCalData)
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}


/**********************************************************************
 * @implemented
 * RIPPED FROM WINE's dlls\kernel\locale.c ver 0.9.29
 *
 *           SetLocaleInfoA    (KERNEL32.@)
 *
 * Set the current locale info.
 *
 * PARAMS
 *  Locale   [I] LCID of the locale
 *  LCType   [I] LCTYPE_ flags from "winnls.h"
 *  lpLCData [I] Information to set
 *
 * RETURNS
 *  Success: TRUE. The information given will be returned by GetLocaleInfoA()
 *           whenever it is called without LOCALE_NOUSEROVERRIDE.
 *  Failure: FALSE. Use GetLastError() to determine the cause.
 */
BOOL
STDCALL
SetLocaleInfoA (
    LCID    Locale,
    LCTYPE  LCType,
    LPCSTR  lpLCData
    )
{
    UINT codepage = CP_ACP;
    WCHAR *strW;
    DWORD len;
    BOOL ret;

    if (!(LCType & LOCALE_USE_CP_ACP)) codepage = get_lcid_codepage( Locale );

    if (!lpLCData)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    len = MultiByteToWideChar( codepage, 0, lpLCData, -1, NULL, 0 );
    if (!(strW = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }
    MultiByteToWideChar( codepage, 0, lpLCData, -1, strW, len );
    ret = SetLocaleInfoW( Locale, LCType, strW );
    HeapFree( GetProcessHeap(), 0, strW );
    return ret;
}


/**********************************************************************
 * @implemented
 * RIPPED FROM WINE's dlls\kernel\locale.c ver 0.9.29
 *
 *           SetLocaleInfoW    (KERNEL32.@)
 *
 * See SetLocaleInfoA.
 *
 */
BOOL
STDCALL
SetLocaleInfoW (
    LCID    Locale,
    LCTYPE  LCType,
    LPCWSTR lpLCData
    )
{
    const WCHAR *value;
    UNICODE_STRING valueW;
    NTSTATUS status;
    HANDLE hkey;

    LCType &= 0xffff;
    value = RosGetLocaleValueName( LCType );

    if (!lpLCData || !value)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if (LCType == LOCALE_IDATE || LCType == LOCALE_ILDATE)
    {
        SetLastError( ERROR_INVALID_FLAGS );
        return FALSE;
    }

    if (!(hkey = RosCreateRegistryKey())) return FALSE;
    RtlInitUnicodeString( &valueW, value );
    status = NtSetValueKey( hkey, &valueW, 0, REG_SZ, (PVOID)lpLCData, (lstrlenW(lpLCData)+1)*sizeof(WCHAR) );

    if (LCType == LOCALE_SSHORTDATE || LCType == LOCALE_SLONGDATE)
    {
        /* Set I-value from S value */
        WCHAR *lpD, *lpM, *lpY;
        WCHAR szBuff[2];

        lpD = wcschr(lpLCData, 'd');
        lpM = wcschr(lpLCData, 'M');
        lpY = wcschr(lpLCData, 'y');

        if (lpD <= lpM)
        {
            szBuff[0] = '1'; /* D-M-Y */
        }
        else
        {
            if (lpY <= lpM)
                szBuff[0] = '2'; /* Y-M-D */
            else
                szBuff[0] = '0'; /* M-D-Y */
        }

        szBuff[1] = '\0';

        if (LCType == LOCALE_SSHORTDATE)
            LCType = LOCALE_IDATE;
        else
            LCType = LOCALE_ILDATE;

        value = RosGetLocaleValueName( LCType );

        RtlInitUnicodeString( &valueW, value );
        status = NtSetValueKey( hkey, &valueW, 0, REG_SZ, szBuff, sizeof(szBuff) );
    }

    NtClose( hkey );

    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/**********************************************************************
 * @implemented
 * RIPPED FROM WINE's dlls\kernel\locale.c rev 1.42
 *
 *           SetThreadLocale    (KERNEL32.@)
 *
 * Set the current threads locale.
 *
 * PARAMS
 *  lcid [I] LCID of the locale to set
 *
 * RETURNS
 *  Success: TRUE. The threads locale is set to lcid.
 *  Failure: FALSE. Use GetLastError() to determine the cause.
 *
 */
BOOL WINAPI SetThreadLocale( LCID lcid )
{
    DPRINT("SetThreadLocale(0x%04lX)\n", lcid);

    lcid = ConvertDefaultLocale(lcid);

    if (lcid != GetThreadLocale())
    {
        if (!IsValidLocale(lcid, LCID_SUPPORTED))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        NtCurrentTeb()->CurrentLocale = lcid;
        /* FIXME: NtCurrentTeb()->code_page = get_lcid_codepage( lcid );
         * Wine save the acp for easy/fast access, but ROS has no such Teb member.
         * Maybe add this member to ros as well?
         */

        /*
        Lag test app for � se om locale etc, endres i en app. etter at prosessen er
        startet, eller om bare nye prosesser blir ber�rt.
        */
    }
    return TRUE;
}


/*
 * @implemented
 */
BOOL STDCALL
SetUserDefaultLCID(LCID lcid)
{
  NTSTATUS Status;

  Status = NtSetDefaultLocale(TRUE, lcid);
  if (!NT_SUCCESS(Status))
  {
      SetLastErrorByStatus(Status);
      return 0;
  }
  return 1;
}


/*
 * @implemented
 */
BOOL STDCALL
SetUserDefaultUILanguage(LANGID LangId)
{
  NTSTATUS Status;

  Status = NtSetDefaultUILanguage(LangId);
  if (!NT_SUCCESS(Status))
  {
      SetLastErrorByStatus(Status);
      return 0;
  }
  return 1;
}


/*
 * @implemented
 */
BOOL
STDCALL
SetUserGeoID(
    GEOID       GeoId)
{
    static const WCHAR geoW[] = {'G','e','o',0};
    static const WCHAR nationW[] = {'N','a','t','i','o','n',0};
    static const WCHAR formatW[] = {'%','i',0};
    UNICODE_STRING nameW,keyW;
    WCHAR bufferW[10];
    OBJECT_ATTRIBUTES attr;
    HANDLE hkey;

    if(!(hkey = create_registry_key())) return FALSE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, geoW );
    RtlInitUnicodeString( &keyW, nationW );

    if (NtCreateKey( &hkey, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ) != STATUS_SUCCESS)

    {
        NtClose(attr.RootDirectory);
        return FALSE;
    }

    swprintf(bufferW, formatW, GeoId);
    NtSetValueKey(hkey, &keyW, 0, REG_SZ, bufferW, (wcslen(bufferW) + 1) * sizeof(WCHAR));
    NtClose(attr.RootDirectory);
    NtClose(hkey);
    return TRUE;
}


/*
 * @implemented
 */
DWORD
STDCALL
VerLanguageNameA (
    DWORD   wLang,
    LPSTR   szLang,
    DWORD   nSize
    )
{
   return GetLocaleInfoA( MAKELCID(wLang, SORT_DEFAULT), LOCALE_SENGLANGUAGE, szLang, nSize );
}


/*
 * @implemented
 */
DWORD
STDCALL
VerLanguageNameW (
    DWORD   wLang,
    LPWSTR  szLang,
    DWORD   nSize
    )
{
    return GetLocaleInfoW( MAKELCID(wLang, SORT_DEFAULT), LOCALE_SENGLANGUAGE, szLang, nSize );
}



