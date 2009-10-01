/*
 * PROJECT:         ReactOS FAT file system driver
 * LICENSE:         GPL - See COPYING in the top level directory
 * FILE:            drivers/filesystems/fastfat/rw.c
 * PURPOSE:         Read/write support
 * PROGRAMMERS:     Aleksey Bragin (aleksey@reactos.org)
 *                  Alexey Vlasov
 */

/* INCLUDES *****************************************************************/

#define NDEBUG
#include "fastfat.h"

/* FUNCTIONS *****************************************************************/

NTSTATUS
NTAPI
FatiRead(PFAT_IRP_CONTEXT IrpContext)
{
    ULONG NumberOfBytes;
    LARGE_INTEGER ByteOffset;
    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN OpenType;
    PIO_STACK_LOCATION IrpSp = IrpContext->Stack;
    PFCB Fcb;
    PVCB Vcb;
    PCCB Ccb;
    PVOID Buffer;
    LONG BytesRead;

    FileObject = IrpSp->FileObject;
    NumberOfBytes = IrpSp->Parameters.Read.Length;
    ByteOffset = IrpSp->Parameters.Read.ByteOffset;
    if (NumberOfBytes == 0)
    {
        FatCompleteRequest(IrpContext, IrpContext->Irp, STATUS_SUCCESS);
        return STATUS_SUCCESS;
    }
    
    OpenType = FatDecodeFileObject(FileObject, &Vcb, &Fcb, &Ccb);

    DPRINT1("FatiRead() Fcb %p, Name %wZ, Offset %d, Length %d, Handle %p\n",
        Fcb, &FileObject->FileName, ByteOffset.LowPart, NumberOfBytes, Fcb->FatHandle);

    /* Perform actual read */

    if (IrpContext->MinorFunction & IRP_MN_MDL)
    {
        DPRINT1("MDL read\n");
    }
    else
    {
        Buffer = FatMapUserBuffer(IrpContext->Irp);
        DPRINT1("Normal cached read, buffer %p\n");

        BytesRead = FF_Read(Fcb->FatHandle, NumberOfBytes, 1, Buffer);
        DPRINT1("Read %d bytes\n", BytesRead);

        /* Indicate we read requested amount of bytes */
        IrpContext->Irp->IoStatus.Information = BytesRead;
        IrpContext->Irp->IoStatus.Status = STATUS_SUCCESS;
    }

    /* Complete the request */
    FatCompleteRequest(IrpContext, IrpContext->Irp, STATUS_SUCCESS);
    return STATUS_SUCCESS;
}

NTSTATUS
NTAPI
FatRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS Status;
    BOOLEAN TopLevel, CanWait;
    PFAT_IRP_CONTEXT IrpContext;

    CanWait = TRUE;
    TopLevel = FALSE;
    Status = STATUS_INVALID_DEVICE_REQUEST;
    /* Get CanWait flag */
    if (IoGetCurrentIrpStackLocation(Irp)->FileObject != NULL)
        CanWait = IoIsOperationSynchronous(Irp);

    /* Enter FsRtl critical region */
    FsRtlEnterFileSystem();

    if (DeviceObject != FatGlobalData.DiskDeviceObject)
    {
        /* Set Top Level IRP if not set */
        if (IoGetTopLevelIrp() == NULL)
        {
            IoSetTopLevelIrp(Irp);
            TopLevel = TRUE;
        }

        /* Build an irp context */
        IrpContext = FatBuildIrpContext(Irp, CanWait);

        /* Perform the actual read */
        Status = FatiRead(IrpContext);

        /* Restore top level Irp */
        if (TopLevel)
            IoSetTopLevelIrp(NULL);
    }
    /* Leave FsRtl critical region */
    FsRtlExitFileSystem();

    DPRINT1("FatRead()\n");
    return Status;
}

NTSTATUS
NTAPI
FatWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    DPRINT1("FatWrite()\n");
    return STATUS_NOT_IMPLEMENTED;
}


