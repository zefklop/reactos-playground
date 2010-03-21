/*
 * PROJECT:         ReactOS ACPI-Compliant Control Method Battery
 * LICENSE:         BSD - See COPYING.ARM in the top level directory
 * FILE:            boot/drivers/bus/acpi/cmbatt/cmbatt.h
 * PURPOSE:         Main Header File
 * PROGRAMMERS:     ReactOS Portable Systems Group
 */

#include <ntddk.h>
#include <initguid.h>
#include <batclass.h>
#include <acpiioct.h>
#include <wmilib.h>
#include <debug.h>

#define CMBATT_GENERIC_STATUS   0x01
#define CMBATT_GENERIC_INFO     0x02
#define CMBATT_GENERIC_WARNING  0x04
#define CMBATT_ACPI_WARNING     0x08
#define CMBATT_POWER_INFO       0x10
#define CMBATT_PNP_INFO         0x20
#define CMBATT_ACPI_ENTRY_EXIT  0x40
#define CMBATT_PNP_ENTRY_EXIT   0x200
#define CMBATT_ACPI_ASSERT      0x400

typedef enum _CMBATT_EXTENSION_TYPE
{
    CmBattAcAdapter,
    CmBattBattery
} CMBATT_EXTENSION_TYPE;

#define ACPI_BUS_CHECK              0x00
#define ACPI_DEVICE_CHECK           0x01

#define ACPI_STA_PRESENT            0x01
#define ACPI_STA_ENABLED            0x02
#define ACPI_STA_SHOW_UI            0x04
#define ACPI_STA_FUNCTIONAL         0x08
#define ACPI_STA_BATTERY_PRESENT    0x10

#define ACPI_BATT_NOTIFY_STATUS     0x80
#define ACPI_BATT_NOTIFY_INFO       0x81

#define ACPI_BATT_STAT_DISCHARG		0x0001
#define ACPI_BATT_STAT_CHARGING		0x0002
#define ACPI_BATT_STAT_CRITICAL		0x0004

#define CM_MAX_VALUE                0x7FFFFFFF
#define CM_UNKNOWN_VALUE            0xFFFFFFFF

typedef struct _ACPI_BST_DATA
{
    ULONG State;
    ULONG PresentRate;
    ULONG RemainingCapacity;
    ULONG PresentVoltage;
} ACPI_BST_DATA, *PACPI_BST_DATA;

#define ACPI_BATT_POWER_UNIT_WATTS  0x0
#define ACPI_BATT_POWER_UNIT_AMPS   0x1

typedef struct _ACPI_BIF_DATA
{
    ULONG PowerUnit;
    ULONG DesignCapacity;
    ULONG LastFullCapacity;
    ULONG BatteryTechnology;
    ULONG DesignVoltage;
    ULONG DesignCapacityWarning;
    ULONG DesignCapacityLow;
    ULONG BatteryCapacityGranularity1;
    ULONG BatteryCapacityGranularity2;
    CHAR ModelNumber[256];
    CHAR SerialNubmer[256];
    CHAR BatteryType[256];
    CHAR OemInfo[256];
} ACPI_BIF_DATA, *PACPI_BIF_DATA;

#define CMBATT_AR_NOTIFY            0x01
#define CMBATT_AR_INSERT            0x02
#define CMBATT_AR_REMOVE            0x04

typedef struct _CMBATT_DEVICE_EXTENSION
{
    CMBATT_EXTENSION_TYPE FdoType;
    PDEVICE_OBJECT DeviceObject;
    PDEVICE_OBJECT FdoDeviceObject;
    PDEVICE_OBJECT PdoDeviceObject;
    PDEVICE_OBJECT AttachedDevice;
    FAST_MUTEX FastMutex;
    ULONG HandleCount;
    PIRP PowerIrp;
    POWER_STATE PowerState;
    WMILIB_CONTEXT WmiLibInfo;
    ULONG WaitWakeEnable;
    ULONG WmiCount;
    KEVENT WmiEvent;
    ULONG DeviceId;
    PUNICODE_STRING DeviceName;
    ACPI_INTERFACE_STANDARD2 AcpiInterface;
    BOOLEAN DelayNotification;
    BOOLEAN ArFlag;
    PVOID ClassData;
    BOOLEAN Started;
    BOOLEAN NotifySent;
    LONG ArLockValue;
    ULONG TagData;
    ULONG Tag;
    ULONG ModelNumberLength;
    PCHAR ModelNumber;
    ULONG SerialNumberLength;
    PCHAR SerialNumber;
    ULONG OemInfoLength;
    PCHAR OemInfo;
    ACPI_BST_DATA BstData;
    ACPI_BIF_DATA BifData;
    ULONG Id;
    ULONG State;
    ULONG RemainingCapacity;
    ULONG PresentVoltage;
    ULONG Rate;
    BATTERY_INFORMATION BatteryInformation;
    ULONG BatteryCapacityGranularity1;
    ULONG BatteryCapacityGranularity2;
    BOOLEAN TripPointSet;
    ULONG TripPointValue;
    ULONG TripPointOld;
    ULONGLONG InterruptTime;
} CMBATT_DEVICE_EXTENSION, *PCMBATT_DEVICE_EXTENSION;

NTSTATUS
NTAPI
CmBattPowerDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
);

NTSTATUS
NTAPI
CmBattPnpDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
);

NTSTATUS
NTAPI
CmBattAddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT DeviceObject
);

NTSTATUS
NTAPI
CmBattSystemControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
);

NTSTATUS
NTAPI
CmBattGetBstData(
    PCMBATT_DEVICE_EXTENSION DeviceExtension,
    PACPI_BST_DATA BstData
);

NTSTATUS
NTAPI
CmBattGetPsrData(
    PDEVICE_OBJECT DeviceObject,
    PULONG PsrData
);

NTSTATUS
NTAPI
CmBattGetStaData(
    PDEVICE_OBJECT DeviceObject,
    PULONG StaData
);

/* EOF */
