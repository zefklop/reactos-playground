/*
 * Win32 definitions for Windows NT
 *
 * Copyright 1996 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_WINNT_H
#define __WINE_WINNT_H

#include_next <winnt.h>

/* non standard; keep the number high enough (but < 0xff) */
#define LANG_ESPERANTO			 0x8f
#define LANG_WALON			 0x90
#define LANG_CORNISH                     0x91
#define LANG_WELSH                       0x92
#define LANG_BRETON                      0x93

#define WINE_UNUSED   __attribute__((unused))

static inline struct _TEB * NtCurrentTeb(void)
{
 struct _TEB * pTeb;

 /* FIXME: instead of hardcoded offsets, use offsetof() - if possible */
 __asm__ __volatile__
 (
  "movl %%fs:0x18, %0\n" /* fs:18h == Teb->Tib.Self */
  : "=r" (pTeb) /* can't have two memory operands */
  : /* no inputs */
 );

 return pTeb;
}

#endif  /* __WINE_WINNT_H */
