/*
 * File elf.c - processing of ELF files
 *
 * Copyright (C) 1996, Eric Youngdale.
 *		 1999-2007 Eric Pouech
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#if defined(__svr4__) || defined(__sun)
#define __ELF__ 1
/* large files are not supported by libelf */
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#include "dbghelp_private.h"

#include "image_private.h"

#include "wine/library.h"
#include "wine/debug.h"

#ifdef __ELF__

#define ELF_INFO_DEBUG_HEADER   0x0001
#define ELF_INFO_MODULE         0x0002
#define ELF_INFO_NAME           0x0004

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

struct elf_info
{
    unsigned                    flags;          /* IN  one (or several) of the ELF_INFO constants */
    DWORD_PTR                   dbg_hdr_addr;   /* OUT address of debug header (if ELF_INFO_DEBUG_HEADER is set) */
    struct module*              module;         /* OUT loaded module (if ELF_INFO_MODULE is set) */
    const WCHAR*                module_name;    /* OUT found module name (if ELF_INFO_NAME is set) */
};

struct symtab_elt
{
    struct hash_table_elt       ht_elt;
    const Elf_Sym*              symp;
    struct symt_compiland*      compiland;
    unsigned                    used;
};

struct elf_thunk_area
{
    const char*                 symname;
    THUNK_ORDINAL               ordinal;
    unsigned long               rva_start;
    unsigned long               rva_end;
};

struct elf_module_info
{
    unsigned long               elf_addr;
    unsigned short	        elf_mark : 1,
                                elf_loader : 1;
    struct image_file_map       file_map;
};

/******************************************************************
 *		elf_map_section
 *
 * Maps a single section into memory from an ELF file
 */
const char* elf_map_section(struct image_section_map* ism)
{
    struct elf_file_map*        fmap = &ism->fmap->u.elf;

    unsigned pgsz = getpagesize();
    unsigned ofst, size;

    assert(ism->fmap->modtype == DMT_ELF);
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.elf.elfhdr.e_shnum ||
        fmap->sect[ism->sidx].shdr.sh_type == SHT_NOBITS)
        return IMAGE_NO_MAP;

    /* align required information on page size (we assume pagesize is a power of 2) */
    ofst = fmap->sect[ism->sidx].shdr.sh_offset & ~(pgsz - 1);
    size = ((fmap->sect[ism->sidx].shdr.sh_offset +
             fmap->sect[ism->sidx].shdr.sh_size + pgsz - 1) & ~(pgsz - 1)) - ofst;
    fmap->sect[ism->sidx].mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE,
                                        fmap->fd, ofst);
    if (fmap->sect[ism->sidx].mapped == IMAGE_NO_MAP) return IMAGE_NO_MAP;
    return fmap->sect[ism->sidx].mapped + (fmap->sect[ism->sidx].shdr.sh_offset & (pgsz - 1));
}

/******************************************************************
 *		elf_find_section
 *
 * Finds a section by name (and type) into memory from an ELF file
 * or its alternate if any
 */
BOOL elf_find_section(struct image_file_map* _fmap, const char* name,
                      unsigned sht, struct image_section_map* ism)
{
    struct elf_file_map*        fmap;
    unsigned i;

    while (_fmap)
    {
        fmap = &_fmap->u.elf;
        if (fmap->shstrtab == IMAGE_NO_MAP)
        {
            struct image_section_map  hdr_ism = {_fmap, fmap->elfhdr.e_shstrndx};
            if ((fmap->shstrtab = elf_map_section(&hdr_ism)) == IMAGE_NO_MAP) break;
        }
        for (i = 0; i < fmap->elfhdr.e_shnum; i++)
        {
            if (strcmp(fmap->shstrtab + fmap->sect[i].shdr.sh_name, name) == 0 &&
                (sht == SHT_NULL || sht == fmap->sect[i].shdr.sh_type))
            {
                ism->fmap = _fmap;
                ism->sidx = i;
                return TRUE;
            }
        }
        _fmap = fmap->alternate;
    }
    ism->fmap = NULL;
    ism->sidx = -1;
    return FALSE;
}

/******************************************************************
 *		elf_unmap_section
 *
 * Unmaps a single section from memory
 */
void elf_unmap_section(struct image_section_map* ism)
{
    struct elf_file_map*        fmap = &ism->fmap->u.elf;

    if (ism->sidx >= 0 && ism->sidx < fmap->elfhdr.e_shnum && fmap->sect[ism->sidx].mapped != IMAGE_NO_MAP)
    {
        unsigned pgsz = getpagesize();
        unsigned ofst, size;

        ofst = fmap->sect[ism->sidx].shdr.sh_offset & ~(pgsz - 1);
        size = ((fmap->sect[ism->sidx].shdr.sh_offset +
             fmap->sect[ism->sidx].shdr.sh_size + pgsz - 1) & ~(pgsz - 1)) - ofst;
        if (munmap((char*)fmap->sect[ism->sidx].mapped, size) < 0)
            WARN("Couldn't unmap the section\n");
        fmap->sect[ism->sidx].mapped = IMAGE_NO_MAP;
    }
}

static void elf_end_find(struct image_file_map* fmap)
{
    struct image_section_map      ism;

    while (fmap)
    {
        ism.fmap = fmap;
        ism.sidx = fmap->u.elf.elfhdr.e_shstrndx;
        elf_unmap_section(&ism);
        fmap->u.elf.shstrtab = IMAGE_NO_MAP;
        fmap = fmap->u.elf.alternate;
    }
}

/******************************************************************
 *		elf_get_map_rva
 *
 * Get the RVA of an ELF section
 */
DWORD_PTR elf_get_map_rva(const struct image_section_map* ism)
{
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.elf.elfhdr.e_shnum)
        return 0;
    return ism->fmap->u.elf.sect[ism->sidx].shdr.sh_addr - ism->fmap->u.elf.elf_start;
}

/******************************************************************
 *		elf_get_map_size
 *
 * Get the size of an ELF section
 */
unsigned elf_get_map_size(const struct image_section_map* ism)
{
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.elf.elfhdr.e_shnum)
        return 0;
    return ism->fmap->u.elf.sect[ism->sidx].shdr.sh_size;
}

static inline void elf_reset_file_map(struct image_file_map* fmap)
{
    fmap->u.elf.fd = -1;
    fmap->u.elf.shstrtab = IMAGE_NO_MAP;
    fmap->u.elf.alternate = NULL;
}

/******************************************************************
 *		elf_map_file
 *
 * Maps an ELF file into memory (and checks it's a real ELF file)
 */
static BOOL elf_map_file(const WCHAR* filenameW, struct image_file_map* fmap)
{
    static const BYTE   elf_signature[4] = { ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3 };
    struct stat	        statbuf;
    int                 i;
    Elf_Phdr            phdr;
    unsigned            tmp, page_mask = getpagesize() - 1;
    char*               filename;
    unsigned            len;
    BOOL                ret = FALSE;

    len = WideCharToMultiByte(CP_UNIXCP, 0, filenameW, -1, NULL, 0, NULL, NULL);
    if (!(filename = HeapAlloc(GetProcessHeap(), 0, len))) return FALSE;
    WideCharToMultiByte(CP_UNIXCP, 0, filenameW, -1, filename, len, NULL, NULL);

    elf_reset_file_map(fmap);

    fmap->modtype = DMT_ELF;
    /* check that the file exists, and that the module hasn't been loaded yet */
    if (stat(filename, &statbuf) == -1 || S_ISDIR(statbuf.st_mode)) goto done;

    /* Now open the file, so that we can mmap() it. */
    if ((fmap->u.elf.fd = open(filename, O_RDONLY)) == -1) goto done;

    if (read(fmap->u.elf.fd, &fmap->u.elf.elfhdr, sizeof(fmap->u.elf.elfhdr)) != sizeof(fmap->u.elf.elfhdr))
        goto done;
    /* and check for an ELF header */
    if (memcmp(fmap->u.elf.elfhdr.e_ident,
               elf_signature, sizeof(elf_signature))) goto done;
    /* and check 32 vs 64 size according to current machine */
#ifdef _WIN64
    if (fmap->u.elf.elfhdr.e_ident[EI_CLASS] != ELFCLASS64) goto done;
#else
    if (fmap->u.elf.elfhdr.e_ident[EI_CLASS] != ELFCLASS32) goto done;
#endif
    fmap->u.elf.sect = HeapAlloc(GetProcessHeap(), 0,
                                 fmap->u.elf.elfhdr.e_shnum * sizeof(fmap->u.elf.sect[0]));
    if (!fmap->u.elf.sect) goto done;

    lseek(fmap->u.elf.fd, fmap->u.elf.elfhdr.e_shoff, SEEK_SET);
    for (i = 0; i < fmap->u.elf.elfhdr.e_shnum; i++)
    {
        read(fmap->u.elf.fd, &fmap->u.elf.sect[i].shdr, sizeof(fmap->u.elf.sect[i].shdr));
        fmap->u.elf.sect[i].mapped = IMAGE_NO_MAP;
    }

    /* grab size of module once loaded in memory */
    lseek(fmap->u.elf.fd, fmap->u.elf.elfhdr.e_phoff, SEEK_SET);
    fmap->u.elf.elf_size = 0;
    fmap->u.elf.elf_start = ~0L;
    for (i = 0; i < fmap->u.elf.elfhdr.e_phnum; i++)
    {
        if (read(fmap->u.elf.fd, &phdr, sizeof(phdr)) == sizeof(phdr) &&
            phdr.p_type == PT_LOAD)
        {
            tmp = (phdr.p_vaddr + phdr.p_memsz + page_mask) & ~page_mask;
            if (fmap->u.elf.elf_size < tmp) fmap->u.elf.elf_size = tmp;
            if (phdr.p_vaddr < fmap->u.elf.elf_start) fmap->u.elf.elf_start = phdr.p_vaddr;
        }
    }
    /* if non relocatable ELF, then remove fixed address from computation
     * otherwise, all addresses are zero based and start has no effect
     */
    fmap->u.elf.elf_size -= fmap->u.elf.elf_start;
    ret = TRUE;
done:
    HeapFree(GetProcessHeap(), 0, filename);
    return ret;
}

/******************************************************************
 *		elf_unmap_file
 *
 * Unmaps an ELF file from memory (previously mapped with elf_map_file)
 */
static void elf_unmap_file(struct image_file_map* fmap)
{
    while (fmap)
    {
        if (fmap->u.elf.fd != -1)
        {
            struct image_section_map  ism;
            ism.fmap = fmap;
            for (ism.sidx = 0; ism.sidx < fmap->u.elf.elfhdr.e_shnum; ism.sidx++)
            {
                elf_unmap_section(&ism);
            }
            HeapFree(GetProcessHeap(), 0, fmap->u.elf.sect);
            close(fmap->u.elf.fd);
        }
        fmap = fmap->u.elf.alternate;
    }
}

static void elf_module_remove(struct process* pcs, struct module_format* modfmt)
{
    elf_unmap_file(&modfmt->u.elf_info->file_map);
    HeapFree(GetProcessHeap(), 0, modfmt);
}

/******************************************************************
 *		elf_is_in_thunk_area
 *
 * Check whether an address lies within one of the thunk area we
 * know of.
 */
int elf_is_in_thunk_area(unsigned long addr,
                         const struct elf_thunk_area* thunks)
{
    unsigned i;

    if (thunks) for (i = 0; thunks[i].symname; i++)
    {
        if (addr >= thunks[i].rva_start && addr < thunks[i].rva_end)
            return i;
    }
    return -1;
}

/******************************************************************
 *		elf_hash_symtab
 *
 * creating an internal hash table to ease use ELF symtab information lookup
 */
static void elf_hash_symtab(struct module* module, struct pool* pool,
                            struct hash_table* ht_symtab, struct image_file_map* fmap,
                            struct elf_thunk_area* thunks)
{
    int		                i, j, nsym;
    const char*                 strp;
    const char*                 symname;
    struct symt_compiland*      compiland = NULL;
    const char*                 ptr;
    const Elf_Sym*              symp;
    struct symtab_elt*          ste;
    struct image_section_map    ism, ism_str;

    if (!elf_find_section(fmap, ".symtab", SHT_SYMTAB, &ism) &&
        !elf_find_section(fmap, ".dynsym", SHT_DYNSYM, &ism)) return;
    if ((symp = (const Elf_Sym*)image_map_section(&ism)) == IMAGE_NO_MAP) return;
    ism_str.fmap = ism.fmap;
    ism_str.sidx = fmap->u.elf.sect[ism.sidx].shdr.sh_link;
    if ((strp = image_map_section(&ism_str)) == IMAGE_NO_MAP)
    {
        image_unmap_section(&ism);
        return;
    }

    nsym = image_get_map_size(&ism) / sizeof(*symp);

    for (j = 0; thunks[j].symname; j++)
        thunks[j].rva_start = thunks[j].rva_end = 0;

    for (i = 0; i < nsym; i++, symp++)
    {
        /* Ignore certain types of entries which really aren't of that much
         * interest.
         */
        if ((ELF32_ST_TYPE(symp->st_info) != STT_NOTYPE &&
             ELF32_ST_TYPE(symp->st_info) != STT_FILE &&
             ELF32_ST_TYPE(symp->st_info) != STT_OBJECT &&
             ELF32_ST_TYPE(symp->st_info) != STT_FUNC) ||
            symp->st_shndx == SHN_UNDEF)
        {
            continue;
        }

        symname = strp + symp->st_name;

        /* handle some specific symtab (that we'll throw away when done) */
        switch (ELF32_ST_TYPE(symp->st_info))
        {
        case STT_FILE:
            if (symname)
                compiland = symt_new_compiland(module, symp->st_value,
                                               source_new(module, NULL, symname));
            else
                compiland = NULL;
            continue;
        case STT_NOTYPE:
            /* we are only interested in wine markers inserted by winebuild */
            for (j = 0; thunks[j].symname; j++)
            {
                if (!strcmp(symname, thunks[j].symname))
                {
                    thunks[j].rva_start = symp->st_value;
                    thunks[j].rva_end   = symp->st_value + symp->st_size;
                    break;
                }
            }
            continue;
        }

        /* FIXME: we don't need to handle them (GCC internals)
         * Moreover, they screw up our symbol lookup :-/
         */
        if (symname[0] == '.' && symname[1] == 'L' && isdigit(symname[2]))
            continue;

        ste = pool_alloc(pool, sizeof(*ste));
        ste->ht_elt.name = symname;
        /* GCC emits, in some cases, a .<digit>+ suffix.
         * This is used for static variable inside functions, so
         * that we can have several such variables with same name in
         * the same compilation unit
         * We simply ignore that suffix when present (we also get rid
         * of it in stabs parsing)
         */
        ptr = symname + strlen(symname) - 1;
        if (isdigit(*ptr))
        {
            while (isdigit(*ptr) && ptr >= symname) ptr--;
            if (ptr > symname && *ptr == '.')
            {
                char* n = pool_alloc(pool, ptr - symname + 1);
                memcpy(n, symname, ptr - symname + 1);
                n[ptr - symname] = '\0';
                ste->ht_elt.name = n;
            }
        }
        ste->symp        = symp;
        ste->compiland   = compiland;
        ste->used        = 0;
        hash_table_add(ht_symtab, &ste->ht_elt);
    }
    /* as we added in the ht_symtab pointers to the symbols themselves,
     * we cannot unmap yet the sections, it will be done when we're over
     * with this ELF file
     */
}

/******************************************************************
 *		elf_lookup_symtab
 *
 * lookup a symbol by name in our internal hash table for the symtab
 */
static const Elf_Sym* elf_lookup_symtab(const struct module* module,
                                          const struct hash_table* ht_symtab,
                                          const char* name, const struct symt* compiland)
{
    struct symtab_elt*          weak_result = NULL; /* without compiland name */
    struct symtab_elt*          result = NULL;
    struct hash_table_iter      hti;
    struct symtab_elt*          ste;
    const char*                 compiland_name;
    const char*                 compiland_basename;
    const char*                 base;

    /* we need weak match up (at least) when symbols of same name, 
     * defined several times in different compilation units,
     * are merged in a single one (hence a different filename for c.u.)
     */
    if (compiland)
    {
        compiland_name = source_get(module,
                                    ((const struct symt_compiland*)compiland)->source);
        compiland_basename = strrchr(compiland_name, '/');
        if (!compiland_basename++) compiland_basename = compiland_name;
    }
    else compiland_name = compiland_basename = NULL;
    
    hash_table_iter_init(ht_symtab, &hti, name);
    while ((ste = hash_table_iter_up(&hti)))
    {
        if (ste->used || strcmp(ste->ht_elt.name, name)) continue;

        weak_result = ste;
        if ((ste->compiland && !compiland_name) || (!ste->compiland && compiland_name))
            continue;
        if (ste->compiland && compiland_name)
        {
            const char* filename = source_get(module, ste->compiland->source);
            if (strcmp(filename, compiland_name))
            {
                base = strrchr(filename, '/');
                if (!base++) base = filename;
                if (strcmp(base, compiland_basename)) continue;
            }
        }
        if (result)
        {
            FIXME("Already found symbol %s (%s) in symtab %s @%08x and %s @%08x\n",
                  name, compiland_name,
                  source_get(module, result->compiland->source), (unsigned int)result->symp->st_value,
                  source_get(module, ste->compiland->source), (unsigned int)ste->symp->st_value);
        }
        else
        {
            result = ste;
            ste->used = 1;
        }
    }
    if (!result && !(result = weak_result))
    {
        FIXME("Couldn't find symbol %s!%s in symtab\n",
              debugstr_w(module->module.ModuleName), name);
        return NULL;
    }
    return result->symp;
}

/******************************************************************
 *		elf_finish_stabs_info
 *
 * - get any relevant information (address & size) from the bits we got from the
 *   stabs debugging information
 */
static void elf_finish_stabs_info(struct module* module, const struct hash_table* symtab)
{
    struct hash_table_iter      hti;
    void*                       ptr;
    struct symt_ht*             sym;
    const Elf_Sym*              symp;
    struct elf_module_info*     elf_info = module->format_info[DFI_ELF]->u.elf_info;

    hash_table_iter_init(&module->ht_symbols, &hti, NULL);
    while ((ptr = hash_table_iter_up(&hti)))
    {
        sym = GET_ENTRY(ptr, struct symt_ht, hash_elt);
        switch (sym->symt.tag)
        {
        case SymTagFunction:
            if (((struct symt_function*)sym)->address != elf_info->elf_addr &&
                ((struct symt_function*)sym)->size)
            {
                break;
            }
            symp = elf_lookup_symtab(module, symtab, sym->hash_elt.name, 
                                     ((struct symt_function*)sym)->container);
            if (symp)
            {
                if (((struct symt_function*)sym)->address != elf_info->elf_addr &&
                    ((struct symt_function*)sym)->address != elf_info->elf_addr + symp->st_value)
                    FIXME("Changing address for %p/%s!%s from %08lx to %08lx\n",
                          sym, debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                          ((struct symt_function*)sym)->address, elf_info->elf_addr + symp->st_value);
                if (((struct symt_function*)sym)->size && ((struct symt_function*)sym)->size != symp->st_size)
                    FIXME("Changing size for %p/%s!%s from %08lx to %08x\n",
                          sym, debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                          ((struct symt_function*)sym)->size, (unsigned int)symp->st_size);

                ((struct symt_function*)sym)->address = elf_info->elf_addr + symp->st_value;
                ((struct symt_function*)sym)->size    = symp->st_size;
            } else
                FIXME("Couldn't find %s!%s\n",
                      debugstr_w(module->module.ModuleName), sym->hash_elt.name);
            break;
        case SymTagData:
            switch (((struct symt_data*)sym)->kind)
            {
            case DataIsGlobal:
            case DataIsFileStatic:
                if (((struct symt_data*)sym)->u.var.offset != elf_info->elf_addr)
                    break;
                symp = elf_lookup_symtab(module, symtab, sym->hash_elt.name, 
                                         ((struct symt_data*)sym)->container);
                if (symp)
                {
                if (((struct symt_data*)sym)->u.var.offset != elf_info->elf_addr &&
                    ((struct symt_data*)sym)->u.var.offset != elf_info->elf_addr + symp->st_value)
                    FIXME("Changing address for %p/%s!%s from %08lx to %08lx\n",
                          sym, debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                          ((struct symt_function*)sym)->address, elf_info->elf_addr + symp->st_value);
                    ((struct symt_data*)sym)->u.var.offset = elf_info->elf_addr + symp->st_value;
                    ((struct symt_data*)sym)->kind = (ELF32_ST_BIND(symp->st_info) == STB_LOCAL) ?
                        DataIsFileStatic : DataIsGlobal;
                } else
                    FIXME("Couldn't find %s!%s\n",
                          debugstr_w(module->module.ModuleName), sym->hash_elt.name);
                break;
            default:;
            }
            break;
        default:
            FIXME("Unsupported tag %u\n", sym->symt.tag);
            break;
        }
    }
    /* since we may have changed some addresses & sizes, mark the module to be resorted */
    module->sortlist_valid = FALSE;
}

/******************************************************************
 *		elf_load_wine_thunks
 *
 * creating the thunk objects for a wine native DLL
 */
static int elf_new_wine_thunks(struct module* module, const struct hash_table* ht_symtab,
                               const struct elf_thunk_area* thunks)
{
    int		                j;
    struct hash_table_iter      hti;
    struct symtab_elt*          ste;
    DWORD_PTR                   addr;
    struct symt_ht*             symt;

    hash_table_iter_init(ht_symtab, &hti, NULL);
    while ((ste = hash_table_iter_up(&hti)))
    {
        if (ste->used) continue;

        addr = module->format_info[DFI_ELF]->u.elf_info->elf_addr + ste->symp->st_value;

        j = elf_is_in_thunk_area(ste->symp->st_value, thunks);
        if (j >= 0) /* thunk found */
        {
            symt_new_thunk(module, ste->compiland, ste->ht_elt.name, thunks[j].ordinal,
                           addr, ste->symp->st_size);
        }
        else
        {
            ULONG64     ref_addr;

            symt = symt_find_nearest(module, addr);
            if (symt && !symt_get_info(module, &symt->symt, TI_GET_ADDRESS, &ref_addr))
                ref_addr = addr;
            if (!symt || addr != ref_addr)
            {
                /* creating public symbols for all the ELF symbols which haven't been
                 * used yet (ie we have no debug information on them)
                 * That's the case, for example, of the .spec.c files
                 */
                switch (ELF32_ST_TYPE(ste->symp->st_info))
                {
                case STT_FUNC:
                    symt_new_function(module, ste->compiland, ste->ht_elt.name,
                                      addr, ste->symp->st_size, NULL);
                    break;
                case STT_OBJECT:
                    symt_new_global_variable(module, ste->compiland, ste->ht_elt.name,
                                             ELF32_ST_BIND(ste->symp->st_info) == STB_LOCAL,
                                             addr, ste->symp->st_size, NULL);
                    break;
                default:
                    FIXME("Shouldn't happen\n");
                    break;
                }
                /* FIXME: this is a hack !!!
                 * we are adding new symbols, but as we're parsing a symbol table
                 * (hopefully without duplicate symbols) we delay rebuilding the sorted
                 * module table until we're done with the symbol table
                 * Otherwise, as we intertwine symbols's add and lookup, performance
                 * is rather bad
                 */
                module->sortlist_valid = TRUE;
            }
            else if (strcmp(ste->ht_elt.name, symt->hash_elt.name))
            {
                ULONG64 xaddr = 0, xsize = 0;
                DWORD   kind = -1;

                symt_get_info(module, &symt->symt, TI_GET_ADDRESS,  &xaddr);
                symt_get_info(module, &symt->symt, TI_GET_LENGTH,   &xsize);
                symt_get_info(module, &symt->symt, TI_GET_DATAKIND, &kind);

                /* If none of symbols has a correct size, we consider they are both markers
                 * Hence, we can silence this warning
                 * Also, we check that we don't have two symbols, one local, the other 
                 * global which is legal
                 */
                if ((xsize || ste->symp->st_size) &&
                    (kind == (ELF32_ST_BIND(ste->symp->st_info) == STB_LOCAL) ? DataIsFileStatic : DataIsGlobal))
                    FIXME("Duplicate in %s: %s<%08lx-%08x> %s<%s-%s>\n",
                          debugstr_w(module->module.ModuleName),
                          ste->ht_elt.name, addr, (unsigned int)ste->symp->st_size,
                          symt->hash_elt.name,
                          wine_dbgstr_longlong(xaddr), wine_dbgstr_longlong(xsize));
            }
        }
    }
    /* see comment above */
    module->sortlist_valid = FALSE;
    return TRUE;
}

/******************************************************************
 *		elf_new_public_symbols
 *
 * Creates a set of public symbols from an ELF symtab
 */
static int elf_new_public_symbols(struct module* module, const struct hash_table* symtab)
{
    struct hash_table_iter      hti;
    struct symtab_elt*          ste;

    if (dbghelp_options & SYMOPT_NO_PUBLICS) return TRUE;

    /* FIXME: we're missing the ELF entry point here */

    hash_table_iter_init(symtab, &hti, NULL);
    while ((ste = hash_table_iter_up(&hti)))
    {
        symt_new_public(module, ste->compiland, ste->ht_elt.name,
                        module->format_info[DFI_ELF]->u.elf_info->elf_addr + ste->symp->st_value,
                        ste->symp->st_size);
    }
    return TRUE;
}

static BOOL elf_check_debug_link(const WCHAR* file, struct image_file_map* fmap, DWORD crc)
{
    BOOL        ret;
    if (!elf_map_file(file, fmap)) return FALSE;
    if (!(ret = crc == calc_crc32(fmap->u.elf.fd)))
    {
        WARN("Bad CRC for file %s (got %08x while expecting %08x)\n",
             debugstr_w(file), calc_crc32(fmap->u.elf.fd), crc);
        elf_unmap_file(fmap);
    }
    return ret;
}

/******************************************************************
 *		elf_locate_debug_link
 *
 * Locate a filename from a .gnu_debuglink section, using the same
 * strategy as gdb:
 * "If the full name of the directory containing the executable is
 * execdir, and the executable has a debug link that specifies the
 * name debugfile, then GDB will automatically search for the
 * debugging information file in three places:
 *  - the directory containing the executable file (that is, it
 *    will look for a file named `execdir/debugfile',
 *  - a subdirectory of that directory named `.debug' (that is, the
 *    file `execdir/.debug/debugfile', and
 *  - a subdirectory of the global debug file directory that includes
 *    the executable's full path, and the name from the link (that is,
 *    the file `globaldebugdir/execdir/debugfile', where globaldebugdir
 *    is the global debug file directory, and execdir has been turned
 *    into a relative path)." (from GDB manual)
 */
static BOOL elf_locate_debug_link(struct image_file_map* fmap, const char* filename,
                                  const WCHAR* loaded_file, DWORD crc)
{
    static const WCHAR globalDebugDirW[] = {'/','u','s','r','/','l','i','b','/','d','e','b','u','g','/'};
    static const WCHAR dotDebugW[] = {'.','d','e','b','u','g','/'};
    const size_t globalDebugDirLen = sizeof(globalDebugDirW) / sizeof(WCHAR);
    size_t filename_len;
    WCHAR* p = NULL;
    WCHAR* slash;
    struct image_file_map* fmap_link = NULL;

    fmap_link = HeapAlloc(GetProcessHeap(), 0, sizeof(*fmap_link));
    if (!fmap_link) return FALSE;

    filename_len = MultiByteToWideChar(CP_UNIXCP, 0, filename, -1, NULL, 0);
    p = HeapAlloc(GetProcessHeap(), 0,
                  (globalDebugDirLen + strlenW(loaded_file) + 6 + 1 + filename_len + 1) * sizeof(WCHAR));
    if (!p) goto found;

    /* we prebuild the string with "execdir" */
    strcpyW(p, loaded_file);
    slash = strrchrW(p, '/');
    if (slash == NULL) slash = p; else slash++;

    /* testing execdir/filename */
    MultiByteToWideChar(CP_UNIXCP, 0, filename, -1, slash, filename_len);
    if (elf_check_debug_link(p, fmap_link, crc)) goto found;

    /* testing execdir/.debug/filename */
    memcpy(slash, dotDebugW, sizeof(dotDebugW));
    MultiByteToWideChar(CP_UNIXCP, 0, filename, -1, slash + sizeof(dotDebugW) / sizeof(WCHAR), filename_len);
    if (elf_check_debug_link(p, fmap_link, crc)) goto found;

    /* testing globaldebugdir/execdir/filename */
    memmove(p + globalDebugDirLen, p, (slash - p) * sizeof(WCHAR));
    memcpy(p, globalDebugDirW, globalDebugDirLen * sizeof(WCHAR));
    slash += globalDebugDirLen;
    MultiByteToWideChar(CP_UNIXCP, 0, filename, -1, slash, filename_len);
    if (elf_check_debug_link(p, fmap_link, crc)) goto found;

    /* finally testing filename */
    if (elf_check_debug_link(slash, fmap_link, crc)) goto found;


    WARN("Couldn't locate or map %s\n", filename);
    HeapFree(GetProcessHeap(), 0, p);
    HeapFree(GetProcessHeap(), 0, fmap_link);
    return FALSE;

found:
    TRACE("Located debug information file %s at %s\n", filename, debugstr_w(p));
    HeapFree(GetProcessHeap(), 0, p);
    fmap->u.elf.alternate = fmap_link;
    return TRUE;
}

/******************************************************************
 *		elf_debuglink_parse
 *
 * Parses a .gnu_debuglink section and loads the debug info from
 * the external file specified there.
 */
static BOOL elf_debuglink_parse(struct image_file_map* fmap, const struct module* module,
                                const BYTE* debuglink)
{
    /* The content of a debug link section is:
     * 1/ a NULL terminated string, containing the file name for the
     *    debug info
     * 2/ padding on 4 byte boundary
     * 3/ CRC of the linked ELF file
     */
    const char* dbg_link = (const char*)debuglink;
    DWORD crc;

    crc = *(const DWORD*)(dbg_link + ((DWORD_PTR)(strlen(dbg_link) + 4) & ~3));
    return elf_locate_debug_link(fmap, dbg_link, module->module.LoadedImageName, crc);
}

/******************************************************************
 *		elf_load_debug_info_from_map
 *
 * Loads the symbolic information from ELF module which mapping is described
 * in fmap
 * the module has been loaded at 'load_offset' address, so symbols' address
 * relocation is performed.
 * CRC is checked if fmap->with_crc is TRUE
 * returns
 *	0 if the file doesn't contain symbolic info (or this info cannot be
 *	read or parsed)
 *	1 on success
 */
static BOOL elf_load_debug_info_from_map(struct module* module,
                                         struct image_file_map* fmap,
                                         struct pool* pool,
                                         struct hash_table* ht_symtab)
{
    BOOL                ret = FALSE, lret;
    struct elf_thunk_area thunks[] = 
    {
        {"__wine_spec_import_thunks",           THUNK_ORDINAL_NOTYPE, 0, 0},    /* inter DLL calls */
        {"__wine_spec_delayed_import_loaders",  THUNK_ORDINAL_LOAD,   0, 0},    /* delayed inter DLL calls */
        {"__wine_spec_delayed_import_thunks",   THUNK_ORDINAL_LOAD,   0, 0},    /* delayed inter DLL calls */
        {"__wine_delay_load",                   THUNK_ORDINAL_LOAD,   0, 0},    /* delayed inter DLL calls */
        {"__wine_spec_thunk_text_16",           -16,                  0, 0},    /* 16 => 32 thunks */
        {"__wine_spec_thunk_text_32",           -32,                  0, 0},    /* 32 => 16 thunks */
        {NULL,                                  0,                    0, 0}
    };

    module->module.SymType = SymExport;

    /* create a hash table for the symtab */
    elf_hash_symtab(module, pool, ht_symtab, fmap, thunks);

    if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY))
    {
        struct image_section_map stab_sect, stabstr_sect;
        struct image_section_map debuglink_sect;

        /* if present, add the .gnu_debuglink file as an alternate to current one */
	if (elf_find_section(fmap, ".gnu_debuglink", SHT_NULL, &debuglink_sect))
        {
            const BYTE* dbg_link;

            dbg_link = (const BYTE*)image_map_section(&debuglink_sect);
            if (dbg_link != IMAGE_NO_MAP)
            {
                lret = elf_debuglink_parse(fmap, module, dbg_link);
                if (!lret)
		    WARN("Couldn't load linked debug file for %s\n",
                         debugstr_w(module->module.ModuleName));
                ret = ret || lret;
            }
            image_unmap_section(&debuglink_sect);
        }
        if (elf_find_section(fmap, ".stab", SHT_NULL, &stab_sect) &&
            elf_find_section(fmap, ".stabstr", SHT_NULL, &stabstr_sect))
        {
            const char* stab;
            const char* stabstr;

            stab = image_map_section(&stab_sect);
            stabstr = image_map_section(&stabstr_sect);
            if (stab != IMAGE_NO_MAP && stabstr != IMAGE_NO_MAP)
            {
                /* OK, now just parse all of the stabs. */
                lret = stabs_parse(module, module->format_info[DFI_ELF]->u.elf_info->elf_addr,
                                   stab, image_get_map_size(&stab_sect),
                                   stabstr, image_get_map_size(&stabstr_sect),
                                   NULL, NULL);
                if (lret)
                    /* and fill in the missing information for stabs */
                    elf_finish_stabs_info(module, ht_symtab);
                else
                    WARN("Couldn't correctly read stabs\n");
                ret = ret || lret;
            }
            else lret = FALSE;
            image_unmap_section(&stab_sect);
            image_unmap_section(&stabstr_sect);
        }
        lret = dwarf2_parse(module, module->format_info[DFI_ELF]->u.elf_info->elf_addr,
                            thunks, fmap);
        ret = ret || lret;
    }
    if (strstrW(module->module.ModuleName, S_ElfW) ||
        !strcmpW(module->module.ModuleName, S_WineLoaderW))
    {
        /* add the thunks for native libraries */
        if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY))
            elf_new_wine_thunks(module, ht_symtab, thunks);
    }
    /* add all the public symbols from symtab */
    if (elf_new_public_symbols(module, ht_symtab) && !ret) ret = TRUE;

    return ret;
}

/******************************************************************
 *		elf_load_debug_info
 *
 * Loads ELF debugging information from the module image file.
 */
BOOL elf_load_debug_info(struct module* module, struct image_file_map* fmap)
{
    BOOL                        ret = TRUE;
    struct pool                 pool;
    struct hash_table           ht_symtab;
    struct image_file_map       my_fmap;
    struct module_format*       modfmt;

    if (module->type != DMT_ELF || !(modfmt = module->format_info[DFI_ELF]) || !modfmt->u.elf_info)
    {
	ERR("Bad elf module '%s'\n", debugstr_w(module->module.LoadedImageName));
	return FALSE;
    }

    pool_init(&pool, 65536);
    hash_table_init(&pool, &ht_symtab, 256);

    if (!fmap)
    {
        fmap = &my_fmap;
        ret = elf_map_file(module->module.LoadedImageName, fmap);
    }
    if (ret)
        ret = elf_load_debug_info_from_map(module, fmap, &pool, &ht_symtab);

    if (ret)
    {
        modfmt->u.elf_info->file_map = *fmap;
        elf_reset_file_map(fmap);
    }

    pool_destroy(&pool);
    if (fmap == &my_fmap) elf_unmap_file(fmap);
    return ret;
}

/******************************************************************
 *		elf_fetch_file_info
 *
 * Gathers some more information for an ELF module from a given file
 */
BOOL elf_fetch_file_info(const WCHAR* name, DWORD* base,
                         DWORD* size, DWORD* checksum)
{
    struct image_file_map fmap;

    if (!elf_map_file(name, &fmap)) return FALSE;
    if (base) *base = fmap.u.elf.elf_start;
    *size = fmap.u.elf.elf_size;
    *checksum = calc_crc32(fmap.u.elf.fd);
    elf_unmap_file(&fmap);
    return TRUE;
}

/******************************************************************
 *		elf_load_file
 *
 * Loads the information for ELF module stored in 'filename'
 * the module has been loaded at 'load_offset' address
 * returns
 *	-1 if the file cannot be found/opened
 *	0 if the file doesn't contain symbolic info (or this info cannot be
 *	read or parsed)
 *	1 on success
 */
static BOOL elf_load_file(struct process* pcs, const WCHAR* filename,
                          unsigned long load_offset, struct elf_info* elf_info)
{
    BOOL                        ret = FALSE;
    struct image_file_map       fmap;

    TRACE("Processing elf file '%s' at %08lx\n", debugstr_w(filename), load_offset);

    if (!elf_map_file(filename, &fmap)) return ret;

    /* Next, we need to find a few of the internal ELF headers within
     * this thing.  We need the main executable header, and the section
     * table.
     */
    if (!fmap.u.elf.elf_start && !load_offset)
        ERR("Relocatable ELF %s, but no load address. Loading at 0x0000000\n",
            debugstr_w(filename));
    if (fmap.u.elf.elf_start && load_offset)
    {
        WARN("Non-relocatable ELF %s, but load address of 0x%08lx supplied. "
             "Assuming load address is corrupt\n", debugstr_w(filename), load_offset);
        load_offset = 0;
    }

    if (elf_info->flags & ELF_INFO_DEBUG_HEADER)
    {
        struct image_section_map        ism;

        if (elf_find_section(&fmap, ".dynamic", SHT_DYNAMIC, &ism))
        {
            Elf_Dyn         dyn;
            char*           ptr = (char*)fmap.u.elf.sect[ism.sidx].shdr.sh_addr;
            unsigned long   len;

            do
            {
                if (!ReadProcessMemory(pcs->handle, ptr, &dyn, sizeof(dyn), &len) ||
                    len != sizeof(dyn))
                    goto leave;
                if (dyn.d_tag == DT_DEBUG)
                {
                    elf_info->dbg_hdr_addr = dyn.d_un.d_ptr;
                    break;
                }
                ptr += sizeof(dyn);
            } while (dyn.d_tag != DT_NULL);
            if (dyn.d_tag == DT_NULL) goto leave;
	}
        elf_end_find(&fmap);
    }

    if (elf_info->flags & ELF_INFO_MODULE)
    {
        struct elf_module_info *elf_module_info;
        struct module_format*   modfmt;

        modfmt = HeapAlloc(GetProcessHeap(), 0,
                          sizeof(struct module_format) + sizeof(struct elf_module_info));
        if (!modfmt) goto leave;
        elf_info->module = module_new(pcs, filename, DMT_ELF, FALSE,
                                      (load_offset) ? load_offset : fmap.u.elf.elf_start,
                                      fmap.u.elf.elf_size, 0, calc_crc32(fmap.u.elf.fd));
        if (!elf_info->module)
        {
            HeapFree(GetProcessHeap(), 0, modfmt);
            goto leave;
        }
        elf_module_info = (void*)(modfmt + 1);
        elf_info->module->format_info[DFI_ELF] = modfmt;
        modfmt->module      = elf_info->module;
        modfmt->remove      = elf_module_remove;
        modfmt->loc_compute = NULL;
        modfmt->u.elf_info  = elf_module_info;

        elf_module_info->elf_addr = load_offset;

        if (dbghelp_options & SYMOPT_DEFERRED_LOADS)
        {
            elf_info->module->module.SymType = SymDeferred;
            elf_module_info->file_map = fmap;
            elf_reset_file_map(&fmap);
            ret = TRUE;
        }
        else ret = elf_load_debug_info(elf_info->module, &fmap);

        elf_module_info->elf_mark = 1;
        elf_module_info->elf_loader = 0;
    } else ret = TRUE;

    if (elf_info->flags & ELF_INFO_NAME)
    {
        WCHAR*  ptr;
        ptr = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(filename) + 1) * sizeof(WCHAR));
        if (ptr)
        {
            strcpyW(ptr, filename);
            elf_info->module_name = ptr;
        }
        else ret = FALSE;
    }
leave:
    elf_unmap_file(&fmap);

    return ret;
}

/******************************************************************
 *		elf_load_file_from_path
 * tries to load an ELF file from a set of paths (separated by ':')
 */
static BOOL elf_load_file_from_path(HANDLE hProcess,
                                    const WCHAR* filename,
                                    unsigned long load_offset,
                                    const char* path,
                                    struct elf_info* elf_info)
{
    BOOL                ret = FALSE;
    WCHAR               *s, *t, *fn;
    WCHAR*	        pathW = NULL;
    unsigned            len;

    if (!path) return FALSE;

    len = MultiByteToWideChar(CP_UNIXCP, 0, path, -1, NULL, 0);
    pathW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    if (!pathW) return FALSE;
    MultiByteToWideChar(CP_UNIXCP, 0, path, -1, pathW, len);

    for (s = pathW; s && *s; s = (t) ? (t+1) : NULL)
    {
	t = strchrW(s, ':');
	if (t) *t = '\0';
	fn = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(filename) + 1 + lstrlenW(s) + 1) * sizeof(WCHAR));
	if (!fn) break;
	strcpyW(fn, s);
	strcatW(fn, S_SlashW);
	strcatW(fn, filename);
	ret = elf_load_file(hProcess, fn, load_offset, elf_info);
	HeapFree(GetProcessHeap(), 0, fn);
	if (ret) break;
	s = (t) ? (t+1) : NULL;
    }

    HeapFree(GetProcessHeap(), 0, pathW);
    return ret;
}

/******************************************************************
 *		elf_load_file_from_dll_path
 *
 * Tries to load an ELF file from the dll path
 */
static BOOL elf_load_file_from_dll_path(HANDLE hProcess,
                                        const WCHAR* filename,
                                        unsigned long load_offset,
                                        struct elf_info* elf_info)
{
    BOOL ret = FALSE;
    unsigned int index = 0;
    const char *path;

    while (!ret && (path = wine_dll_enum_load_path( index++ )))
    {
        WCHAR *name;
        unsigned len;

        len = MultiByteToWideChar(CP_UNIXCP, 0, path, -1, NULL, 0);

        name = HeapAlloc( GetProcessHeap(), 0,
                          (len + lstrlenW(filename) + 2) * sizeof(WCHAR) );

        if (!name) break;
        MultiByteToWideChar(CP_UNIXCP, 0, path, -1, name, len);
        strcatW( name, S_SlashW );
        strcatW( name, filename );
        ret = elf_load_file(hProcess, name, load_offset, elf_info);
        HeapFree( GetProcessHeap(), 0, name );
    }
    return ret;
}

/******************************************************************
 *		elf_search_and_load_file
 *
 * lookup a file in standard ELF locations, and if found, load it
 */
static BOOL elf_search_and_load_file(struct process* pcs, const WCHAR* filename,
                                     unsigned long load_offset,
                                     struct elf_info* elf_info)
{
    BOOL                ret = FALSE;
    struct module*      module;
    static WCHAR        S_libstdcPPW[] = {'l','i','b','s','t','d','c','+','+','\0'};

    if (filename == NULL || *filename == '\0') return FALSE;
    if ((module = module_is_already_loaded(pcs, filename)))
    {
        elf_info->module = module;
        elf_info->module->format_info[DFI_ELF]->u.elf_info->elf_mark = 1;
        return module->module.SymType;
    }

    if (strstrW(filename, S_libstdcPPW)) return FALSE; /* We know we can't do it */
    ret = elf_load_file(pcs, filename, load_offset, elf_info);
    /* if relative pathname, try some absolute base dirs */
    if (!ret && !strchrW(filename, '/'))
    {
        ret = elf_load_file_from_path(pcs, filename, load_offset,
                                      getenv("PATH"), elf_info) ||
            elf_load_file_from_path(pcs, filename, load_offset,
                                    getenv("LD_LIBRARY_PATH"), elf_info);
        if (!ret) ret = elf_load_file_from_dll_path(pcs, filename, load_offset, elf_info);
    }
    
    return ret;
}

/******************************************************************
 *		elf_enum_modules_internal
 *
 * Enumerate ELF modules from a running process
 */
static BOOL elf_enum_modules_internal(const struct process* pcs,
                                      const WCHAR* main_name,
                                      enum_modules_cb cb, void* user)
{
    struct r_debug      dbg_hdr;
    void*               lm_addr;
    struct link_map     lm;
    char		bufstr[256];
    WCHAR               bufstrW[MAX_PATH];

    if (!pcs->dbg_hdr_addr ||
        !ReadProcessMemory(pcs->handle, (void*)pcs->dbg_hdr_addr,
                           &dbg_hdr, sizeof(dbg_hdr), NULL))
        return FALSE;

    /* Now walk the linked list.  In all known ELF implementations,
     * the dynamic loader maintains this linked list for us.  In some
     * cases the first entry doesn't appear with a name, in other cases it
     * does.
     */
    for (lm_addr = (void*)dbg_hdr.r_map; lm_addr; lm_addr = (void*)lm.l_next)
    {
	if (!ReadProcessMemory(pcs->handle, lm_addr, &lm, sizeof(lm), NULL))
	    return FALSE;

	if (lm.l_prev != NULL && /* skip first entry, normally debuggee itself */
	    lm.l_name != NULL &&
	    ReadProcessMemory(pcs->handle, lm.l_name, bufstr, sizeof(bufstr), NULL))
        {
	    bufstr[sizeof(bufstr) - 1] = '\0';
            MultiByteToWideChar(CP_UNIXCP, 0, bufstr, -1, bufstrW, sizeof(bufstrW) / sizeof(WCHAR));
            if (main_name && !bufstrW[0]) strcpyW(bufstrW, main_name);
            if (!cb(bufstrW, (unsigned long)lm.l_addr, user)) break;
	}
    }
    return TRUE;
}

struct elf_sync
{
    struct process*     pcs;
    struct elf_info     elf_info;
};

static BOOL elf_enum_sync_cb(const WCHAR* name, unsigned long addr, void* user)
{
    struct elf_sync*    es = user;

    elf_search_and_load_file(es->pcs, name, addr, &es->elf_info);
    return TRUE;
}

/******************************************************************
 *		elf_synchronize_module_list
 *
 * this functions rescans the debuggee module's list and synchronizes it with
 * the one from 'pcs', ie:
 * - if a module is in debuggee and not in pcs, it's loaded into pcs
 * - if a module is in pcs and not in debuggee, it's unloaded from pcs
 */
BOOL	elf_synchronize_module_list(struct process* pcs)
{
    struct module*      module;
    struct elf_sync     es;

    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module->type == DMT_ELF && !module->is_virtual)
            module->format_info[DFI_ELF]->u.elf_info->elf_mark = 0;
    }

    es.pcs = pcs;
    es.elf_info.flags = ELF_INFO_MODULE;
    if (!elf_enum_modules_internal(pcs, NULL, elf_enum_sync_cb, &es))
        return FALSE;

    module = pcs->lmodules;
    while (module)
    {
        if (module->type == DMT_ELF && !module->is_virtual)
        {
            struct elf_module_info* elf_info = module->format_info[DFI_ELF]->u.elf_info;

            if (!elf_info->elf_mark && !elf_info->elf_loader)
            {
                module_remove(pcs, module);
                /* restart all over */
                module = pcs->lmodules;
                continue;
            }
        }
        module = module->next;
    }
    return TRUE;
}

/******************************************************************
 *		elf_search_loader
 *
 * Lookup in a running ELF process the loader, and sets its ELF link
 * address (for accessing the list of loaded .so libs) in pcs.
 * If flags is ELF_INFO_MODULE, the module for the loader is also
 * added as a module into pcs.
 */
static BOOL elf_search_loader(struct process* pcs, struct elf_info* elf_info)
{
    BOOL                ret;
    const char*         ptr;

    /* All binaries are loaded with WINELOADER (if run from tree) or by the
     * main executable
     */
    if ((ptr = getenv("WINELOADER")))
    {
        WCHAR   tmp[MAX_PATH];
        MultiByteToWideChar(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp) / sizeof(WCHAR));
        ret = elf_search_and_load_file(pcs, tmp, 0, elf_info);
    }
    else
    {
        ret = elf_search_and_load_file(pcs, S_WineW, 0, elf_info);
    }
    return ret;
}

/******************************************************************
 *		elf_read_wine_loader_dbg_info
 *
 * Try to find a decent wine executable which could have loaded the debuggee
 */
BOOL elf_read_wine_loader_dbg_info(struct process* pcs)
{
    struct elf_info     elf_info;

    elf_info.flags = ELF_INFO_DEBUG_HEADER | ELF_INFO_MODULE;
    if (!elf_search_loader(pcs, &elf_info)) return FALSE;
    elf_info.module->format_info[DFI_ELF]->u.elf_info->elf_loader = 1;
    module_set_module(elf_info.module, S_WineLoaderW);
    return (pcs->dbg_hdr_addr = elf_info.dbg_hdr_addr) != 0;
}

/******************************************************************
 *		elf_enum_modules
 *
 * Enumerates the ELF loaded modules from a running target (hProc)
 * This function doesn't require that someone has called SymInitialize
 * on this very process.
 */
BOOL elf_enum_modules(HANDLE hProc, enum_modules_cb cb, void* user)
{
    struct process      pcs;
    struct elf_info     elf_info;
    BOOL                ret;

    memset(&pcs, 0, sizeof(pcs));
    pcs.handle = hProc;
    elf_info.flags = ELF_INFO_DEBUG_HEADER | ELF_INFO_NAME;
    if (!elf_search_loader(&pcs, &elf_info)) return FALSE;
    pcs.dbg_hdr_addr = elf_info.dbg_hdr_addr;
    ret = elf_enum_modules_internal(&pcs, elf_info.module_name, cb, user);
    HeapFree(GetProcessHeap(), 0, (char*)elf_info.module_name);
    return ret;
}

struct elf_load
{
    struct process*     pcs;
    struct elf_info     elf_info;
    const WCHAR*        name;
    BOOL                ret;
};

/******************************************************************
 *		elf_load_cb
 *
 * Callback for elf_load_module, used to walk the list of loaded
 * modules.
 */
static BOOL elf_load_cb(const WCHAR* name, unsigned long addr, void* user)
{
    struct elf_load*    el = user;
    const WCHAR*        p;

    /* memcmp is needed for matches when bufstr contains also version information
     * el->name: libc.so, name: libc.so.6.0
     */
    p = strrchrW(name, '/');
    if (!p++) p = name;
    if (!memcmp(p, el->name, lstrlenW(el->name) * sizeof(WCHAR)))
    {
        el->ret = elf_search_and_load_file(el->pcs, name, addr, &el->elf_info);
        return FALSE;
    }
    return TRUE;
}

/******************************************************************
 *		elf_load_module
 *
 * loads an ELF module and stores it in process' module list
 * Also, find module real name and load address from
 * the real loaded modules list in pcs address space
 */
struct module*  elf_load_module(struct process* pcs, const WCHAR* name, unsigned long addr)
{
    struct elf_load     el;

    TRACE("(%p %s %08lx)\n", pcs, debugstr_w(name), addr);

    el.elf_info.flags = ELF_INFO_MODULE;
    el.ret = FALSE;

    if (pcs->dbg_hdr_addr) /* we're debugging a life target */
    {
        el.pcs = pcs;
        /* do only the lookup from the filename, not the path (as we lookup module
         * name in the process' loaded module list)
         */
        el.name = strrchrW(name, '/');
        if (!el.name++) el.name = name;
        el.ret = FALSE;

        if (!elf_enum_modules_internal(pcs, NULL, elf_load_cb, &el))
            return NULL;
    }
    else if (addr)
    {
        el.name = name;
        el.ret = elf_search_and_load_file(pcs, el.name, addr, &el.elf_info);
    }
    if (!el.ret) return NULL;
    assert(el.elf_info.module);
    return el.elf_info.module;
}

#else	/* !__ELF__ */

BOOL         elf_find_section(struct image_file_map* fmap, const char* name,
                              unsigned sht, struct image_section_map* ism)
{
    return FALSE;
}

const char*  elf_map_section(struct image_section_map* ism)
{
    return NULL;
}

void         elf_unmap_section(struct image_section_map* ism)
{}

unsigned     elf_get_map_size(const struct image_section_map* ism)
{
    return 0;
}

DWORD_PTR elf_get_map_rva(const struct image_section_map* ism)
{
    return 0;
}

BOOL	elf_synchronize_module_list(struct process* pcs)
{
    return FALSE;
}

BOOL elf_fetch_file_info(const WCHAR* name, DWORD* base,
                         DWORD* size, DWORD* checksum)
{
    return FALSE;
}

BOOL elf_read_wine_loader_dbg_info(struct process* pcs)
{
    return FALSE;
}

BOOL elf_enum_modules(HANDLE hProc, enum_modules_cb cb, void* user)
{
    return FALSE;
}

struct module*  elf_load_module(struct process* pcs, const WCHAR* name, unsigned long addr)
{
    return NULL;
}

BOOL elf_load_debug_info(struct module* module, struct image_file_map* fmap)
{
    return FALSE;
}

int elf_is_in_thunk_area(unsigned long addr,
                         const struct elf_thunk_area* thunks)
{
    return -1;
}
#endif  /* __ELF__ */
