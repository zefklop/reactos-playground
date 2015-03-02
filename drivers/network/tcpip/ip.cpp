/*
 * PROJECT:         ReactOS tcpip.sys
 * COPYRIGHT:       GPL - See COPYING in the top level directory
 * FILE:            drivers/network/tcpip/ip.cpp
 * PURPOSE:         \Device\Ip implementation
 */

#include "tcpip.h"

#define NDEBUG
#include <debug.h>

LPCWSTR IpDevice::Name = L"\\Device\\Ip";
LPCWSTR IpDevice::DosName = L"\\DosDevices\\Ip";

// IRP_MJ_CREATE handler
NTSTATUS
IpDevice::CreateControlChannel(
    _Inout_ PIRP Irp,
    _Inout_ PIO_STACK_LOCATION IrpSp)
{
    /* For now, we don't allocate anything, just mark the file object. */
    IrpSp->FileObject->FsContext2 = reinterpret_cast<PVOID>(TDI_CONTROL_CHANNEL_FILE);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_SUCCESS;
}

// IRP_MJ_CLEANUP handler
NTSTATUS
IpDevice::Cleanup(
    _Inout_ PIRP Irp)
{
    DPRINT("Ip device: IRP_MJ_CLEANUP\n");

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    if (IrpSp->FileObject->FsContext2 == reinterpret_cast<PVOID>(TDI_CONTROL_CHANNEL_FILE))
    {
        /* We don't allocate anything for a control channel file */
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_SUCCESS;
    }

    UNIMPLEMENTED

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

// IRP_MJ_CLOSE handler
NTSTATUS
IpDevice::Close(
    _Inout_ PIRP Irp)
{
    DPRINT("Ip device: IRP_MJ_CLOSE\n");

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);

    if (IrpSp->FileObject->FsContext2 == reinterpret_cast<PVOID>(TDI_CONTROL_CHANNEL_FILE))
    {
        /* We don't allocate anything for a control channel file */
        Irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
        return STATUS_SUCCESS;
    }

    UNIMPLEMENTED

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

// IRP_MJ_DEVICE_CONTROL handler
NTSTATUS
IpDevice::Dispatch(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_DEVICE_CONTROL\n");

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

// IRP_MJ_DEVICE_CONTROL_INTERNAL handler
NTSTATUS
IpDevice::DispatchInternal(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_DEVICE_CONTROL_INTERNAL\n");

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}
