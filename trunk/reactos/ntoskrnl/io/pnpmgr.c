/* $Id: pnpmgr.c,v 1.16 2003/09/28 12:52:53 navaraf Exp $
 *
 * COPYRIGHT:      See COPYING in the top level directory
 * PROJECT:        ReactOS kernel
 * FILE:           ntoskrnl/io/pnpmgr/pnpmgr.c
 * PURPOSE:        Initializes the PnP manager
 * PROGRAMMER:     Casper S. Hornstrup (chorns@users.sourceforge.net)
 * UPDATE HISTORY:
 *  16/04/2001 CSH Created
 */

/* INCLUDES ******************************************************************/

#include <ddk/ntddk.h>
#include <reactos/bugcodes.h>
#include <internal/io.h>
#include <internal/po.h>
#include <internal/ldr.h>
#include <internal/registry.h>
#include <internal/module.h>

#include <ole32/guiddef.h>
//#include <ddk/pnpfuncs.h>
#ifdef DEFINE_GUID
DEFINE_GUID(GUID_CLASS_COMPORT,          0x86e0d1e0L, 0x8089, 0x11d0, 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73);
DEFINE_GUID(GUID_SERENUM_BUS_ENUMERATOR, 0x4D36E978L, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);
#endif // DEFINE_GUID

//#define NDEBUG
#include <internal/debug.h>


/* GLOBALS *******************************************************************/

PDEVICE_NODE IopRootDeviceNode;
KSPIN_LOCK IopDeviceTreeLock;

/* DATA **********************************************************************/

PDRIVER_OBJECT IopRootDriverObject;

/* FUNCTIONS *****************************************************************/

/*
 * @unimplemented
 */
VOID
STDCALL
IoAdjustPagingPathCount(
  IN PLONG Count,
  IN BOOLEAN Increment)
{
}

NTSTATUS
IopQueryBusInformation(
  PDEVICE_OBJECT DeviceObject,
  PPNP_BUS_INFORMATION BusInformation)
{
  IO_STATUS_BLOCK IoStatusBlock;
  IO_STACK_LOCATION Stack;
  
  IoStatusBlock.Information = (ULONG)BusInformation;
  return IopInitiatePnpIrp(DeviceObject, &IoStatusBlock,
    IRP_MN_QUERY_BUS_INFORMATION, &Stack);
}

/*
 * @unimplemented
 */
NTSTATUS
STDCALL
IoGetDeviceProperty(
  IN PDEVICE_OBJECT DeviceObject,
  IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
  IN ULONG BufferLength,
  OUT PVOID PropertyBuffer,
  OUT PULONG ResultLength)
{
  PNP_BUS_INFORMATION BusInformation;
  NTSTATUS Status;

  DPRINT("IoGetDeviceProperty called");

  /*
   * Used IRPs:
   *  IRP_MN_QUERY_ID
   *  IRP_MN_QUERY_BUS_INFORMATION
   */
  switch (DeviceProperty)
  {
    /* Complete, untested */
    case DevicePropertyBusNumber:
      *ResultLength = sizeof(ULONG);
      if (BufferLength < sizeof(ULONG))
        return STATUS_BUFFER_TOO_SMALL;
      Status = IopQueryBusInformation(DeviceObject, &BusInformation);
      if (NT_SUCCESS(Status))
        *((ULONG *)PropertyBuffer) = BusInformation.BusNumber;
      return Status;

    /* Complete, untested */
    case DevicePropertyBusTypeGuid:
      *ResultLength = 39 * sizeof(WCHAR);
      if (BufferLength < (39 * sizeof(WCHAR)))
        return STATUS_BUFFER_TOO_SMALL;
      Status = IopQueryBusInformation(DeviceObject, &BusInformation);
      if (NT_SUCCESS(Status))
        swprintf((PWSTR)PropertyBuffer,
          L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
          BusInformation.BusTypeGuid.Data1,
          BusInformation.BusTypeGuid.Data2,
          BusInformation.BusTypeGuid.Data3,
          BusInformation.BusTypeGuid.Data4[0],
          BusInformation.BusTypeGuid.Data4[1],
          BusInformation.BusTypeGuid.Data4[2],
          BusInformation.BusTypeGuid.Data4[3],
          BusInformation.BusTypeGuid.Data4[4],
          BusInformation.BusTypeGuid.Data4[5],
          BusInformation.BusTypeGuid.Data4[6],
          BusInformation.BusTypeGuid.Data4[7]);
      return Status;

    /* Complete, untested */
    case DevicePropertyLegacyBusType:
      *ResultLength = sizeof(INTERFACE_TYPE);
      if (BufferLength < sizeof(INTERFACE_TYPE))
        return STATUS_BUFFER_TOO_SMALL;
      Status = IopQueryBusInformation(DeviceObject, &BusInformation);
      if (NT_SUCCESS(Status))
        memcpy(PropertyBuffer, &BusInformation.LegacyBusType,
          sizeof(INTERFACE_TYPE));
      return Status;

    case DevicePropertyAddress:
    case DevicePropertyBootConfiguration:
    case DevicePropertyBootConfigurationTranslated:
    case DevicePropertyClassGuid:
    case DevicePropertyClassName:
    case DevicePropertyCompatibleIDs:
    case DevicePropertyDeviceDescription:
    case DevicePropertyDriverKeyName:
    case DevicePropertyEnumeratorName: 
    case DevicePropertyFriendlyName:
    case DevicePropertyHardwareID:
    case DevicePropertyLocationInformation:
    case DevicePropertyManufacturer:
    case DevicePropertyPhysicalDeviceObjectName:
    case DevicePropertyUINumber:
       break;

    default:
       return STATUS_INVALID_PARAMETER_2;
  }

  return STATUS_NOT_IMPLEMENTED;
}

/*
 * @unimplemented
 */
VOID
STDCALL
IoInvalidateDeviceRelations(
  IN PDEVICE_OBJECT DeviceObject,
  IN DEVICE_RELATION_TYPE Type)
{
}

/*
 * @unimplemented
 */
VOID
STDCALL
IoInvalidateDeviceState(
  IN PDEVICE_OBJECT PhysicalDeviceObject)
{
}

/*
 * @unimplemented
 */
NTSTATUS
STDCALL
IoOpenDeviceRegistryKey(
  IN PDEVICE_OBJECT DeviceObject,
  IN ULONG DevInstKeyType,
  IN ACCESS_MASK DesiredAccess,
  OUT PHANDLE DevInstRegKey)
{
  return STATUS_NOT_IMPLEMENTED;
}

/*
 * @unimplemented
 */
VOID
STDCALL
IoRequestDeviceEject(
  IN PDEVICE_OBJECT PhysicalDeviceObject)
{
}


BOOLEAN
IopCreateUnicodeString(
  PUNICODE_STRING	Destination,
  PWSTR Source,
  POOL_TYPE PoolType)
{
  ULONG Length;

  if (!Source)
  {
    RtlInitUnicodeString(Destination, NULL);
    return TRUE;
  }

  Length = (wcslen(Source) + 1) * sizeof(WCHAR);

  Destination->Buffer = ExAllocatePool(PoolType, Length);

  if (Destination->Buffer == NULL)
  {
    return FALSE;
  }

  RtlCopyMemory(Destination->Buffer, Source, Length);

  Destination->MaximumLength = Length;

  Destination->Length = Length - sizeof(WCHAR);

  return TRUE;
}

NTSTATUS
IopGetSystemPowerDeviceObject(PDEVICE_OBJECT *DeviceObject)
{
  KIRQL OldIrql;

  assert(PopSystemPowerDeviceNode);

  KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);
  *DeviceObject = PopSystemPowerDeviceNode->Pdo;
  KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);

  return STATUS_SUCCESS;
}

/**********************************************************************
 * DESCRIPTION
 * 	Creates a device node
 *
 * ARGUMENTS
 *   ParentNode           = Pointer to parent device node
 *   PhysicalDeviceObject = Pointer to PDO for device object. Pass NULL
 *                          to have the root device node create one
 *                          (eg. for legacy drivers)
 *   DeviceNode           = Pointer to storage for created device node
 *
 * RETURN VALUE
 * 	Status
 */
NTSTATUS
IopCreateDeviceNode(PDEVICE_NODE ParentNode,
  PDEVICE_OBJECT PhysicalDeviceObject,
  PDEVICE_NODE *DeviceNode)
{
  PDEVICE_NODE Node;
  NTSTATUS Status;
  KIRQL OldIrql;

  DPRINT("ParentNode %x PhysicalDeviceObject %x\n",
    ParentNode, PhysicalDeviceObject);

  Node = (PDEVICE_NODE)ExAllocatePool(PagedPool, sizeof(DEVICE_NODE));
  if (!Node)
    {
      return STATUS_INSUFFICIENT_RESOURCES;
    }

  RtlZeroMemory(Node, sizeof(DEVICE_NODE));

  if (!PhysicalDeviceObject)
    {
      Status = PnpRootCreateDevice(&PhysicalDeviceObject);
      if (!NT_SUCCESS(Status))
        {
          ExFreePool(Node);
          return Status;
        }

      /* This is for drivers passed on the command line to ntoskrnl.exe */
      IopDeviceNodeSetFlag(Node, DNF_STARTED);
      IopDeviceNodeSetFlag(Node, DNF_LEGACY_DRIVER);
    }

  Node->Pdo = PhysicalDeviceObject;

  if (ParentNode)
    {
      KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);
      Node->Parent = ParentNode;
      Node->NextSibling = ParentNode->Child;
      if (ParentNode->Child != NULL)
	{
	  ParentNode->Child->PrevSibling = Node;	  
	}
      ParentNode->Child = Node;
      KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);
    }

  *DeviceNode = Node;

  return STATUS_SUCCESS;
}

NTSTATUS
IopFreeDeviceNode(PDEVICE_NODE DeviceNode)
{
  KIRQL OldIrql;

  /* All children must be deleted before a parent is deleted */
  assert(!DeviceNode->Child);

  KeAcquireSpinLock(&IopDeviceTreeLock, &OldIrql);

  assert(DeviceNode->Pdo);

  ObDereferenceObject(DeviceNode->Pdo);

  /* Unlink from parent if it exists */

  if ((DeviceNode->Parent) && (DeviceNode->Parent->Child == DeviceNode))
    {
      DeviceNode->Parent->Child = DeviceNode->NextSibling;
    }

  /* Unlink from sibling list */

  if (DeviceNode->PrevSibling)
    {
      DeviceNode->PrevSibling->NextSibling = DeviceNode->NextSibling;
    }

  if (DeviceNode->NextSibling)
    {
  DeviceNode->NextSibling->PrevSibling = DeviceNode->PrevSibling;
    }

  KeReleaseSpinLock(&IopDeviceTreeLock, OldIrql);

  RtlFreeUnicodeString(&DeviceNode->InstancePath);

  RtlFreeUnicodeString(&DeviceNode->ServiceName);

  if (DeviceNode->CapabilityFlags)
    {
  ExFreePool(DeviceNode->CapabilityFlags);
    }

  if (DeviceNode->CmResourceList)
    {
  ExFreePool(DeviceNode->CmResourceList);
    }

  if (DeviceNode->BootResourcesList)
    {
  ExFreePool(DeviceNode->BootResourcesList);
    }

  if (DeviceNode->ResourceRequirementsList)
    {
  ExFreePool(DeviceNode->ResourceRequirementsList);
    }

  RtlFreeUnicodeString(&DeviceNode->DeviceID);

  RtlFreeUnicodeString(&DeviceNode->InstanceID);

  RtlFreeUnicodeString(&DeviceNode->HardwareIDs);

  RtlFreeUnicodeString(&DeviceNode->CompatibleIDs);

  RtlFreeUnicodeString(&DeviceNode->DeviceText);

  RtlFreeUnicodeString(&DeviceNode->DeviceTextLocation);

  if (DeviceNode->BusInformation)
    {
  ExFreePool(DeviceNode->BusInformation);
    }

  ExFreePool(DeviceNode);

  return STATUS_SUCCESS;
}

NTSTATUS
IopInitiatePnpIrp(
  PDEVICE_OBJECT DeviceObject,
  PIO_STATUS_BLOCK IoStatusBlock,
  ULONG MinorFunction,
  PIO_STACK_LOCATION Stack OPTIONAL)
{
  PDEVICE_OBJECT TopDeviceObject;
  PIO_STACK_LOCATION IrpSp;
  NTSTATUS Status;
  KEVENT Event;
  PIRP Irp;

  /* Always call the top of the device stack */
  TopDeviceObject = IoGetAttachedDeviceReference(DeviceObject);

  KeInitializeEvent(
    &Event,
	  NotificationEvent,
	  FALSE);

  /* PNP IRPs are always initialized with a status code of
     STATUS_NOT_IMPLEMENTED */
  IoStatusBlock->Status = STATUS_NOT_IMPLEMENTED;
  IoStatusBlock->Information = 0;

  Irp = IoBuildSynchronousFsdRequest(
    IRP_MJ_PNP,
    TopDeviceObject,
	  NULL,
	  0,
	  NULL,
	  &Event,
	  IoStatusBlock);

  IrpSp = IoGetNextIrpStackLocation(Irp);
  IrpSp->MinorFunction = MinorFunction;

  if (Stack)
  {
    RtlMoveMemory(
      &IrpSp->Parameters,
      &Stack->Parameters,
      sizeof(Stack->Parameters));
  }

	Status = IoCallDriver(TopDeviceObject, Irp);
	if (Status == STATUS_PENDING)
	  {
		  KeWaitForSingleObject(
        &Event,
        Executive,
		    KernelMode,
		    FALSE,
		    NULL);
      Status = IoStatusBlock->Status;
    }

  ObDereferenceObject(TopDeviceObject);

  return Status;
}


NTSTATUS
IopQueryCapabilities(
  PDEVICE_OBJECT Pdo,
  PDEVICE_CAPABILITIES *Capabilities)
{
  IO_STATUS_BLOCK	IoStatusBlock;
  PDEVICE_CAPABILITIES Caps;
  IO_STACK_LOCATION Stack;
  NTSTATUS Status;

  DPRINT("Sending IRP_MN_QUERY_CAPABILITIES to device stack\n");

  *Capabilities = NULL;

  Caps = ExAllocatePool(PagedPool, sizeof(DEVICE_CAPABILITIES));
  if (!Caps)
  {
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  RtlZeroMemory(Caps, sizeof(DEVICE_CAPABILITIES));
  Caps->Size = sizeof(DEVICE_CAPABILITIES);
  Caps->Version = 1;
  Caps->Address = -1;
  Caps->UINumber = -1;

  Stack.Parameters.DeviceCapabilities.Capabilities = Caps;

  Status = IopInitiatePnpIrp(
    Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_CAPABILITIES,
    &Stack);
  if (NT_SUCCESS(Status))
  {
    *Capabilities = Caps;
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
  }

  return Status;
}


NTSTATUS
IopTraverseDeviceTreeNode(
  PDEVICETREE_TRAVERSE_CONTEXT Context)
{
  PDEVICE_NODE ParentDeviceNode;
  PDEVICE_NODE ChildDeviceNode;
  NTSTATUS Status;

  /* Copy context data so we don't overwrite it in subsequent calls to this function */
  ParentDeviceNode = Context->DeviceNode;

  /* Call the action routine */
  Status = (Context->Action)(ParentDeviceNode, Context->Context);
  if (!NT_SUCCESS(Status))
  {
    return Status;
  }

  /* Traversal of all children nodes */
  for (ChildDeviceNode = ParentDeviceNode->Child;
       ChildDeviceNode != NULL;
       ChildDeviceNode = ChildDeviceNode->NextSibling)
  {
    /* Pass the current device node to the action routine */
    Context->DeviceNode = ChildDeviceNode;

    Status = IopTraverseDeviceTreeNode(Context);
    if (!NT_SUCCESS(Status))
    {
      return Status;
    }
  }

  return Status;
}


NTSTATUS
IopTraverseDeviceTree(
  PDEVICETREE_TRAVERSE_CONTEXT Context)
{
  NTSTATUS Status;

  DPRINT("Context %x\n", Context);

  DPRINT("IopTraverseDeviceTree(DeviceNode %x  FirstDeviceNode %x  Action %x  Context %x)\n",
    Context->DeviceNode, Context->FirstDeviceNode, Context->Action, Context->Context);

  /* Start from the specified device node */
  Context->DeviceNode = Context->FirstDeviceNode;

  /* Recursively traverse the device tree */
  Status = IopTraverseDeviceTreeNode(Context);
  if (Status == STATUS_UNSUCCESSFUL)
  {
    /* The action routine just wanted to terminate the traversal with status
       code STATUS_SUCCESS */
    Status = STATUS_SUCCESS;
  }

  return Status;
}


NTSTATUS
IopActionInterrogateDeviceStack(
  PDEVICE_NODE DeviceNode,
  PVOID Context)
/*
 * FUNCTION: Retrieve information for all (direct) child nodes of a parent node
 * ARGUMENTS:
 *   DeviceNode = Pointer to device node
 *   Context    = Pointer to parent node to retrieve child node information for
 * NOTES:
 *   We only return a status code indicating an error (STATUS_UNSUCCESSFUL)
 *   when we reach a device node which is not a direct child of the device node
 *   for which we retrieve information of child nodes for. Any errors that occur
 *   is logged instead so that all child services have a chance of beeing
 *   interrogated.
 */
{
  IO_STATUS_BLOCK	IoStatusBlock;
  PDEVICE_NODE ParentDeviceNode;
  WCHAR InstancePath[MAX_PATH];
  IO_STACK_LOCATION Stack;
  NTSTATUS Status;
  PWSTR KeyBuffer;

  DPRINT("DeviceNode %x  Context %x\n", DeviceNode, Context);

  DPRINT("PDO %x\n", DeviceNode->Pdo);


  ParentDeviceNode = (PDEVICE_NODE)Context;

  /* We are called for the parent too, but we don't need to do special
     handling for this node */
  if (DeviceNode == ParentDeviceNode)
  {
    DPRINT("Success\n");
    return STATUS_SUCCESS;
  }

  /* Make sure this device node is a direct child of the parent device node
     that is given as an argument */
  if (DeviceNode->Parent != ParentDeviceNode)
  {
    /* Stop the traversal immediately and indicate successful operation */
    DPRINT("Stop\n");
    return STATUS_UNSUCCESSFUL;
  }


  /* FIXME: For critical errors, cleanup and disable device, but always return STATUS_SUCCESS */


  DPRINT("Sending IRP_MN_QUERY_ID.BusQueryDeviceID to device stack\n");

  Stack.Parameters.QueryId.IdType = BusQueryDeviceID;

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_ID,
    &Stack);
  if (NT_SUCCESS(Status))
  {
    RtlInitUnicodeString(
      &DeviceNode->DeviceID,
      (LPWSTR)IoStatusBlock.Information);

    /* FIXME: Check for valid characters, if there is invalid characters then bugcheck */
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    RtlInitUnicodeString(&DeviceNode->DeviceID, NULL);
  }


  DPRINT("Sending IRP_MN_QUERY_ID.BusQueryInstanceID to device stack\n");

  Stack.Parameters.QueryId.IdType = BusQueryInstanceID;

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_ID,
    &Stack);
  if (NT_SUCCESS(Status))
  {
    RtlInitUnicodeString(
      &DeviceNode->InstanceID,
      (LPWSTR)IoStatusBlock.Information);

    /* FIXME: Check for valid characters, if there is invalid characters then bugcheck */
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    RtlInitUnicodeString(&DeviceNode->InstanceID, NULL);
  }


  /* FIXME: SEND IRP_QUERY_ID.BusQueryHardwareIDs */
  /* FIXME: SEND IRP_QUERY_ID.BusQueryCompatibleIDs */


  Status = IopQueryCapabilities(DeviceNode->Pdo, &DeviceNode->CapabilityFlags);
  if (NT_SUCCESS(Status))
  {
  }
  else
  {
  }


  DPRINT("Sending IRP_MN_QUERY_DEVICE_TEXT.DeviceTextDescription to device stack\n");

  Stack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;
  Stack.Parameters.QueryDeviceText.LocaleId = 0; // FIXME

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_DEVICE_TEXT,
    &Stack);
  if (NT_SUCCESS(Status))
  {
    RtlInitUnicodeString(
      &DeviceNode->DeviceText,
      (LPWSTR)IoStatusBlock.Information);
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    RtlInitUnicodeString(&DeviceNode->DeviceText, NULL);
  }


  DPRINT("Sending IRP_MN_QUERY_DEVICE_TEXT.DeviceTextLocation to device stack\n");

  Stack.Parameters.QueryDeviceText.DeviceTextType = DeviceTextLocationInformation;
  Stack.Parameters.QueryDeviceText.LocaleId = 0; // FIXME

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_DEVICE_TEXT,
    &Stack);
  if (NT_SUCCESS(Status))
  {
    RtlInitUnicodeString(
      &DeviceNode->DeviceTextLocation,
      (LPWSTR)IoStatusBlock.Information);
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    RtlInitUnicodeString(&DeviceNode->DeviceTextLocation, NULL);
  }


  DPRINT("Sending IRP_MN_QUERY_BUS_INFORMATION to device stack\n");

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_BUS_INFORMATION,
    NULL);
  if (NT_SUCCESS(Status))
  {
    DeviceNode->BusInformation =
      (PPNP_BUS_INFORMATION)IoStatusBlock.Information;
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    DeviceNode->BusInformation = NULL;
  }


  DPRINT("Sending IRP_MN_QUERY_RESOURCES to device stack\n");

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_RESOURCES,
    NULL);
  if (NT_SUCCESS(Status))
  {
    DeviceNode->BootResourcesList =
      (PCM_RESOURCE_LIST)IoStatusBlock.Information;
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    DeviceNode->BootResourcesList = NULL;
  }


  DPRINT("Sending IRP_MN_QUERY_RESOURCE_REQUIREMENTS to device stack\n");

  Status = IopInitiatePnpIrp(
    DeviceNode->Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_RESOURCE_REQUIREMENTS,
    NULL);
  if (NT_SUCCESS(Status))
  {
    DeviceNode->ResourceRequirementsList =
      (PIO_RESOURCE_REQUIREMENTS_LIST)IoStatusBlock.Information;
  }
  else
  {
    DPRINT("IopInitiatePnpIrp() failed (Status %x)\n", Status);
    DeviceNode->ResourceRequirementsList = NULL;
  }


  /* Assemble the instance path for the device */

  wcscpy(InstancePath, DeviceNode->DeviceID.Buffer);
  wcscat(InstancePath, L"\\");
  wcscat(InstancePath, DeviceNode->InstanceID.Buffer);

  if (!DeviceNode->CapabilityFlags->UniqueID)
  {
    DPRINT("Instance ID is not unique\n");
    /* FIXME: Add information from parent bus driver to InstancePath */
  }

  if (!IopCreateUnicodeString(&DeviceNode->InstancePath, InstancePath, PagedPool)) {
    DPRINT("No resources\n");
    /* FIXME: Cleanup and disable device */
  }

  DPRINT("InstancePath is %S\n", DeviceNode->InstancePath.Buffer);

  /*
   * Create registry key for the instance id, if it doesn't exist yet
   */  
  KeyBuffer = ExAllocatePool(PagedPool, (49 + DeviceNode->InstancePath.Length) * sizeof(WCHAR));
  wcscpy(KeyBuffer, L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\");
  wcscat(KeyBuffer, DeviceNode->InstancePath.Buffer);  
  RtlpCreateRegistryKeyPath(KeyBuffer);
  ExFreePool(KeyBuffer);
  DeviceNode->Flags |= DNF_PROCESSED;

  return STATUS_SUCCESS;
}


NTSTATUS
IopActionConfigureChildServices(
  PDEVICE_NODE DeviceNode,
  PVOID Context)
/*
 * FUNCTION: Retrieve configuration for all (direct) child nodes of a parent node
 * ARGUMENTS:
 *   DeviceNode = Pointer to device node
 *   Context    = Pointer to parent node to retrieve child node configuration for
 * NOTES:
 *   We only return a status code indicating an error (STATUS_UNSUCCESSFUL)
 *   when we reach a device node which is not a direct child of the device
 *   node for which we configure child services for. Any errors that occur is
 *   logged instead so that all child services have a chance of beeing
 *   configured.
 */
{
/*
 * FIXME: There are two versions of this function: one that creates registry
 * and one that doesn't. The second is the right, but could not be used because
 * there is not automatic parent key generation.
 *
 * Update: It could propably be used now, but there's no need anymore.
 *
 * FiN
 */
#if 1
  RTL_QUERY_REGISTRY_TABLE QueryTable[2];
  PDEVICE_NODE ParentDeviceNode;
  PUNICODE_STRING Service;
  NTSTATUS Status;

  DPRINT("DeviceNode %x  Context %x\n", DeviceNode, Context);

  ParentDeviceNode = (PDEVICE_NODE)Context;

  /* We are called for the parent too, but we don't need to do special
     handling for this node */
  if (DeviceNode == ParentDeviceNode)
  {
    DPRINT("Success\n");
    return STATUS_SUCCESS;
  }

  /* Make sure this device node is a direct child of the parent device node
     that is given as an argument */
  if (DeviceNode->Parent != ParentDeviceNode)
  {
    /* Stop the traversal immediately and indicate successful operation */
    DPRINT("Stop\n");
    return STATUS_UNSUCCESSFUL;
  }

  /* Retrieve configuration from Enum key */

  Service = &DeviceNode->ServiceName;

  RtlZeroMemory(QueryTable, sizeof(QueryTable));

  RtlInitUnicodeString(Service, NULL);

  QueryTable[0].Name = L"Service";
  QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
  QueryTable[0].EntryContext = Service;

  Status = RtlQueryRegistryValues(
	 	RTL_REGISTRY_ENUM,
	 	DeviceNode->InstancePath.Buffer,
	 	QueryTable,
	 	NULL,
	 	NULL);

  DPRINT("RtlQueryRegistryValues() returned status %x\n", Status);
  DPRINT("Key: %S\n", DeviceNode->InstancePath.Buffer);

  if (!NT_SUCCESS(Status))
  {
    /* FIXME: Log the error */
    CPRINT("Could not retrieve configuration for device %S (Status %x)\n",
      DeviceNode->InstancePath.Buffer, Status);
    IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
    return STATUS_SUCCESS;
  }

  if (Service->Buffer == NULL)
  {
    IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
    return STATUS_SUCCESS;
  }

  DPRINT("Got Service %S\n", Service->Buffer);

  return STATUS_SUCCESS;
#else
  RTL_QUERY_REGISTRY_TABLE QueryTable[2];
  PDEVICE_NODE ParentDeviceNode;
  PUNICODE_STRING Service;
  HANDLE KeyHandle;
  NTSTATUS Status;

  DPRINT("DeviceNode %x  Context %x\n", DeviceNode, Context);

  ParentDeviceNode = (PDEVICE_NODE)Context;

  /* We are called for the parent too, but we don't need to do special
     handling for this node */
  if (DeviceNode == ParentDeviceNode)
  {
    DPRINT("Success\n");
    return STATUS_SUCCESS;
  }

  /* Make sure this device node is a direct child of the parent device node
     that is given as an argument */
  if (DeviceNode->Parent != ParentDeviceNode)
  {
    /* Stop the traversal immediately and indicate successful operation */
    DPRINT("Stop\n");
    return STATUS_UNSUCCESSFUL;
  }

  /* Retrieve configuration from Enum key */

  Service = &DeviceNode->ServiceName;

  Status = RtlpGetRegistryHandle(
    RTL_REGISTRY_ENUM,
	  DeviceNode->InstancePath.Buffer,
		TRUE,
		&KeyHandle);
  if (!NT_SUCCESS(Status))
  {
    DPRINT("RtlpGetRegistryHandle() failed (Status %x)\n", Status);
    return Status;
  }

  RtlZeroMemory(QueryTable, sizeof(QueryTable));

  RtlInitUnicodeString(Service, NULL);

  QueryTable[0].Name = L"Service";
  QueryTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
  QueryTable[0].EntryContext = Service;

  Status = RtlQueryRegistryValues(
    RTL_REGISTRY_HANDLE,
	 	(PWSTR)KeyHandle,
	 	QueryTable,
	 	NULL,
	 	NULL);
  NtClose(KeyHandle);

  DPRINT("RtlQueryRegistryValues() returned status %x\n", Status);

  if (!NT_SUCCESS(Status))
  {
    /* FIXME: Log the error */
    CPRINT("Could not retrieve configuration for device %S (Status %x)\n",
      DeviceNode->InstancePath.Buffer, Status);
    IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);
    return STATUS_SUCCESS;
  }

  DPRINT("Got Service %S\n", Service->Buffer);

  return STATUS_SUCCESS;
#endif
}


NTSTATUS
IopActionInitChildServices(
  PDEVICE_NODE DeviceNode,
  PVOID Context)
/*
 * FUNCTION: Initialize the service for all (direct) child nodes of a parent node
 * ARGUMENTS:
 *   DeviceNode = Pointer to device node
 *   Context    = Pointer to parent node to initialize child node services for
 * NOTES:
 *   If the driver image for a service is not loaded and initialized
 *   it is done here too.
 *   We only return a status code indicating an error (STATUS_UNSUCCESSFUL)
 *   when we reach a device node which is not a direct child of the device
 *   node for which we initialize child services for. Any errors that occur is
 *   logged instead so that all child services have a chance of beeing
 *   initialized.
 */
{
  PDEVICE_NODE ParentDeviceNode;
  NTSTATUS Status;

  DPRINT("DeviceNode %x  Context %x\n", DeviceNode, Context);

  ParentDeviceNode = (PDEVICE_NODE)Context;

  /* We are called for the parent too, but we don't need to do special
     handling for this node */
  if (DeviceNode == ParentDeviceNode)
  {
    DPRINT("Success\n");
    return STATUS_SUCCESS;
  }

  /* Make sure this device node is a direct child of the parent device node
     that is given as an argument */
  if (DeviceNode->Parent != ParentDeviceNode)
  {
    /* Stop the traversal immediately and indicate successful operation */
    DPRINT("Stop\n");
    return STATUS_UNSUCCESSFUL;
  }

  if (!IopDeviceNodeHasFlag(DeviceNode, DNF_DISABLED) &&
    !IopDeviceNodeHasFlag(DeviceNode, DNF_ADDED) &&
    !IopDeviceNodeHasFlag(DeviceNode, DNF_STARTED))
  {
    Status = IopInitializeDeviceNodeService(DeviceNode);
    if (NT_SUCCESS(Status))
    {
      IopDeviceNodeSetFlag(DeviceNode, DNF_STARTED);
    }
    else
    {
      IopDeviceNodeSetFlag(DeviceNode, DNF_DISABLED);

      /* FIXME: Log the error (possibly in IopInitializeDeviceNodeService) */
      CPRINT("Initialization of service %S failed (Status %x)\n",
        DeviceNode->ServiceName.Buffer, Status);
    }
  }
  else
  {
    DPRINT("Service %S is disabled or already initialized\n",
        DeviceNode->ServiceName.Buffer);
  }

  return STATUS_SUCCESS;
}


NTSTATUS
IopInterrogateBusExtender(
  PDEVICE_NODE DeviceNode,
  PDEVICE_OBJECT Pdo,
  BOOLEAN BootDriversOnly)
{
  DEVICETREE_TRAVERSE_CONTEXT Context;
  PDEVICE_RELATIONS DeviceRelations;
	IO_STATUS_BLOCK	IoStatusBlock;
  PDEVICE_NODE ChildDeviceNode;
  IO_STACK_LOCATION Stack;
  NTSTATUS Status;
  ULONG i;

  DPRINT("DeviceNode %x  Pdo %x  BootDriversOnly %d\n", DeviceNode, Pdo, BootDriversOnly);

  DPRINT("Sending IRP_MN_QUERY_DEVICE_RELATIONS to device stack\n");

  Stack.Parameters.QueryDeviceRelations.Type = BusRelations;

  Status = IopInitiatePnpIrp(
    Pdo,
    &IoStatusBlock,
    IRP_MN_QUERY_DEVICE_RELATIONS,
    &Stack);
  if (!NT_SUCCESS(Status))
  {
    DPRINT("IopInitiatePnpIrp() failed\n");
    return Status;
  }

  DeviceRelations = (PDEVICE_RELATIONS)IoStatusBlock.Information;

  if ((!DeviceRelations) || (DeviceRelations->Count <= 0))
  {
    DPRINT("No PDOs\n");
    if (DeviceRelations)
    {
      ExFreePool(DeviceRelations);
    }
    return STATUS_SUCCESS;
  }

  DPRINT("Got %d PDOs\n", DeviceRelations->Count);

  /* Create device nodes for all discovered devices */
  for (i = 0; i < DeviceRelations->Count; i++)
  {
    Status = IopCreateDeviceNode(
      DeviceNode,
      DeviceRelations->Objects[i],
      &ChildDeviceNode);
    DeviceNode->Flags |= DNF_ENUMERATED;
    if (!NT_SUCCESS(Status))
    {
      DPRINT("No resources\n");
      for (i = 0; i < DeviceRelations->Count; i++)
        ObDereferenceObject(DeviceRelations->Objects[i]);
      ExFreePool(DeviceRelations);
      return STATUS_INSUFFICIENT_RESOURCES;
    }
  }

  ExFreePool(DeviceRelations);


  /* Retrieve information about all discovered children from the bus driver */

  IopInitDeviceTreeTraverseContext(
    &Context,
    DeviceNode,
    IopActionInterrogateDeviceStack,
    DeviceNode);

  Status = IopTraverseDeviceTree(&Context);
  if (!NT_SUCCESS(Status))
  {
	  DPRINT("IopTraverseDeviceTree() failed with status (%x)\n", Status);
    return Status;
  }


  /* Retrieve configuration from the registry for discovered children */

  IopInitDeviceTreeTraverseContext(
    &Context,
    DeviceNode,
    IopActionConfigureChildServices,
    DeviceNode);

  Status = IopTraverseDeviceTree(&Context);
  if (!NT_SUCCESS(Status))
  {
	  DPRINT("IopTraverseDeviceTree() failed with status (%x)\n", Status);
    return Status;
  }


  /* Initialize services for discovered children */

  IopInitDeviceTreeTraverseContext(
    &Context,
    DeviceNode,
    IopActionInitChildServices,
    DeviceNode);

  Status = IopTraverseDeviceTree(&Context);
  if (!NT_SUCCESS(Status))
  {
	  DPRINT("IopTraverseDeviceTree() failed with status (%x)\n", Status);
    return Status;
  }

  return STATUS_SUCCESS;
}

VOID IopLoadBootStartDrivers(VOID)
{
  IopInterrogateBusExtender(
    IopRootDeviceNode,
    IopRootDeviceNode->Pdo,
    TRUE);
}

VOID PnpInit(VOID)
{
  PDEVICE_OBJECT Pdo;
  NTSTATUS Status;

  DPRINT("Called\n");

  KeInitializeSpinLock(&IopDeviceTreeLock);

  Status = IopCreateDriverObject(&IopRootDriverObject, NULL, FALSE, NULL, 0);
  if (!NT_SUCCESS(Status))
  {
    CPRINT("IoCreateDriverObject() failed\n");
    KEBUGCHECK(PHASE1_INITIALIZATION_FAILED);
  }

  Status = IoCreateDevice(
    IopRootDriverObject,
    0,
    NULL,
    FILE_DEVICE_CONTROLLER,
    0,
    FALSE,
    &Pdo);
  if (!NT_SUCCESS(Status))
  {
    CPRINT("IoCreateDevice() failed\n");
    KEBUGCHECK(PHASE1_INITIALIZATION_FAILED);
  }

  Status = IopCreateDeviceNode(
    NULL,
    Pdo,
    &IopRootDeviceNode);
  if (!NT_SUCCESS(Status))
  {
    CPRINT("Insufficient resources\n");
    KEBUGCHECK(PHASE1_INITIALIZATION_FAILED);
  }

  IopRootDeviceNode->Pdo->Flags |= DO_BUS_ENUMERATED_DEVICE;

  IopRootDeviceNode->DriverObject = IopRootDriverObject;

  PnpRootDriverEntry(IopRootDriverObject, NULL);

  IopRootDriverObject->DriverExtension->AddDevice(
    IopRootDriverObject,
    IopRootDeviceNode->Pdo);
}

/* EOF */
