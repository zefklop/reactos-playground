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
 // winfs.cpp
 //
 // Martin Fuchs, 23.07.2003
 //


#include "../utility/utility.h"
#include "../utility/shellclasses.h"

#include "entries.h"
#include "winfs.h"


void WinDirectory::read_directory()
{
	Entry* first_entry = NULL;
	Entry* last = NULL;
	Entry* entry;

	int level = _level + 1;

	LPCTSTR path = (LPCTSTR)_path;
	TCHAR buffer[MAX_PATH], *p;
	for(p=buffer; *path; )
		*p++ = *path++;

	lstrcpy(p, TEXT("\\*"));

	WIN32_FIND_DATA w32fd;
	HANDLE hFind = FindFirstFile(buffer, &w32fd);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			lstrcpy(p+1, w32fd.cFileName);

			if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				entry = new WinDirectory(this, buffer);
			else
				entry = new WinEntry(this);

			if (!first_entry)
				first_entry = entry;

			if (last)
				last->_next = entry;

			memcpy(&entry->_data, &w32fd, sizeof(WIN32_FIND_DATA));
			entry->_down = NULL;
			entry->_expanded = false;
			entry->_scanned = false;
			entry->_level = level;
			entry->_bhfi_valid = false;

			HANDLE hFile = CreateFile(buffer, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
										0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

			if (hFile != INVALID_HANDLE_VALUE) {
				if (GetFileInformationByHandle(hFile, &entry->_bhfi))
					entry->_bhfi_valid = true;

				CloseHandle(hFile);
			}

			last = entry;
		} while(FindNextFile(hFind, &w32fd));

		last->_next = NULL;

		FindClose(hFind);
	}

	_down = first_entry;
	_scanned = true;
}


const void* WinDirectory::get_next_path_component(const void* p)
{
	LPCTSTR s = (LPCTSTR) p;

	while(*s && *s!=TEXT('\\') && *s!=TEXT('/'))
		++s;

	while(*s==TEXT('\\') || *s==TEXT('/'))
		++s;

	if (!*s)
		return NULL;

	return s;
}


Entry* WinDirectory::find_entry(const void* p)
{
	LPCTSTR name = (LPCTSTR)p;

	for(Entry*entry=_down; entry; entry=entry->_next) {
		LPCTSTR p = name;
		LPCTSTR q = entry->_data.cFileName;

		do {
			if (!*p || *p==TEXT('\\') || *p==TEXT('/'))
				return entry;
		} while(tolower(*p++) == tolower(*q++));

		p = name;
		q = entry->_data.cAlternateFileName;

		do {
			if (!*p || *p==TEXT('\\') || *p==TEXT('/'))
				return entry;
		} while(tolower(*p++) == tolower(*q++));
	}

	return NULL;
}


 // get full path of specified directory entry
void WinEntry::get_path(PTSTR path) const
{
	int level = 0;
	int len = 0;

	for(const Entry* entry=this; entry; level++) {
		LPCTSTR name = entry->_data.cFileName;
		int l = 0;

		for(LPCTSTR s=name; *s && *s!=TEXT('/') && *s!=TEXT('\\'); s++)
			++l;

		if (entry->_up) {
			if (l > 0) {
				memmove(path+l+1, path, len*sizeof(TCHAR));
				memcpy(path+1, name, l*sizeof(TCHAR));
				len += l+1;

				path[0] = TEXT('\\');
			}

			entry = entry->_up;
		} else {
			memmove(path+l, path, len*sizeof(TCHAR));
			memcpy(path, name, l*sizeof(TCHAR));
			len += l;
			break;
		}
	}

	if (!level)
		path[len++] = TEXT('\\');

	path[len] = TEXT('\0');
}
