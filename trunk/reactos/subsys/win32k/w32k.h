/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS Graphics Subsystem
 * FILE:            subsys/win32k/w32k.h
 * PURPOSE:         Main Win32K Header
 * PROGRAMMER:      Alex Ionescu (alex@relsoft.net)
 */

/* INCLUDES ******************************************************************/

#define _NO_COM

/* DDK/NDK/SDK Headers */
#include <ntifs.h>
#include <ntddk.h>
#include <ntddmou.h>
#include <ntndk.h>

/* Win32 Headers */
/* FIXME: Defines in winbase.h that we need... */
typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
#define WINBASEAPI
#define STARTF_USESIZE 2
#define STARTF_USEPOSITION 4
#include <stdarg.h>
#include <windef.h>
#include <winerror.h>
#include <wingdi.h>
#include <winddi.h>
#include <winuser.h>
#include <prntfont.h>
#include <dde.h>
#include <wincon.h>
#define _NOCSECT_TYPE
#include <ddrawi.h>

/* SEH Support with PSEH */
#include <pseh/pseh.h>

/* CSRSS Header */
#include <csrss/csrss.h>

/* Helper Header */
#include <reactos/helper.h>

/* Probe and capture */
#include <reactos/probe.h>

/* Public Win32K Headers */
#include <win32k/callback.h>
#include <win32k/ntusrtyp.h>
#include <win32k/ntuser.h>
#include <win32k/ntgdityp.h>
#include <win32k/ntgdibad.h>
#include <ntgdi.h>

/* For access to SECTION_OBJECT. FIXME: Once compatible with NT, use NDK! */
#include <internal/mm.h>

/* Internal Win32K Header */
#include "include/win32k.h"

/* Undocumented stuff */
typedef DRIVEROBJ *PDRIVEROBJ;
#define WM_SYSTIMER 280
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#define M_PI_2 1.57079632679489661923
#endif

