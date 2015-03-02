
#pragma once

#include "device.h"

class TcpIpDevice;

class IpDevice : public TcpIpDevice
{
public:
    static LPCWSTR Name;
    static LPCWSTR DosName;

    /* IRP_MJ_* handlers for the Ip device */
    virtual NTSTATUS CreateControlChannel(_Inout_ PIRP Irp, _Inout_ PIO_STACK_LOCATION IrpSp);
    virtual NTSTATUS Cleanup(_Inout_ PIRP Irp);
    virtual NTSTATUS Close(_Inout_ PIRP Irp);
    virtual NTSTATUS Dispatch(_Inout_ PIRP Irp);
    virtual NTSTATUS DispatchInternal(_Inout_ PIRP Irp);

    static void* operator new(size_t Size, void* ptr)
    {
        // We don't do anything, the class is allocated into the device extension.

        // sizeof(IpDevice) is the amount of space we allocated. Check this is actually what the runtime needs
        ASSERT(Size == sizeof(IpDevice));

        return ptr;
    }

    static void operator delete(void* ptr)
    {
        // Nothing to do: the memory is allocated into the device extension
    }
};
