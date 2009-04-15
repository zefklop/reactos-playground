/*
 * shaders implementation
 *
 * Copyright 2002-2003 Jason Edmeades
 * Copyright 2002-2003 Raphael Junqueira
 * Copyright 2004 Christian Costa
 * Copyright 2005 Oliver Stieber
 * Copyright 2006 Ivan Gyurdiev
 * Copyright 2007-2008 Stefan Dösinger for CodeWeavers
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
#include <string.h>
#include <stdio.h>
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d_shader);
WINE_DECLARE_DEBUG_CHANNEL(d3d);

static void shader_dump_param(const DWORD param, const DWORD addr_token, int input, DWORD shader_version);

static inline BOOL shader_is_version_token(DWORD token) {
    return shader_is_pshader_version(token) ||
           shader_is_vshader_version(token);
}

void shader_buffer_init(struct SHADER_BUFFER *buffer)
{
    buffer->buffer = HeapAlloc(GetProcessHeap(), 0, SHADER_PGMSIZE);
    buffer->buffer[0] = '\0';
    buffer->bsize = 0;
    buffer->lineNo = 0;
    buffer->newline = TRUE;
}

void shader_buffer_free(struct SHADER_BUFFER *buffer)
{
    HeapFree(GetProcessHeap(), 0, buffer->buffer);
}

int shader_vaddline(SHADER_BUFFER* buffer, const char *format, va_list args)
{
    char* base = buffer->buffer + buffer->bsize;
    int rc;

    rc = vsnprintf(base, SHADER_PGMSIZE - 1 - buffer->bsize, format, args);

    if (rc < 0 ||                                   /* C89 */ 
        rc > SHADER_PGMSIZE - 1 - buffer->bsize) {  /* C99 */

        ERR("The buffer allocated for the shader program string "
            "is too small at %d bytes.\n", SHADER_PGMSIZE);
        buffer->bsize = SHADER_PGMSIZE - 1;
        return -1;
    }

    if (buffer->newline) {
        TRACE("GL HW (%u, %u) : %s", buffer->lineNo + 1, buffer->bsize, base);
        buffer->newline = FALSE;
    } else {
        TRACE("%s", base);
    }

    buffer->bsize += rc;
    if (buffer->buffer[buffer->bsize-1] == '\n') {
        buffer->lineNo++;
        buffer->newline = TRUE;
    }
    return 0;
}

int shader_addline(SHADER_BUFFER* buffer, const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = shader_vaddline(buffer, format, args);
    va_end(args);

    return ret;
}

void shader_init(struct IWineD3DBaseShaderClass *shader,
        IWineD3DDevice *device, const SHADER_OPCODE *instruction_table)
{
    shader->ref = 1;
    shader->device = device;
    shader->shader_ins = instruction_table;
    list_init(&shader->linked_programs);
}

const SHADER_OPCODE *shader_get_opcode(const SHADER_OPCODE *opcode_table, DWORD shader_version, DWORD code)
{
    DWORD i = 0;

    /** TODO: use dichotomic search */
    while (opcode_table[i].name)
    {
        if ((code & WINED3DSI_OPCODE_MASK) == opcode_table[i].opcode
                && shader_version >= opcode_table[i].min_version
                && (!opcode_table[i].max_version || shader_version <= opcode_table[i].max_version))
        {
            return &opcode_table[i];
        }
        ++i;
    }

    FIXME("Unsupported opcode %#x(%d) masked %#x, shader version %#x\n",
            code, code, code & WINED3DSI_OPCODE_MASK, shader_version);

    return NULL;
}

/* Read a parameter opcode from the input stream,
 * and possibly a relative addressing token.
 * Return the number of tokens read */
static int shader_get_param(const DWORD *pToken, DWORD shader_version, DWORD *param, DWORD *addr_token)
{
    /* PS >= 3.0 have relative addressing (with token)
     * VS >= 2.0 have relative addressing (with token)
     * VS >= 1.0 < 2.0 have relative addressing (without token)
     * The version check below should work in general */

    char rel_token = WINED3DSHADER_VERSION_MAJOR(shader_version) >= 2 &&
        ((*pToken & WINED3DSHADER_ADDRESSMODE_MASK) == WINED3DSHADER_ADDRMODE_RELATIVE);

    *param = *pToken;
    *addr_token = rel_token? *(pToken + 1): 0;
    return rel_token? 2:1;
}

/* Return the number of parameters to skip for an opcode */
static inline int shader_skip_opcode(const SHADER_OPCODE *curOpcode, DWORD opcode_token, DWORD shader_version)
{
   /* Shaders >= 2.0 may contain address tokens, but fortunately they
    * have a useful length mask - use it here. Shaders 1.0 contain no such tokens */
    return (WINED3DSHADER_VERSION_MAJOR(shader_version) >= 2)
            ? ((opcode_token & WINED3DSI_INSTLENGTH_MASK) >> WINED3DSI_INSTLENGTH_SHIFT) : curOpcode->num_params;
}

/* Read the parameters of an unrecognized opcode from the input stream
 * Return the number of tokens read. 
 * 
 * Note: This function assumes source or destination token format.
 * It will not work with specially-formatted tokens like DEF or DCL, 
 * but hopefully those would be recognized */
static int shader_skip_unrecognized(const DWORD *pToken, DWORD shader_version)
{
    int tokens_read = 0;
    int i = 0;

    /* TODO: Think of a good name for 0x80000000 and replace it with a constant */
    while (*pToken & 0x80000000) {

        DWORD param, addr_token;
        tokens_read += shader_get_param(pToken, shader_version, &param, &addr_token);
        pToken += tokens_read;

        FIXME("Unrecognized opcode param: token=0x%08x "
            "addr_token=0x%08x name=", param, addr_token);
        shader_dump_param(param, addr_token, i, shader_version);
        FIXME("\n");
        ++i;
    }
    return tokens_read;
}

/* Convert floating point offset relative
 * to a register file to an absolute offset for float constants */
static unsigned int shader_get_float_offset(const DWORD reg)
{
     unsigned int regnum = reg & WINED3DSP_REGNUM_MASK;
     int regtype = shader_get_regtype(reg);

     switch (regtype) {
        case WINED3DSPR_CONST: return regnum;
        case WINED3DSPR_CONST2: return 2048 + regnum;
        case WINED3DSPR_CONST3: return 4096 + regnum;
        case WINED3DSPR_CONST4: return 6144 + regnum;
        default:
            FIXME("Unsupported register type: %d\n", regtype);
            return regnum;
     }
}

static void shader_delete_constant_list(struct list* clist) {

    struct list *ptr;
    struct local_constant* constant;

    ptr = list_head(clist);
    while (ptr) {
        constant = LIST_ENTRY(ptr, struct local_constant, entry);
        ptr = list_next(clist, ptr);
        HeapFree(GetProcessHeap(), 0, constant);
    }
    list_init(clist);
}

static void shader_parse_dst_param(DWORD param, DWORD addr_param, struct wined3d_shader_dst_param *dst)
{
    dst->register_type = ((param & WINED3DSP_REGTYPE_MASK) >> WINED3DSP_REGTYPE_SHIFT)
            | ((param & WINED3DSP_REGTYPE_MASK2) >> WINED3DSP_REGTYPE_SHIFT2);
    dst->register_idx = param & WINED3DSP_REGNUM_MASK;
    dst->write_mask = param & WINED3DSP_WRITEMASK_ALL;
    dst->modifiers = param & WINED3DSP_DSTMOD_MASK;
    dst->shift = (param & WINED3DSP_DSTSHIFT_MASK) >> WINED3DSP_DSTSHIFT_SHIFT;
    dst->has_rel_addr = param & WINED3DSHADER_ADDRMODE_RELATIVE;
    dst->addr_token = addr_param;
}

/* Note that this does not count the loop register
 * as an address register. */

HRESULT shader_get_registers_used(IWineD3DBaseShader *iface, struct shader_reg_maps *reg_maps,
        struct wined3d_shader_semantic *semantics_in, struct wined3d_shader_semantic *semantics_out,
        const DWORD *byte_code)
{
    IWineD3DBaseShaderImpl* This = (IWineD3DBaseShaderImpl*) iface;
    const SHADER_OPCODE *shader_ins = This->baseShader.shader_ins;
    DWORD shader_version;
    unsigned int cur_loop_depth = 0, max_loop_depth = 0;
    const DWORD* pToken = byte_code;
    char pshader;

    /* There are some minor differences between pixel and vertex shaders */

    memset(reg_maps->bumpmat, 0, sizeof(reg_maps->bumpmat));
    memset(reg_maps->luminanceparams, 0, sizeof(reg_maps->luminanceparams));

    /* get_registers_used is called on every compile on some 1.x shaders, which can result
     * in stacking up a collection of local constants. Delete the old constants if existing
     */
    shader_delete_constant_list(&This->baseShader.constantsF);
    shader_delete_constant_list(&This->baseShader.constantsB);
    shader_delete_constant_list(&This->baseShader.constantsI);

    /* The version token is supposed to be the first token */
    if (!shader_is_version_token(*pToken))
    {
        FIXME("First token is not a version token, invalid shader.\n");
        return WINED3DERR_INVALIDCALL;
    }
    reg_maps->shader_version = shader_version = *pToken++;
    pshader = shader_is_pshader_version(shader_version);

    while (WINED3DVS_END() != *pToken) {
        CONST SHADER_OPCODE* curOpcode;
        DWORD opcode_token;

        /* Skip comments */
        if (shader_is_comment(*pToken))
        {
             DWORD comment_len = (*pToken & WINED3DSI_COMMENTSIZE_MASK) >> WINED3DSI_COMMENTSIZE_SHIFT;
             ++pToken;
             pToken += comment_len;
             continue;
        }

        /* Fetch opcode */
        opcode_token = *pToken++;
        curOpcode = shader_get_opcode(shader_ins, shader_version, opcode_token);

        /* Unhandled opcode, and its parameters */
        if (NULL == curOpcode) {
           while (*pToken & 0x80000000)
               ++pToken;

        /* Handle declarations */
        } else if (WINED3DSIO_DCL == curOpcode->opcode) {

            DWORD usage = *pToken++;
            DWORD param = *pToken++;
            DWORD regtype = shader_get_regtype(param);
            unsigned int regnum = param & WINED3DSP_REGNUM_MASK;

            /* Vshader: mark attributes used
               Pshader: mark 3.0 input registers used, save token */
            if (WINED3DSPR_INPUT == regtype) {

                if (!pshader)
                    reg_maps->attributes[regnum] = 1;
                else
                    reg_maps->packed_input[regnum] = 1;

                semantics_in[regnum].usage = (usage & WINED3DSP_DCL_USAGE_MASK) >> WINED3DSP_DCL_USAGE_SHIFT;
                semantics_in[regnum].usage_idx =
                        (usage & WINED3DSP_DCL_USAGEINDEX_MASK) >> WINED3DSP_DCL_USAGEINDEX_SHIFT;
                shader_parse_dst_param(param, 0, &semantics_in[regnum].reg);

            /* Vshader: mark 3.0 output registers used, save token */
            } else if (WINED3DSPR_OUTPUT == regtype) {
                reg_maps->packed_output[regnum] = 1;
                semantics_out[regnum].usage = (usage & WINED3DSP_DCL_USAGE_MASK) >> WINED3DSP_DCL_USAGE_SHIFT;
                semantics_out[regnum].usage_idx =
                        (usage & WINED3DSP_DCL_USAGEINDEX_MASK) >> WINED3DSP_DCL_USAGEINDEX_SHIFT;
                shader_parse_dst_param(param, 0, &semantics_out[regnum].reg);

                if (usage & (WINED3DDECLUSAGE_FOG << WINED3DSP_DCL_USAGE_SHIFT))
                    reg_maps->fog = 1;

            /* Save sampler usage token */
            } else if (WINED3DSPR_SAMPLER == regtype)
                reg_maps->samplers[regnum] = usage;

        } else if (WINED3DSIO_DEF == curOpcode->opcode) {

            local_constant* lconst = HeapAlloc(GetProcessHeap(), 0, sizeof(local_constant));
            if (!lconst) return E_OUTOFMEMORY;
            lconst->idx = *pToken & WINED3DSP_REGNUM_MASK;
            memcpy(lconst->value, pToken + 1, 4 * sizeof(DWORD));

            /* In pixel shader 1.X shaders, the constants are clamped between [-1;1] */
            if (WINED3DSHADER_VERSION_MAJOR(shader_version) == 1 && pshader)
            {
                float *value = (float *) lconst->value;
                if(value[0] < -1.0) value[0] = -1.0;
                else if(value[0] >  1.0) value[0] =  1.0;
                if(value[1] < -1.0) value[1] = -1.0;
                else if(value[1] >  1.0) value[1] =  1.0;
                if(value[2] < -1.0) value[2] = -1.0;
                else if(value[2] >  1.0) value[2] =  1.0;
                if(value[3] < -1.0) value[3] = -1.0;
                else if(value[3] >  1.0) value[3] =  1.0;
            }

            list_add_head(&This->baseShader.constantsF, &lconst->entry);
            pToken += curOpcode->num_params;

        } else if (WINED3DSIO_DEFI == curOpcode->opcode) {

            local_constant* lconst = HeapAlloc(GetProcessHeap(), 0, sizeof(local_constant));
            if (!lconst) return E_OUTOFMEMORY;
            lconst->idx = *pToken & WINED3DSP_REGNUM_MASK;
            memcpy(lconst->value, pToken + 1, 4 * sizeof(DWORD));
            list_add_head(&This->baseShader.constantsI, &lconst->entry);
            pToken += curOpcode->num_params;

        } else if (WINED3DSIO_DEFB == curOpcode->opcode) {

            local_constant* lconst = HeapAlloc(GetProcessHeap(), 0, sizeof(local_constant));
            if (!lconst) return E_OUTOFMEMORY;
            lconst->idx = *pToken & WINED3DSP_REGNUM_MASK;
            memcpy(lconst->value, pToken + 1, 1 * sizeof(DWORD));
            list_add_head(&This->baseShader.constantsB, &lconst->entry);
            pToken += curOpcode->num_params;

        /* If there's a loop in the shader */
        } else if (WINED3DSIO_LOOP == curOpcode->opcode ||
                   WINED3DSIO_REP == curOpcode->opcode) {
            cur_loop_depth++;
            if(cur_loop_depth > max_loop_depth)
                max_loop_depth = cur_loop_depth;
            pToken += curOpcode->num_params;

            /* Rep and Loop always use an integer constant for the control parameters */
            This->baseShader.uses_int_consts = TRUE;
        } else if (WINED3DSIO_ENDLOOP == curOpcode->opcode ||
                   WINED3DSIO_ENDREP == curOpcode->opcode) {
            cur_loop_depth--;

        /* For subroutine prototypes */
        } else if (WINED3DSIO_LABEL == curOpcode->opcode) {

            DWORD snum = *pToken & WINED3DSP_REGNUM_MASK; 
            reg_maps->labels[snum] = 1;
            pToken += curOpcode->num_params;

        /* Set texture, address, temporary registers */
        } else {
            int i, limit;

            /* Declare 1.X samplers implicitly, based on the destination reg. number */
            if (WINED3DSHADER_VERSION_MAJOR(shader_version) == 1
                    && pshader /* Filter different instructions with the same enum values in VS */
                    && (WINED3DSIO_TEX == curOpcode->opcode
                        || WINED3DSIO_TEXBEM == curOpcode->opcode
                        || WINED3DSIO_TEXBEML == curOpcode->opcode
                        || WINED3DSIO_TEXDP3TEX == curOpcode->opcode
                        || WINED3DSIO_TEXM3x2TEX == curOpcode->opcode
                        || WINED3DSIO_TEXM3x3SPEC == curOpcode->opcode
                        || WINED3DSIO_TEXM3x3TEX == curOpcode->opcode
                        || WINED3DSIO_TEXM3x3VSPEC == curOpcode->opcode
                        || WINED3DSIO_TEXREG2AR == curOpcode->opcode
                        || WINED3DSIO_TEXREG2GB == curOpcode->opcode
                        || WINED3DSIO_TEXREG2RGB == curOpcode->opcode))
            {
                /* Fake sampler usage, only set reserved bit and ttype */
                DWORD sampler_code = *pToken & WINED3DSP_REGNUM_MASK;

                TRACE("Setting fake 2D sampler for 1.x pixelshader\n");
                reg_maps->samplers[sampler_code] = (0x1 << 31) | WINED3DSTT_2D;

                /* texbem is only valid with < 1.4 pixel shaders */
                if(WINED3DSIO_TEXBEM  == curOpcode->opcode ||
                    WINED3DSIO_TEXBEML == curOpcode->opcode) {
                    reg_maps->bumpmat[sampler_code] = TRUE;
                    if(WINED3DSIO_TEXBEML == curOpcode->opcode) {
                        reg_maps->luminanceparams[sampler_code] = TRUE;
                    }
                }
            }
            if(WINED3DSIO_NRM  == curOpcode->opcode) {
                reg_maps->usesnrm = 1;
            } else if(WINED3DSIO_BEM == curOpcode->opcode && pshader) {
                DWORD regnum = *pToken & WINED3DSP_REGNUM_MASK;
                reg_maps->bumpmat[regnum] = TRUE;
            } else if(WINED3DSIO_DSY  == curOpcode->opcode) {
                reg_maps->usesdsy = 1;
            }

            /* This will loop over all the registers and try to
             * make a bitmask of the ones we're interested in. 
             *
             * Relative addressing tokens are ignored, but that's 
             * okay, since we'll catch any address registers when 
             * they are initialized (required by spec) */

            limit = (opcode_token & WINED3DSHADER_INSTRUCTION_PREDICATED)?
                curOpcode->num_params + 1: curOpcode->num_params;

            for (i = 0; i < limit; ++i) {

                DWORD param, addr_token, reg, regtype;
                pToken += shader_get_param(pToken, shader_version, &param, &addr_token);

                regtype = shader_get_regtype(param);
                reg = param & WINED3DSP_REGNUM_MASK;

                if (WINED3DSPR_TEXTURE == regtype) { /* vs: WINED3DSPR_ADDR */

                    if (pshader)
                        reg_maps->texcoord[reg] = 1;
                    else
                        reg_maps->address[reg] = 1;
                }

                else if (WINED3DSPR_TEMP == regtype)
                    reg_maps->temporary[reg] = 1;

                else if (WINED3DSPR_INPUT == regtype) {
                    if( !pshader)
                        reg_maps->attributes[reg] = 1;
                    else {
                        if(param & WINED3DSHADER_ADDRMODE_RELATIVE) {
                            /* If relative addressing is used, we must assume that all registers
                             * are used. Even if it is a construct like v3[aL], we can't assume
                             * that v0, v1 and v2 aren't read because aL can be negative
                             */
                            unsigned int i;
                            for(i = 0; i < MAX_REG_INPUT; i++) {
                                ((IWineD3DPixelShaderImpl *) This)->input_reg_used[i] = TRUE;
                            }
                        } else {
                            ((IWineD3DPixelShaderImpl *) This)->input_reg_used[reg] = TRUE;
                        }
                    }
                }

                else if (WINED3DSPR_RASTOUT == regtype && reg == 1)
                    reg_maps->fog = 1;

                else if (WINED3DSPR_MISCTYPE == regtype && reg == 0 && pshader)
                    reg_maps->vpos = 1;

                else if(WINED3DSPR_CONST == regtype) {
                    if(param & WINED3DSHADER_ADDRMODE_RELATIVE) {
                        if(!pshader) {
                            if(reg <= ((IWineD3DVertexShaderImpl *) This)->min_rel_offset) {
                                ((IWineD3DVertexShaderImpl *) This)->min_rel_offset = reg;
                            } else if(reg >= ((IWineD3DVertexShaderImpl *) This)->max_rel_offset) {
                                ((IWineD3DVertexShaderImpl *) This)->max_rel_offset = reg;
                            }
                        }
                        reg_maps->usesrelconstF = TRUE;
                    }
                }
                else if(WINED3DSPR_CONSTINT == regtype) {
                    This->baseShader.uses_int_consts = TRUE;
                }
                else if(WINED3DSPR_CONSTBOOL == regtype) {
                    This->baseShader.uses_bool_consts = TRUE;
                }

                /* WINED3DSPR_TEXCRDOUT is the same as WINED3DSPR_OUTPUT. _OUTPUT can be > MAX_REG_TEXCRD and is used
                 * in >= 3.0 shaders. Filter 3.0 shaders to prevent overflows, and also filter pixel shaders because TECRDOUT
                 * isn't used in them, but future register types might cause issues
                 */
                else if (WINED3DSPR_TEXCRDOUT == regtype && i == 0 /* Only look at writes */
                        && !pshader && WINED3DSHADER_VERSION_MAJOR(shader_version) < 3)
                {
                    reg_maps->texcoord_mask[reg] |= shader_get_writemask(param);
                }
            }
        }
    }
    ++pToken;
    reg_maps->loop_depth = max_loop_depth;

    This->baseShader.functionLength = ((char *)pToken - (char *)byte_code);

    return WINED3D_OK;
}

static void shader_dump_decl_usage(DWORD decl, DWORD param, DWORD shader_version)
{
    DWORD regtype = shader_get_regtype(param);

    TRACE("dcl");

    if (regtype == WINED3DSPR_SAMPLER) {
        DWORD ttype = decl & WINED3DSP_TEXTURETYPE_MASK;

        switch (ttype) {
            case WINED3DSTT_2D: TRACE("_2d"); break;
            case WINED3DSTT_CUBE: TRACE("_cube"); break;
            case WINED3DSTT_VOLUME: TRACE("_volume"); break;
            default: TRACE("_unknown_ttype(0x%08x)", ttype);
       }

    } else { 

        DWORD usage = decl & WINED3DSP_DCL_USAGE_MASK;
        DWORD idx = (decl & WINED3DSP_DCL_USAGEINDEX_MASK) >> WINED3DSP_DCL_USAGEINDEX_SHIFT;

        /* Pixel shaders 3.0 don't have usage semantics */
        if (shader_is_pshader_version(shader_version) && shader_version < WINED3DPS_VERSION(3,0))
            return;
        else
            TRACE("_");

        switch(usage) {
        case WINED3DDECLUSAGE_POSITION:
            TRACE("position%d", idx);
            break;
        case WINED3DDECLUSAGE_BLENDINDICES:
            TRACE("blend");
            break;
        case WINED3DDECLUSAGE_BLENDWEIGHT:
            TRACE("weight");
            break;
        case WINED3DDECLUSAGE_NORMAL:
            TRACE("normal%d", idx);
            break;
        case WINED3DDECLUSAGE_PSIZE:
            TRACE("psize");
            break;
        case WINED3DDECLUSAGE_COLOR:
            if(idx == 0)  {
                TRACE("color");
            } else {
                TRACE("specular%d", (idx - 1));
            }
            break;
        case WINED3DDECLUSAGE_TEXCOORD:
            TRACE("texture%d", idx);
            break;
        case WINED3DDECLUSAGE_TANGENT:
            TRACE("tangent");
            break;
        case WINED3DDECLUSAGE_BINORMAL:
            TRACE("binormal");
            break;
        case WINED3DDECLUSAGE_TESSFACTOR:
            TRACE("tessfactor");
            break;
        case WINED3DDECLUSAGE_POSITIONT:
            TRACE("positionT%d", idx);
            break;
        case WINED3DDECLUSAGE_FOG:
            TRACE("fog");
            break;
        case WINED3DDECLUSAGE_DEPTH:
            TRACE("depth");
            break;
        case WINED3DDECLUSAGE_SAMPLE:
            TRACE("sample");
            break;
        default:
            FIXME("unknown_semantics(0x%08x)", usage);
        }
    }
}

static void shader_dump_arr_entry(const DWORD param, const DWORD addr_token,
        unsigned int reg, int input, DWORD shader_version)
{
    char relative =
        ((param & WINED3DSHADER_ADDRESSMODE_MASK) == WINED3DSHADER_ADDRMODE_RELATIVE);

    if (relative) {
        TRACE("[");
        if (addr_token)
            shader_dump_param(addr_token, 0, input, shader_version);
        else
            TRACE("a0.x");
        TRACE(" + ");
     }
     TRACE("%u", reg);
     if (relative)
         TRACE("]");
}

static void shader_dump_param(const DWORD param, const DWORD addr_token, int input, DWORD shader_version)
{
    static const char * const rastout_reg_names[] = { "oPos", "oFog", "oPts" };
    static const char * const misctype_reg_names[] = { "vPos", "vFace"};
    const char *swizzle_reg_chars = "xyzw";

    DWORD reg = param & WINED3DSP_REGNUM_MASK;
    DWORD regtype = shader_get_regtype(param);
    DWORD modifier = param & WINED3DSP_SRCMOD_MASK;

    /* There are some minor differences between pixel and vertex shaders */
    char pshader = shader_is_pshader_version(shader_version);

    if (input) {
        if ( (modifier == WINED3DSPSM_NEG) ||
             (modifier == WINED3DSPSM_BIASNEG) ||
             (modifier == WINED3DSPSM_SIGNNEG) ||
             (modifier == WINED3DSPSM_X2NEG) ||
             (modifier == WINED3DSPSM_ABSNEG) )
            TRACE("-");
        else if (modifier == WINED3DSPSM_COMP)
            TRACE("1-");
        else if (modifier == WINED3DSPSM_NOT)
            TRACE("!");

        if (modifier == WINED3DSPSM_ABS || modifier == WINED3DSPSM_ABSNEG) 
            TRACE("abs(");
    }

    switch (regtype) {
        case WINED3DSPR_TEMP:
            TRACE("r%u", reg);
            break;
        case WINED3DSPR_INPUT:
            TRACE("v");
            shader_dump_arr_entry(param, addr_token, reg, input, shader_version);
            break;
        case WINED3DSPR_CONST:
        case WINED3DSPR_CONST2:
        case WINED3DSPR_CONST3:
        case WINED3DSPR_CONST4:
            TRACE("c");
            shader_dump_arr_entry(param, addr_token, shader_get_float_offset(param), input, shader_version);
            break;
        case WINED3DSPR_TEXTURE: /* vs: case D3DSPR_ADDR */
            TRACE("%c%u", (pshader? 't':'a'), reg);
            break;        
        case WINED3DSPR_RASTOUT:
            TRACE("%s", rastout_reg_names[reg]);
            break;
        case WINED3DSPR_COLOROUT:
            TRACE("oC%u", reg);
            break;
        case WINED3DSPR_DEPTHOUT:
            TRACE("oDepth");
            break;
        case WINED3DSPR_ATTROUT:
            TRACE("oD%u", reg);
            break;
        case WINED3DSPR_TEXCRDOUT: 

            /* Vertex shaders >= 3.0 use general purpose output registers
             * (WINED3DSPR_OUTPUT), which can include an address token */

            if (WINED3DSHADER_VERSION_MAJOR(shader_version) >= 3) {
                TRACE("o");
                shader_dump_arr_entry(param, addr_token, reg, input, shader_version);
            }
            else 
               TRACE("oT%u", reg);
            break;
        case WINED3DSPR_CONSTINT:
            TRACE("i");
            shader_dump_arr_entry(param, addr_token, reg, input, shader_version);
            break;
        case WINED3DSPR_CONSTBOOL:
            TRACE("b");
            shader_dump_arr_entry(param, addr_token, reg, input, shader_version);
            break;
        case WINED3DSPR_LABEL:
            TRACE("l%u", reg);
            break;
        case WINED3DSPR_LOOP:
            TRACE("aL");
            break;
        case WINED3DSPR_SAMPLER:
            TRACE("s%u", reg);
            break;
        case WINED3DSPR_MISCTYPE:
            if (reg > 1) {
                FIXME("Unhandled misctype register %d\n", reg);
            } else {
                TRACE("%s", misctype_reg_names[reg]);
            }
            break;
        case WINED3DSPR_PREDICATE:
            TRACE("p%u", reg);
            break;
        default:
            TRACE("unhandled_rtype(%#x)", regtype);
            break;
   }

   if (!input) {
       /* operand output (for modifiers and shift, see dump_ins_modifiers) */

       if ((param & WINED3DSP_WRITEMASK_ALL) != WINED3DSP_WRITEMASK_ALL) {
           TRACE(".");
           if (param & WINED3DSP_WRITEMASK_0) TRACE("%c", swizzle_reg_chars[0]);
           if (param & WINED3DSP_WRITEMASK_1) TRACE("%c", swizzle_reg_chars[1]);
           if (param & WINED3DSP_WRITEMASK_2) TRACE("%c", swizzle_reg_chars[2]);
           if (param & WINED3DSP_WRITEMASK_3) TRACE("%c", swizzle_reg_chars[3]);
       }

   } else {
        /** operand input */
        DWORD swizzle = (param & WINED3DSP_SWIZZLE_MASK) >> WINED3DSP_SWIZZLE_SHIFT;
        DWORD swizzle_x = swizzle & 0x03;
        DWORD swizzle_y = (swizzle >> 2) & 0x03;
        DWORD swizzle_z = (swizzle >> 4) & 0x03;
        DWORD swizzle_w = (swizzle >> 6) & 0x03;

        if (0 != modifier) {
            switch (modifier) {
                case WINED3DSPSM_NONE:    break;
                case WINED3DSPSM_NEG:     break;
                case WINED3DSPSM_NOT:     break;
                case WINED3DSPSM_BIAS:    TRACE("_bias"); break;
                case WINED3DSPSM_BIASNEG: TRACE("_bias"); break;
                case WINED3DSPSM_SIGN:    TRACE("_bx2"); break;
                case WINED3DSPSM_SIGNNEG: TRACE("_bx2"); break;
                case WINED3DSPSM_COMP:    break;
                case WINED3DSPSM_X2:      TRACE("_x2"); break;
                case WINED3DSPSM_X2NEG:   TRACE("_x2"); break;
                case WINED3DSPSM_DZ:      TRACE("_dz"); break;
                case WINED3DSPSM_DW:      TRACE("_dw"); break;
                case WINED3DSPSM_ABSNEG:  TRACE(")"); break;
                case WINED3DSPSM_ABS:     TRACE(")"); break;
                default:
                    TRACE("_unknown_modifier(%#x)", modifier >> WINED3DSP_SRCMOD_SHIFT);
            }
        }

        /**
        * swizzle bits fields:
        *  RRGGBBAA
        */
        if ((WINED3DVS_NOSWIZZLE >> WINED3DVS_SWIZZLE_SHIFT) != swizzle) {
            if (swizzle_x == swizzle_y &&
                swizzle_x == swizzle_z &&
                swizzle_x == swizzle_w) {
                    TRACE(".%c", swizzle_reg_chars[swizzle_x]);
            } else {
                TRACE(".%c%c%c%c",
                swizzle_reg_chars[swizzle_x],
                swizzle_reg_chars[swizzle_y],
                swizzle_reg_chars[swizzle_z],
                swizzle_reg_chars[swizzle_w]);
            }
        }
    }
}

/* Shared code in order to generate the bulk of the shader string.
 * NOTE: A description of how to parse tokens can be found on msdn */
void shader_generate_main(IWineD3DBaseShader *iface, SHADER_BUFFER* buffer,
        const shader_reg_maps* reg_maps, CONST DWORD* pFunction)
{
    IWineD3DBaseShaderImpl* This = (IWineD3DBaseShaderImpl*) iface;
    IWineD3DDeviceImpl *device = (IWineD3DDeviceImpl *) This->baseShader.device; /* To access shader backend callbacks */
    const SHADER_OPCODE *opcode_table = This->baseShader.shader_ins;
    const SHADER_HANDLER *handler_table = device->shader_backend->shader_instruction_handler_table;
    DWORD shader_version = reg_maps->shader_version;
    struct wined3d_shader_dst_param dst_param;
    struct wined3d_shader_instruction ins;
    const DWORD *pToken = pFunction;
    const SHADER_OPCODE *curOpcode;
    SHADER_HANDLER hw_fct;
    DWORD i;

    /* Initialize current parsing state */
    ins.shader = iface;
    ins.buffer = buffer;
    ins.reg_maps = reg_maps;
    ins.dst = &dst_param;
    This->baseShader.parse_state.current_row = 0;

    while (WINED3DPS_END() != *pToken)
    {
        DWORD opcode_token;

        /* Skip version token */
        if (shader_is_version_token(*pToken))
        {
            ++pToken;
            continue;
        }

        /* Skip comment tokens */
        if (shader_is_comment(*pToken))
        {
            pToken += (*pToken & WINED3DSI_COMMENTSIZE_MASK) >> WINED3DSI_COMMENTSIZE_SHIFT;
            ++pToken;
            continue;
        }

        /* Read opcode */
        opcode_token = *pToken++;
        curOpcode = shader_get_opcode(opcode_table, shader_version, opcode_token);

        /* Unknown opcode and its parameters */
        if (!curOpcode)
        {
            FIXME("Unrecognized opcode: token=0x%08x\n", opcode_token);
            pToken += shader_skip_unrecognized(pToken, shader_version);
            continue;
        }

        /* Nothing to do */
        if (WINED3DSIO_DCL == curOpcode->opcode
                || WINED3DSIO_NOP == curOpcode->opcode
                || WINED3DSIO_DEF == curOpcode->opcode
                || WINED3DSIO_DEFI == curOpcode->opcode
                || WINED3DSIO_DEFB == curOpcode->opcode
                || WINED3DSIO_PHASE == curOpcode->opcode
                || WINED3DSIO_RET == curOpcode->opcode)
        {
            pToken += shader_skip_opcode(curOpcode, opcode_token, shader_version);
            continue;
        }

        /* Select handler */
        hw_fct = handler_table[curOpcode->handler_idx];

        /* Unhandled opcode */
        if (!hw_fct)
        {
            FIXME("Can't handle opcode %s in hwShader\n", curOpcode->name);
            pToken += shader_skip_opcode(curOpcode, opcode_token, shader_version);
            continue;
        }

        ins.handler_idx = curOpcode->handler_idx;
        ins.flags = opcode_token & WINED3D_OPCODESPECIFICCONTROL_MASK;
        ins.coissue = opcode_token & WINED3DSI_COISSUE;

        /* Destination token */
        ins.dst_count = curOpcode->dst_token ? 1 : 0;
        if (ins.dst_count)
        {
            DWORD param, addr_param = 0;
            pToken += shader_get_param(pToken, shader_version, &param, &addr_param);
            shader_parse_dst_param(param, addr_param, &dst_param);
        }

        /* Predication token */
        if (opcode_token & WINED3DSHADER_INSTRUCTION_PREDICATED) ins.predicate = *pToken++;

        /* Other source tokens */
        ins.src_count = curOpcode->num_params - curOpcode->dst_token;
        for (i = 0; i < ins.src_count; ++i)
        {
            DWORD param, addr_token = 0;
            pToken += shader_get_param(pToken, shader_version, &param, &addr_token);
            ins.src[i] = param;
            ins.src_addr[i] = addr_token;
        }

        /* Call appropriate function for output target */
        hw_fct(&ins);

        /* Process instruction modifiers for GLSL apps ( _sat, etc. ) */
        /* FIXME: This should be internal to the shader backend.
         * Also, right now this is the only reason "shader_mode" exists. */
        if (This->baseShader.shader_mode == SHADER_GLSL) shader_glsl_add_instruction_modifiers(&ins);
    }
}

static void shader_dump_ins_modifiers(const DWORD output)
{
    DWORD shift = (output & WINED3DSP_DSTSHIFT_MASK) >> WINED3DSP_DSTSHIFT_SHIFT;
    DWORD mmask = output & WINED3DSP_DSTMOD_MASK;

    switch (shift) {
        case 0: break;
        case 13: TRACE("_d8"); break;
        case 14: TRACE("_d4"); break;
        case 15: TRACE("_d2"); break;
        case 1: TRACE("_x2"); break;
        case 2: TRACE("_x4"); break;
        case 3: TRACE("_x8"); break;
        default: TRACE("_unhandled_shift(%d)", shift); break;
    }

    if (mmask & WINED3DSPDM_SATURATE)         TRACE("_sat");
    if (mmask & WINED3DSPDM_PARTIALPRECISION) TRACE("_pp");
    if (mmask & WINED3DSPDM_MSAMPCENTROID)    TRACE("_centroid");

    mmask &= ~(WINED3DSPDM_SATURATE | WINED3DSPDM_PARTIALPRECISION | WINED3DSPDM_MSAMPCENTROID);
    if (mmask)
        FIXME("_unrecognized_modifier(%#x)", mmask >> WINED3DSP_DSTMOD_SHIFT);
}

void shader_trace_init(const DWORD *pFunction, const SHADER_OPCODE *opcode_table)
{
    const DWORD* pToken = pFunction;
    const SHADER_OPCODE* curOpcode = NULL;
    DWORD shader_version;
    DWORD opcode_token;
    DWORD i;

    TRACE("Parsing %p\n", pFunction);

    /* The version token is supposed to be the first token */
    if (!shader_is_version_token(*pToken))
    {
        FIXME("First token is not a version token, invalid shader.\n");
        return;
    }
    shader_version = *pToken++;
    TRACE("%s_%u_%u\n", shader_is_pshader_version(shader_version) ? "ps": "vs",
            WINED3DSHADER_VERSION_MAJOR(shader_version), WINED3DSHADER_VERSION_MINOR(shader_version));

    while (WINED3DVS_END() != *pToken)
    {
        if (shader_is_comment(*pToken)) /* comment */
        {
            DWORD comment_len = (*pToken & WINED3DSI_COMMENTSIZE_MASK) >> WINED3DSI_COMMENTSIZE_SHIFT;
            ++pToken;
            TRACE("//%s\n", (const char*)pToken);
            pToken += comment_len;
            continue;
        }
        opcode_token = *pToken++;
        curOpcode = shader_get_opcode(opcode_table, shader_version, opcode_token);

        if (!curOpcode)
        {
            int tokens_read;
            FIXME("Unrecognized opcode: token=0x%08x\n", opcode_token);
            tokens_read = shader_skip_unrecognized(pToken, shader_version);
            pToken += tokens_read;
        }
        else
        {
            if (curOpcode->opcode == WINED3DSIO_DCL)
            {
                DWORD usage = *pToken;
                DWORD param = *(pToken + 1);

                shader_dump_decl_usage(usage, param, shader_version);
                shader_dump_ins_modifiers(param);
                TRACE(" ");
                shader_dump_param(param, 0, 0, shader_version);
                pToken += 2;
            }
            else if (curOpcode->opcode == WINED3DSIO_DEF)
            {
                unsigned int offset = shader_get_float_offset(*pToken);

                TRACE("def c%u = %f, %f, %f, %f", offset,
                        *(const float *)(pToken + 1),
                        *(const float *)(pToken + 2),
                        *(const float *)(pToken + 3),
                        *(const float *)(pToken + 4));
                pToken += 5;
            }
            else if (curOpcode->opcode == WINED3DSIO_DEFI)
            {
                TRACE("defi i%u = %d, %d, %d, %d", *pToken & WINED3DSP_REGNUM_MASK,
                        *(pToken + 1),
                        *(pToken + 2),
                        *(pToken + 3),
                        *(pToken + 4));
                pToken += 5;
            }
            else if (curOpcode->opcode == WINED3DSIO_DEFB)
            {
                TRACE("defb b%u = %s", *pToken & WINED3DSP_REGNUM_MASK,
                        *(pToken + 1)? "true": "false");
                pToken += 2;
            }
            else
            {
                DWORD param, addr_token;
                int tokens_read;

                /* Print out predication source token first - it follows
                 * the destination token. */
                if (opcode_token & WINED3DSHADER_INSTRUCTION_PREDICATED)
                {
                    TRACE("(");
                    shader_dump_param(*(pToken + 2), 0, 1, shader_version);
                    TRACE(") ");
                }
                if (opcode_token & WINED3DSI_COISSUE)
                {
                    /* PixWin marks instructions with the coissue flag with a '+' */
                    TRACE("+");
                }

                TRACE("%s", curOpcode->name);

                if (curOpcode->opcode == WINED3DSIO_IFC
                        || curOpcode->opcode == WINED3DSIO_BREAKC)
                {
                    DWORD op = (opcode_token & INST_CONTROLS_MASK) >> INST_CONTROLS_SHIFT;

                    switch (op)
                    {
                        case COMPARISON_GT: TRACE("_gt"); break;
                        case COMPARISON_EQ: TRACE("_eq"); break;
                        case COMPARISON_GE: TRACE("_ge"); break;
                        case COMPARISON_LT: TRACE("_lt"); break;
                        case COMPARISON_NE: TRACE("_ne"); break;
                        case COMPARISON_LE: TRACE("_le"); break;
                        default: TRACE("_(%u)", op);
                    }
                }
                else if (curOpcode->opcode == WINED3DSIO_TEX
                        && shader_version >= WINED3DPS_VERSION(2,0)
                        && (opcode_token & WINED3DSI_TEXLD_PROJECT))
                {
                    TRACE("p");
                }

                /* Destination token */
                if (curOpcode->dst_token)
                {
                    tokens_read = shader_get_param(pToken, shader_version, &param, &addr_token);
                    pToken += tokens_read;

                    shader_dump_ins_modifiers(param);
                    TRACE(" ");
                    shader_dump_param(param, addr_token, 0, shader_version);
                }

                /* Predication token - already printed out, just skip it */
                if (opcode_token & WINED3DSHADER_INSTRUCTION_PREDICATED)
                {
                    pToken++;
                }

                /* Other source tokens */
                for (i = curOpcode->dst_token; i < curOpcode->num_params; ++i)
                {
                    tokens_read = shader_get_param(pToken, shader_version, &param, &addr_token);
                    pToken += tokens_read;

                    TRACE((i == 0)? " " : ", ");
                    shader_dump_param(param, addr_token, 1, shader_version);
                }
            }
            TRACE("\n");
        }
    }
}

void shader_cleanup(IWineD3DBaseShader *iface)
{
    IWineD3DBaseShaderImpl *This = (IWineD3DBaseShaderImpl *)iface;

    ((IWineD3DDeviceImpl *)This->baseShader.device)->shader_backend->shader_destroy(iface);
    HeapFree(GetProcessHeap(), 0, This->baseShader.function);
    shader_delete_constant_list(&This->baseShader.constantsF);
    shader_delete_constant_list(&This->baseShader.constantsB);
    shader_delete_constant_list(&This->baseShader.constantsI);
    list_remove(&This->baseShader.shader_list_entry);
}

static const SHADER_HANDLER shader_none_instruction_handler_table[WINED3DSIH_TABLE_SIZE] = {0};
static void shader_none_select(IWineD3DDevice *iface, BOOL usePS, BOOL useVS) {}
static void shader_none_select_depth_blt(IWineD3DDevice *iface, enum tex_types tex_type) {}
static void shader_none_deselect_depth_blt(IWineD3DDevice *iface) {}
static void shader_none_update_float_vertex_constants(IWineD3DDevice *iface, UINT start, UINT count) {}
static void shader_none_update_float_pixel_constants(IWineD3DDevice *iface, UINT start, UINT count) {}
static void shader_none_load_constants(IWineD3DDevice *iface, char usePS, char useVS) {}
static void shader_none_load_np2fixup_constants(IWineD3DDevice *iface, char usePS, char useVS) {}
static void shader_none_destroy(IWineD3DBaseShader *iface) {}
static HRESULT shader_none_alloc(IWineD3DDevice *iface) {return WINED3D_OK;}
static void shader_none_free(IWineD3DDevice *iface) {}
static BOOL shader_none_dirty_const(IWineD3DDevice *iface) {return FALSE;}
static GLuint shader_none_generate_pshader(IWineD3DPixelShader *iface, SHADER_BUFFER *buffer, const struct ps_compile_args *args) {
    FIXME("NONE shader backend asked to generate a pixel shader\n");
    return 0;
}
static GLuint shader_none_generate_vshader(IWineD3DVertexShader *iface, SHADER_BUFFER *buffer, const struct vs_compile_args *args) {
    FIXME("NONE shader backend asked to generate a vertex shader\n");
    return 0;
}

#define GLINFO_LOCATION      (*gl_info)
static void shader_none_get_caps(WINED3DDEVTYPE devtype, const WineD3D_GL_Info *gl_info, struct shader_caps *pCaps)
{
    /* Set the shader caps to 0 for the none shader backend */
    pCaps->VertexShaderVersion  = 0;
    pCaps->PixelShaderVersion    = 0;
    pCaps->PixelShader1xMaxValue = 0.0;
}
#undef GLINFO_LOCATION
static BOOL shader_none_color_fixup_supported(struct color_fixup_desc fixup)
{
    if (TRACE_ON(d3d_shader) && TRACE_ON(d3d))
    {
        TRACE("Checking support for fixup:\n");
        dump_color_fixup_desc(fixup);
    }

    /* Faked to make some apps happy. */
    if (!is_yuv_fixup(fixup))
    {
        TRACE("[OK]\n");
        return TRUE;
    }

    TRACE("[FAILED]\n");
    return FALSE;
}

const shader_backend_t none_shader_backend = {
    shader_none_instruction_handler_table,
    shader_none_select,
    shader_none_select_depth_blt,
    shader_none_deselect_depth_blt,
    shader_none_update_float_vertex_constants,
    shader_none_update_float_pixel_constants,
    shader_none_load_constants,
    shader_none_load_np2fixup_constants,
    shader_none_destroy,
    shader_none_alloc,
    shader_none_free,
    shader_none_dirty_const,
    shader_none_generate_pshader,
    shader_none_generate_vshader,
    shader_none_get_caps,
    shader_none_color_fixup_supported,
};
