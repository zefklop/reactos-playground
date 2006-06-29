/* $Id$
 *
 * PROJECT:         ReactOS Console Configuration DLL
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            dll/win32/console/options.c
 * PURPOSE:         displays options dialog
 * PROGRAMMERS:     Johannes Anderwald (johannes.anderwald@student.tugraz.at)
 */

#include "console.h"


INT_PTR 
CALLBACK
OptionsProc(
  HWND hwndDlg,
  UINT uMsg,
  WPARAM wParam,
  LPARAM lParam
)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			return TRUE;

		default:
			break;
	}

	return FALSE;
}
