
#pragma once

class TcpIpDevice
{
public:
    /* Handlers for IRP_MJ_* requests called for this device */
    virtual NTSTATUS CreateControlChannel(_Inout_ PIRP Irp, _Inout_ PIO_STACK_LOCATION IrpSp) = 0;
    virtual NTSTATUS Close(_Inout_ PIRP Irp) = 0;
    virtual NTSTATUS Cleanup(_Inout_ PIRP Irp) = 0;
    virtual NTSTATUS Dispatch(_Inout_ PIRP Irp) = 0;
    virtual NTSTATUS DispatchInternal(_Inout_ PIRP Irp) = 0;

    virtual ~TcpIpDevice() { };

    static void operator delete(void* ptr)
    {
        // Nothing to do: the memory is allocated into the device extension
    }
};

/* We must use a container, because we can't directly dynamic_cast for lack of RTTI in kernel */
struct TCPIP_DEVICE_EXTENSION
{
    TcpIpDevice* DevExt;
};
