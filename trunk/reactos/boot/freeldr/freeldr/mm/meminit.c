/*
 *  FreeLoader
 *  Copyright (C) 2006-2008     Aleksey Bragin  <aleksey@reactos.org>
 *  Copyright (C) 2006-2009     Herv� Poussineau  <hpoussin@reactos.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <freeldr.h>
#include <debug.h>

DBG_DEFAULT_CHANNEL(MEMORY);

#if DBG
typedef struct
{
    TYPE_OF_MEMORY Type;
    PCSTR TypeString;
} FREELDR_MEMORY_TYPE, *PFREELDR_MEMORY_TYPE;

FREELDR_MEMORY_TYPE MemoryTypeArray[] =
{
    { LoaderMaximum, "Unknown memory" },
    { LoaderFree, "Free memory" },
    { LoaderBad, "Bad memory" },
    { LoaderLoadedProgram, "LoadedProgram" },
    { LoaderFirmwareTemporary, "FirmwareTemporary" },
    { LoaderFirmwarePermanent, "FirmwarePermanent" },
    { LoaderOsloaderHeap, "OsloaderHeap" },
    { LoaderOsloaderStack, "OsloaderStack" },
    { LoaderSystemCode, "SystemCode" },
    { LoaderHalCode, "HalCode" },
    { LoaderBootDriver, "BootDriver" },
    { LoaderRegistryData, "RegistryData" },
    { LoaderMemoryData, "MemoryData" },
    { LoaderNlsData, "NlsData" },
    { LoaderSpecialMemory, "SpecialMemory" },
    { LoaderReserve, "Reserve" },
};
ULONG MemoryTypeCount = sizeof(MemoryTypeArray) / sizeof(MemoryTypeArray[0]);
#endif

PVOID	PageLookupTableAddress = NULL;
PFN_NUMBER TotalPagesInLookupTable = 0;
PFN_NUMBER FreePagesInLookupTable = 0;
PFN_NUMBER LastFreePageHint = 0;
PFN_NUMBER MmLowestPhysicalPage = 0xFFFFFFFF;
PFN_NUMBER MmHighestPhysicalPage = 0;

PFREELDR_MEMORY_DESCRIPTOR BiosMemoryMap;
ULONG BiosMemoryMapEntryCount;

extern ULONG_PTR	MmHeapPointer;
extern ULONG_PTR	MmHeapStart;

ULONG
AddMemoryDescriptor(
    IN OUT PFREELDR_MEMORY_DESCRIPTOR List,
    IN ULONG MaxCount,
    IN PFN_NUMBER BasePage,
    IN PFN_NUMBER PageCount,
    IN TYPE_OF_MEMORY MemoryType)
{
    ULONG i, c;
    PFN_NUMBER NextBase;
    TRACE("AddMemoryDescriptor(0x%lx-0x%lx [0x%lx pages])\n",
          BasePage, BasePage + PageCount, PageCount);

    /* Scan through all existing descriptors */
    for (i = 0, c = 0; (c < MaxCount) && (List[c].PageCount != 0); c++)
    {
        /* Count entries completely below the new range */
        if (List[i].BasePage + List[i].PageCount <= BasePage) i++;
    }

    /* Check if the list is full */
    if (c >= MaxCount) return c;

    /* Is there an existing descriptor starting before the new range */
    while ((i < c) && (List[i].BasePage <= BasePage))
    {
        /* The end of the existing one is the minimum for the new range */
        NextBase = List[i].BasePage + List[i].PageCount;

        /* Bail out, if everything is trimmed away */
        if ((BasePage + PageCount) <= NextBase) return c;

        /* Trim the naew range at the lower end */
        PageCount -= (NextBase - BasePage);
        BasePage = NextBase;

        /* Go to the next entry and repeat */
        i++;
    }

    ASSERT(PageCount > 0);

    /* Are there still entries above? */
    if (i < c)
    {
        /* Shift the following entries one up */
        RtlMoveMemory(&List[i+1], &List[i], (c - i) * sizeof(List[0]));

        /* Insert the new range */
        List[i].BasePage = BasePage;
        List[i].PageCount = min(PageCount, List[i+1].BasePage - BasePage);
        List[i].MemoryType = MemoryType;
        c++;

        TRACE("Inserting at i=%ld: (0x%lx:0x%lx)\n",
              i, List[i].BasePage, List[i].PageCount);

        /* Check if the range was trimmed */
        if (PageCount > List[i].PageCount)
        {
            /* Recursively process the trimmed part */
            c = AddMemoryDescriptor(List,
                                    MaxCount,
                                    BasePage + List[i].PageCount,
                                    PageCount - List[i].PageCount,
                                    MemoryType);
        }
    }
    else
    {
        /* We can simply add the range here */
        TRACE("Adding i=%ld: (0x%lx:0x%lx)\n", i, BasePage, PageCount);
        List[i].BasePage = BasePage;
        List[i].PageCount = PageCount;
        List[i].MemoryType = MemoryType;
        c++;
    }

    /* Return the new count */
    return c;
}

const FREELDR_MEMORY_DESCRIPTOR*
ArcGetMemoryDescriptor(const FREELDR_MEMORY_DESCRIPTOR* Current)
{
    if (Current == NULL)
    {
        return BiosMemoryMap;
    }
    else
    {
        Current++;
        if (Current->PageCount == 0) return NULL;
        return Current;
    }
}


BOOLEAN
MmCheckFreeldrImageFile()
{
    PIMAGE_NT_HEADERS NtHeaders;
    PIMAGE_FILE_HEADER FileHeader;
    PIMAGE_OPTIONAL_HEADER OptionalHeader;

    /* Get the NT headers */
    NtHeaders = RtlImageNtHeader(&__ImageBase);
    if (!NtHeaders)
    {
        ERR("Coult not get NtHeaders!\n");
        return FALSE;
    }

    /* Check the file header */
    FileHeader = &NtHeaders->FileHeader;
    if ((FileHeader->Machine != IMAGE_FILE_MACHINE_NATIVE) ||
        (FileHeader->NumberOfSections != FREELDR_SECTION_COUNT) ||
        (FileHeader->PointerToSymbolTable != 0) ||
        (FileHeader->NumberOfSymbols != 0) ||
        (FileHeader->NumberOfSymbols != 0) ||
        (FileHeader->SizeOfOptionalHeader != 0xE0))
    {
        return FALSE;
    }

    /* Check the optional header */
    OptionalHeader = &NtHeaders->OptionalHeader;
    if ((OptionalHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC) ||
        (OptionalHeader->Subsystem != 1) || // native
        (OptionalHeader->ImageBase != FREELDR_PE_BASE) ||
        (OptionalHeader->SizeOfImage > MAX_FREELDR_PE_SIZE) ||
        (OptionalHeader->SectionAlignment != OptionalHeader->FileAlignment))
    {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN MmInitializeMemoryManager(VOID)
{
#if DBG
	const FREELDR_MEMORY_DESCRIPTOR* MemoryDescriptor = NULL;
#endif

	TRACE("Initializing Memory Manager.\n");

	/* Check the freeldr binary */
	if (!MmCheckFreeldrImageFile())
	{
		FrLdrBugCheck(FREELDR_IMAGE_CORRUPTION);
	}

    BiosMemoryMap = MachVtbl.GetMemoryMap(&BiosMemoryMapEntryCount);

#if DBG
	// Dump the system memory map
	TRACE("System Memory Map (Base Address, Length, Type):\n");
	while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL)
	{
		TRACE("%x\t %x\t %s\n",
			MemoryDescriptor->BasePage * MM_PAGE_SIZE,
			MemoryDescriptor->PageCount * MM_PAGE_SIZE,
			MmGetSystemMemoryMapTypeString(MemoryDescriptor->MemoryType));
	}
#endif

	// Find address for the page lookup table
	TotalPagesInLookupTable = MmGetAddressablePageCountIncludingHoles();
	PageLookupTableAddress = MmFindLocationForPageLookupTable(TotalPagesInLookupTable);
	LastFreePageHint = MmHighestPhysicalPage;

	if (PageLookupTableAddress == 0)
	{
		// If we get here then we probably couldn't
		// find a contiguous chunk of memory big
		// enough to hold the page lookup table
		printf("Error initializing memory manager!\n");
		return FALSE;
	}

	// Initialize the page lookup table
	MmInitPageLookupTable(PageLookupTableAddress, TotalPagesInLookupTable);

	MmUpdateLastFreePageHint(PageLookupTableAddress, TotalPagesInLookupTable);

	FreePagesInLookupTable = MmCountFreePagesInLookupTable(PageLookupTableAddress,
                                                        TotalPagesInLookupTable);

	MmInitializeHeap(PageLookupTableAddress);

	TRACE("Memory Manager initialized. 0x%x pages available.\n", FreePagesInLookupTable);


	return TRUE;
}

#if DBG
PCSTR MmGetSystemMemoryMapTypeString(TYPE_OF_MEMORY Type)
{
	ULONG		Index;

	for (Index=1; Index<MemoryTypeCount; Index++)
	{
		if (MemoryTypeArray[Index].Type == Type)
		{
			return MemoryTypeArray[Index].TypeString;
		}
	}

	return MemoryTypeArray[0].TypeString;
}
#endif

PFN_NUMBER MmGetPageNumberFromAddress(PVOID Address)
{
	return ((ULONG_PTR)Address) / MM_PAGE_SIZE;
}

PFN_NUMBER MmGetAddressablePageCountIncludingHoles(VOID)
{
    const FREELDR_MEMORY_DESCRIPTOR* MemoryDescriptor = NULL;
    PFN_NUMBER PageCount;

    //
    // Go through the whole memory map to get max address
    //
    while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL)
    {
        //
        // Check if we got a higher end page address
        //
        if (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount > MmHighestPhysicalPage)
        {
            //
            // Yes, remember it if this is real memory
            //
            if (MemoryDescriptor->MemoryType == LoaderFree)
                MmHighestPhysicalPage = MemoryDescriptor->BasePage + MemoryDescriptor->PageCount;
        }

        //
        // Check if we got a higher (usable) start page address
        //
        if (MemoryDescriptor->BasePage < MmLowestPhysicalPage)
        {
            //
            // Yes, remember it if this is real memory
            //
            MmLowestPhysicalPage = MemoryDescriptor->BasePage;
        }
    }

    TRACE("lo/hi %lx %lx\n", MmLowestPhysicalPage, MmHighestPhysicalPage);
    PageCount = MmHighestPhysicalPage - MmLowestPhysicalPage;
    TRACE("MmGetAddressablePageCountIncludingHoles() returning 0x%x\n", PageCount);
    return PageCount;
}

PVOID MmFindLocationForPageLookupTable(PFN_NUMBER TotalPageCount)
{
    const FREELDR_MEMORY_DESCRIPTOR* MemoryDescriptor = NULL;
    SIZE_T PageLookupTableSize;
    PFN_NUMBER PageLookupTablePages;
    PFN_NUMBER PageLookupTableStartPage = 0;
    PVOID PageLookupTableMemAddress = NULL;

    // Calculate how much pages we need to keep the page lookup table
    PageLookupTableSize = TotalPageCount * sizeof(PAGE_LOOKUP_TABLE_ITEM);
    PageLookupTablePages = PageLookupTableSize / MM_PAGE_SIZE;

    // Search the highest memory block big enough to contain lookup table
    while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL)
    {
        // Continue, if memory is not free
        if (MemoryDescriptor->MemoryType != LoaderFree) continue;

        // Continue, if the block is not big enough?
        if (MemoryDescriptor->PageCount < PageLookupTablePages) continue;

        // Continue, if it is not at a higher address than previous address
        if (MemoryDescriptor->BasePage < PageLookupTableStartPage) continue;

        // Continue, if the address is too high
        if (MemoryDescriptor->BasePage >= MM_MAX_PAGE) continue;

        // Memory block is more suitable than the previous one
        PageLookupTableStartPage = MemoryDescriptor->BasePage;
        PageLookupTableMemAddress = (PVOID)((ULONG_PTR)
            (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount) * MM_PAGE_SIZE
            - PageLookupTableSize);
    }

    TRACE("MmFindLocationForPageLookupTable() returning 0x%x\n", PageLookupTableMemAddress);

    return PageLookupTableMemAddress;
}

VOID MmInitPageLookupTable(PVOID PageLookupTable, PFN_NUMBER TotalPageCount)
{
    const FREELDR_MEMORY_DESCRIPTOR* MemoryDescriptor = NULL;
    PFN_NUMBER PageLookupTableStartPage;
    PFN_NUMBER PageLookupTablePageCount;

    TRACE("MmInitPageLookupTable()\n");

    // Mark every page as allocated initially
    // We will go through and mark pages again according to the memory map
    // But this will mark any holes not described in the map as allocated
    MmMarkPagesInLookupTable(PageLookupTable, MmLowestPhysicalPage, TotalPageCount, LoaderFirmwarePermanent);

    // Parse the whole memory map
    while ((MemoryDescriptor = ArcGetMemoryDescriptor(MemoryDescriptor)) != NULL)
    {
        // Mark used pages in the lookup table

        if (MemoryDescriptor->BasePage + MemoryDescriptor->PageCount <= TotalPageCount)
        {
            TRACE("Marking pages 0x%lx-0x%lx as type %s\n",
                  MemoryDescriptor->BasePage,
                  MemoryDescriptor->BasePage + MemoryDescriptor->PageCount,
                  MmGetSystemMemoryMapTypeString(MemoryDescriptor->MemoryType));
            MmMarkPagesInLookupTable(PageLookupTable,
                                     MemoryDescriptor->BasePage,
                                     MemoryDescriptor->PageCount,
                                     MemoryDescriptor->MemoryType);
        }
        else
            TRACE("Ignoring pages 0x%lx-0x%lx (%s)\n",
                  MemoryDescriptor->BasePage,
                  MemoryDescriptor->BasePage + MemoryDescriptor->PageCount,
                  MmGetSystemMemoryMapTypeString(MemoryDescriptor->MemoryType));
    }

    // Mark the pages that the lookup table occupies as reserved
    PageLookupTableStartPage = MmGetPageNumberFromAddress(PageLookupTable);
    PageLookupTablePageCount = MmGetPageNumberFromAddress((PVOID)((ULONG_PTR)PageLookupTable + ROUND_UP(TotalPageCount * sizeof(PAGE_LOOKUP_TABLE_ITEM), MM_PAGE_SIZE))) - PageLookupTableStartPage;
    TRACE("Marking the page lookup table pages as reserved StartPage: 0x%x PageCount: 0x%x\n", PageLookupTableStartPage, PageLookupTablePageCount);
    MmMarkPagesInLookupTable(PageLookupTable, PageLookupTableStartPage, PageLookupTablePageCount, LoaderFirmwareTemporary);
}

VOID MmMarkPagesInLookupTable(PVOID PageLookupTable, PFN_NUMBER StartPage, PFN_NUMBER PageCount, TYPE_OF_MEMORY PageAllocated)
{
	PPAGE_LOOKUP_TABLE_ITEM RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER Index;
	TRACE("MmMarkPagesInLookupTable()\n");

    /* Validate the range */
    if ((StartPage < MmLowestPhysicalPage) ||
        ((StartPage + PageCount - 1) > MmHighestPhysicalPage))
    {
        ERR("Memory (0x%lx:0x%lx) outside of lookup table! Valid range: 0x%lx-0x%lx.\n",
            StartPage, PageCount, MmLowestPhysicalPage, MmHighestPhysicalPage);
        return;
    }

    StartPage -= MmLowestPhysicalPage;
	for (Index=StartPage; Index<(StartPage+PageCount); Index++)
	{
#if 0
		if ((Index <= (StartPage + 16)) || (Index >= (StartPage+PageCount-16)))
		{
			TRACE("Index = 0x%x StartPage = 0x%x PageCount = 0x%x\n", Index, StartPage, PageCount);
		}
#endif
		RealPageLookupTable[Index].PageAllocated = PageAllocated;
		RealPageLookupTable[Index].PageAllocationLength = (PageAllocated != LoaderFree) ? 1 : 0;
	}
	TRACE("MmMarkPagesInLookupTable() Done\n");
}

VOID MmAllocatePagesInLookupTable(PVOID PageLookupTable, PFN_NUMBER StartPage, PFN_NUMBER PageCount, TYPE_OF_MEMORY MemoryType)
{
	PPAGE_LOOKUP_TABLE_ITEM		RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER					Index;

    StartPage -= MmLowestPhysicalPage;
	for (Index=StartPage; Index<(StartPage+PageCount); Index++)
	{
		RealPageLookupTable[Index].PageAllocated = MemoryType;
		RealPageLookupTable[Index].PageAllocationLength = (Index == StartPage) ? PageCount : 0;
	}
}

PFN_NUMBER MmCountFreePagesInLookupTable(PVOID PageLookupTable, PFN_NUMBER TotalPageCount)
{
	PPAGE_LOOKUP_TABLE_ITEM		RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER							Index;
	PFN_NUMBER							FreePageCount;

	FreePageCount = 0;
	for (Index=0; Index<TotalPageCount; Index++)
	{
		if (RealPageLookupTable[Index].PageAllocated == LoaderFree)
		{
			FreePageCount++;
		}
	}

	return FreePageCount;
}

PFN_NUMBER MmFindAvailablePages(PVOID PageLookupTable, PFN_NUMBER TotalPageCount, PFN_NUMBER PagesNeeded, BOOLEAN FromEnd)
{
	PPAGE_LOOKUP_TABLE_ITEM RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER AvailablePagesSoFar;
	PFN_NUMBER Index;

	if (LastFreePageHint > TotalPageCount)
	{
		LastFreePageHint = TotalPageCount;
	}

	AvailablePagesSoFar = 0;
	if (FromEnd)
	{
		/* Allocate "high" (from end) pages */
		for (Index=LastFreePageHint-1; Index>0; Index--)
		{
			if (RealPageLookupTable[Index].PageAllocated != LoaderFree)
			{
				AvailablePagesSoFar = 0;
				continue;
			}
			else
			{
				AvailablePagesSoFar++;
			}

			if (AvailablePagesSoFar >= PagesNeeded)
			{
				return Index + MmLowestPhysicalPage;
			}
		}
	}
	else
	{
		TRACE("Alloc low memory, LastFreePageHint 0x%x, TPC 0x%x\n", LastFreePageHint, TotalPageCount);
		/* Allocate "low" pages */
		for (Index=1; Index < LastFreePageHint; Index++)
		{
			if (RealPageLookupTable[Index].PageAllocated != LoaderFree)
			{
				AvailablePagesSoFar = 0;
				continue;
			}
			else
			{
				AvailablePagesSoFar++;
			}

			if (AvailablePagesSoFar >= PagesNeeded)
			{
				return Index - AvailablePagesSoFar + 1 + MmLowestPhysicalPage;
			}
		}
	}

	return 0;
}

PFN_NUMBER MmFindAvailablePagesBeforePage(PVOID PageLookupTable, PFN_NUMBER TotalPageCount, PFN_NUMBER PagesNeeded, PFN_NUMBER LastPage)
{
	PPAGE_LOOKUP_TABLE_ITEM		RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER					AvailablePagesSoFar;
	PFN_NUMBER					Index;

	if (LastPage > TotalPageCount)
	{
		return MmFindAvailablePages(PageLookupTable, TotalPageCount, PagesNeeded, TRUE);
	}

	AvailablePagesSoFar = 0;
	for (Index=LastPage-1; Index>0; Index--)
	{
		if (RealPageLookupTable[Index].PageAllocated != LoaderFree)
		{
			AvailablePagesSoFar = 0;
			continue;
		}
		else
		{
			AvailablePagesSoFar++;
		}

		if (AvailablePagesSoFar >= PagesNeeded)
		{
			return Index + MmLowestPhysicalPage;
		}
	}

	return 0;
}

VOID MmUpdateLastFreePageHint(PVOID PageLookupTable, PFN_NUMBER TotalPageCount)
{
	PPAGE_LOOKUP_TABLE_ITEM		RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER							Index;

	for (Index=TotalPageCount-1; Index>0; Index--)
	{
		if (RealPageLookupTable[Index].PageAllocated == LoaderFree)
		{
			LastFreePageHint = Index + 1 + MmLowestPhysicalPage;
			break;
		}
	}
}

BOOLEAN MmAreMemoryPagesAvailable(PVOID PageLookupTable, PFN_NUMBER TotalPageCount, PVOID PageAddress, PFN_NUMBER PageCount)
{
	PPAGE_LOOKUP_TABLE_ITEM		RealPageLookupTable = (PPAGE_LOOKUP_TABLE_ITEM)PageLookupTable;
	PFN_NUMBER							StartPage;
	PFN_NUMBER							Index;

	StartPage = MmGetPageNumberFromAddress(PageAddress);

	if (StartPage < MmLowestPhysicalPage) return FALSE;

    StartPage -= MmLowestPhysicalPage;

	// Make sure they aren't trying to go past the
	// end of availabe memory
	if ((StartPage + PageCount) > TotalPageCount)
	{
		return FALSE;
	}

	for (Index = StartPage; Index < (StartPage + PageCount); Index++)
	{
		// If this page is allocated then there obviously isn't
		// memory availabe so return FALSE
		if (RealPageLookupTable[Index].PageAllocated != LoaderFree)
		{
			return FALSE;
		}
	}

	return TRUE;
}
