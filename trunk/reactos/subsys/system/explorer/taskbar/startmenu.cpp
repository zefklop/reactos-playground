/*
 * Copyright 2003 Martin Fuchs
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


 //
 // Explorer clone
 //
 // startmenu.cpp
 //
 // Explorer start menu
 //
 // Martin Fuchs, 19.08.2003
 //
 // Credits: Thanks to Everaldo (http://www.everaldo.com) for his nice looking icons.
 //


#include "../utility/utility.h"

#include "../explorer.h"
#include "../globals.h"
#include "../externals.h"
#include "../explorer_intres.h"

#include "desktopbar.h"
#include "startmenu.h"

#include "../dialogs/searchprogram.h"
#include "../dialogs/settings.h"


StartMenu::StartMenu(HWND hwnd)
 :	super(hwnd)
{
	_next_id = IDC_FIRST_MENU;
	_submenu_id = 0;
	_border_left = 0;
	_border_top = 0;
	_last_pos = WindowRect(hwnd).pos();
#ifdef _LIGHT_STARTMENU
	_selected_id = -1;
	_last_mouse_pos = 0;
#endif
}

StartMenu::StartMenu(HWND hwnd, const StartMenuCreateInfo& create_info)
 :	super(hwnd),
	_create_info(create_info)
{
	for(StartMenuFolders::const_iterator it=create_info._folders.begin(); it!=create_info._folders.end(); ++it)
		if (*it)
			_dirs.push_back(ShellDirectory(GetDesktopFolder(), *it, _hwnd));

	_next_id = IDC_FIRST_MENU;
	_submenu_id = 0;
	_border_left = 0;
	_border_top = create_info._border_top;
	_last_pos = WindowRect(hwnd).pos();
#ifdef _LIGHT_STARTMENU
	_selected_id = -1;
	_last_mouse_pos = 0;
#endif
}

StartMenu::~StartMenu()
{
	SendParent(PM_STARTMENU_CLOSED);
}


 // We need this wrapper function for s_wcStartMenu, it calls the WIN32 API,
 // though static C++ initializers are not allowed for Winelib applications.
BtnWindowClass& StartMenu::GetWndClasss()
{
	static BtnWindowClass s_wcStartMenu(CLASSNAME_STARTMENU);

	return s_wcStartMenu;
}


Window::CREATORFUNC_INFO StartMenu::s_def_creator = STARTMENU_CREATOR(StartMenu);

HWND StartMenu::Create(int x, int y, const StartMenuFolders& folders, HWND hwndParent, LPCTSTR title, CREATORFUNC_INFO creator)
{
	UINT style, ex_style;
	int top_height;

	if (hwndParent) {
		style = WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN|WS_VISIBLE;
		ex_style = 0;
		top_height = STARTMENU_TOP_BTN_SPACE;
	} else {
		style = WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN|WS_VISIBLE;
		ex_style = WS_EX_TOOLWINDOW;
		top_height = 0;
	}

	RECT rect = {x, y-STARTMENU_LINE_HEIGHT-top_height, x+STARTMENU_WIDTH_MIN, y};

#ifndef _LIGHT_STARTMENU
	rect.top += STARTMENU_LINE_HEIGHT;
#endif

	AdjustWindowRectEx(&rect, style, FALSE, ex_style);

	StartMenuCreateInfo create_info;

	create_info._folders = folders;
	create_info._border_top = top_height;
	create_info._creator = creator;

	if (title)
		create_info._title = title;

	HWND hwnd = Window::Create(creator, &create_info, ex_style, GetWndClasss(), title,
								style, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, hwndParent);

	 // make sure the window is not off the screen
	MoveVisible(hwnd);

	return hwnd;
}


LRESULT	StartMenu::Init(LPCREATESTRUCT pcs)
{
	try {
		AddEntries();

		if (super::Init(pcs))
			return 1;

		 // create buttons for registered entries in _entries
		for(ShellEntryMap::const_iterator it=_entries.begin(); it!=_entries.end(); ++it) {
			const StartMenuEntry& sme = it->second;
			bool hasSubmenu = false;

			for(ShellEntrySet::const_iterator it=sme._entries.begin(); it!=sme._entries.end(); ++it)
				if ((*it)->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					hasSubmenu = true;

#ifdef _LIGHT_STARTMENU
			_buttons.push_back(SMBtnInfo(sme, it->first, hasSubmenu));
#else
			AddButton(sme._title, sme._hIcon, hasSubmenu, it->first);
#endif
		}

#ifdef _LIGHT_STARTMENU
		if (_buttons.empty())
#else
		if (!GetWindow(_hwnd, GW_CHILD))
#endif
			AddButton(ResString(IDS_EMPTY), ICID_NONE, false, 0, false);

#ifdef _LIGHT_STARTMENU
		ResizeToButtons();
#endif

#ifdef _LAZY_ICONEXTRACT
		PostMessage(_hwnd, PM_UPDATE_ICONS, 0, 0);
#endif
	} catch(COMException& e) {
		HandleException(e, pcs->hwndParent);	// destroys the start menu window while switching focus
	}

	return 0;
}

void StartMenu::AddEntries()
{
	for(StartMenuShellDirs::iterator it=_dirs.begin(); it!=_dirs.end(); ++it) {
		StartMenuDirectory& smd = *it;
		ShellDirectory& dir = smd._dir;

		if (!dir._scanned) {
			WaitCursor wait;

#ifdef _LAZY_ICONEXTRACT
			dir.smart_scan(SCAN_FILESYSTEM);	// lazy icon extraction, try to read directly from filesystem
#else
			dir.smart_scan(SCAN_EXTRACT_ICONS|SCAN_FILESYSTEM);
#endif
		}

		AddShellEntries(dir, -1, smd._subfolders);
	}
}


void StartMenu::AddShellEntries(const ShellDirectory& dir, int max, bool subfolders)
{
	int cnt = 0;

	for(Entry*entry=dir._down; entry; entry=entry->_next) {
		 // hide files like "desktop.ini"
		if (entry->_shell_attribs & SFGAO_HIDDEN)
		//not appropriate for drive roots: if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
			continue;

		 // hide subfolders if requested
		if (!subfolders)
			if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

		 // only 'max' entries shall be added.
		if (++cnt == max)
			break;

		if (entry->_etype == ET_SHELL)
			AddEntry(dir._folder, static_cast<ShellEntry*>(entry));
		else
			AddEntry(dir._folder, entry);
	}
}


LRESULT StartMenu::WndProc(UINT nmsg, WPARAM wparam, LPARAM lparam)
{
	switch(nmsg) {
	  case WM_PAINT: {
		PaintCanvas canvas(_hwnd);
		Paint(canvas);
		break;}

	  case WM_SIZE:
		ResizeButtons(LOWORD(lparam)-_border_left);
		break;

	  case WM_MOVE: {
		POINTS& pos = MAKEPOINTS(lparam);

		 // move open submenus of floating menus
		if (_submenu) {
			int dx = pos.x - _last_pos.x;
			int dy = pos.y - _last_pos.y;

			if (dx || dy) {
				WindowRect rt(_submenu);
				SetWindowPos(_submenu, 0, rt.left+dx, rt.top+dy, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE);
				//MoveVisible(_submenu);
			}
		}

		_last_pos.x = pos.x;
		_last_pos.y = pos.y;
		goto def;}

	  case WM_NCHITTEST: {
		LRESULT res = super::WndProc(nmsg, wparam, lparam);

		if (res>=HTSIZEFIRST && res<=HTSIZELAST)
			return HTCLIENT;	// disable window resizing

		return res;}

	  case WM_LBUTTONDOWN: {
		RECT rect;

		 // check mouse cursor for coordinates of floating button
		GetFloatingButtonRect(&rect);

		if (PtInRect(&rect, Point(lparam))) {
			 // create a floating copy of the current start menu
 			WindowRect pos(_hwnd);

			///@todo do something similar to StartMenuRoot::TrackStartmenu() in order to automatically close submenus when clicking on the desktop background
			StartMenu::Create(pos.left+3, pos.bottom-3, _create_info._folders, 0, _create_info._title, _create_info._creator);
			CloseStartMenu();
		}

#ifdef _LIGHT_STARTMENU
		int id = ButtonHitTest(Point(lparam));

		if (id)
			Command(id, BN_CLICKED);
#endif
		break;}

	  case WM_SYSCOMMAND:
		if ((wparam&0xFFF0) == SC_SIZE)
			return 0;			// disable window resizing
		goto def;

	  case WM_ACTIVATEAPP:
		 // close start menu when activating another application
		if (!wparam)
			CloseStartMenu();
		break;	// don't call super::WndProc in case "this" has been deleted

	  case WM_CANCELMODE:
		CloseStartMenu();
		break;

#ifdef _LIGHT_STARTMENU
	  case WM_MOUSEMOVE: {
		 // automatically set the focus to startmenu entries when moving the mouse over them
		if (lparam != _last_mouse_pos) { // don't process WM_MOUSEMOVE when opening submenus using keyboard navigation
			int new_id = ButtonHitTest(Point(lparam));

			if (new_id != _selected_id)
				SelectButton(new_id);

			_last_mouse_pos = lparam;
		}
		break;}

	  case WM_KEYDOWN:
		ProcessKey(wparam);
		break;
#else
	  case PM_STARTENTRY_FOCUSED: {	///@todo use TrackMouseEvent() and WM_MOUSEHOVER to wait a bit before opening submenus
		BOOL hasSubmenu = wparam;
		HWND hctrl = (HWND)lparam;

		 // automatically open submenus
		if (hasSubmenu) {
			UpdateWindow(_hwnd);	// draw focused button before waiting on submenu creation
			//SendMessage(_hwnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hctrl),BN_CLICKED), (LPARAM)hctrl);
			Command(GetDlgCtrlID(hctrl), BN_CLICKED);
		} else {
			 // close any open submenu
			CloseOtherSubmenus();
		}
		break;}
#endif

#ifdef _LAZY_ICONEXTRACT
	  case PM_UPDATE_ICONS:
		UpdateIcons(/*wparam*/);
		break;
#endif

	  case PM_STARTENTRY_LAUNCHED:
		if (GetWindowStyle(_hwnd) & WS_CAPTION)	// don't automatically close floating menus
			return 0;

		 // route message to the parent menu and close menus after launching an entry
		if (!SendParent(nmsg, wparam, lparam))
			CloseStartMenu();
		return 1;	// signal that we have received and processed the message

	  case PM_STARTMENU_CLOSED:
		_submenu = 0;
		break;

	  case PM_SELECT_ENTRY:
		SelectButtonIndex(0, wparam?true:false);
		break;

	  default: def:
		return super::WndProc(nmsg, wparam, lparam);
	}

	return 0;
}


#ifdef _LIGHT_STARTMENU

int StartMenu::ButtonHitTest(POINT pt)
{
	ClientRect clnt(_hwnd);
	RECT rect = {_border_left, _border_top, clnt.right, STARTMENU_LINE_HEIGHT};

	if (pt.x<rect.left || pt.x>rect.right)
		return 0;

	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
		const SMBtnInfo& info = *it;

		if (rect.top > pt.y)
			break;

		rect.bottom = rect.top + (info._id==-1? STARTMENU_SEP_HEIGHT: STARTMENU_LINE_HEIGHT);

		if (pt.y < rect.bottom)	// PtInRect(&rect, pt)
			return info._id;

		rect.top = rect.bottom;
	}

	return 0;
}

void StartMenu::InvalidateSelection()
{
	if (!_selected_id)
		return;

	ClientRect clnt(_hwnd);
	RECT rect = {_border_left, _border_top, clnt.right, STARTMENU_LINE_HEIGHT};

	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
		const SMBtnInfo& info = *it;

		rect.bottom = rect.top + (info._id==-1? STARTMENU_SEP_HEIGHT: STARTMENU_LINE_HEIGHT);

		if (info._id == _selected_id) {
			InvalidateRect(_hwnd, &rect, TRUE);
			break;
		}

		rect.top = rect.bottom;
	}
}

const SMBtnInfo* StartMenu::GetButtonInfo(int id) const
{
	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it)
		if (it->_id == id)
			return &*it;

	return NULL;
}

bool StartMenu::SelectButton(int id, bool open_sub)
{
	if (id == -1)
		return false;

	if (id == _selected_id)
		return true;

	InvalidateSelection();

	const SMBtnInfo* btn = GetButtonInfo(id);

	if (btn && btn->_enabled) {
		_selected_id = id;

		InvalidateSelection();

		 // automatically open submenus
		if (btn->_hasSubmenu) {
			if (open_sub)
				OpenSubmenu();
		} else
			CloseOtherSubmenus();	// close any open submenu

		return true;
	} else {
		_selected_id = -1;
		return false;
	}
}

bool StartMenu::OpenSubmenu(bool select_first)
{
	if (_selected_id == -1)
		return false;

	InvalidateSelection();

	const SMBtnInfo* btn = GetButtonInfo(_selected_id);

	 // automatically open submenus
	if (btn->_hasSubmenu) {
		//@@ allows destroying of startmenu when processing PM_UPDATE_ICONS -> GPF
		UpdateWindow(_hwnd);	// draw focused button before waiting on submenu creation
		Command(_selected_id, BN_CLICKED);

		if (select_first && _submenu)
			SendMessage(_submenu, PM_SELECT_ENTRY, (WPARAM)false, 0);

		return true;
	} else
		return false;
}


int StartMenu::GetSelectionIndex()
{
	if (_selected_id == -1)
		return -1;

	for(int i=0; i<(int)_buttons.size(); ++i)
		if (_buttons[i]._id == _selected_id)
			return i;

	return -1;
}

bool StartMenu::SelectButtonIndex(int idx, bool open_sub)
{
	if (idx>=0 && idx<(int)_buttons.size())
		return SelectButton(_buttons[idx]._id, open_sub);
	else
		return false;
}

void StartMenu::ProcessKey(int vk)
{
	switch(vk) {
	  case VK_RETURN:
		if (_selected_id)
			Command(_selected_id, BN_CLICKED);
		break;

	  case VK_UP:
		Navigate(-1);
		break;

	  case VK_DOWN:
		Navigate(+1);
		break;

	  case VK_HOME:
		SelectButtonIndex(0, false);
		break;

	  case VK_END:
		SelectButtonIndex(_buttons.size()-1, false);
		break;

	  case VK_LEFT:
		if (_submenu)
			CloseOtherSubmenus();
		else if (!(GetWindowStyle(_hwnd) & WS_CAPTION))	// don't automatically close floating menus
			DestroyWindow(_hwnd);
		break;

	  case VK_RIGHT:
		OpenSubmenu(true);
		break;

	  case VK_ESCAPE:
		CloseStartMenu();
		break;

	  default:
		if (vk>='0' && vk<='Z')
			JumpToNextShortcut(vk);
	}
}

bool StartMenu::Navigate(int step)
{
	int idx = GetSelectionIndex();

	if (idx == -1)
		if (step > 0)
			idx = 0 - step;
		else
			idx = _buttons.size() - step;

	for(;;) {
		idx += step;

		if (idx<0 || idx>(int)_buttons.size())
			break;

		if (SelectButtonIndex(idx, false))
			return true;
	}

	return false;
}

bool StartMenu::JumpToNextShortcut(char c)
{
	int cur_idx = GetSelectionIndex();

	if (cur_idx == -1)
		cur_idx = 0;

	int first_found = 0;
	int found_more = 0;

	SMBtnVector::const_iterator cur_it = _buttons.begin();
	cur_it += cur_idx + 1;

	 // first search down from current item...
	SMBtnVector::const_iterator it = cur_it;
	for(; it!=_buttons.end(); ++it) {
		const SMBtnInfo& btn = *it;

		if (!btn._title.empty() && toupper((TBYTE)btn._title.at(0)) == c) {
			if (!first_found)
				first_found = btn._id;
			else
				++found_more;
		}
	}

	 // ...now search from top to the current item
	it = _buttons.begin();
	for(; it!=_buttons.end() && it!=cur_it; ++it) {
		const SMBtnInfo& btn = *it;

		if (!btn._title.empty() && toupper((TBYTE)btn._title.at(0)) == c) {
			if (!first_found)
				first_found = btn._id;
			else
				++found_more;
		}
	}

	if (first_found) {
		SelectButton(first_found);

		if (!found_more)
			Command(first_found, BN_CLICKED);

		return true;
	} else
		return false;
}

#endif // _LIGHT_STARTMENU


bool StartMenu::GetButtonRect(int id, PRECT prect) const
{
#ifdef _LIGHT_STARTMENU
	ClientRect clnt(_hwnd);
	RECT rect = {_border_left, _border_top, clnt.right, STARTMENU_LINE_HEIGHT};

	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
		const SMBtnInfo& info = *it;

		rect.bottom = rect.top + (info._id==-1? STARTMENU_SEP_HEIGHT: STARTMENU_LINE_HEIGHT);

		if (info._id == id) {
			*prect = rect;
			return true;
		}

		rect.top = rect.bottom;
	}

	return false;
#else
	HWND btn = GetDlgItem(_hwnd, id);

	if (btn) {
		GetWindowRect(btn, prect);
		ScreenToClient(_hwnd, prect);

		return true;
	} else
		return false;
#endif
}


void StartMenu::DrawFloatingButton(HDC hdc)
{
	static ResIconEx floatingIcon(IDI_FLOATING, 8, 4);

	ClientRect clnt(_hwnd);

	DrawIconEx(hdc, clnt.right-12, 0, floatingIcon, 8, 4, 0, 0, DI_NORMAL);
}

void StartMenu::GetFloatingButtonRect(LPRECT prect)
{
	GetClientRect(_hwnd, prect);

	prect->right -= 4;
	prect->left = prect->right - 8;
	prect->bottom = 4;
}


void StartMenu::Paint(PaintCanvas& canvas)
{
	if (_border_top)
		DrawFloatingButton(canvas);

#ifdef _LIGHT_STARTMENU
	ClientRect clnt(_hwnd);
	RECT rect = {_border_left, _border_top, clnt.right, STARTMENU_LINE_HEIGHT};

	int sep_width = rect.right-rect.left - 4;

	FontSelection font(canvas, GetStockFont(DEFAULT_GUI_FONT));
	BkMode bk_mode(canvas, TRANSPARENT);

	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
		const SMBtnInfo& btn = *it;

		if (rect.top > canvas.rcPaint.bottom)
			break;

		if (btn._id == -1) {	// a separator?
			rect.bottom = rect.top + STARTMENU_SEP_HEIGHT;

			BrushSelection brush_sel(canvas, GetSysColorBrush(COLOR_BTNSHADOW));
			PatBlt(canvas, rect.left+2, rect.top+STARTMENU_SEP_HEIGHT/2-1, sep_width, 1, PATCOPY);

			SelectBrush(canvas, GetSysColorBrush(COLOR_BTNHIGHLIGHT));
			PatBlt(canvas, rect.left+2, rect.top+STARTMENU_SEP_HEIGHT/2, sep_width, 1, PATCOPY);
		} else {
			rect.bottom = rect.top + STARTMENU_LINE_HEIGHT;

			if (rect.top >= canvas.rcPaint.top)
				DrawStartMenuButton(canvas, rect, btn._title, btn, btn._id==_selected_id, false);
		}

		rect.top = rect.bottom;
	}
#endif
}

#ifdef _LAZY_ICONEXTRACT
void StartMenu::UpdateIcons(/*int idx*/)
{
	UpdateWindow(_hwnd);

#ifdef _SINGLE_ICONEXTRACT

	//if (idx >= 0)
	int idx = 0;

	for(; idx<(int)_buttons.size(); ++idx) {
		SMBtnInfo& btn = _buttons[idx];

		if (btn._icon_id==ICID_UNKNOWN && btn._id>0) {
			StartMenuEntry& sme = _entries[btn._id];

			btn._icon_id = ICID_NONE;

			for(ShellEntrySet::iterator it=sme._entries.begin(); it!=sme._entries.end(); ++it) {
				Entry* entry = *it;

				if (entry->_icon_id == ICID_UNKNOWN)
					entry->extract_icon();

				if (entry->_icon_id > ICID_NONE) {
					btn._icon_id = (ICON_ID)/*@@*/ entry->_icon_id;

					RECT rect;

					GetButtonRect(btn._id, &rect);
					WindowCanvas canvas(_hwnd);
					DrawStartMenuButton(canvas, rect, NULL, btn, btn._id==_selected_id, false);

					//InvalidateRect(_hwnd, &rect, FALSE);
					//UpdateWindow(_hwnd);
					//break;

					break;
				}
			}
		}
	}

//	if (++idx < (int)_buttons.size())
//		PostMessage(_hwnd, PM_UPDATE_ICONS, idx, 0);

#else

	int icons_extracted = 0;
	int icons_updated = 0;

	for(StartMenuShellDirs::iterator it=_dirs.begin(); it!=_dirs.end(); ++it) {
		ShellDirectory& dir = it->_dir;

		icons_extracted += dir.extract_icons();
	}

	if (icons_extracted) {
		for(ShellEntryMap::iterator it1=_entries.begin(); it1!=_entries.end(); ++it1) {
			StartMenuEntry& sme = it1->second;

			if (!sme._hIcon) {
				sme._hIcon = (HICON)-1;

				for(ShellEntrySet::const_iterator it2=sme._entries.begin(); it2!=sme._entries.end(); ++it2) {
					const Entry* sm_entry = *it2;

					if (sm_entry->_hIcon) {
						sme._hIcon = sm_entry->_hIcon;
						break;
					}
				}
			}
		}

		for(SMBtnVector::iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
			SMBtnInfo& info = *it;

			if (info._id>0 && !info._hIcon) {
				info._hIcon = _entries[info._id]._hIcon;
				++icons_updated;
			}
		}
	}

	if (icons_updated) {
		InvalidateRect(_hwnd, NULL, FALSE);
		UpdateWindow(_hwnd);
	}
#endif
}
#endif


 // resize child button controls to accomodate for new window size
void StartMenu::ResizeButtons(int cx)
{
	HDWP hdwp = BeginDeferWindowPos(10);

	for(HWND ctrl=GetWindow(_hwnd,GW_CHILD); ctrl; ctrl=GetNextWindow(ctrl,GW_HWNDNEXT)) {
		ClientRect rt(ctrl);

		if (rt.right != cx) {
			int height = rt.bottom - rt.top;

			 // special handling for separator controls
			if (!height && (GetWindowStyle(ctrl)&SS_TYPEMASK)==SS_ETCHEDHORZ)
				height = 2;

			hdwp = DeferWindowPos(hdwp, ctrl, 0, 0, 0, cx, height, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
		}
	}

	EndDeferWindowPos(hdwp);
}


int StartMenu::Command(int id, int code)
{
#ifndef _LIGHT_STARTMENU
	switch(id) {
	  case IDCANCEL:
		CloseStartMenu(id);
		break;

	  default: {
#endif
		ShellEntryMap::iterator found = _entries.find(id);

		if (found != _entries.end()) {
			ActivateEntry(id, found->second._entries);
			return 0;
		}

		return super::Command(id, code);
#ifndef _LIGHT_STARTMENU
	  }
	}

	return 0;
#endif
}


StartMenuEntry& StartMenu::AddEntry(const String& title, ICON_ID icon_id, Entry* entry)
{
	 // search for an already existing subdirectory entry with the same name
	if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		for(ShellEntryMap::iterator it=_entries.begin(); it!=_entries.end(); ++it) {
			StartMenuEntry& sme = it->second;

			if (sme._title == title)	///@todo speed up by using a map indexed by name
				for(ShellEntrySet::iterator it2=sme._entries.begin(); it2!=sme._entries.end(); ++it2) {
					if ((*it2)->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
						 // merge the new shell entry with the existing of the same name
						sme._entries.insert(entry);
						return sme;
					}
				}
		}

	StartMenuEntry& sme = AddEntry(title, icon_id);

	sme._entries.insert(entry);

	return sme;
}

StartMenuEntry& StartMenu::AddEntry(const String& title, ICON_ID icon_id, int id)
{
	if (id == -1)
		id = ++_next_id;

	StartMenuEntry& sme = _entries[id];

	sme._title = title;
	sme._icon_id = icon_id;

	return sme;
}

StartMenuEntry& StartMenu::AddEntry(const ShellFolder folder, ShellEntry* entry)
{
	ICON_ID icon_id;

	if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		icon_id = ICID_FOLDER;
	else
		icon_id = (ICON_ID)/*@@*/ entry->_icon_id;

	return AddEntry(folder.get_name(entry->_pidl), icon_id, entry);
}

StartMenuEntry& StartMenu::AddEntry(const ShellFolder folder, Entry* entry)
{
	ICON_ID icon_id;

	if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		icon_id = ICID_FOLDER;
	else
		icon_id = (ICON_ID)/*@@*/ entry->_icon_id;

	return AddEntry(entry->_display_name, icon_id, entry);
}


void StartMenu::AddButton(LPCTSTR title, ICON_ID icon_id, bool hasSubmenu, int id, bool enabled)
{
#ifdef _LIGHT_STARTMENU
	_buttons.push_back(SMBtnInfo(title, icon_id, id, hasSubmenu, enabled));
#else
	DWORD style = enabled? WS_VISIBLE|WS_CHILD|BS_OWNERDRAW: WS_VISIBLE|WS_CHILD|BS_OWNERDRAW|WS_DISABLED;

	WindowRect rect(_hwnd);
	ClientRect clnt(_hwnd);

	 // increase window height to make room for the new button
	rect.top -= STARTMENU_LINE_HEIGHT;

	 // move down if we are too high now
	if (rect.top < 0) {
		rect.top += STARTMENU_LINE_HEIGHT;
		rect.bottom += STARTMENU_LINE_HEIGHT;
	}

	WindowCanvas canvas(_hwnd);
	FontSelection font(canvas, GetStockFont(DEFAULT_GUI_FONT));

	 // widen window, if it is too small
	int text_width = GetStartMenuBtnTextWidth(canvas, title, _hwnd) + 16/*icon*/ + 10/*placeholder*/ + 16/*arrow*/;

	int cx = clnt.right - _border_left;
	if (text_width > cx)
		rect.right += text_width-cx;

	MoveWindow(_hwnd, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);

	StartMenuCtrl(_hwnd, _border_left, clnt.bottom, rect.right-rect.left-_border_left,
					title, id, g_Globals._icon_cache.get_icon(icon_id)._hIcon, hasSubmenu, style);
#endif
}

void StartMenu::AddSeparator()
{
#ifdef _LIGHT_STARTMENU
	_buttons.push_back(SMBtnInfo(NULL, ICID_NONE, -1, false));
#else
	WindowRect rect(_hwnd);
	ClientRect clnt(_hwnd);

	 // increase window height to make room for the new separator
	rect.top -= STARTMENU_SEP_HEIGHT;

	 // move down if we are too high now
	if (rect.top < 0) {
		rect.top += STARTMENU_LINE_HEIGHT;
		rect.bottom += STARTMENU_LINE_HEIGHT;
	}

	MoveWindow(_hwnd, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);

	StartMenuSeparator(_hwnd, _border_left, clnt.bottom, rect.right-rect.left-_border_left);
#endif
}


bool StartMenu::CloseOtherSubmenus(int id)
{
	if (_submenu) {
		if (IsWindow(_submenu)) {
			if (_submenu_id == id)
				return false;
			else {
				_submenu_id = 0;
				DestroyWindow(_submenu);
				// _submenu should be reset automatically by PM_STARTMENU_CLOSED, but safety first...
			}
		}

		_submenu = 0;
	}

	return true;
}


void StartMenu::CreateSubmenu(int id, LPCTSTR title, CREATORFUNC_INFO creator)
{
	CreateSubmenu(id, StartMenuFolders(), title, creator);
}

bool StartMenu::CreateSubmenu(int id, int folder_id, LPCTSTR title, CREATORFUNC_INFO creator)
{
	try {
		SpecialFolderPath folder(folder_id, _hwnd);

		StartMenuFolders new_folders;
		new_folders.push_back(folder);

		CreateSubmenu(id, new_folders, title, creator);

		return true;
	} catch(COMException&) {
		// ignore Exception and don't display anything
		CloseOtherSubmenus(id);
		_buttons[GetSelectionIndex()]._enabled = false;	// disable entries for non-existing folders
		return false;
	}
}

bool StartMenu::CreateSubmenu(int id, int folder_id1, int folder_id2, LPCTSTR title, CREATORFUNC_INFO creator)
{
	StartMenuFolders new_folders;

	try {
		new_folders.push_back(SpecialFolderPath(folder_id1, _hwnd));
	} catch(COMException&) {
	}

	try {
		new_folders.push_back(SpecialFolderPath(folder_id2, _hwnd));
	} catch(COMException&) {
	}

	if (!new_folders.empty()) {
		CreateSubmenu(id, new_folders, title, creator);
		return true;
	} else {
		CloseOtherSubmenus(id);
		_buttons[GetSelectionIndex()]._enabled = false;	// disable entries for non-existing folders
		return false;
	}
}

void StartMenu::CreateSubmenu(int id, const StartMenuFolders& new_folders, LPCTSTR title, CREATORFUNC_INFO creator)
{
	 // Only open one submenu at a time.
	if (!CloseOtherSubmenus(id))
		return;

	RECT rect;
	int x, y;

	if (GetButtonRect(id, &rect)) {
		ClientToScreen(_hwnd, &rect);

		x = rect.right;	// Submenus should overlap their parent a bit.
		y = rect.top+STARTMENU_LINE_HEIGHT-_border_top;
	} else {
		WindowRect pos(_hwnd);

		x = pos.right;
		y = pos.top;
	}

	_submenu_id = id;
	_submenu = StartMenu::Create(x, y, new_folders, _hwnd, title, creator);
}


void StartMenu::ActivateEntry(int id, const ShellEntrySet& entries)
{
	StartMenuFolders new_folders;
	String title;

	for(ShellEntrySet::const_iterator it=entries.begin(); it!=entries.end(); ++it) {
		Entry* entry = const_cast<Entry*>(*it);

		if (entry->_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

			///@todo If the user explicitly clicked on a submenu, display this folder as floating start menu.

			if (entry->_etype == ET_SHELL)
				new_folders.push_back(entry->create_absolute_pidl());
			else {
				TCHAR path[MAX_PATH];

				if (entry->get_path(path))
					new_folders.push_back(path);
			}

			if (title.empty())
				title = entry->_display_name;
		} else {
			 // If the entry is no subdirectory, there can only be one shell entry.
			assert(entries.size()==1);

			HWND hparent = GetParent(_hwnd);
			ShellPath shell_path = entry->create_absolute_pidl();

			 // close start menus after launching the selected entry
			CloseStartMenu(id);

			///@todo launch in the background; specify correct HWND for error message box titles
			SHELLEXECUTEINFO shexinfo;

			shexinfo.cbSize = sizeof(SHELLEXECUTEINFO);
			shexinfo.fMask = SEE_MASK_INVOKEIDLIST;	// SEE_MASK_IDLIST is also possible.
			shexinfo.hwnd = hparent;
			shexinfo.lpVerb = NULL;
			shexinfo.lpFile = NULL;
			shexinfo.lpParameters = NULL;
			shexinfo.lpDirectory = NULL;
			shexinfo.nShow = SW_SHOWNORMAL;

			shexinfo.lpIDList = &*shell_path;

			 // add PIDL to the recent file list
			SHAddToRecentDocs(SHARD_PIDL, shexinfo.lpIDList);

			if (!ShellExecuteEx(&shexinfo))
				display_error(hparent, GetLastError());

			 // we may have deleted 'this' - ensure we leave the loop and function
			return;
		}
	}

	if (!new_folders.empty()) {
		 // Only open one submenu at a time.
		if (!CloseOtherSubmenus(id))
			return;

		CreateSubmenu(id, new_folders, title);
	}
}


 /// close all windows of the start menu popup
void StartMenu::CloseStartMenu(int id)
{
	if (!(GetWindowStyle(_hwnd) & WS_CAPTION)) {	// don't automatically close floating menus
		if (!SendParent(PM_STARTENTRY_LAUNCHED, id, (LPARAM)_hwnd))
			DestroyWindow(_hwnd);
	} else if (_submenu)	// instead close submenus of floating parent menus
		CloseSubmenus();
}


int GetStartMenuBtnTextWidth(HDC hdc, LPCTSTR title, HWND hwnd)
{
	RECT rect = {0, 0, 0, 0};
	DrawText(hdc, title, -1, &rect, DT_SINGLELINE|DT_NOPREFIX|DT_CALCRECT);

	return rect.right-rect.left;
}

#ifdef _LIGHT_STARTMENU
void DrawStartMenuButton(HDC hdc, const RECT& rect, LPCTSTR title, const SMBtnInfo& btn, bool has_focus, bool pushed)
#else
void DrawStartMenuButton(HDC hdc, const RECT& rect, LPCTSTR title, HICON hIcon,
								bool hasSubmenu, bool enabled, bool has_focus, bool pushed);
#endif
{
	UINT style = DFCS_BUTTONPUSH;

	if (!btn._enabled)
		style |= DFCS_INACTIVE;

	POINT iconPos = {rect.left+2, (rect.top+rect.bottom-16)/2};
	RECT textRect = {rect.left+16+4, rect.top+2, rect.right-4, rect.bottom-4};

	if (pushed) {
		style |= DFCS_PUSHED;
		++iconPos.x;		++iconPos.y;
		++textRect.left;	++textRect.top;
		++textRect.right;	++textRect.bottom;
	}

	int bk_color_idx = COLOR_BTNFACE;
	int text_color_idx = COLOR_BTNTEXT;

	if (has_focus) {
		bk_color_idx = COLOR_HIGHLIGHT;
		text_color_idx = COLOR_HIGHLIGHTTEXT;
	}

	COLORREF bk_color = GetSysColor(bk_color_idx);
	HBRUSH bk_brush = GetSysColorBrush(bk_color_idx);

	if (title)
		FillRect(hdc, &rect, bk_brush);

	if (btn._icon_id > ICID_NONE)
		g_Globals._icon_cache.get_icon(btn._icon_id).draw(hdc, iconPos.x, iconPos.y, 16, 16, bk_color, bk_brush);

	 // draw submenu arrow at the right
	if (btn._hasSubmenu) {
		static SmallIcon arrowIcon(IDI_ARROW);
		static SmallIcon selArrowIcon(IDI_ARROW_SELECTED);

		DrawIconEx(hdc, rect.right-16, iconPos.y,
					has_focus? selArrowIcon: arrowIcon,
					16, 16, 0, bk_brush, DI_NORMAL);
	}

	if (title) {
		BkMode bk_mode(hdc, TRANSPARENT);

		if (!btn._enabled)	// dis->itemState & (ODS_DISABLED|ODS_GRAYED)
			DrawGrayText(hdc, &textRect, title, DT_SINGLELINE|DT_NOPREFIX|DT_VCENTER);
		else {
			TextColor lcColor(hdc, GetSysColor(text_color_idx));
			DrawText(hdc, title, -1, &textRect, DT_SINGLELINE|DT_NOPREFIX|DT_VCENTER);
		}
	}
}


#ifdef _LIGHT_STARTMENU

void StartMenu::ResizeToButtons()
{
	WindowRect rect(_hwnd);

	WindowCanvas canvas(_hwnd);
	FontSelection font(canvas, GetStockFont(DEFAULT_GUI_FONT));

	int max_width = STARTMENU_WIDTH_MIN;
	int height = 0;

	for(SMBtnVector::const_iterator it=_buttons.begin(); it!=_buttons.end(); ++it) {
		int w = GetStartMenuBtnTextWidth(canvas, it->_title, _hwnd);

		if (w > max_width)
			max_width = w;

		if (it->_id == -1)
			height += STARTMENU_SEP_HEIGHT;
		else
			height += STARTMENU_LINE_HEIGHT;
	}

	 // calculate new window size
	int text_width = max_width + 16/*icon*/ + 10/*placeholder*/ + 16/*arrow*/;

	RECT rt_hgt = {rect.left, rect.bottom-_border_top-height, rect.left+_border_left+text_width, rect.bottom};
	AdjustWindowRectEx(&rt_hgt, GetWindowStyle(_hwnd), FALSE, GetWindowExStyle(_hwnd));

	 // ignore movement, only look at the size change
	rect.right = rect.left + (rt_hgt.right-rt_hgt.left);
	rect.top = rect.bottom - (rt_hgt.bottom-rt_hgt.top);

	 // move down if we are too high
	if (rect.top < 0) {
		rect.top += STARTMENU_LINE_HEIGHT;
		rect.bottom += STARTMENU_LINE_HEIGHT;
	}

	MoveWindow(_hwnd, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, TRUE);
}

#else // _LIGHT_STARTMENU

LRESULT StartMenuButton::WndProc(UINT nmsg, WPARAM wparam, LPARAM lparam)
{
	switch(nmsg) {
	  case WM_MOUSEMOVE:
		 // automatically set the focus to startmenu entries when moving the mouse over them
		if (GetFocus()!=_hwnd && !(GetWindowStyle(_hwnd)&WS_DISABLED))
			SetFocus(_hwnd);
		break;

	  case WM_SETFOCUS:
		PostParent(PM_STARTENTRY_FOCUSED, _hasSubmenu, (LPARAM)_hwnd);
		goto def;

	  case WM_CANCELMODE:
		 // route WM_CANCELMODE to the startmenu window
		return SendParent(nmsg, wparam, lparam);

	  default: def:
		return super::WndProc(nmsg, wparam, lparam);
	}

	return 0;
}

void StartMenuButton::DrawItem(LPDRAWITEMSTRUCT dis)
{
	TCHAR title[BUFFER_LEN];

	GetWindowText(_hwnd, title, BUFFER_LEN);

	DrawStartMenuButton(dis->hDC, dis->rcItem, title, _hIcon,
						_hasSubmenu,
						!(dis->itemState & ODS_DISABLED),
						dis->itemState&ODS_FOCUS? true: false,
						dis->itemState&ODS_SELECTED? true: false);
}

#endif


StartMenuRoot::StartMenuRoot(HWND hwnd)
 :	super(hwnd)
{
#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NOCOMMONGROUPS))
#endif
		try {
			 // insert directory "All Users\Start Menu"
			ShellDirectory cmn_startmenu(GetDesktopFolder(), SpecialFolderPath(CSIDL_COMMON_STARTMENU, _hwnd), _hwnd);
			_dirs.push_back(StartMenuDirectory(cmn_startmenu, false));	// don't add subfolders
		} catch(COMException&) {
			// ignore exception and don't show additional shortcuts
		}

	try {
		 // insert directory "<user name>\Start Menu"

		ShellDirectory usr_startmenu(GetDesktopFolder(), SpecialFolderPath(CSIDL_STARTMENU, _hwnd), _hwnd);
		_dirs.push_back(StartMenuDirectory(usr_startmenu, false));	// don't add subfolders
	} catch(COMException&) {
		// ignore exception and don't show additional shortcuts
	}

	 // read size of logo bitmap
	BITMAP bmp_hdr;
	GetObject(ResBitmap(IDB_LOGOV), sizeof(BITMAP), &bmp_hdr);
	_logo_size.cx = bmp_hdr.bmWidth;
	_logo_size.cy = bmp_hdr.bmHeight;

	_border_left = _logo_size.cx + 1;
}


HWND StartMenuRoot::Create(HWND hwndOwner)
{
	WindowRect pos(hwndOwner);

	RECT rect = {pos.left, pos.top-STARTMENU_LINE_HEIGHT-4, pos.left+STARTMENU_WIDTH_MIN, pos.top};

#ifndef _LIGHT_STARTMENU
	rect.top += STARTMENU_LINE_HEIGHT;
#endif

	AdjustWindowRectEx(&rect, WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN|WS_VISIBLE, FALSE, 0);

	return Window::Create(WINDOW_CREATOR(StartMenuRoot), 0, GetWndClasss(), TITLE_STARTMENU,
							WS_POPUP|WS_THICKFRAME|WS_CLIPCHILDREN,
							rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, hwndOwner);
}


void StartMenuRoot::TrackStartmenu()
{
	MSG msg;
	HWND hwnd = _hwnd;

#ifdef _LIGHT_STARTMENU
	_selected_id = -1;
#endif

	 // show previously hidden start menu
	ShowWindow(hwnd, SW_SHOW);
	SetForegroundWindow(hwnd);

	while(IsWindow(hwnd)) {
		if (!GetMessage(&msg, 0, 0, 0)) {
			PostQuitMessage(msg.wParam);
			break;
		}

		 // Check for a mouse click on any window, which is not part of the start menu
		if (msg.message==WM_LBUTTONDOWN || msg.message==WM_MBUTTONDOWN || msg.message==WM_RBUTTONDOWN) {
			StartMenu* menu_wnd = NULL;

			for(HWND hwnd=msg.hwnd; hwnd; hwnd=GetParent(hwnd)) {
				menu_wnd = WINDOW_DYNAMIC_CAST(StartMenu, hwnd);

				if (menu_wnd)
					break;
			}

			if (!menu_wnd) {
				CloseStartMenu();
				break;
			}
		}

		try {
			if (pretranslate_msg(&msg))
				continue;

			if (dispatch_dialog_msg(&msg))
				continue;

			TranslateMessage(&msg);

			try {
				DispatchMessage(&msg);
			} catch(COMException& e) {
				HandleException(e, _hwnd);
			}
		} catch(COMException& e) {
			HandleException(e, _hwnd);
		}
	}
}


LRESULT	StartMenuRoot::Init(LPCREATESTRUCT pcs)
{
	 // add buttons for entries in _entries
	if (super::Init(pcs))
		return 1;

	AddSeparator();


#ifdef __MINGW32__
	HKEY hkey, hkeyAdv;
	DWORD value, len;

	if (RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer"), &hkey))
		hkey = 0;

	if (RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"), &hkeyAdv))
		hkeyAdv = 0;

#define	IS_VALUE_ZERO(hk, name) \
	(!hk || (len=sizeof(value),RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE)&value, &len) || !value))

#define	IS_VALUE_NOT_ZERO(hk, name) \
	(!hk || (len=sizeof(value),RegQueryValueEx(hk, name, NULL, NULL, (LPBYTE)&value, &len) || value>0))
#endif


	 // insert hard coded start entries
	AddButton(ResString(IDS_PROGRAMS),		ICID_APPS, true, IDC_PROGRAMS);

	AddButton(ResString(IDS_DOCUMENTS),		ICID_DOCUMENTS, true, IDC_DOCUMENTS);

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NORECENTDOCSMENU))
#else
	if (IS_VALUE_ZERO(hkey, _T("NoRecentDocsMenu")))
#endif
		AddButton(ResString(IDS_RECENT),	ICID_DOCUMENTS, true, IDC_RECENT);

	AddButton(ResString(IDS_FAVORITES),		ICID_FAVORITES, true, IDC_FAVORITES);

	AddButton(ResString(IDS_SETTINGS),		ICID_CONFIG, true, IDC_SETTINGS);

	AddButton(ResString(IDS_BROWSE),		ICID_FOLDER, true, IDC_BROWSE);

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NOFIND))
#else
	if (IS_VALUE_ZERO(hkey, _T("NoFind")))
#endif
		AddButton(ResString(IDS_SEARCH),	ICID_SEARCH, true, IDC_SEARCH);

	AddButton(ResString(IDS_START_HELP),	ICID_INFO, false, IDC_START_HELP);

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NORUN))
#else
	if (IS_VALUE_ZERO(hkey, _T("NoRun")))
#endif
		AddButton(ResString(IDS_LAUNCH),	ICID_ACTION, false, IDC_LAUNCH);


	AddSeparator();


#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NOCLOSE))
#else
	if (IS_VALUE_NOT_ZERO(hkeyAdv, _T("StartMenuLogoff")))
#endif
		AddButton(ResString(IDS_LOGOFF),	ICID_LOGOFF, false, IDC_LOGOFF);


#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || SHRestricted(REST_STARTMENULOGOFF) != 1)
#else
	if (IS_VALUE_ZERO(hkey, _T("NoClose")))
#endif
		AddButton(ResString(IDS_SHUTDOWN),	ICID_LOGOFF, false, IDC_SHUTDOWN);


#ifdef __MINGW32__
	RegCloseKey(hkeyAdv);
	RegCloseKey(hkey);
#endif


#ifdef _LIGHT_STARTMENU
	 // set the window size to fit all buttons
	ResizeToButtons();
#endif

	return 0;
}


void StartMenuRoot::AddEntries()
{
	super::AddEntries();

	AddButton(ResString(IDS_EXPLORE),	ICID_EXPLORER, false, IDC_EXPLORE);
}


LRESULT	StartMenuRoot::WndProc(UINT nmsg, WPARAM wparam, LPARAM lparam)
{
	switch(nmsg) {
	  case WM_PAINT: {
		PaintCanvas canvas(_hwnd);
		Paint(canvas);
		break;}

	  default:
		return super::WndProc(nmsg, wparam, lparam);
	}

	return 0;
}

void StartMenuRoot::Paint(PaintCanvas& canvas)
{
	int clr_bits;
	{WindowCanvas dc(_hwnd); clr_bits=GetDeviceCaps(dc, BITSPIXEL);}

	MemCanvas mem_dc;
	ResBitmap bmp(clr_bits<=8? clr_bits<=4? IDB_LOGOV16: IDB_LOGOV256: IDB_LOGOV);
	BitmapSelection sel(mem_dc, bmp);

	ClientRect clnt(_hwnd);
	int h = min(_logo_size.cy, clnt.bottom);

	RECT rect = {0, 0, _logo_size.cx, clnt.bottom-h};
	HBRUSH hbr = CreateSolidBrush(GetPixel(mem_dc, 0, 0));
	FillRect(canvas, &rect, hbr);
	DeleteObject(hbr);

	PatBlt(canvas, _logo_size.cx, 0, 1, clnt.bottom, WHITENESS);

	BitBlt(canvas, 0, clnt.bottom-h, _logo_size.cx, h, mem_dc, 0, 0, SRCCOPY);

	super::Paint(canvas);
}


void StartMenuRoot::CloseStartMenu(int id)
{
	if (_submenu)
		CloseSubmenus();

	ShowWindow(_hwnd, SW_HIDE);
}

void StartMenuRoot::ProcessKey(int vk)
{
	switch(vk) {
	  case VK_LEFT:
		if (_submenu)
			CloseOtherSubmenus();
		// don't close start menu root
		break;

	  default:
		super::ProcessKey(vk);
	}
}


int StartMenuHandler::Command(int id, int code)
{
	switch(id) {

	// start menu root

	  case IDC_PROGRAMS:
		CreateSubmenu(id, CSIDL_COMMON_PROGRAMS, CSIDL_PROGRAMS, ResString(IDS_PROGRAMS));
		break;

	  case IDC_EXPLORE:
		CloseStartMenu(id);
		explorer_show_frame(SW_SHOWNORMAL);
		break;

	  case IDC_LAUNCH:
		CloseStartMenu(id);
		ShowLaunchDialog(g_Globals._hwndDesktopBar);
		break;

	  case IDC_DOCUMENTS:
		CreateSubmenu(id, CSIDL_PERSONAL, ResString(IDS_DOCUMENTS));
		break;

	  case IDC_RECENT:
		CreateSubmenu(id, CSIDL_RECENT, ResString(IDS_RECENT), STARTMENU_CREATOR(RecentStartMenu));
		break;

	  case IDC_FAVORITES:
		CreateSubmenu(id, CSIDL_FAVORITES, ResString(IDS_FAVORITES));
		break;

	  case IDC_BROWSE:
		CreateSubmenu(id, ResString(IDS_BROWSE), STARTMENU_CREATOR(BrowseMenu));
		break;

	  case IDC_SETTINGS:
		CreateSubmenu(id, ResString(IDS_SETTINGS), STARTMENU_CREATOR(SettingsMenu));
		break;

	  case IDC_SEARCH:
		CreateSubmenu(id, ResString(IDS_SEARCH), STARTMENU_CREATOR(SearchMenu));
		break;

	  case IDC_START_HELP:
		CloseStartMenu(id);
		MessageBox(g_Globals._hwndDesktopBar, TEXT("Help not yet implemented"), ResString(IDS_TITLE), MB_OK);
		break;

	  case IDC_LOGOFF:
		/* The shell32 Dialog prompts about some system setting change. This is not what we want to display here.
		CloseStartMenu(id);
		ShowRestartDialog(g_Globals._hwndDesktopBar, EWX_LOGOFF);*/
		DestroyWindow(GetParent(_hwnd));
		break;

	  case IDC_SHUTDOWN:
		CloseStartMenu(id);
		ShowExitWindowsDialog(g_Globals._hwndDesktopBar);
		break;


	// settings menu

	  case ID_DESKTOPBAR_SETTINGS:
		CloseStartMenu(id);
		ExplorerPropertySheet(g_Globals._hwndDesktopBar);
		break;

	  case IDC_SETTINGS_MENU:
		CreateSubmenu(id, CSIDL_CONTROLS, ResString(IDS_SETTINGS_MENU));
		break;

	  case IDC_PRINTERS:
#ifdef _ROS_	// to be removed when printer folder will be implemented
		MessageBox(0, TEXT("printer folder not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
#else
		CreateSubmenu(id, CSIDL_PRINTERS, CSIDL_PRINTHOOD, ResString(IDS_PRINTERS));
#endif
		break;

	  case IDC_CONTROL_PANEL:
		CloseStartMenu(id);
		MainFrame::Create(TEXT("::{20D04FE0-3AEA-1069-A2D8-08002B30309D}\\::{21EC2020-3AEA-1069-A2DD-08002B30309D}"), 0);
		break;

	  case IDC_ADMIN:
		CreateSubmenu(id, CSIDL_COMMON_ADMINTOOLS, CSIDL_ADMINTOOLS, ResString(IDS_ADMIN));
		break;

	  case IDC_CONNECTIONS:
#ifdef _ROS_	// to be removed when RAS will be implemented
		MessageBox(0, TEXT("RAS folder not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
#else
		CreateSubmenu(id, CSIDL_CONNECTIONS, ResString(IDS_CONNECTIONS));
#endif
		break;


	// browse menu

	  case IDC_NETWORK:
#ifdef _ROS_	// to be removed when network will be implemented
		MessageBox(0, TEXT("network not yet implemented"), ResString(IDS_TITLE), MB_OK);
#else
		CreateSubmenu(id, CSIDL_NETWORK, ResString(IDS_NETWORK));
#endif
		break;

	  case IDC_DRIVES:
		///@todo exclude removeable drives
		CreateSubmenu(id, CSIDL_DRIVES, ResString(IDS_DRIVES));
		break;


	// search menu

	  case IDC_SEARCH_PROGRAM:
		CloseStartMenu(id);
		Dialog::DoModal(IDD_SEARCH_PROGRAM, WINDOW_CREATOR(FindProgramDlg));
		break;

	  case IDC_SEARCH_FILES:
		CloseStartMenu(id);
		ShowSearchDialog();
		break;

	  case IDC_SEARCH_COMPUTER:
		CloseStartMenu(id);
		ShowSearchComputer();
		break;


	  default:
		return super::Command(id, code);
	}

	return 0;
}


void StartMenuHandler::ShowSearchDialog()
{
	static DynamicFct<SHFINDFILES> SHFindFiles(TEXT("SHELL32"), 90);

	if (SHFindFiles)
		(*SHFindFiles)(NULL, NULL);
	else
		MessageBox(0, TEXT("SHFindFiles() not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
}

void StartMenuHandler::ShowSearchComputer()
{
	static DynamicFct<SHFINDCOMPUTER> SHFindComputer(TEXT("SHELL32"), 91);

	if (SHFindComputer)
		(*SHFindComputer)(NULL, NULL);
	else
		MessageBox(0, TEXT("SHFindComputer() not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
}

void StartMenuHandler::ShowLaunchDialog(HWND hwndOwner)
{
	 ///@todo All text phrases should be put into the resources.
	static LPCSTR szTitle = "Run";
	static LPCSTR szText = "Type the name of a program, folder, document, or Internet resource, and Explorer will open it for you.";

	static DynamicFct<RUNFILEDLG> RunFileDlg(TEXT("SHELL32"), 61);

	 // Show "Run..." dialog
	if (RunFileDlg) {
#ifndef _ROS_ /* FIXME: our shell32 always expects Ansi strings */
#define	W_VER_NT 0
		if ((HIWORD(GetVersion())>>14) == W_VER_NT) {
			WCHAR wTitle[40], wText[256];

			MultiByteToWideChar(CP_ACP, 0, szTitle, -1, wTitle, 40);
			MultiByteToWideChar(CP_ACP, 0, szText, -1, wText, 256);

			(*RunFileDlg)(hwndOwner, 0, NULL, (LPCSTR)wTitle, (LPCSTR)wText, RFF_CALCDIRECTORY);
		}
		else
#endif
			(*RunFileDlg)(hwndOwner, 0, NULL, szTitle, szText, RFF_CALCDIRECTORY);
	}
}

void StartMenuHandler::ShowRestartDialog(HWND hwndOwner, UINT flags)
{
	static DynamicFct<RESTARTWINDOWSDLG> RestartDlg(TEXT("SHELL32"), 59);

	if (RestartDlg)
		(*RestartDlg)(hwndOwner, (LPWSTR)L"You selected <Log Off>.\n\n", flags);	///@todo ANSI string conversion if needed
	else
		MessageBox(hwndOwner, TEXT("RestartDlg() not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
}

void ShowExitWindowsDialog(HWND hwndOwner)
{
	static DynamicFct<EXITWINDOWSDLG> ExitWindowsDlg(TEXT("SHELL32"), 60);

	if (ExitWindowsDlg)
		(*ExitWindowsDlg)(hwndOwner);
	else
		MessageBox(hwndOwner, TEXT("ExitWindowsDlg() not yet implemented in SHELL32"), ResString(IDS_TITLE), MB_OK);
}


void SettingsMenu::AddEntries()
{
	super::AddEntries();

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NOCONTROLPANEL))
#endif
		AddButton(ResString(IDS_CONTROL_PANEL),	ICID_CONFIG, false, IDC_CONTROL_PANEL);

#ifdef _ROS_	// to be removed when printer/network will be implemented
	AddButton(ResString(IDS_PRINTERS),		ICID_PRINTER, false, IDC_PRINTERS);
	AddButton(ResString(IDS_CONNECTIONS),	ICID_NETWORK, false, IDC_CONNECTIONS);
#else
	AddButton(ResString(IDS_PRINTERS),		ICID_PRINTER, true, IDC_PRINTERS);
	AddButton(ResString(IDS_CONNECTIONS),	ICID_NETWORK, true, IDC_CONNECTIONS);
#endif
	AddButton(ResString(IDS_ADMIN),			ICID_CONFIG, true, IDC_ADMIN);

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NOCONTROLPANEL))
#endif
		AddButton(ResString(IDS_SETTINGS_MENU),	ICID_CONFIG, true, IDC_SETTINGS_MENU);

	AddButton(ResString(IDS_DESKTOPBAR_SETTINGS), ICID_CONFIG, false, ID_DESKTOPBAR_SETTINGS);
}

void BrowseMenu::AddEntries()
{
	super::AddEntries();

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_NONETHOOD))	// or REST_NOENTIRENETWORK ?
#endif
#ifdef _ROS_	// to be removed when printer/network will be implemented
		AddButton(ResString(IDS_NETWORK),	ICID_NETWORK, false, IDC_NETWORK);
#else
		AddButton(ResString(IDS_NETWORK),	ICID_NETWORK, true, IDC_NETWORK);
#endif

	AddButton(ResString(IDS_DRIVES),	ICID_FOLDER, true, IDC_DRIVES);
}

void SearchMenu::AddEntries()
{
	super::AddEntries();

	AddButton(ResString(IDS_SEARCH_PRG),	ICID_APPS, false, IDC_SEARCH_PROGRAM);

	AddButton(ResString(IDS_SEARCH_FILES),	ICID_SEARCH_DOC, false, IDC_SEARCH_FILES);

#ifndef __MINGW32__	// SHRestricted() missing in MinGW (as of 29.10.2003)
	if (!g_Globals._SHRestricted || !SHRestricted(REST_HASFINDCOMPUTERS))
#endif
		AddButton(ResString(IDS_SEARCH_COMPUTER),	ICID_COMPUTER, false, IDC_SEARCH_COMPUTER);
}


void RecentStartMenu::AddEntries()
{
	for(StartMenuShellDirs::iterator it=_dirs.begin(); it!=_dirs.end(); ++it) {
		StartMenuDirectory& smd = *it;
		ShellDirectory& dir = smd._dir;

		if (!dir._scanned) {
			WaitCursor wait;

#ifdef _LAZY_ICONEXTRACT
			dir.smart_scan(SCAN_FILESYSTEM);
#else
			dir.smart_scan(SCAN_EXTRACT_ICONS|SCAN_FILESYSTEM);
#endif
		}

		dir.sort_directory(SORT_DATE);
		AddShellEntries(dir, 16, smd._subfolders);	///@todo read max. count of entries from registry
	}
}
