/*
 * PROJECT:         ReactOS tcpip.sys
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            drivers/network/tcpip/tcpip.cpp
 * PURPOSE:         tcpip.sys driver entry
 */

#include "tcpip.h"

#include <new>

#define NDEBUG
#include <debug.h>

/* Global Initializers */
PDEVICE_OBJECT TcpIpDriver::IpDeviceObject = NULL;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, TcpIpDriver::Create)
#pragma alloc_text(PAGE, TcpIpDriver::Cleanup)
#pragma alloc_text(PAGE, TcpIpDriver::Close)
#pragma alloc_text(PAGE, TcpIpDriver::DispatchInternal)
#pragma alloc_text(PAGE, TcpIpDriver::Dispatch)
#endif

_Use_decl_annotations_
NTAPI
VOID
TcpIpDriver::Unload(
    _In_  struct _DRIVER_OBJECT *DriverObject
)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt;

    /* We must explicitly call the destructor of our device extension classes, because they were not allocated with the calssic 'new' operator */
    TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(IpDeviceObject->DeviceObjectExtension);
    delete static_cast<IpDevice*>(TcpIpDevExt->DevExt);
    IoDeleteDevice(TcpIpDriver::IpDeviceObject);
}


extern "C"
{
DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

_Use_decl_annotations_
NTSTATUS
NTAPI
DriverEntry(
  _In_  struct _DRIVER_OBJECT *DriverObject,
  _In_  PUNICODE_STRING RegistryPath
)
{
    NTSTATUS Status;
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt;
    UNICODE_STRING DeviceName;
    UNICODE_STRING DosDeviceName;

    // Create the IP device object
    RtlInitUnicodeString(&DeviceName, IpDevice::Name);
    Status = IoCreateDevice(
        DriverObject,
        sizeof(TCPIP_DEVICE_EXTENSION) + sizeof(IpDevice),
        &DeviceName,
        FILE_DEVICE_NETWORK,
        0,
        FALSE,
        &TcpIpDriver::IpDeviceObject);
    if (!NT_SUCCESS(Status))
        return Status;

    /* Do the DOS device name binding */
    RtlInitUnicodeString(&DosDeviceName, IpDevice::DosName);
    Status = IoCreateSymbolicLink(&DosDeviceName, &DeviceName);
    if (!NT_SUCCESS(Status))
    {
        IoDeleteDevice(TcpIpDriver::IpDeviceObject);
        return Status;
    }

    TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(TcpIpDriver::IpDeviceObject->DeviceObjectExtension);
    /* Call constructor inside the memory we just allocated */
    IpDevice* IpDev = new((TcpIpDevExt + 1)) IpDevice();
    TcpIpDevExt->DevExt = IpDev;

    /* Register callbacks */
    DriverObject->MajorFunction[IRP_MJ_CREATE] = TcpIpDriver::Create;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = TcpIpDriver::Close;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = TcpIpDriver::DispatchInternal;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TcpIpDriver::Dispatch;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = TcpIpDriver::Cleanup;

    DriverObject->DriverUnload = TcpIpDriver::Unload;

    return STATUS_SUCCESS;
}

} // extern "C"

_Use_decl_annotations_
NTSTATUS
NTAPI
TcpIpDriver::Create(
    _In_    PDEVICE_OBJECT DevObj,
    _Inout_ PIRP Irp)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(DevObj->DeviceObjectExtension);

    /* pass it down to the targeted device */
    return TcpIpDevExt->DevExt->Create(Irp);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
TcpIpDriver::Close(
    _In_    PDEVICE_OBJECT DevObj,
    _Inout_ PIRP Irp)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(DevObj->DeviceObjectExtension);

    /* pass it down to the targeted device */
    return TcpIpDevExt->DevExt->Close(Irp);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
TcpIpDriver::DispatchInternal(
    _In_    PDEVICE_OBJECT DevObj,
    _Inout_ PIRP Irp)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(DevObj->DeviceObjectExtension);

    /* pass it down to the targeted device */
    return TcpIpDevExt->DevExt->DispatchInternal(Irp);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
TcpIpDriver::Dispatch(
    _In_    PDEVICE_OBJECT DevObj,
    _Inout_ PIRP Irp)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(DevObj->DeviceObjectExtension);

    /* pass it down to the targeted device */
    return TcpIpDevExt->DevExt->Dispatch(Irp);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
TcpIpDriver::Cleanup(
    _In_    PDEVICE_OBJECT DevObj,
    _Inout_ PIRP Irp)
{
    TCPIP_DEVICE_EXTENSION* TcpIpDevExt = reinterpret_cast<TCPIP_DEVICE_EXTENSION*>(DevObj->DeviceObjectExtension);

    /* pass it down to the targeted device */
    return TcpIpDevExt->DevExt->Cleanup(Irp);
}


// C++ glue
#ifndef _MSC_VER
extern "C" void __cxa_pure_virtual()
{
    ASSERT(FALSE);
}
#else
int __cdecl _purecall()
{
    ASSERT(FALSE);
    return 0;
}
#endif


