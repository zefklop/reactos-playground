/*
 *  ReactOS kernel
 *  Copyright (C) 2002 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* $Id: dirctl.c,v 1.2 2002/04/26 23:21:28 ekohl Exp $
 *
 * COPYRIGHT:        See COPYING in the top level directory
 * PROJECT:          ReactOS kernel
 * FILE:             services/fs/cdfs/dirctl.c
 * PURPOSE:          CDROM (ISO 9660) filesystem driver
 * PROGRAMMER:       Art Yerkes
 * UPDATE HISTORY: 
 */

/* INCLUDES *****************************************************************/

#include <ddk/ntddk.h>

#define NDEBUG
#include <debug.h>

#include "cdfs.h"


/* FUNCTIONS ****************************************************************/


static BOOLEAN
wstrcmpjoki(PWSTR s1, PWSTR s2)
/*
 * FUNCTION: Compare two wide character strings, s2 with jokers (* or ?)
 * return TRUE if s1 like s2
 */
{
  while ((*s2=='*')||(*s2=='?')||(towlower(*s1)==towlower(*s2)))
    {
      if ((*s1)==0 && (*s2)==0)
        return(TRUE);

      if(*s2=='*')
	{
	  s2++;
	  while (*s1)
	    if (wstrcmpjoki(s1,s2))
	      return TRUE;
	    else
	      s1++;
	}
      else
	{
	  s1++;
	  s2++;
	}
    }

  if ((*s2)=='.')
    {
      for (;((*s2)=='.')||((*s2)=='*')||((*s2)=='?');s2++)
	;
    }

  if ((*s1)==0 && (*s2)==0)
    return(TRUE);

  return(FALSE);
}


static void
CdfsSwapString(PWCHAR Out,
	       PUCHAR In,
	       ULONG Count)
{
  PUCHAR t = (PUCHAR)Out;
  ULONG i;

  for (i = 0; i < Count; i += 2)
    {
      t[i] = In[i+1];
      t[i+1] = In[i];
    }
  t[i] = 0;
  t[i+1] = 0;
}


static VOID
CdfsDateTimeToFileTime(PFCB Fcb,
		       TIME *FileTime)
{
  TIME_FIELDS TimeFields;

  TimeFields.Milliseconds = 0;
  TimeFields.Second = Fcb->Entry.Second;
  TimeFields.Minute = Fcb->Entry.Minute;
  TimeFields.Hour = Fcb->Entry.Hour;

  TimeFields.Day = Fcb->Entry.Day;
  TimeFields.Month = Fcb->Entry.Month;
  TimeFields.Year = Fcb->Entry.Year + 1900;

  RtlTimeFieldsToTime(&TimeFields,
		      (PLARGE_INTEGER)FileTime);
}


NTSTATUS
CdfsGetEntryName(PDEVICE_EXTENSION DeviceExt,
		 PVOID Block,
		 ULONG BlockLength,
		 PVOID *Ptr,
		 PWSTR Name,
		 PULONG pIndex,
		 PULONG pIndex2)
/*
 * FUNCTION: Retrieves the file name, be it in short or long file name format
 */
{
  PDIR_RECORD Record;
  NTSTATUS Status;
  ULONG Index = 0;
  ULONG Offset = 0;

  Record = (PDIR_RECORD)Block;
  while(Index < *pIndex)
    {
      Offset = Offset + Record->RecordLength;

      Record = (PDIR_RECORD)(Block + Offset);
      if (Record->RecordLength == 0)
	{
	  Offset = ROUND_UP(Offset, 2048);
	  Record = (PDIR_RECORD)(Block + Offset);
	}

      if (Offset >= BlockLength)
	return(STATUS_NO_MORE_ENTRIES);

      Index++;
    }

  DPRINT("Index %lu  RecordLength %lu  Offset %lu\n",
	 Index, Record->RecordLength, Offset);

  if (Record->FileIdLength == 1 && Record->FileId[0] == 0)
    {
      wcscpy(Name, L".");
    }
  else if (Record->FileIdLength == 1 && Record->FileId[0] == 1)
    {
      wcscpy(Name, L"..");
    }
  else
    {
      if (DeviceExt->CdInfo.JolietLevel == 0)
	{
	  ULONG i;

	  for (i = 0; i < Record->FileIdLength && Record->FileId[i] != ';'; i++)
	    Name[i] = (WCHAR)Record->FileId[i];
	  Name[i] = 0;
	}
      else
	{
	  CdfsSwapString(Name, Record->FileId, Record->FileIdLength);
	}
    }

  DPRINT("Name '%S'\n", Name);

  *Ptr = Record;

  *pIndex = Index;

  return(STATUS_SUCCESS);
}


NTSTATUS
CdfsFindFile(PDEVICE_EXTENSION DeviceExt,
	     PFCB Fcb,
	     PFCB Parent,
	     PWSTR FileToFind,
	     PULONG pDirIndex,
	     PULONG pDirIndex2)
/*
 * FUNCTION: Find a file
 */
{
  WCHAR name[256];
  char * block;
  WCHAR TempStr[2];
  NTSTATUS Status;
  ULONG len;
  ULONG DirIndex;
  ULONG Offset;
  ULONG FirstSector;
  ULONG Read;
  BOOLEAN IsRoot;
  LARGE_INTEGER FileOffset;
  PVOID Context = NULL;

  ULONG DirSize;
  ULONG BufferSize;
  ULONG SectorCount;
  PUCHAR Ptr;
  PDIR_RECORD Record;

  DPRINT("FindFile(Parent %x, FileToFind '%S', DirIndex: %d)\n", Parent, FileToFind, pDirIndex ? *pDirIndex : 0);
  DPRINT("FindFile: old Pathname %x, old Objectname %x)\n",Fcb->PathName, Fcb->ObjectName);

  IsRoot = FALSE;
  DirIndex = 0;
  if (wcslen (FileToFind) == 0)
    {
      CHECKPOINT;
      TempStr[0] = (WCHAR) '.';
      TempStr[1] = 0;
      FileToFind = (PWSTR)&TempStr;
    }

  if (Parent)
    {
      FirstSector = Parent->Entry.ExtentLocationL;
      if (FirstSector == DeviceExt->CdInfo.RootStart)
	{
	  IsRoot = TRUE;
	}
    }
  else
    {
      IsRoot = TRUE;
    }

  if (IsRoot == TRUE)
    {
      FirstSector = DeviceExt->CdInfo.RootStart;
      DirSize = DeviceExt->CdInfo.RootSize;


      if (FileToFind[0] == 0 || (FileToFind[0] == '\\' && FileToFind[1] == 0)
	  || (FileToFind[0] == '.' && FileToFind[1] == 0))
	{
	  /* it's root : complete essentials fields then return ok */
	  RtlZeroMemory(Fcb, sizeof(FCB));

	  Fcb->PathName[0]='\\';
	  Fcb->ObjectName = &Fcb->PathName[1];
	  Fcb->Entry.ExtentLocationL = DeviceExt->CdInfo.RootStart;
	  Fcb->Entry.DataLengthL = DeviceExt->CdInfo.RootSize;
	  Fcb->Entry.FileFlags = 0x02; //FILE_ATTRIBUTE_DIRECTORY;

	  if (pDirIndex)
	    *pDirIndex = 0;
	  if (pDirIndex2)
	    *pDirIndex2 = 0;
	  DPRINT("CdfsFindFile: new Pathname %S, new Objectname %S)\n",Fcb->PathName, Fcb->ObjectName);
	  return (STATUS_SUCCESS);
	}
    }
  else
    {
      FirstSector = Parent->Entry.ExtentLocationL;
      DirSize = Parent->Entry.DataLengthL;
    }

  DPRINT("FirstSector %lu  DirSize %lu\n", FirstSector, DirSize);

  if (pDirIndex && (*pDirIndex))
    DirIndex = *pDirIndex;

  BufferSize = ROUND_UP(DirSize, BLOCKSIZE);
  SectorCount = BufferSize / BLOCKSIZE;

  DPRINT("FirstSector %lu  DirSize %lu  BufferSize %lu  SectorCount %lu\n",
	 FirstSector, DirSize, BufferSize, SectorCount);

  block = ExAllocatePool(NonPagedPool, BufferSize);

  Status = CdfsReadSectors(DeviceExt->StorageDevice,
			   FirstSector,
			   SectorCount,
			   block);
  if (!NT_SUCCESS(Status))
    {
      DPRINT("Reading directory extent failed (Status %lx)\n", Status);
      ExFreePool(block);
      return(Status);
    }

  Ptr = (PUCHAR)block;
  while(TRUE)
    {
      Record = (PDIR_RECORD)Ptr;
      if (Record->RecordLength == 0)
	{
	  DPRINT1("Stopped!\n");
	  break;
	}
	
      DPRINT("RecordLength %u  ExtAttrRecordLength %u  NameLength %u\n",
	     Record->RecordLength, Record->ExtAttrRecordLength, Record->FileIdLength);

      Status = CdfsGetEntryName(DeviceExt, block, DirSize, (PVOID*)&Ptr, name, &DirIndex, pDirIndex2);
      if (Status == STATUS_NO_MORE_ENTRIES)
	{
	  break;
	}

//      DPRINT("Name '%S'\n", name);

      if (wstrcmpjoki(name, FileToFind)) /* || wstrcmpjoki (name2, FileToFind)) */
	{
	  if (Parent && Parent->PathName)
	    {
	      len = wcslen(Parent->PathName);
	      memcpy(Fcb->PathName, Parent->PathName, len*sizeof(WCHAR));
	      Fcb->ObjectName=&Fcb->PathName[len];
	      if (len != 1 || Fcb->PathName[0] != '\\')
		{
		  Fcb->ObjectName[0] = '\\';
		  Fcb->ObjectName = &Fcb->ObjectName[1];
		}
	    }
	  else
	    {
	      Fcb->ObjectName=Fcb->PathName;
	      Fcb->ObjectName[0]='\\';
	      Fcb->ObjectName=&Fcb->ObjectName[1];
	    }

	  DPRINT("PathName '%S'  ObjectName '%S'\n", Fcb->PathName, Fcb->ObjectName);

	  memcpy(&Fcb->Entry, Ptr, sizeof(DIR_RECORD));
	  wcsncpy(Fcb->ObjectName, name, MAX_PATH);
	  if (pDirIndex)
	    *pDirIndex = DirIndex;

	  DPRINT("FindFile: new Pathname %S, new Objectname %S, DirIndex %d\n",Fcb->PathName, Fcb->ObjectName, DirIndex);

	  ExFreePool(block);

	  return(STATUS_SUCCESS);
	}


      Ptr = Ptr + Record->RecordLength;
      DirIndex++;

      if (((ULONG)Ptr - (ULONG)block) >= DirSize)
	{
	  DPRINT("Stopped!\n");
	  break;
	}
    }

  ExFreePool(block);

  if (pDirIndex)
    *pDirIndex = DirIndex;

  return(STATUS_UNSUCCESSFUL);
}


static NTSTATUS
CdfsGetBothInformation(PFCB Fcb,
		       PDEVICE_EXTENSION DeviceExt,
		       PFILE_BOTH_DIRECTORY_INFORMATION Info,
		       ULONG BufferLength)
{
//  short i;
//  ULONGLONG AllocSize;
  ULONG Length;

  DPRINT("FileBothDirectoryInformation() called\n");

  Length = wcslen(Fcb->ObjectName) * sizeof(WCHAR);
  if ((sizeof (FILE_BOTH_DIRECTORY_INFORMATION) + Length) > BufferLength)
    return(STATUS_BUFFER_OVERFLOW);

  Info->FileNameLength = Length;
  Info->NextEntryOffset =
    ROUND_UP(sizeof(FILE_BOTH_DIRECTORY_INFORMATION) + Length, 4);
  memcpy(Info->FileName, Fcb->ObjectName, Length);

//  Info->FileIndex=;

  /* Convert file times */
  CdfsDateTimeToFileTime(Fcb,
			 &Info->CreationTime);
  CdfsDateTimeToFileTime(Fcb,
			 &Info->LastAccessTime);
  CdfsDateTimeToFileTime(Fcb,
			 &Info->LastWriteTime);
  CdfsDateTimeToFileTime(Fcb,
			 &Info->ChangeTime);

  Info->EndOfFile.QuadPart = Fcb->Entry.DataLengthL;

  /* Make AllocSize a rounded up multiple of the sector size */
//  AllocSize = ((Fcb->Entry.DataLengthL + BLOCKSIZE - 1) /
//	       BLOCKSIZE) * BLOCKSIZE;
//  Info->AllocationSize.QuadPart = AllocSize;
  Info->AllocationSize.QuadPart = ROUND_UP(Fcb->Entry.DataLengthL, BLOCKSIZE);

  /* FIXME: Convert file flags to file attributes */
  Info->FileAttributes = (Fcb->Entry.FileFlags & 0x02) ? FILE_ATTRIBUTE_DIRECTORY : 0;
  Info->EaSize = 0;

  /* FIXME: Copy or create a short file name */
#if 0
  for (i = 0; i < 8 && (pFcb->entry.Filename[i] != ' '); i++)
    pInfo->ShortName[i] = pFcb->entry.Filename[i];
  pInfo->ShortNameLength = i;
  pInfo->ShortName[i] = '.';
  for (i = 0; i < 3 && (pFcb->entry.Ext[i] != ' '); i++)
    pInfo->ShortName[i + 1 + pInfo->ShortNameLength] = pFcb->entry.Ext[i];
  if (i)
    pInfo->ShortNameLength += (i + 1);
  pInfo->ShortNameLength *= sizeof(WCHAR);
#endif

  Info->ShortName[0] = 0;
  Info->ShortNameLength = 0;

  return STATUS_SUCCESS;
}



static NTSTATUS
CdfsQueryDirectory(PDEVICE_OBJECT DeviceObject,
		   PIRP Irp)
{
  PDEVICE_EXTENSION DeviceExtension;
  LONG BufferLength = 0;
  PUNICODE_STRING SearchPattern = NULL;
  FILE_INFORMATION_CLASS FileInformationClass;
  ULONG FileIndex = 0;
  PUCHAR Buffer = NULL;
  PFILE_NAMES_INFORMATION Buffer0 = NULL;
  PFCB Fcb;
  PCCB Ccb;
  FCB TempFcb;
  BOOLEAN First = FALSE;
  PIO_STACK_LOCATION Stack;
  PFILE_OBJECT FileObject;
  NTSTATUS Status = STATUS_SUCCESS;

  DPRINT("CdfsQueryDirectory() called\n");

  DeviceExtension = DeviceObject->DeviceExtension;
  Stack = IoGetCurrentIrpStackLocation(Irp);
  FileObject = Stack->FileObject;

  Ccb = (PCCB)FileObject->FsContext2;
  Fcb = Ccb->Fcb;

  /* Obtain the callers parameters */
  BufferLength = Stack->Parameters.QueryDirectory.Length;
  SearchPattern = Stack->Parameters.QueryDirectory.FileName;
  FileInformationClass =
    Stack->Parameters.QueryDirectory.FileInformationClass;
  FileIndex = Stack->Parameters.QueryDirectory.FileIndex;


  if (SearchPattern != NULL)
    {
      if (!Ccb->DirectorySearchPattern)
	{
	  First = TRUE;
	  Ccb->DirectorySearchPattern =
	    ExAllocatePool(NonPagedPool, SearchPattern->Length + sizeof(WCHAR));
	  if (!Ccb->DirectorySearchPattern)
	    {
	      return(STATUS_INSUFFICIENT_RESOURCES);
	    }

	  memcpy(Ccb->DirectorySearchPattern,
		 SearchPattern->Buffer,
		 SearchPattern->Length);
	  Ccb->DirectorySearchPattern[SearchPattern->Length / sizeof(WCHAR)] = 0;
	}
    }
  else if (!Ccb->DirectorySearchPattern)
    {
      First = TRUE;
      Ccb->DirectorySearchPattern = ExAllocatePool(NonPagedPool, 2 * sizeof(WCHAR));
      if (!Ccb->DirectorySearchPattern)
	{
	  return(STATUS_INSUFFICIENT_RESOURCES);
	}
      Ccb->DirectorySearchPattern[0] = L'*';
      Ccb->DirectorySearchPattern[1] = 0;
    }
  DPRINT("Search pattern '%S'\n", Ccb->DirectorySearchPattern);

  /* Determine directory index */
  if (Stack->Flags & SL_INDEX_SPECIFIED)
    {
      Ccb->Entry = Ccb->CurrentByteOffset.u.LowPart;
    }
  else if (First || (Stack->Flags & SL_RESTART_SCAN))
    {
      Ccb->Entry = 0;
    }

  /* Determine Buffer for result */
  if (Irp->MdlAddress)
    {
      Buffer = MmGetSystemAddressForMdl(Irp->MdlAddress);
    }
  else
    {
      Buffer = Irp->UserBuffer;
    }
  DPRINT("Buffer=%x tofind=%S\n", Buffer, Ccb->DirectorySearchPattern);

  TempFcb.ObjectName = TempFcb.PathName;
  while (Status == STATUS_SUCCESS && BufferLength > 0)
    {
      Status = CdfsFindFile(DeviceExtension,
			    &TempFcb,
			    Fcb,
			    Ccb->DirectorySearchPattern,
			    &Ccb->Entry,
			    NULL);
      DPRINT("Found %S, Status=%x, entry %x\n", TempFcb.ObjectName, Status, Ccb->Entry);

      if (NT_SUCCESS(Status))
	{
	  switch (FileInformationClass)
	    {
	      case FileNameInformation:
		DPRINT1("FileNameInformation\n");
//		Status = VfatGetFileNameInformation(&TempFcb,
//		            (PFILE_NAMES_INFORMATION) Buffer, BufferLength);
//		break;

	      case FileDirectoryInformation:
		DPRINT1("FileDirectoryInformation\n");
//		Status = VfatGetFileDirectoryInformation(&TempFcb, DeviceExtension,
//		                 (PFILE_DIRECTORY_INFORMATION) Buffer, BufferLength);
		break;

	      case FileFullDirectoryInformation:
		DPRINT1("FileFullDirecrotyInformation\n");
//		Status = CdfsGetFullDirectoryInformation(&TempFcb,
//							 DeviceExtension,
//		                (PFILE_FULL_DIRECTORY_INFORMATION)Buffer, BufferLength);
		break;

	      case FileBothDirectoryInformation:
		Status = CdfsGetBothInformation(&TempFcb,
						DeviceExtension,
						(PFILE_BOTH_DIRECTORY_INFORMATION)Buffer,
						BufferLength);
		break;

	      default:
		Status = STATUS_INVALID_INFO_CLASS;
	    }

	  if (Status == STATUS_BUFFER_OVERFLOW)
	    {
	      if (Buffer0)
		{
		  Buffer0->NextEntryOffset = 0;
		}
	      break;
	    }
	}
      else
	{
	  if (Buffer0)
	    {
	      Buffer0->NextEntryOffset = 0;
	    }

	  if (First)
	    {
	      Status = STATUS_NO_SUCH_FILE;
	    }
	  else
	    {
	      Status = STATUS_NO_MORE_FILES;
	    }
	  break;
	}

      Buffer0 = (PFILE_NAMES_INFORMATION)Buffer;
      Buffer0->FileIndex = FileIndex++;
      Ccb->Entry++;

      if (Stack->Flags & SL_RETURN_SINGLE_ENTRY)
	{
	  break;
	}
      BufferLength -= Buffer0->NextEntryOffset;
      Buffer += Buffer0->NextEntryOffset;
    }

  if (Buffer0)
    {
      Buffer0->NextEntryOffset = 0;
    }

  if (FileIndex > 0)
    {
      Status = STATUS_SUCCESS;
    }

  return(Status);
}



NTSTATUS STDCALL
CdfsDirectoryControl(PDEVICE_OBJECT DeviceObject,
		     PIRP Irp)
{
  PIO_STACK_LOCATION Stack;
  NTSTATUS Status;

  DPRINT("CdfsDirectoryControl() called\n");

  Stack = IoGetCurrentIrpStackLocation(Irp);

  switch (Stack->MinorFunction)
    {
      case IRP_MN_QUERY_DIRECTORY:
	Status = CdfsQueryDirectory(DeviceObject,
				    Irp);
	break;

      case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
	DPRINT1("IRP_MN_NOTIFY_CHANGE_DIRECTORY\n");
	Status = STATUS_NOT_IMPLEMENTED;
	break;

      default:
	DPRINT1("CDFS: MinorFunction %d\n", Stack->MinorFunction);
	Status = STATUS_INVALID_DEVICE_REQUEST;
	break;
    }

  Irp->IoStatus.Status = Status;
  Irp->IoStatus.Information = 0;

  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return(Status);
}

/* EOF */
