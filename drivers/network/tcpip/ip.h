
#pragma once

#include "device.h"

class TcpIpDevice;

class IpDevice : public TcpIpDevice
{
public:
    static LPCWSTR Name;
    static LPCWSTR DosName;

    /* IRP_MJ_* handlers for the Ip device */
    virtual NTSTATUS Create(_Inout_ PIRP Irp);
    virtual NTSTATUS Cleanup(_Inout_ PIRP Irp);
    virtual NTSTATUS Close(_Inout_ PIRP Irp);
    virtual NTSTATUS Dispatch(_Inout_ PIRP Irp);
    virtual NTSTATUS DispatchInternal(_Inout_ PIRP Irp);

    static void operator delete(void* ptr)
    {
        // Nothing to do: the memory is allocated into the device extension
    }
};
