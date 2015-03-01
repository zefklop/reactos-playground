/*
 * PROJECT:         ReactOS tcpip.sys
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            drivers/network/tcpip/tcpip.cpp
 * PURPOSE:         tcpip.sys driver entry
 */

#include "tcpip.h"

extern "C"
{
DRIVER_INITIALIZE DriverEntry;

_Use_decl_annotations_
NTSTATUS
NTAPI
DriverEntry(
  _In_  struct _DRIVER_OBJECT *DriverObject,
  _In_  PUNICODE_STRING RegistryPath
)
{
    return STATUS_SUCCESS;
}

} // extern "C"
