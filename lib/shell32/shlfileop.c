/*
 * SHFileOperation
 *
 * Copyright 2000 Juergen Schmied
 * Copyright 2002 Andriy Palamarchuk
 * Copyright 2004 Dietrich Teickner (from Odin)
 * Copyright 2004 Rolf Kalbermatter
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "shellapi.h"
#include "wingdi.h"
#include "winuser.h"
#include "shlobj.h"
#include "shresdef.h"
#define NO_SHLWAPI_STREAM
#include "shlwapi.h"
#include "shell32_main.h"
#include "undocshell.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

#define IsAttrib(x, y)  ((INVALID_FILE_ATTRIBUTES != (x)) && ((x) & (y)))
#define IsAttribFile(x) (!((x) & FILE_ATTRIBUTE_DIRECTORY))
#define IsAttribDir(x)  IsAttrib(x, FILE_ATTRIBUTE_DIRECTORY)
#define IsDotDir(x)     ((x[0] == '.') && ((x[1] == 0) || ((x[1] == '.') && (x[2] == 0))))

#define FO_MASK         0xF

static const WCHAR wWildcardFile[] = {'*',0};
static const WCHAR wWildcardChars[] = {'*','?',0};
static const WCHAR wBackslash[] = {'\\',0};

static BOOL SHELL_DeleteDirectoryW(LPCWSTR path, BOOL bShowUI);
static DWORD SHNotifyCreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sec);
static DWORD SHNotifyCreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES sec);
static DWORD SHNotifyRemoveDirectoryA(LPCSTR path);
static DWORD SHNotifyRemoveDirectoryW(LPCWSTR path);
static DWORD SHNotifyDeleteFileA(LPCSTR path);
static DWORD SHNotifyDeleteFileW(LPCWSTR path);
static DWORD SHNotifyMoveFileW(LPCWSTR src, LPCWSTR dest);
static DWORD SHNotifyCopyFileW(LPCWSTR src, LPCWSTR dest, BOOL bFailIfExists);
static DWORD SHFindAttrW(LPCWSTR pName, BOOL fileOnly);

typedef struct
{
	UINT caption_resource_id, text_resource_id;
} SHELL_ConfirmIDstruc;

static BOOL SHELL_ConfirmIDs(int nKindOfDialog, SHELL_ConfirmIDstruc *ids)
{
	switch (nKindOfDialog) {
	  case ASK_DELETE_FILE:
	    ids->caption_resource_id  = IDS_DELETEITEM_CAPTION;
	    ids->text_resource_id  = IDS_DELETEITEM_TEXT;
	    return TRUE;
	  case ASK_DELETE_FOLDER:
	    ids->caption_resource_id  = IDS_DELETEFOLDER_CAPTION;
	    ids->text_resource_id  = IDS_DELETEITEM_TEXT;
	    return TRUE;
	  case ASK_DELETE_MULTIPLE_ITEM:
	    ids->caption_resource_id  = IDS_DELETEITEM_CAPTION;
	    ids->text_resource_id  = IDS_DELETEMULTIPLE_TEXT;
	    return TRUE;
	  case ASK_OVERWRITE_FILE:
	    ids->caption_resource_id  = IDS_OVERWRITEFILE_CAPTION;
	    ids->text_resource_id  = IDS_OVERWRITEFILE_TEXT;
	    return TRUE;
	  default:
	    FIXME(" Unhandled nKindOfDialog %d stub\n", nKindOfDialog);
	}
	return FALSE;
}

BOOL SHELL_ConfirmDialog(int nKindOfDialog, LPCSTR szDir)
{
	CHAR szCaption[255], szText[255], szBuffer[MAX_PATH + 256];
	SHELL_ConfirmIDstruc ids;

	if (!SHELL_ConfirmIDs(nKindOfDialog, &ids))
	  return FALSE;

	LoadStringA(shell32_hInstance, ids.caption_resource_id, szCaption, sizeof(szCaption));
	LoadStringA(shell32_hInstance, ids.text_resource_id, szText, sizeof(szText));

	FormatMessageA(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ARGUMENT_ARRAY,
	               szText, 0, 0, szBuffer, sizeof(szBuffer), (va_list*)&szDir);

	return (IDOK == MessageBoxA(GetActiveWindow(), szBuffer, szCaption, MB_OKCANCEL | MB_ICONEXCLAMATION));
}

BOOL SHELL_ConfirmDialogW(int nKindOfDialog, LPCWSTR szDir)
{
	WCHAR szCaption[255], szText[255], szBuffer[MAX_PATH + 256];
	SHELL_ConfirmIDstruc ids;

	if (!SHELL_ConfirmIDs(nKindOfDialog, &ids))
	  return FALSE;

	LoadStringW(shell32_hInstance, ids.caption_resource_id, szCaption, sizeof(szCaption));
	LoadStringW(shell32_hInstance, ids.text_resource_id, szText, sizeof(szText));

	FormatMessageW(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ARGUMENT_ARRAY,
	               szText, 0, 0, szBuffer, sizeof(szBuffer), (va_list*)&szDir);

	return (IDOK == MessageBoxW(GetActiveWindow(), szBuffer, szCaption, MB_OKCANCEL | MB_ICONEXCLAMATION));
}

static DWORD SHELL32_AnsiToUnicodeBuf(LPCSTR aPath, LPWSTR *wPath, DWORD minChars)
{
	DWORD len = MultiByteToWideChar(CP_ACP, 0, aPath, -1, NULL, 0);

	if (len < minChars)
	  len = minChars;

	*wPath = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
	if (*wPath)
	{
	  MultiByteToWideChar(CP_ACP, 0, aPath, -1, *wPath, len);
	  return NO_ERROR;
	}
	return E_OUTOFMEMORY;
}

static void SHELL32_FreeUnicodeBuf(LPWSTR wPath)
{
	HeapFree(GetProcessHeap(), 0, wPath);
}

/**************************************************************************
 * SHELL_DeleteDirectory()  [internal]
 *
 * Asks for confirmation when bShowUI is true and deletes the directory and
 * all its subdirectories and files if necessary.
 */
BOOL SHELL_DeleteDirectoryA(LPCSTR pszDir, BOOL bShowUI)
{
	LPWSTR wPath;
	BOOL ret = FALSE;

	if (!SHELL32_AnsiToUnicodeBuf(pszDir, &wPath, 0))
	{
	  ret = SHELL_DeleteDirectoryW(wPath, bShowUI);
	  SHELL32_FreeUnicodeBuf(wPath);
	}
	return ret;
}

static BOOL SHELL_DeleteDirectoryW(LPCWSTR pszDir, BOOL bShowUI)
{
	BOOL    ret = TRUE;
	HANDLE  hFind;
	WIN32_FIND_DATAW wfd;
	WCHAR   szTemp[MAX_PATH];

	/* Make sure the directory exists before eventually prompting the user */
	PathCombineW(szTemp, pszDir, wWildcardFile);
	hFind = FindFirstFileW(szTemp, &wfd);
	if (hFind == INVALID_HANDLE_VALUE)
	  return FALSE;

	if (!bShowUI || (ret = SHELL_ConfirmDialogW(ASK_DELETE_FOLDER, pszDir)))
	{
	  do
	  {
	    LPWSTR lp = wfd.cAlternateFileName;
	    if (!lp[0])
	      lp = wfd.cFileName;
	    if (IsDotDir(lp))
	      continue;
	    PathCombineW(szTemp, pszDir, lp);
	    if (FILE_ATTRIBUTE_DIRECTORY & wfd.dwFileAttributes)
	      ret = SHELL_DeleteDirectoryW(szTemp, FALSE);
	    else
	      ret = (SHNotifyDeleteFileW(szTemp) == ERROR_SUCCESS);
	  } while (ret && FindNextFileW(hFind, &wfd));
	}
	FindClose(hFind);
	if (ret)
	  ret = (SHNotifyRemoveDirectoryW(pszDir) == ERROR_SUCCESS);
	return ret;
}

/**************************************************************************
 *  SHELL_DeleteFileA()      [internal]
 */
BOOL SHELL_DeleteFileA(LPCSTR pszFile, BOOL bShowUI)
{
	if (bShowUI && !SHELL_ConfirmDialog(ASK_DELETE_FILE, pszFile))
	  return FALSE;

	return (SHNotifyDeleteFileA(pszFile) == ERROR_SUCCESS);
}

BOOL SHELL_DeleteFileW(LPCWSTR pszFile, BOOL bShowUI)
{
	if (bShowUI && !SHELL_ConfirmDialogW(ASK_DELETE_FILE, pszFile))
	  return FALSE;

	return (SHNotifyDeleteFileW(pszFile) == ERROR_SUCCESS);
}

/**************************************************************************
 * Win32CreateDirectory      [SHELL32.93]
 *
 * Creates a directory. Also triggers a change notify if one exists.
 *
 * PARAMS
 *  path       [I]   path to directory to create
 *
 * RETURNS
 *  TRUE if successful, FALSE otherwise
 *
 * NOTES:
 *  Verified on Win98 / IE 5 (SHELL32 4.72, March 1999 build) to be ANSI.
 *  This is Unicode on NT/2000
 */
static DWORD SHNotifyCreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sec)
{
	LPWSTR wPath;
	DWORD retCode;

	TRACE("(%s, %p)\n", debugstr_a(path), sec);

	retCode = SHELL32_AnsiToUnicodeBuf(path, &wPath, 0);
	if (!retCode)
	{
	  retCode = SHNotifyCreateDirectoryW(wPath, sec);
	  SHELL32_FreeUnicodeBuf(wPath);
	}
	return retCode;
}

/**********************************************************************/

static DWORD SHNotifyCreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES sec)
{
	TRACE("(%s, %p)\n", debugstr_w(path), sec);

	if (CreateDirectoryW(path, sec))
	{
	  SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, path, NULL);
	  return ERROR_SUCCESS;
	}
	return GetLastError();
}

/**********************************************************************/

BOOL WINAPI Win32CreateDirectoryAW(LPCVOID path, LPSECURITY_ATTRIBUTES sec)
{
	if (SHELL_OsIsUnicode())
	  return (SHNotifyCreateDirectoryW(path, sec) == ERROR_SUCCESS);
	return (SHNotifyCreateDirectoryA(path, sec) == ERROR_SUCCESS);
}

/************************************************************************
 * Win32RemoveDirectory      [SHELL32.94]
 *
 * Deletes a directory. Also triggers a change notify if one exists.
 *
 * PARAMS
 *  path       [I]   path to directory to delete
 *
 * RETURNS
 *  TRUE if successful, FALSE otherwise
 *
 * NOTES:
 *  Verified on Win98 / IE 5 (SHELL32 4.72, March 1999 build) to be ANSI.
 *  This is Unicode on NT/2000
 */
static DWORD SHNotifyRemoveDirectoryA(LPCSTR path)
{
	LPWSTR wPath;
	DWORD retCode;

	TRACE("(%s)\n", debugstr_a(path));

	retCode = SHELL32_AnsiToUnicodeBuf(path, &wPath, 0);
	if (!retCode)
	{
	  retCode = SHNotifyRemoveDirectoryW(wPath);
	  SHELL32_FreeUnicodeBuf(wPath);
	}
	return retCode;
}

/***********************************************************************/

static DWORD SHNotifyRemoveDirectoryW(LPCWSTR path)
{
	BOOL ret;
	TRACE("(%s)\n", debugstr_w(path));

	ret = RemoveDirectoryW(path);
	if (!ret)
	{
	  /* Directory may be write protected */
	  DWORD dwAttr = GetFileAttributesW(path);
	  if (IsAttrib(dwAttr, FILE_ATTRIBUTE_READONLY))
	    if (SetFileAttributesW(path, dwAttr & ~FILE_ATTRIBUTE_READONLY))
	      ret = RemoveDirectoryW(path);
	}
	if (ret)
	{
	  SHChangeNotify(SHCNE_RMDIR, SHCNF_PATHW, path, NULL);
	  return ERROR_SUCCESS;
	}
	return GetLastError();
}

/***********************************************************************/

BOOL WINAPI Win32RemoveDirectoryAW(LPCVOID path)
{
	if (SHELL_OsIsUnicode())
	  return (SHNotifyRemoveDirectoryW(path) == ERROR_SUCCESS);
	return (SHNotifyRemoveDirectoryA(path) == ERROR_SUCCESS);
}

/************************************************************************
 * Win32DeleteFile           [SHELL32.164]
 *
 * Deletes a file. Also triggers a change notify if one exists.
 *
 * PARAMS
 *  path       [I]   path to file to delete
 *
 * RETURNS
 *  TRUE if successful, FALSE otherwise
 *
 * NOTES:
 *  Verified on Win98 / IE 5 (SHELL32 4.72, March 1999 build) to be ANSI.
 *  This is Unicode on NT/2000
 */
static DWORD SHNotifyDeleteFileA(LPCSTR path)
{
	LPWSTR wPath;
	DWORD retCode;

	TRACE("(%s)\n", debugstr_a(path));

	retCode = SHELL32_AnsiToUnicodeBuf(path, &wPath, 0);
	if (!retCode)
	{
	  retCode = SHNotifyDeleteFileW(wPath);
	  SHELL32_FreeUnicodeBuf(wPath);
	}
	return retCode;
}

/***********************************************************************/

static DWORD SHNotifyDeleteFileW(LPCWSTR path)
{
	BOOL ret;

	TRACE("(%s)\n", debugstr_w(path));

	ret = DeleteFileW(path);
	if (!ret)
	{
	  /* File may be write protected or a system file */
	  DWORD dwAttr = GetFileAttributesW(path);
	  if (IsAttrib(dwAttr, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM))
	    if (SetFileAttributesW(path, dwAttr & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)))
	      ret = DeleteFileW(path);
	}
	if (ret)
	{
	  SHChangeNotify(SHCNE_DELETE, SHCNF_PATHW, path, NULL);
	  return ERROR_SUCCESS;
	}
	return GetLastError();
}

/***********************************************************************/

DWORD WINAPI Win32DeleteFileAW(LPCVOID path)
{
	if (SHELL_OsIsUnicode())
	  return (SHNotifyDeleteFileW(path) == ERROR_SUCCESS);
	return (SHNotifyDeleteFileA(path) == ERROR_SUCCESS);
}

/************************************************************************
 * SHNotifyMoveFile          [internal]
 *
 * Moves a file. Also triggers a change notify if one exists.
 *
 * PARAMS
 *  src        [I]   path to source file to move
 *  dest       [I]   path to target file to move to
 *
 * RETURNS
 *  ERORR_SUCCESS if successful
 */
static DWORD SHNotifyMoveFileW(LPCWSTR src, LPCWSTR dest)
{
	BOOL ret;

	TRACE("(%s %s)\n", debugstr_w(src), debugstr_w(dest));

	ret = MoveFileW(src, dest);
	if (!ret)
	{
	  DWORD dwAttr;

	  dwAttr = SHFindAttrW(dest, FALSE);
	  if (INVALID_FILE_ATTRIBUTES == dwAttr)
	  {
	    /* Source file may be write protected or a system file */
	    dwAttr = GetFileAttributesW(src);
	    if (IsAttrib(dwAttr, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM))
	      if (SetFileAttributesW(src, dwAttr & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)))
	        ret = MoveFileW(src, dest);
	  }
	}
	if (ret)
	{
	  SHChangeNotify(SHCNE_RENAMEITEM, SHCNF_PATHW, src, dest);
	  return ERROR_SUCCESS;
	}
	return GetLastError();
}

/************************************************************************
 * SHNotifyCopyFile          [internal]
 *
 * Copies a file. Also triggers a change notify if one exists.
 *
 * PARAMS
 *  src           [I]   path to source file to move
 *  dest          [I]   path to target file to move to
 *  bFailIfExists [I]   if TRUE, the target file will not be overwritten if
 *                      a file with this name already exists
 *
 * RETURNS
 *  ERROR_SUCCESS if successful
 */
static DWORD SHNotifyCopyFileW(LPCWSTR src, LPCWSTR dest, BOOL bFailIfExists)
{
	BOOL ret;

	TRACE("(%s %s %s)\n", debugstr_w(src), debugstr_w(dest), bFailIfExists ? "failIfExists" : "");

	ret = CopyFileW(src, dest, bFailIfExists);
	if (ret)
	{
	  SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, dest, NULL);
	  return ERROR_SUCCESS;
	}
	return GetLastError();
}

/*************************************************************************
 * SHCreateDirectory         [SHELL32.165]
 *
 * This function creates a file system folder whose fully qualified path is
 * given by path. If one or more of the intermediate folders do not exist,
 * they will be created as well.
 *
 * PARAMS
 *  hWnd       [I]
 *  path       [I]   path of directory to create
 *
 * RETURNS
 *  ERRROR_SUCCESS or one of the following values:
 *  ERROR_BAD_PATHNAME if the path is relative
 *  ERROR_FILE_EXISTS when a file with that name exists
 *  ERROR_PATH_NOT_FOUND can't find the path, probably invalid
 *  ERROR_INVLID_NAME if the path contains invalid chars
 *  ERROR_ALREADY_EXISTS when the directory already exists
 *  ERROR_FILENAME_EXCED_RANGE if the filename was to long to process
 *
 * NOTES
 *  exported by ordinal
 *  Win9x exports ANSI
 *  WinNT/2000 exports Unicode
 */
DWORD WINAPI SHCreateDirectory(HWND hWnd, LPCVOID path)
{
	if (SHELL_OsIsUnicode())
	  return SHCreateDirectoryExW(hWnd, path, NULL);
	return SHCreateDirectoryExA(hWnd, path, NULL);
}

/*************************************************************************
 * SHCreateDirectoryExA      [SHELL32.@]
 *
 * This function creates a file system folder whose fully qualified path is
 * given by path. If one or more of the intermediate folders do not exist,
 * they will be created as well.
 *
 * PARAMS
 *  hWnd       [I]
 *  path       [I]   path of directory to create
 *  sec        [I]   security attributes to use or NULL
 *
 * RETURNS
 *  ERRROR_SUCCESS or one of the following values:
 *  ERROR_BAD_PATHNAME or ERROR_PATH_NOT_FOUND if the path is relative
 *  ERROR_INVLID_NAME if the path contains invalid chars
 *  ERROR_FILE_EXISTS when a file with that name exists
 *  ERROR_ALREADY_EXISTS when the directory already exists
 *  ERROR_FILENAME_EXCED_RANGE if the filename was to long to process
 *
 *  FIXME: Not implemented yet;
 *  SHCreateDirectoryEx also verifies that the files in the directory will be visible
 *  if the path is a network path to deal with network drivers which might have a limited
 *  but unknown maximum path length. If not:
 *
 *  If hWnd is set to a valid window handle, a message box is displayed warning
 *  the user that the files may not be accessible. If the user chooses not to
 *  proceed, the function returns ERROR_CANCELLED.
 *
 *  If hWnd is set to NULL, no user interface is displayed and the function
 *  returns ERROR_CANCELLED.
 */
int WINAPI SHCreateDirectoryExA(HWND hWnd, LPCSTR path, LPSECURITY_ATTRIBUTES sec)
{
	LPWSTR wPath;
	DWORD retCode;

	TRACE("(%s, %p)\n", debugstr_a(path), sec);

	retCode = SHELL32_AnsiToUnicodeBuf(path, &wPath, 0);
	if (!retCode)
	{
	  retCode = SHCreateDirectoryExW(hWnd, wPath, sec);
	  SHELL32_FreeUnicodeBuf(wPath);
	}
	return retCode;
}

/*************************************************************************
 * SHCreateDirectoryExW      [SHELL32.@]
 */
int WINAPI SHCreateDirectoryExW(HWND hWnd, LPCWSTR path, LPSECURITY_ATTRIBUTES sec)
{
	int ret = ERROR_BAD_PATHNAME;
	TRACE("(%p, %s, %p)\n", hWnd, debugstr_w(path), sec);

	if (PathIsRelativeW(path))
	{
	  SetLastError(ret);
	}
	else
	{
	  ret = SHNotifyCreateDirectoryW(path, sec);
	  /* Refuse to work on certain error codes before trying to create directories recursively */
	  if (ret != ERROR_FILE_EXISTS &&
	      ret != ERROR_ALREADY_EXISTS &&
	      ret != ERROR_FILENAME_EXCED_RANGE)
	  {
	    WCHAR *pEnd, *pSlash, szTemp[MAX_PATH + 1];  /* extra for PathAddBackslash() */

	    lstrcpynW(szTemp, path, MAX_PATH);
	    pEnd = PathAddBackslashW(szTemp);
	    pSlash = szTemp + 3;

	    while (*pSlash)
	    {
	      while (*pSlash && *pSlash != '\\')
	        pSlash = CharNextW(pSlash);

	      if (*pSlash)
	      {
	        *pSlash = 0;    /* terminate path at separator */

	        ret = SHNotifyCreateDirectoryW(szTemp, pSlash + 1 == pEnd ? sec : NULL);
	      }
	      *pSlash++ = '\\'; /* put the separator back */
	    }
	  }

	  if (ret && hWnd && (ERROR_CANCELLED != ret))
	  {
	    /* We failed and should show a dialog box */
	    FIXME("Show system error message, creating path %s, failed with error %d\n", debugstr_w(path), ret);
	    ret = ERROR_CANCELLED; /* Error has been already presented to user (not really yet!) */
	  }
	}
	return ret;
}


/*************************************************************************
 * SHFindAttrW      [internal]
 *
 * Get the Attributes for a file or directory. The difference to GetAttributes()
 * is that this function will also work for paths containing wildcard characters
 * in its filename.

 * PARAMS
 *  path       [I]   path of directory or file to check
 *  fileOnly   [I]   TRUE if only files should be found
 *
 * RETURNS
 *  INVALID_FILE_ATTRIBUTES if the path does not exist, the actual attributes of
 *  the first file or directory found otherwise
 */
static DWORD SHFindAttrW(LPCWSTR pName, BOOL fileOnly)
{
	WIN32_FIND_DATAW wfd;
	BOOL b_FileMask = fileOnly && (NULL != StrPBrkW(pName, wWildcardChars));
	DWORD dwAttr = INVALID_FILE_ATTRIBUTES;
	HANDLE hFind = FindFirstFileW(pName, &wfd);

	TRACE("%s %d\n", debugstr_w(pName), fileOnly);
	if (INVALID_HANDLE_VALUE != hFind)
	{
	  do
	  {
	    if (b_FileMask && IsAttribDir(wfd.dwFileAttributes))
	       continue;
	    dwAttr = wfd.dwFileAttributes;
	    break;
	  }
	  while (FindNextFileW(hFind, &wfd));
	  FindClose(hFind);
	}
	return dwAttr;
}

/*************************************************************************
 *
 * SHFileStrICmp HelperFunction for SHFileOperationW
 *
 */
BOOL SHFileStrICmpW(LPWSTR p1, LPWSTR p2, LPWSTR p1End, LPWSTR p2End)
{
	WCHAR C1 = '\0';
	WCHAR C2 = '\0';
	int i_Temp = -1;
	int i_len1 = lstrlenW(p1);
	int i_len2 = lstrlenW(p2);

	if (p1End && (&p1[i_len1] >= p1End) && ('\\' == p1End[0]))
	{
	  C1 = p1End[0];
	  p1End[0] = '\0';
	  i_len1 = lstrlenW(p1);
	}
	if (p2End)
	{
	  if ((&p2[i_len2] >= p2End) && ('\\' == p2End[0]))
	  {
	    C2 = p2End[0];
	    if (C2)
	      p2End[0] = '\0';
	  }
	}
	else
	{
	  if ((i_len1 <= i_len2) && ('\\' == p2[i_len1]))
	  {
	    C2 = p2[i_len1];
	    if (C2)
	      p2[i_len1] = '\0';
	  }
	}
	i_len2 = lstrlenW(p2);
	if (i_len1 == i_len2)
	  i_Temp = lstrcmpiW(p1,p2);
	if (C1)
	  p1[i_len1] = C1;
	if (C2)
	  p2[i_len2] = C2;
	return !(i_Temp);
}

/*************************************************************************
 *
 * SHFileStrCpyCat HelperFunction for SHFileOperationW
 *
 */
LPWSTR SHFileStrCpyCatW(LPWSTR pTo, LPCWSTR pFrom, LPCWSTR pCatStr)
{
	LPWSTR pToFile = NULL;
	int  i_len;
	if (pTo)
	{
	  if (pFrom)
	    lstrcpyW(pTo, pFrom);
	  if (pCatStr)
	  {
	    i_len = lstrlenW(pTo);
	    if ((i_len) && (pTo[--i_len] != '\\'))
	      i_len++;
	    pTo[i_len] = '\\';
	    if (pCatStr[0] == '\\')
	      pCatStr++; \
	    lstrcpyW(&pTo[i_len+1], pCatStr);
	  }
	  pToFile = StrRChrW(pTo,NULL,'\\');
	  /* termination of the new string-group */
	  pTo[(lstrlenW(pTo)) + 1] = '\0';
	}
	return pToFile;
}

/**************************************************************************
 *	SHELL_FileNamesMatch()
 *
 * Accepts two \0 delimited lists of the file names. Checks whether number of
 * files in both lists is the same, and checks also if source-name exists.
 */
BOOL SHELL_FileNamesMatch(LPCWSTR pszFiles1, LPCWSTR pszFiles2, BOOL bOnlySrc)
{
	while ((pszFiles1[0] != '\0') &&
	       (bOnlySrc || (pszFiles2[0] != '\0')))
	{
	  if (NULL == StrPBrkW(pszFiles1, wWildcardChars))
	  {
	    if (INVALID_FILE_ATTRIBUTES == GetFileAttributesW(pszFiles1))
	      return FALSE;
	  }
	  pszFiles1 += lstrlenW(pszFiles1) + 1;
	  if (!bOnlySrc)
	    pszFiles2 += lstrlenW(pszFiles2) + 1;
	}
	return ((pszFiles1[0] == '\0') && (bOnlySrc || (pszFiles2[0] == '\0')));
}

/*************************************************************************
 *
 * SHNameTranslate HelperFunction for SHFileOperationA
 *
 * Translates a list of 0 terminated ASCII strings into Unicode. If *wString
 * is NULL, only the necessary size of the string is determined and returned,
 * otherwise the ASCII strings are copied into it and the buffer is increased
 * to point to the location after the final 0 termination char.
 */
DWORD SHNameTranslate(LPWSTR* wString, LPCWSTR* pWToFrom, BOOL more)
{
	DWORD size = 0, aSize = 0;
	LPCSTR aString = (LPCSTR)*pWToFrom;

	if (aString)
	{
	  do
	  {
	    size = lstrlenA(aString) + 1;
	    aSize += size;
	    aString += size;
	  } while ((size != 1) && more);
	  /* The two sizes might be different in the case of multibyte chars */
	  size = MultiByteToWideChar(CP_ACP, 0, aString, aSize, *wString, 0);
	  if (*wString) /* only in the second loop */
	  {
	    MultiByteToWideChar(CP_ACP, 0, (LPCSTR)*pWToFrom, aSize, *wString, size);
	    *pWToFrom = *wString;
	    *wString += size;
	  }
	}
	return size;
}
/*************************************************************************
 * SHFileOperationA          [SHELL32.@]
 *
 * Function to copy, move, delete and create one or more files with optional
 * user prompts.
 *
 * PARAMS
 *  lpFileOp   [I/O] pointer to a structure containing all the necessary information
 *
 * NOTES
 *  exported by name
 */
int WINAPI SHFileOperationA(LPSHFILEOPSTRUCTA lpFileOp)
{
	SHFILEOPSTRUCTW nFileOp = *((LPSHFILEOPSTRUCTW)lpFileOp);
	int retCode = 0;
	DWORD size;
	LPWSTR ForFree = NULL, /* we change wString in SHNameTranslate and can't use it for freeing */
	       wString = NULL; /* we change this in SHNameTranslate */

	TRACE("\n");
	if (FO_DELETE == (nFileOp.wFunc & FO_MASK))
	  nFileOp.pTo = NULL; /* we need a NULL or a valid pointer for translation */
	if (!(nFileOp.fFlags & FOF_SIMPLEPROGRESS))
	  nFileOp.lpszProgressTitle = NULL; /* we need a NULL or a valid pointer for translation */
	while (1) /* every loop calculate size, second translate also, if we have storage for this */
	{
	  size = SHNameTranslate(&wString, &nFileOp.lpszProgressTitle, FALSE); /* no loop */
	  size += SHNameTranslate(&wString, &nFileOp.pFrom, TRUE); /* internal loop */
	  size += SHNameTranslate(&wString, &nFileOp.pTo, TRUE); /* internal loop */

	  if (ForFree)
	  {
	    retCode = SHFileOperationW(&nFileOp);
	    HeapFree(GetProcessHeap(), 0, ForFree); /* we can not use wString, it was changed */
	    break;
	  }
	  else
	  {
	    wString = ForFree = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));
	    if (ForFree) continue;
	    retCode = ERROR_OUTOFMEMORY;
	    nFileOp.fAnyOperationsAborted = TRUE;
	    SetLastError(retCode);
	    return retCode;
	  }
	}

	lpFileOp->hNameMappings = nFileOp.hNameMappings;
	lpFileOp->fAnyOperationsAborted = nFileOp.fAnyOperationsAborted;
	return retCode;
}

static const char * debug_shfileops_flags( DWORD fFlags )
{
    return wine_dbg_sprintf( "%s%s%s%s%s%s%s%s%s%s%s%s%s",
	fFlags & FOF_MULTIDESTFILES ? "FOF_MULTIDESTFILES " : "",
	fFlags & FOF_CONFIRMMOUSE ? "FOF_CONFIRMMOUSE " : "",
	fFlags & FOF_SILENT ? "FOF_SILENT " : "",
	fFlags & FOF_RENAMEONCOLLISION ? "FOF_RENAMEONCOLLISION " : "",
	fFlags & FOF_NOCONFIRMATION ? "FOF_NOCONFIRMATION " : "",
	fFlags & FOF_WANTMAPPINGHANDLE ? "FOF_WANTMAPPINGHANDLE " : "",
	fFlags & FOF_ALLOWUNDO ? "FOF_ALLOWUNDO " : "",
	fFlags & FOF_FILESONLY ? "FOF_FILESONLY " : "",
	fFlags & FOF_SIMPLEPROGRESS ? "FOF_SIMPLEPROGRESS " : "",
	fFlags & FOF_NOCONFIRMMKDIR ? "FOF_NOCONFIRMMKDIR " : "",
	fFlags & FOF_NOERRORUI ? "FOF_NOERRORUI " : "",
	fFlags & FOF_NOCOPYSECURITYATTRIBS ? "FOF_NOCOPYSECURITYATTRIBS" : "",
	fFlags & 0xf000 ? "MORE-UNKNOWN-Flags" : "");
}

static const char * debug_shfileops_action( DWORD op )
{
    LPCSTR cFO_Name [] = {"FO_????","FO_MOVE","FO_COPY","FO_DELETE","FO_RENAME"};
    return wine_dbg_sprintf("%s", cFO_Name[ op ]);
}

#define ERROR_SHELL_INTERNAL_FILE_NOT_FOUND 1026
#define HIGH_ADR (LPWSTR)0xffffffff

static int file_operation_delete( WIN32_FIND_DATAW *wfd,SHFILEOPSTRUCTW nFileOp, LPWSTR pFromFile,LPWSTR pTempFrom,HANDLE *hFind)

{
    LPWSTR lpFileName;
    BOOL    b_Mask = (NULL != StrPBrkW(&pFromFile[1], wWildcardChars));
    int retCode = 0;
    do
    {
        lpFileName = wfd->cAlternateFileName;
        if (!lpFileName[0])
            lpFileName = wfd->cFileName;
        if (IsDotDir(lpFileName) ||
                ((b_Mask) && IsAttribDir(wfd->dwFileAttributes) && (nFileOp.fFlags & FOF_FILESONLY)))
            continue;
        SHFileStrCpyCatW(&pFromFile[1], lpFileName, NULL);
        /* TODO: Check the SHELL_DeleteFileOrDirectoryW() function in shell32.dll */
        if (IsAttribFile(wfd->dwFileAttributes))
        {
            if(SHNotifyDeleteFileW(pTempFrom) != ERROR_SUCCESS)
            {
                nFileOp.fAnyOperationsAborted = TRUE;
                retCode = 0x78; /* value unknown */
            }
        }
        else if(!SHELL_DeleteDirectoryW(pTempFrom, (!(nFileOp.fFlags & FOF_NOCONFIRMATION))))
        {
            nFileOp.fAnyOperationsAborted = TRUE;
            retCode = 0x79; /* value unknown */
        }
    }
    while (!nFileOp.fAnyOperationsAborted && FindNextFileW(*hFind,wfd));
    FindClose(*hFind);
    *hFind = INVALID_HANDLE_VALUE;
    return retCode;
}

/*
 * Summary of flags:
 *
 * implemented flags:
 * FOF_MULTIDESTFILES, FOF_NOCONFIRMATION, FOF_FILESONLY
 *
 * unimplememented and ignored flags:
 * FOF_CONFIRMMOUSE, FOF_SILENT, FOF_NOCONFIRMMKDIR,
 *       FOF_SIMPLEPROGRESS, FOF_NOCOPYSECURITYATTRIBS
 *
 * partially implemented, breaks if file exists:
 * FOF_RENAMEONCOLLISION
 *
 * unimplemented and break if any other flag set:
 * FOF_ALLOWUNDO, FOF_WANTMAPPINGHANDLE
 */

static int file_operation_checkFlags(SHFILEOPSTRUCTW nFileOp)
{
    FILEOP_FLAGS OFl = ((FILEOP_FLAGS)nFileOp.fFlags & 0xfff);
    long FuncSwitch = (nFileOp.wFunc & FO_MASK);
    long level= nFileOp.wFunc >> 4;

    TRACE("%s level=%ld nFileOp.fFlags=0x%x\n", 
            debug_shfileops_action(FuncSwitch), level, nFileOp.fFlags);
    /*    OFl &= (-1 - (FOF_MULTIDESTFILES | FOF_FILESONLY)); */
    /*    OFl ^= (FOF_SILENT | FOF_NOCONFIRMATION | FOF_SIMPLEPROGRESS | FOF_NOCONFIRMMKDIR); */
    OFl &= (~(FOF_MULTIDESTFILES | FOF_NOCONFIRMATION | FOF_FILESONLY));  /* implemented */
    OFl ^= (FOF_SILENT | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_NOCOPYSECURITYATTRIBS); /* ignored, if one */
    OFl &= (~FOF_SIMPLEPROGRESS); /* ignored, only with FOF_SILENT */
    if (OFl)
    {
        if (OFl & (~(FOF_CONFIRMMOUSE | FOF_SILENT | FOF_RENAMEONCOLLISION |
                        FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_NOCOPYSECURITYATTRIBS)))
        {
            TRACE("%s level=%ld lpFileOp->fFlags=0x%x not implemented, Aborted=TRUE, stub\n",
                    debug_shfileops_action(FuncSwitch), level, OFl);
            return 0x403; /* 1027, we need an extension to shlfileop */
        }
        else
        {
            TRACE("%s level=%ld lpFileOp->fFlags=0x%x not fully implemented, stub\n", 
                    debug_shfileops_action(FuncSwitch), level, OFl);
        } 
    } 
    return 0;
}

/*************************************************************************
 * SHFileOperationW          [SHELL32.@]
 *
 * See SHFileOperationA
 */
int WINAPI SHFileOperationW(LPSHFILEOPSTRUCTW lpFileOp)
{
	SHFILEOPSTRUCTW nFileOp = *(lpFileOp);

	LPCWSTR pNextFrom = nFileOp.pFrom;
	LPCWSTR pNextTo = nFileOp.pTo;
	LPCWSTR pFrom = pNextFrom;
	LPCWSTR pTo = NULL;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW wfd;
	LPWSTR pTempFrom = NULL;
	LPWSTR pTempTo = NULL;
	LPWSTR pFromFile;
	LPWSTR pToFile = NULL;
	LPWSTR lpFileName;
	int retCode = 0;
	DWORD ToAttr;
	DWORD ToPathAttr;

	BOOL b_Multi = (nFileOp.fFlags & FOF_MULTIDESTFILES);

	BOOL b_MultiTo = (FO_DELETE != (lpFileOp->wFunc & FO_MASK));
	BOOL b_MultiPaired = (!b_MultiTo);
	BOOL b_MultiFrom = FALSE;
	BOOL not_overwrite;
	BOOL ask_overwrite;
	BOOL b_SameRoot;
	BOOL b_SameTailName;
	BOOL b_ToInvalidTail = FALSE;
	BOOL b_ToValid; /* for W98-Bug for FO_MOVE with source and target in same rootdrive */
	BOOL b_Mask;
	BOOL b_ToTailSlash = FALSE;

	long FuncSwitch = (nFileOp.wFunc & FO_MASK);
	long level= nFileOp.wFunc>>4;

        int ret;

	/*  default no error */
	nFileOp.fAnyOperationsAborted = FALSE;

	if ((FuncSwitch < FO_MOVE) || (FuncSwitch > FO_RENAME))
	    goto shfileop_end; /* no valid FunctionCode */

	if (level == 0)
            TRACE("%s: flags (0x%04x) : %s\n",
                debug_shfileops_action(FuncSwitch), nFileOp.fFlags,
                debug_shfileops_flags(nFileOp.fFlags) );

        /* establish when pTo is interpreted as the name of the destination file
         * or the directory where the Fromfile should be copied to.
         * This depends on:
         * (1) pTo points to the name of an existing directory;
         * (2) the flag FOF_MULTIDESTFILES is present;
         * (3) whether pFrom point to multiple filenames.
         *
         * Some experiments:
         *
         * destisdir               1 1 1 1 0 0 0 0
         * FOF_MULTIDESTFILES      1 1 0 0 1 1 0 0
         * multiple from filenames 1 0 1 0 1 0 1 0
         *                         ---------------
         * copy files to dir       1 0 1 1 0 0 1 0
         * create dir              0 0 0 0 0 0 1 0
         */

        ret = file_operation_checkFlags(nFileOp);
        if (ret != 0)
        {
            retCode = ret;
            goto shfileop_end;
        }

        if ((pNextFrom) && (!(b_MultiTo) || (pNextTo)))
        {
	    nFileOp.pFrom = pTempFrom = HeapAlloc(GetProcessHeap(), 0, ((1 + 2 * (b_MultiTo)) * MAX_PATH + 6) * sizeof(WCHAR));
	    if (!pTempFrom)
	    {
                retCode = ERROR_OUTOFMEMORY;
                SetLastError(retCode);
                goto shfileop_end;
	    }
	    if (b_MultiTo)
                pTempTo = &pTempFrom[MAX_PATH + 4];
	    nFileOp.pTo = pTempTo;
	    ask_overwrite = (!(nFileOp.fFlags & FOF_NOCONFIRMATION) && !(nFileOp.fFlags & FOF_RENAMEONCOLLISION));
	    not_overwrite = (!(nFileOp.fFlags & FOF_NOCONFIRMATION) ||  (nFileOp.fFlags & FOF_RENAMEONCOLLISION));
        }
        else
        {
	    retCode = ERROR_SHELL_INTERNAL_FILE_NOT_FOUND;
	    goto shfileop_end;
        }
        /* need break at error before change sourcepointer */
        while(!nFileOp.fAnyOperationsAborted && (pNextFrom[0]))
        {
	    nFileOp.wFunc =  ((level + 1) << 4) + FuncSwitch;
	    nFileOp.fFlags = lpFileOp->fFlags;

	    if (b_MultiTo)
	    {
                pTo = pNextTo;
                pNextTo = &pNextTo[lstrlenW(pTo)+1];
                b_MultiTo = (b_Multi && pNextTo[0]);
	    }

	    pFrom = pNextFrom;
	    pNextFrom = &pNextFrom[lstrlenW(pNextFrom)+1];
	    if (!b_MultiFrom && !b_MultiTo)
                b_MultiFrom = (pNextFrom[0]);

	    pFromFile = SHFileStrCpyCatW(pTempFrom, pFrom, NULL);

	    if (pTo)
	    {
                pToFile = SHFileStrCpyCatW(pTempTo, pTo, NULL);
	    }
	    if (!b_MultiPaired)
	    {
                b_MultiPaired =
                    SHELL_FileNamesMatch(lpFileOp->pFrom, lpFileOp->pTo, (!b_Multi || b_MultiFrom));
	    }
	    if (!(b_MultiPaired) || !(pFromFile) || !(pFromFile[1]) || ((pTo) && !(pToFile)))
	    {
                retCode = ERROR_SHELL_INTERNAL_FILE_NOT_FOUND;
                goto shfileop_end;
	    }
	    if (pTo)
	    {
                b_ToTailSlash = (!pToFile[1]);
                if (b_ToTailSlash)
                {
                    pToFile[0] = '\0';
                    if (StrChrW(pTempTo,'\\'))
                    {
                        pToFile = SHFileStrCpyCatW(pTempTo, NULL, NULL);
                    }
                }
                b_ToInvalidTail = (NULL != StrPBrkW(&pToFile[1], wWildcardChars));
	    }

	    /* for all */
	    b_Mask = (NULL != StrPBrkW(&pFromFile[1], wWildcardChars));
	    if (FO_RENAME == FuncSwitch)
	    {
                /* temporary only for FO_RENAME */
                if (b_MultiTo || b_MultiFrom || (b_Mask && !b_ToInvalidTail))
                {
#ifndef W98_FO_FUNCTION
                    retCode = ERROR_GEN_FAILURE;  /* W2K ERROR_GEN_FAILURE, W98 returns no error */
#endif
                    goto shfileop_end;
                }
	    }
             
	    hFind = FindFirstFileW(pFrom, &wfd);
	    if (INVALID_HANDLE_VALUE == hFind)
	    {
                if ((FO_DELETE == FuncSwitch) && (b_Mask))
                {
                    DWORD FromPathAttr;
                    pFromFile[0] = '\0';
                    FromPathAttr = GetFileAttributesW(pTempFrom);
                    pFromFile[0] = '\\';
                    if (IsAttribDir(FromPathAttr))
                    {
                        /* FO_DELETE with mask and without found is valid */
                        goto shfileop_end;
                    }
                }
                /* root (without mask) is also not allowed as source, tested in W98 */
                retCode = ERROR_SHELL_INTERNAL_FILE_NOT_FOUND;
                goto shfileop_end;
	    }

            /* for all */

            /* ??? b_Mask = (!SHFileStrICmpA(&pFromFile[1], &wfd.cFileName[0], HIGH_ADR, HIGH_ADR)); */
	    if (!pTo) /* FO_DELETE */
	    {
                ret = file_operation_delete(&wfd,nFileOp,pFromFile,pTempFrom,&hFind);
                /* if ret is not 0, nFileOp.fAnyOperationsAborted is TRUE */
                if (ret != 0)
                {
                  retCode = ret;
                  goto shfileop_end;
                }
                continue;
	    } /* FO_DELETE ends, pTo must be always valid from here */

	    b_SameRoot = (toupperW(pTempFrom[0]) == toupperW(pTempTo[0]));
	    b_SameTailName = SHFileStrICmpW(pToFile, pFromFile, NULL, NULL);

	    ToPathAttr = ToAttr = GetFileAttributesW(pTempTo);
	    if (!b_Mask && (ToAttr == INVALID_FILE_ATTRIBUTES) && (pToFile))
	    {
                pToFile[0] = '\0';
                ToPathAttr = GetFileAttributesW(pTempTo);
                pToFile[0] = '\\';
	    }

	    if (FO_RENAME == FuncSwitch)
	    {
                if (!b_SameRoot || b_Mask /* FO_RENAME works not with Mask */
                    || !SHFileStrICmpW(pTempFrom, pTempTo, pFromFile, NULL)
                    || (SHFileStrICmpW(pTempFrom, pTempTo, pFromFile, HIGH_ADR) && !b_ToTailSlash))
                {
                    retCode = 0x73;
                    goto shfileop_end;
                }
                if (b_ToInvalidTail)
                {
                    retCode=0x2;
                    goto shfileop_end;
                }
                if (INVALID_FILE_ATTRIBUTES == ToPathAttr)
                {
                    retCode = 0x75;
                    goto shfileop_end;
                }
                if (IsAttribDir(wfd.dwFileAttributes) && IsAttribDir(ToAttr))
                {
                    retCode = (b_ToTailSlash) ? 0xb7 : 0x7b;
                    goto shfileop_end;
                }
                /* we use SHNotifyMoveFile() instead MoveFileW */
                if (SHNotifyMoveFileW(pTempFrom, pTempTo) != ERROR_SUCCESS)
                {
                    /* we need still the value for the returncode, we use the mostly assumed */
                    retCode = 0xb7;
                    goto shfileop_end;
                }
                goto shfileop_end;
	    }

	    /* W98 Bug with FO_MOVE different from FO_COPY, better the same as FO_COPY */
	    b_ToValid = ((b_SameTailName &&  b_SameRoot && (FO_COPY == FuncSwitch)) ||
                         (b_SameTailName && !b_SameRoot) || (b_ToInvalidTail));

	    /* handle mask in source */
	    if (b_Mask)
	    {
                if (!IsAttribDir(ToAttr))
                {
                    retCode = (b_ToInvalidTail &&/* b_SameTailName &&*/ (FO_MOVE == FuncSwitch)) \
                        ? 0x2 : 0x75;
                    goto shfileop_end;
                }
                pToFile = SHFileStrCpyCatW(pTempTo, NULL, wBackslash);
                nFileOp.fFlags = (nFileOp.fFlags | FOF_MULTIDESTFILES);
                do
                {
                    lpFileName = wfd.cAlternateFileName;
                    if (!lpFileName[0])
                        lpFileName = wfd.cFileName;
                    if (IsDotDir(lpFileName) ||
                        (IsAttribDir(wfd.dwFileAttributes) && (nFileOp.fFlags & FOF_FILESONLY)))
                        continue; /* next name in pTempFrom(dir) */
                    SHFileStrCpyCatW(&pToFile[1], lpFileName, NULL);
                    SHFileStrCpyCatW(&pFromFile[1], lpFileName, NULL);
                    retCode = SHFileOperationW (&nFileOp);
                } while(!nFileOp.fAnyOperationsAborted && FindNextFileW(hFind, &wfd));
	    }
	    FindClose(hFind);
	    hFind = INVALID_HANDLE_VALUE;
	    /* FO_COPY/FO_MOVE with mask, FO_DELETE and FO_RENAME are solved */
	    if (b_Mask)
                continue;

	    /* only FO_COPY/FO_MOVE without mask, all others are (must be) solved */
	    if (IsAttribDir(wfd.dwFileAttributes) && (ToAttr == INVALID_FILE_ATTRIBUTES))
	    {
                if (pToFile)
                {
                    pToFile[0] = '\0';
                    ToPathAttr = GetFileAttributesW(pTempTo);
                    if ((ToPathAttr == INVALID_FILE_ATTRIBUTES) && b_ToValid)
                    {
                        /* create dir must be here, sample target D:\y\ *.* create with RC=10003 */
                        if (SHNotifyCreateDirectoryW(pTempTo, NULL))
                        {
                            retCode = 0x73;/* value unknown */
                            goto shfileop_end;
                        }
                        ToPathAttr = GetFileAttributesW(pTempTo);
                    }
                    pToFile[0] = '\\';
                    if (b_ToInvalidTail)
                    {
                        retCode = 0x10003;
                        goto shfileop_end;
                    }
                }
	    }

	    /* trailing BackSlash is ever removed and pToFile points to BackSlash before */
	    if (!b_MultiTo && (b_MultiFrom || (!(b_Multi) && IsAttribDir(ToAttr))))
	    {
                if ((FO_MOVE == FuncSwitch) && IsAttribDir(ToAttr) && IsAttribDir(wfd.dwFileAttributes))
                {
                    if (b_Multi)
                    {
                        retCode = 0x73; /* !b_Multi = 0x8 ?? */
                        goto shfileop_end;
                    }
                }
                pToFile = SHFileStrCpyCatW(pTempTo, NULL, wfd.cFileName);
                ToAttr = GetFileAttributesW(pTempTo);
	    }

	    if (IsAttribDir(ToAttr))
	    {
                if (IsAttribFile(wfd.dwFileAttributes))
                {
                    retCode = (FO_COPY == FuncSwitch) ? 0x75 : 0xb7;
                    goto shfileop_end;
                }
	    }
	    else
	    {
                pToFile[0] = '\0';
                ToPathAttr = GetFileAttributesW(pTempTo);
                pToFile[0] = '\\';
                if (IsAttribFile(ToPathAttr))
                {
                    /* error, is this tested ? */
                    retCode = 0x777402;
                    goto shfileop_end;
                }
	    }

	    /* singlesource + no mask */
	    if (INVALID_FILE_ATTRIBUTES == (ToAttr & ToPathAttr))
	    {
                /* Target-dir does not exist, and cannot be created */
                retCode=0x75;
                goto shfileop_end;
	    }

	    switch(FuncSwitch)
	    {
	    case FO_MOVE:
                pToFile = NULL;
                if ((ToAttr == INVALID_FILE_ATTRIBUTES) && SHFileStrICmpW(pTempFrom, pTempTo, pFromFile, NULL))
                {
                    nFileOp.wFunc =  ((level+1)<<4) + FO_RENAME;
                }
                else
                {
                    if (b_SameRoot && IsAttribDir(ToAttr) && IsAttribDir(wfd.dwFileAttributes))
                    {
                        /* we need pToFile for FO_DELETE after FO_MOVE contence */
                        pToFile = SHFileStrCpyCatW(pTempFrom, NULL, wWildcardFile);
                    }
                    else
                    {
                        nFileOp.wFunc =  ((level+1)<<4) + FO_COPY;
                    }
                }
                retCode = SHFileOperationW(&nFileOp);
                if (pToFile)
                    ((DWORD*)pToFile)[0] = '\0';
                if (!nFileOp.fAnyOperationsAborted && (FO_RENAME != (nFileOp.wFunc & 0xf)))
                {
                    nFileOp.wFunc =  ((level+1)<<4) + FO_DELETE;
                    retCode = SHFileOperationW(&nFileOp);
                }
                continue;
	    case FO_COPY:
                if (SHFileStrICmpW(pTempFrom, pTempTo, NULL, NULL))
                { /* target is the same as source ? */
                    /* we still need the value for the returncode, we assume 0x71 */
                    retCode = 0x71;
                    goto shfileop_end;
                }
                if (IsAttribDir((ToAttr & wfd.dwFileAttributes)))
                {
                    if (IsAttribDir(ToAttr) || !SHNotifyCreateDirectoryW(pTempTo, NULL))
                    {
                        /* ??? nFileOp.fFlags = (nFileOp.fFlags | FOF_MULTIDESTFILES); */
                        SHFileStrCpyCatW(pTempFrom, NULL, wWildcardFile);
                        retCode = SHFileOperationW(&nFileOp);
                    }
                    else
                    {
                        retCode = 0x750;/* value unknown */
                        goto shfileop_end;
                    }
                }
                else
                {
                    if (!(ask_overwrite && SHELL_ConfirmDialogW(ASK_OVERWRITE_FILE, pTempTo))
                        && (not_overwrite))
                    {
                        /* we still need the value for the returncode, we use the mostly assumed */
                        retCode = 0x73;
                        goto shfileop_end;
                    }
                    if (SHNotifyCopyFileW(pTempFrom, pTempTo, TRUE) != ERROR_SUCCESS)
                    {
                        retCode = 0x77; /* value unknown */
                        goto shfileop_end;
                    }
                }
	    }
        }

shfileop_end:
	if (hFind != INVALID_HANDLE_VALUE)
	    FindClose(hFind);
	hFind = INVALID_HANDLE_VALUE;
        HeapFree(GetProcessHeap(), 0, pTempFrom);
	if (retCode)
	    nFileOp.fAnyOperationsAborted = TRUE;
	TRACE("%s level=%ld AnyOpsAborted=%s ret=0x%x, with %s %s%s\n",
              debug_shfileops_action(FuncSwitch), level,
	      nFileOp.fAnyOperationsAborted ? "TRUE":"FALSE",
	      retCode, debugstr_w(pFrom), pTo ? "-> ":"", debugstr_w(pTo));

	lpFileOp->fAnyOperationsAborted = nFileOp.fAnyOperationsAborted;
	return retCode;
}

#define SHDSA_GetItemCount(hdsa) (*(int*)(hdsa))

/*************************************************************************
 * SHFreeNameMappings      [shell32.246]
 *
 * Free the mapping handle returned by SHFileoperation if FOF_WANTSMAPPINGHANDLE
 * was specified.
 *
 * PARAMS
 *  hNameMapping [I] handle to the name mappings used during renaming of files
 *
 */
void WINAPI SHFreeNameMappings(HANDLE hNameMapping)
{
	if (hNameMapping)
	{
	  int i = SHDSA_GetItemCount((HDSA)hNameMapping) - 1;

	  for (; i>= 0; i--)
	  {
	    LPSHNAMEMAPPINGW lp = DSA_GetItemPtr((HDSA)hNameMapping, i);

	    SHFree(lp->pszOldPath);
	    SHFree(lp->pszNewPath);
	  }
	  DSA_Destroy((HDSA)hNameMapping);
	}
}

/*************************************************************************
 * SheGetDirW [SHELL32.281]
 *
 */
HRESULT WINAPI SheGetDirW(LPWSTR u, LPWSTR v)
{	FIXME("%p %p stub\n",u,v);
	return 0;
}

/*************************************************************************
 * SheChangeDirW [SHELL32.274]
 *
 */
HRESULT WINAPI SheChangeDirW(LPWSTR u)
{	FIXME("(%s),stub\n",debugstr_w(u));
	return 0;
}

/*************************************************************************
 * IsNetDrive			[SHELL32.66]
 */
BOOL WINAPI IsNetDrive(DWORD drive)
{
	char root[4];
	strcpy(root, "A:\\");
	root[0] += (char)drive;
	return (GetDriveTypeA(root) == DRIVE_REMOTE);
}


/*************************************************************************
 * RealDriveType                [SHELL32.524]
 */
INT WINAPI RealDriveType(INT drive, BOOL bQueryNet)
{
    char root[] = "A:\\";
    root[0] += (char)drive;
    return GetDriveTypeA(root);
}
