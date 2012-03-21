/* $Id: acpienum.c 21698 2006-04-22 05:55:17Z tretiakov $
 *
 * PROJECT:         ReactOS ACPI bus driver
 * FILE:            acpi/ospm/acpienum.c
 * PURPOSE:         ACPI namespace enumerator
 * PROGRAMMERS:     Casper S. Hornstrup (chorns@users.sourceforge.net)
 * UPDATE HISTORY:
 *      01-05-2001  CSH  Created
 */
#include <acpi.h>
#include <acpisys.h>
#include <acpi_bus.h>
#include <acpi_drivers.h>
#include <list.h>

#define NDEBUG
#include <debug.h>

#define HAS_CHILDREN(d)		((d)->children.next != &((d)->children))
#define HAS_SIBLINGS(d)		(((d)->parent) && ((d)->node.next != &(d)->parent->children))
#define NODE_TO_DEVICE(n)	(list_entry(n, struct acpi_device, node))

extern struct acpi_device		*acpi_root;

NTSTATUS
Bus_PlugInDevice (
    struct acpi_device *Device,
    PFDO_DEVICE_DATA            FdoData
    )
{
    PDEVICE_OBJECT      pdo;
    PPDO_DEVICE_DATA    pdoData;
    NTSTATUS            status;
    ULONG               index;
	WCHAR               temp[256];
    PLIST_ENTRY         entry; 

    PAGED_CODE ();

    //Don't enumerate the root device
    if (Device->handle == ACPI_ROOT_OBJECT)
        return STATUS_SUCCESS;

    /* Check we didnt add this already */
    for (entry = FdoData->ListOfPDOs.Flink;
        entry != &FdoData->ListOfPDOs; entry = entry->Flink)
    {
        struct acpi_device *CurrentDevice;

        pdoData = CONTAINING_RECORD (entry, PDO_DEVICE_DATA, Link);

        //dont duplicate devices
        if (pdoData->AcpiHandle == Device->handle)
            return STATUS_SUCCESS;

        //check everything but fixed feature devices
        if (pdoData->AcpiHandle)
            acpi_bus_get_device(pdoData->AcpiHandle, &CurrentDevice);
        else
            continue;

        //check if the HID matches
        if (strstr(Device->pnp.hardware_id, CurrentDevice->pnp.hardware_id))
        {
            //check if UID exists for both and matches
            if (Device->flags.unique_id && CurrentDevice->flags.unique_id &&
                strstr(Device->pnp.unique_id, CurrentDevice->pnp.unique_id))
            {
                /* We have a UID on both but they're the same so we have to ignore it */
                DPRINT1("Detected duplicate device: %hs %hs\n", Device->pnp.hardware_id, Device->pnp.unique_id);
                return STATUS_SUCCESS;
            }
            else if (!Device->flags.unique_id && !CurrentDevice->flags.unique_id)
            {
                /* No UID so we can only legally have 1 of these devices */
                DPRINT1("Detected duplicate device: %hs\n", Device->pnp.hardware_id);
                return STATUS_SUCCESS;
            }
        }
    }

    DPRINT("Exposing PDO\n"
           "======AcpiHandle:   %p\n"
           "======HardwareId:     %s\n",
           Device->handle,
           Device->pnp.hardware_id);


    //
    // Create the PDO
    //

    DPRINT("FdoData->NextLowerDriver = 0x%p\n", FdoData->NextLowerDriver);

    status = IoCreateDevice(FdoData->Common.Self->DriverObject,
                              sizeof(PDO_DEVICE_DATA),
                              NULL,
                              FILE_DEVICE_CONTROLLER,
                              FILE_AUTOGENERATED_DEVICE_NAME,
                              FALSE,
                              &pdo);

    if (!NT_SUCCESS (status)) {
        return status;
    }

    pdoData = (PPDO_DEVICE_DATA) pdo->DeviceExtension;
	pdoData->AcpiHandle = Device->handle;

    //
    // Copy the hardware IDs
    //
		index = 0;
		index += swprintf(&temp[index],
							L"ACPI\\%hs",
							Device->pnp.hardware_id);
		index++;

		index += swprintf(&temp[index],
							L"*%hs",
							Device->pnp.hardware_id);
		index++;
		temp[++index] = UNICODE_NULL;

		pdoData->HardwareIDs  = ExAllocatePoolWithTag(NonPagedPool, index*sizeof(WCHAR), 'IPCA');


    if (!pdoData->HardwareIDs) {
        IoDeleteDevice(pdo);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	RtlCopyMemory (pdoData->HardwareIDs, temp, index*sizeof(WCHAR));
    Bus_InitializePdo (pdo, FdoData);

    //
    // Device Relation changes if a new pdo is created. So let
    // the PNP system now about that. This forces it to send bunch of pnp
    // queries and cause the function driver to be loaded.
    //

    //IoInvalidateDeviceRelations (FdoData->UnderlyingPDO, BusRelations);

    return status;
}


/* looks alot like acpi_bus_walk doesnt it */
NTSTATUS
ACPIEnumerateDevices(PFDO_DEVICE_DATA DeviceExtension)
{
    ULONG Count = 0;
    struct acpi_device *Device = acpi_root;

    while(Device)
    {
        if (Device->status.present && Device->status.enabled &&
            Device->flags.hardware_id)
        {
            Bus_PlugInDevice(Device, DeviceExtension);
            Count++;
        }

        if (HAS_CHILDREN(Device)) {
            Device = NODE_TO_DEVICE(Device->children.next);
            continue;
        }
        if (HAS_SIBLINGS(Device)) {
            Device = NODE_TO_DEVICE(Device->node.next);
            continue;
        }
        while ((Device = Device->parent)) {
            if (HAS_SIBLINGS(Device)) {
                Device = NODE_TO_DEVICE(Device->node.next);
                break;
            }
        }
    }
    DPRINT("acpi device count: %d\n", Count);
    return STATUS_SUCCESS;
}

/* EOF */
