#include <windows.h>
#include <msvcrt/io.h>
#include <msvcrt/errno.h>
#define NDEBUG
#include <msvcrt/msvcrtdbg.h>

#ifndef F_OK
 #define F_OK	0x01
#endif
#ifndef R_OK
 #define R_OK	0x02
#endif
#ifndef W_OK
 #define W_OK	0x04
#endif
#ifndef X_OK
 #define X_OK	0x08
#endif
#ifndef D_OK
 #define D_OK	0x10
#endif

int _access( const char *_path, int _amode )
{
	DWORD Attributes = GetFileAttributesA(_path);
	DPRINT("_access('%s', %x)\n", _path, _amode);

	if ( Attributes == -1 )	{
	        __set_errno(ENOENT);
		return -1;
	}

	if ( (_amode & W_OK) == W_OK ) {
		if ( (Attributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY ) {
			__set_errno(EACCES);
			return -1;
		}
	}
	if ( (_amode & D_OK) == D_OK ) {
		if ( (Attributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY ) {
			__set_errno(EACCES);
			return -1;
		}
	}

	return 0;
}

int _waccess( const wchar_t *_path, int _amode )
{
	DWORD Attributes = GetFileAttributesW(_path);

	if ( Attributes == -1 ) {
	        __set_errno(ENOENT);
		return -1;
	}

	if ( (_amode & W_OK) == W_OK ) {
		if ( (Attributes & FILE_ATTRIBUTE_READONLY) == FILE_ATTRIBUTE_READONLY ) {
			__set_errno(EACCES);
			return -1;
		}
	}
	if ( (_amode & D_OK) == D_OK ) {
		if ( (Attributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY ) {
			__set_errno(EACCES);
			return -1;
		}
	}

	return 0;
}
