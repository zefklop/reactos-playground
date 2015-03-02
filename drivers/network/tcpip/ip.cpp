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
IpDevice::Create(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_CREATE\n");

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

// IRP_MJ_CLEANUP handler
NTSTATUS
IpDevice::Cleanup(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_CLEANUP\n");

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}

// IRP_MJ_CLOSE handler
NTSTATUS
IpDevice::Close(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_CLOSE\n");

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

// IRP_MJ_DEVICE_CONTROL handler
NTSTATUS
IpDevice::DispatchInternal(
    _Inout_ PIRP Irp)
{
    DPRINT1("Ip device: IRP_MJ_DEVICE_CONTROL_INTERNAL\n");

    Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
    IoCompleteRequest(Irp, IO_NETWORK_INCREMENT);
    return STATUS_NOT_IMPLEMENTED;
}
