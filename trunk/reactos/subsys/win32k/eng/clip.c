/*
 * COPYRIGHT:         See COPYING in the top level directory
 * PROJECT:           ReactOS kernel
 * PURPOSE:           GDI Clipping Functions
 * FILE:              subsys/win32k/eng/clip.c
 * PROGRAMER:         Jason Filby
 * REVISION HISTORY:
 *                 21/8/1999: Created
 */

#include <ddk/winddi.h>
#include <ddk/ntddk.h>
#include "objects.h"
#include "clip.h"
#include <include/object.h>

// FIXME: Complex clipping doesn't work

CLIPOBJ *EngCreateClipRegion(ULONG NumRects, RECTL Rects[],
                             ULONG Mode, ULONG Options)
{
  HCLIP NewClip;
  CLIPOBJ *ClipObj;
  CLIPGDI *ClipGDI;

  ClipObj = EngAllocMem(FL_ZERO_MEMORY, sizeof(CLIPOBJ), 0);
  ClipGDI = EngAllocMem(FL_ZERO_MEMORY, sizeof(CLIPGDI), 0);

  NewClip = CreateGDIHandle(ClipGDI, ClipObj);

  ClipGDI->NumRegionRects  = NumRects;
  ClipGDI->RegionRects     = Rects;
  ClipObj->iMode           = Mode;
  ClipObj->fjOptions       = Options;
  ClipObj->iDComplexity    = DC_TRIVIAL;

  if(NumRects == 1)
  {
    ClipObj->iFComplexity = FC_RECT;
    ClipObj->iDComplexity = DC_RECT;

    // FIXME: Is this correct??
    ClipObj->rclBounds = Rects[0];
  } else
  {
    ClipObj->iDComplexity = DC_COMPLEX;
    if(NumRects <= 4)
    {
      ClipObj->iFComplexity = FC_RECT4;
    } else
    {
      ClipObj->iFComplexity = FC_COMPLEX;
    }
  }

  return ClipObj;
}

VOID EngDeleteClipRegion(CLIPOBJ *ClipObj)
{
  HCLIP HClip      = AccessHandleFromUserObject(ClipObj);
  CLIPGDI *ClipGDI = (CLIPGDI*)AccessInternalObject(HClip);

  EngFreeMem(ClipGDI);
  EngFreeMem(ClipObj);
  FreeGDIHandle(HClip);
}

VOID EngIntersectClipRegion(CLIPOBJ *ClipObj, ULONG NumRects, RECTL *IntersectRects)
{
  CLIPGDI *ClipGDI = (CLIPGDI*)AccessInternalObjectFromUserObject(ClipObj);

  ClipGDI->NumIntersectRects = NumRects;
  ClipGDI->IntersectRects    = IntersectRects;

  if(NumRects == 1)
  {
    ClipObj->iDComplexity = DC_RECT;
    ClipObj->rclBounds = IntersectRects[0];
  } else
  {
    ClipObj->iDComplexity = DC_COMPLEX;
    ClipGDI->IntersectRects = IntersectRects;
  }
}

CLIPOBJ * STDCALL
EngCreateClip(VOID)
{
  return EngAllocMem(FL_ZERO_MEMORY, sizeof(CLIPOBJ), 0);
}

VOID STDCALL
EngDeleteClip(CLIPOBJ *ClipRegion)
{
  EngFreeMem(ClipRegion);
}

ULONG STDCALL
CLIPOBJ_cEnumStart(IN PCLIPOBJ ClipObj,
		   IN BOOL ShouldDoAll,
		   IN ULONG ClipType,
		   IN ULONG BuildOrder,
		   IN ULONG MaxRects)
{
  CLIPGDI *ClipGDI = (CLIPGDI*)AccessInternalObjectFromUserObject(ClipObj);

  ClipGDI->EnumPos     = 0;
  ClipGDI->EnumRects.c = MaxRects;

  // Return the number of rectangles enumerated
  if(ClipGDI->EnumRects.c>MaxRects)
  {
    ClipGDI->EnumRects.c = 0xFFFFFFFF;
  }

  return ClipGDI->EnumRects.c;
}

BOOL STDCALL
CLIPOBJ_bEnum(IN PCLIPOBJ ClipObj,
	      IN ULONG ObjSize,
	      OUT ULONG *EnumRects)
{
  CLIPGDI *ClipGDI = (CLIPGDI*)AccessInternalObjectFromUserObject(ClipObj);

  ClipGDI->EnumPos++;

  if(ClipGDI->EnumPos > ClipGDI->EnumRects.c)
  {
    return FALSE;
  } else
    return TRUE;
}
