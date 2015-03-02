
#include <ntddk.h>
#include <tdikrnl.h>

#include "device.h"
#include "ip.h"

class TcpIpDriver
{
public:
    /* The device objects we will create */
    static PDEVICE_OBJECT IpDeviceObject;

    /* Those are the IRP_MJ* callbacks */
    _Dispatch_type_(IRP_MJ_CREATE) static DRIVER_DISPATCH Create;
    _Dispatch_type_(IRP_MJ_CLEANUP) static DRIVER_DISPATCH Cleanup;
    _Dispatch_type_(IRP_MJ_CLOSE) static DRIVER_DISPATCH Close;
    _Dispatch_type_(IRP_MJ_INTERNAL_DEVICE_CONTROL) static DRIVER_DISPATCH DispatchInternal;
    _Dispatch_type_(IRP_MJ_DEVICE_CONTROL) static DRIVER_DISPATCH Dispatch;

    /* The driver unload callback */
    static DRIVER_UNLOAD Unload;
};
