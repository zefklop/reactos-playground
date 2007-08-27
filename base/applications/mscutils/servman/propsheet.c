/*
 * PROJECT:     ReactOS Services
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        base/applications/mscutils/servman/propsheet.c
 * PURPOSE:     Property dialog box message handler
 * COPYRIGHT:   Copyright 2006-2007 Ged Murphy <gedmurphy@reactos.org>
 *
 */

#include "precomp.h"

static VOID
SetButtonStates(PMAIN_WND_INFO Info,
                HWND hwndDlg)
{
    HWND hButton;
    DWORD Flags, State;

    Flags = Info->pCurrentService->ServiceStatusProcess.dwControlsAccepted;
    State = Info->pCurrentService->ServiceStatusProcess.dwCurrentState;

    if (State == SERVICE_STOPPED)
    {
        hButton = GetDlgItem(hwndDlg, IDC_START);
        EnableWindow (hButton, TRUE);
    }

    if ( (Flags & SERVICE_ACCEPT_STOP) && (State == SERVICE_RUNNING) )
    {
        hButton = GetDlgItem(hwndDlg, IDC_STOP);
        EnableWindow (hButton, TRUE);
    }

    if ( (Flags & SERVICE_ACCEPT_PAUSE_CONTINUE) && (State == SERVICE_RUNNING) )
    {
        hButton = GetDlgItem(hwndDlg, IDC_PAUSE);
        EnableWindow (hButton, TRUE);
    }

    if ( (Flags & SERVICE_ACCEPT_STOP) && (State == SERVICE_RUNNING) )
    {
        hButton = GetDlgItem(hwndDlg, IDC_PAUSE);
        EnableWindow (hButton, TRUE);
    }
}

/*
 * Fills the 'startup type' combo box with possible
 * values and sets it to value of the selected item
 */
static VOID
SetStartupType(PMAIN_WND_INFO Info,
               HWND hwndDlg)
{
    HWND hList;
    HKEY hKey;
    TCHAR buf[25];
    DWORD dwValueSize = 0;
    DWORD StartUp = 0;
    LPCTSTR Path = _T("System\\CurrentControlSet\\Services\\%s");
    TCHAR KeyBuf[300];

    /* open the registry key for the service */
    _sntprintf(KeyBuf,
               sizeof(KeyBuf) / sizeof(TCHAR),
               Path,
               Info->pCurrentService->lpServiceName);

    RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                 KeyBuf,
                 0,
                 KEY_READ,
                 &hKey);

    hList = GetDlgItem(hwndDlg, IDC_START_TYPE);

    LoadString(hInstance, IDS_SERVICES_AUTO, buf, sizeof(buf) / sizeof(TCHAR));
    SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)buf);
    LoadString(hInstance, IDS_SERVICES_MAN, buf, sizeof(buf) / sizeof(TCHAR));
    SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)buf);
    LoadString(hInstance, IDS_SERVICES_DIS, buf, sizeof(buf) / sizeof(TCHAR));
    SendMessage(hList, CB_ADDSTRING, 0, (LPARAM)buf);

    dwValueSize = sizeof(DWORD);
    if (RegQueryValueEx(hKey,
                        _T("Start"),
                        NULL,
                        NULL,
                        (LPBYTE)&StartUp,
                        &dwValueSize))
    {
        RegCloseKey(hKey);
        return;
    }

    if (StartUp == 0x02)
        SendMessage(hList, CB_SETCURSEL, 0, 0);
    else if (StartUp == 0x03)
        SendMessage(hList, CB_SETCURSEL, 1, 0);
    else if (StartUp == 0x04)
        SendMessage(hList, CB_SETCURSEL, 2, 0);
}


/*
 * Populates the General Properties dialog with
 * the relevant service information
 */
static VOID
GetDlgInfo(PMAIN_WND_INFO Info,
           HWND hwndDlg)
{
    LPTSTR lpDescription;
    LPTSTR lpPathToExe;

    /* set the service name */
    SendDlgItemMessage(hwndDlg,
                       IDC_SERV_NAME,
                       WM_SETTEXT,
                       0,
                       (LPARAM)Info->pCurrentService->lpServiceName);

    /* set the display name */
    SendDlgItemMessage(hwndDlg,
                       IDC_DISP_NAME,
                       WM_SETTEXT,
                       0,
                       (LPARAM)Info->pCurrentService->lpDisplayName);

    /* set the description */
    if ((lpDescription = GetDescription(Info->pCurrentService->lpServiceName)))
    {
        SendDlgItemMessage(hwndDlg,
                           IDC_DESCRIPTION,
                           WM_SETTEXT,
                           0,
                           (LPARAM)lpDescription);

        HeapFree(ProcessHeap,
                 0,
                 lpDescription);


    }

    /* set the executable path */
    if ((lpPathToExe = GetExecutablePath(Info)))
    {
        SendDlgItemMessage(hwndDlg,
                           IDC_EXEPATH,
                           WM_SETTEXT,
                           0,
                           (LPARAM)lpPathToExe);

        HeapFree(ProcessHeap,
                 0,
                 lpPathToExe);
    }

    /* set startup type */
    SetStartupType(Info, hwndDlg);

    /* set service status */
    if (Info->pCurrentService->ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING)
    {
        TCHAR szServiceStatus[32];

        LoadString(hInstance,
                   IDS_SERVICES_STARTED,
                   szServiceStatus,
                   _tcslen(szServiceStatus) + 1);

        SendDlgItemMessage(hwndDlg,
                           IDC_SERV_STATUS,
                           WM_SETTEXT,
                           0,
                           (LPARAM)szServiceStatus);
    }
    else
    {
        TCHAR szServiceStatus[32];

        LoadString(hInstance,
                   IDS_SERVICES_STOPPED,
                   szServiceStatus,
                   _tcslen(szServiceStatus) + 1);

        SendDlgItemMessage(hwndDlg,
                           IDC_SERV_STATUS,
                           WM_SETTEXT,
                           0,
                           (LPARAM)szServiceStatus);
    }
}



/*
 * General Property dialog callback.
 * Controls messages to the General dialog
 */
static INT_PTR CALLBACK
GeneralPageProc(HWND hwndDlg,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam)
{
    PMAIN_WND_INFO Info;

    /* Get the window context */
    Info = (PMAIN_WND_INFO)GetWindowLongPtr(hwndDlg,
                                            GWLP_USERDATA);

    if (Info == NULL && uMsg != WM_INITDIALOG)
    {
        return FALSE;
    }

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Info = (PMAIN_WND_INFO)(((LPPROPSHEETPAGE)lParam)->lParam);
            if (Info != NULL)
            {
                SetWindowLongPtr(hwndDlg,
                                 GWLP_USERDATA,
                                 (LONG_PTR)Info);
                GetDlgInfo(Info, hwndDlg);
                SetButtonStates(Info, hwndDlg);
            }
        }
        break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_START_TYPE:
                    /* Enable the 'Apply' button */
                    //PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
                break;

                case IDC_START:
                    SendMessage(Info->hMainWnd, WM_COMMAND, ID_START, 0);
                break;

                case IDC_STOP:
                    SendMessage(Info->hMainWnd, WM_COMMAND, ID_STOP, 0);
                break;

                case IDC_PAUSE:
                    SendMessage(Info->hMainWnd, WM_COMMAND, ID_PAUSE, 0);
                break;

                case IDC_RESUME:
                    SendMessage(Info->hMainWnd, WM_COMMAND, ID_RESUME, 0);
                break;

                case IDC_START_PARAM:
                    /* Enable the 'Apply' button */
                    //PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
                break;

            }
            break;

        case WM_DESTROY:
        break;

        case WM_NOTIFY:
            {
                LPNMHDR lpnm = (LPNMHDR)lParam;

                switch (lpnm->code)
                {
                    case MCN_SELECT:
                        /* Enable the 'Apply' button */
                        PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
                    break;
                }
            }
        break;
    }

    return FALSE;
}



/*
 * Dependancies Property dialog callback.
 * Controls messages to the Dependancies dialog
 */
static INT_PTR CALLBACK
DependanciesPageProc(HWND hwndDlg,
                     UINT uMsg,
                     WPARAM wParam,
                     LPARAM lParam)
{
    PMAIN_WND_INFO Info;

    /* Get the window context */
    Info = (PMAIN_WND_INFO)GetWindowLongPtr(hwndDlg,
                                            GWLP_USERDATA);

    if (Info == NULL && uMsg != WM_INITDIALOG)
    {
        return FALSE;
    }

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            Info = (PMAIN_WND_INFO)(((LPPROPSHEETPAGE)lParam)->lParam);
            if (Info != NULL)
            {
                SetWindowLongPtr(hwndDlg,
                                 GWLP_USERDATA,
                                 (LONG_PTR)Info);
            }
        }
        break;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {

            }
            break;

        case WM_DESTROY:
            break;

        case WM_NOTIFY:
            {
                LPNMHDR lpnm = (LPNMHDR)lParam;

                switch (lpnm->code)
                {

                }

            }
            break;
    }

    return FALSE;
}


static INT CALLBACK
AddEditButton(HWND hwnd, UINT message, LPARAM lParam)
{
    HWND hEditButton;

    switch (message)
    {
        case PSCB_PRECREATE:
            /*hEditButton = CreateWindowEx(0,
                                         WC_BUTTON,
                                         NULL,
                                         WS_CHILD | WS_VISIBLE,
                                         20, 300, 30, 15,
                                         hwnd,
                                         NULL,
                                         hInstance,
                                         NULL);
            if (hEditButton == NULL)
                GetError(0);*/

            hEditButton = GetDlgItem(hwnd, PSBTN_OK);
            DestroyWindow(hEditButton);
            //SetWindowText(hEditButton, _T("test"));

            return TRUE;
    }
    return TRUE;
}


static VOID
InitPropSheetPage(PROPSHEETPAGE *psp,
                  PMAIN_WND_INFO Info,
                  WORD idDlg,
                  DLGPROC DlgProc)
{
  ZeroMemory(psp, sizeof(PROPSHEETPAGE));
  psp->dwSize = sizeof(PROPSHEETPAGE);
  psp->dwFlags = PSP_DEFAULT;
  psp->hInstance = hInstance;
  psp->pszTemplate = MAKEINTRESOURCE(idDlg);
  psp->pfnDlgProc = DlgProc;
  psp->lParam = (LPARAM)Info;
}


LONG APIENTRY
OpenPropSheet(PMAIN_WND_INFO Info)
{
    PROPSHEETHEADER psh;
    PROPSHEETPAGE psp[2];

    ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags =  PSH_PROPSHEETPAGE | PSH_PROPTITLE | PSH_USECALLBACK;
    psh.hwndParent = Info->hMainWnd;
    psh.hInstance = hInstance;
    psh.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SM_ICON));
    psh.pszCaption = Info->pCurrentService->lpDisplayName;
    psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.nStartPage = 0;
    psh.pfnCallback = AddEditButton;
    psh.ppsp = psp;


    InitPropSheetPage(&psp[0], Info, IDD_DLG_GENERAL, GeneralPageProc);
    //InitPropSheetPage(&psp[1], Info, IDD_DLG_GENERAL, LogonPageProc);
    //InitPropSheetPage(&psp[2], Info, IDD_DLG_GENERAL, RecoveryPageProc);
    InitPropSheetPage(&psp[1], Info, IDD_DLG_DEPEND, DependanciesPageProc);

    return (LONG)(PropertySheet(&psh) != -1);
}

