#ifndef __INCLUDE_DDK_KDFUNCS_H
#define __INCLUDE_DDK_KDFUNCS_H
/* $Id: kdfuncs.h,v 1.2 2000/02/29 23:57:44 ea Exp $ */

/* --- NTOSKRNL.EXE --- */
#if defined(__NTOSKRNL__)
extern BOOLEAN KdDebuggerEnabled __declspec(dllexport);
extern BOOLEAN KdDebuggerNotPresent __declspec(dllexport);
#else
extern BOOLEAN KdDebuggerEnabled __declspec(dllimport);
extern BOOLEAN KdDebuggerNotPresent __declspec(dllimport);
#endif

BYTE
STDCALL
KdPollBreakIn (
	VOID
	);

/* --- HAL.DLL --- */
#if defined(__NTOSKRNL__)
extern ULONG KdComPortInUse __declspec(dllexport);
#else
extern ULONG KdComPortInUse __declspec(dllimport);
#endif

BOOLEAN
STDCALL
KdPortInitialize (
	PKD_PORT_INFORMATION	PortInformation,
	DWORD	Unknown1,
	DWORD	Unknown2
	);
BYTE
STDCALL
KdPortGetByte (
	VOID
	);
BYTE
STDCALL
KdPortPollByte (
	VOID
	);
VOID
STDCALL
KdPortPutByte (
	UCHAR ByteToSend
	);
VOID
STDCALL
KdPortRestore (
	VOID
	);
VOID
STDCALL
KdPortSave (
	VOID
	);

#endif /* __INCLUDE_DDK_KDFUNCS_H */
