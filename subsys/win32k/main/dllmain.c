/* $Id: dllmain.c,v 1.12 2000/02/21 22:44:37 ekohl Exp $
 * 
 *  Entry Point for win32k.sys
 */

#undef WIN32_LEAN_AND_MEAN
#define WIN32_NO_STATUS
#include <windows.h>
#include <ddk/ntddk.h>
#include <ddk/winddi.h>
#include <internal/service.h>

#include <win32k/win32k.h>

/*
 * NOTE: the table is actually in the file ./svctab.c,
 * generated by iface/addsys/mktab.c + w32ksvc.db
 */
#include "svctab.c"

/*
 * This definition doesn't work
 */
// WINBOOL STDCALL DllMain(VOID)
NTSTATUS
STDCALL
DllMain (
	IN	PDRIVER_OBJECT	DriverObject,
	IN	PUNICODE_STRING	RegistryPath
	)
{
	BOOLEAN Result;

	DbgPrint("Win32 kernel mode driver\n");

	/*
	 * Register user mode call interface
	 * (system service table index = 1)
	 */
	Result = KeAddSystemServiceTable (Win32kSSDT,
	                                  NULL,
	                                  NUMBER_OF_SYSCALLS,
	                                  Win32kSSPT,
	                                  1);
	if (Result == FALSE)
	{
		DbgPrint("Adding system services failed!\n");
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("System services added successfully!\n");
	return STATUS_SUCCESS;
}

/* EOF */
