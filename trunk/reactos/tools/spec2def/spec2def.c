#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

typedef struct _STRING
{
    const char *buf;
    int len;
} STRING, *PSTRING;

typedef struct
{
    STRING strName;
    STRING strTarget;
    int nCallingConvention;
    int nOrdinal;
    int nStackBytes;
    int nArgCount;
    int anArgs[30];
    unsigned int uFlags;
    int nNumber;
} EXPORT;

enum _ARCH
{
    ARCH_X86,
    ARCH_AMD64,
    ARCH_IA64,
    ARCH_ARM,
    ARCH_PPC
};

typedef int (*PFNOUTLINE)(FILE *, EXPORT *);
int gbMSComp = 0;
int gbImportLib = 0;
int giArch = ARCH_X86;
char *pszArchString = "i386";
char *pszArchString2;
char *pszDllName = 0;
char *gpszUnderscore = "";
int gbDebug;
#define DbgPrint(...) (!gbDebug || fprintf(stderr, __VA_ARGS__))

enum
{
    FL_PRIVATE = 1,
    FL_STUB = 2,
    FL_NONAME = 4,
    FL_ORDINAL = 8,
};

enum
{
    CC_STDCALL,
    CC_CDECL,
    CC_FASTCALL,
    CC_THISCALL,
    CC_EXTERN,
    CC_STUB,
};

enum
{
    ARG_LONG,
    ARG_PTR,
    ARG_STR,
    ARG_WSTR,
    ARG_DBL,
    ARG_INT64,
    ARG_INT128,
    ARG_FLOAT
};

char* astrCallingConventions[] =
{
    "STDCALL",
    "CDECL",
    "FASTCALL",
    "THISCALL",
    "EXTERN"
};

static
int
IsSeparator(char chr)
{
    return ((chr <= ',' && chr != '$') ||
            (chr >= ':' && chr < '?') );
}

int
CompareToken(const char *token, const char *comparand)
{
    while (*comparand)
    {
        if (*token != *comparand) return 0;
        token++;
        comparand++;
    }
    if (!IsSeparator(*token)) return 0;
    return 1;
}

const char *
ScanToken(const char *token, char chr)
{
    while (!IsSeparator(*token))
    {
        if (*token == chr) return token;
        token++;
    }
    return 0;
}

char *
NextLine(char *pc)
{
    while (*pc != 0)
    {
        if (pc[0] == '\n' && pc[1] == '\r') return pc + 2;
        else if (pc[0] == '\n') return pc + 1;
        pc++;
    }
    return pc;
}

int
TokenLength(char *pc)
{
    int length = 0;

    while (!IsSeparator(*pc++)) length++;

    return length;
}

char *
NextToken(char *pc)
{
    /* Skip token */
    while (!IsSeparator(*pc)) pc++;

    /* Skip white spaces */
    while (*pc == ' ' || *pc == '\t') pc++;

    /* Check for end of line */
    if (*pc == '\n' || *pc == '\r' || *pc == 0) return 0;

    /* Check for comment */
    if (*pc == '#' || *pc == ';') return 0;

    return pc;
}

void
OutputHeader_stub(FILE *file)
{
    fprintf(file, "/* This file is autogenerated, do not edit. */\n\n"
            "#include <stubs.h>\n\n");
}

int
OutputLine_stub(FILE *file, EXPORT *pexp)
{
    int i;

    if (pexp->nCallingConvention != CC_STUB &&
        (pexp->uFlags & FL_STUB) == 0) return 0;

    fprintf(file, "int ");
    if ((giArch == ARCH_X86) &&
        pexp->nCallingConvention == CC_STDCALL)
    {
        fprintf(file, "__stdcall ");
    }

    /* Check for C++ */
    if (pexp->strName.buf[0] == '?')
    {
        fprintf(file, "stub_function%d(", pexp->nNumber);
    }
    else
    {
        fprintf(file, "%.*s(", pexp->strName.len, pexp->strName.buf);
    }

    for (i = 0; i < pexp->nArgCount; i++)
    {
        if (i != 0) fprintf(file, ", ");
        switch (pexp->anArgs[i])
        {
            case ARG_LONG: fprintf(file, "long"); break;
            case ARG_PTR:  fprintf(file, "void*"); break;
            case ARG_STR:  fprintf(file, "char*"); break;
            case ARG_WSTR: fprintf(file, "wchar_t*"); break;
            case ARG_DBL:
            case ARG_INT64 :  fprintf(file, "__int64"); break;
            case ARG_INT128 :  fprintf(file, "__int128"); break;
            case ARG_FLOAT: fprintf(file, "float"); break;
        }
        fprintf(file, " a%d", i);
    }
    fprintf(file, ")\n{\n\tDbgPrint(\"WARNING: calling stub %.*s(",
            pexp->strName.len, pexp->strName.buf);

    for (i = 0; i < pexp->nArgCount; i++)
    {
        if (i != 0) fprintf(file, ",");
        switch (pexp->anArgs[i])
        {
            case ARG_LONG: fprintf(file, "0x%%lx"); break;
            case ARG_PTR:  fprintf(file, "0x%%p"); break;
            case ARG_STR:  fprintf(file, "'%%s'"); break;
            case ARG_WSTR: fprintf(file, "'%%ws'"); break;
            case ARG_DBL:  fprintf(file, "%%f"); break;
            case ARG_INT64: fprintf(file, "%%\"PRix64\""); break;
            case ARG_INT128: fprintf(file, "%%\"PRix128\""); break;
            case ARG_FLOAT: fprintf(file, "%%f"); break;
        }
    }
    fprintf(file, ")\\n\"");

    for (i = 0; i < pexp->nArgCount; i++)
    {
        fprintf(file, ", ");
        switch (pexp->anArgs[i])
        {
            case ARG_LONG: fprintf(file, "(long)a%d", i); break;
            case ARG_PTR:  fprintf(file, "(void*)a%d", i); break;
            case ARG_STR:  fprintf(file, "(char*)a%d", i); break;
            case ARG_WSTR: fprintf(file, "(wchar_t*)a%d", i); break;
            case ARG_DBL:  fprintf(file, "(double)a%d", i); break;
            case ARG_INT64: fprintf(file, "(__int64)a%d", i); break;
            case ARG_INT128: fprintf(file, "(__int128)a%d", i); break;
            case ARG_FLOAT: fprintf(file, "(float)a%d", i); break;
        }
    }
    fprintf(file, ");\n");

    if (pexp->nCallingConvention == CC_STUB)
    {
        fprintf(file, "\t__wine_spec_unimplemented_stub(\"%s\", __FUNCTION__);\n", pszDllName);
    }

    fprintf(file, "\treturn 0;\n}\n\n");

    return 1;
}

void
OutputHeader_asmstub(FILE *file, char *libname)
{
    fprintf(file, "; File generated automatically, do not edit! \n\n");

    if (giArch == ARCH_X86)
    {
        fprintf(file, ".586\n.model flat\n");
    }
    else if (giArch == ARCH_ARM)
    {
        fprintf(file, "#include <kxarm.h>\n        TEXTAREA\n");
    }

    fprintf(file, ".code\n");
}

int
OutputLine_asmstub(FILE *fileDest, EXPORT *pexp)
{
    /* Handle autoname */
    if (pexp->strName.len == 1 && pexp->strName.buf[0] == '@')
    {
        fprintf(fileDest, "PUBLIC %sordinal%d\n%sordinal%d: nop\n",
                gpszUnderscore, pexp->nOrdinal, gpszUnderscore, pexp->nOrdinal);
    }
    else if (giArch != ARCH_X86)
    {
        fprintf(fileDest, "PUBLIC _stub_%.*s\n_stub_%.*s: nop\n",
                pexp->strName.len, pexp->strName.buf,
                pexp->strName.len, pexp->strName.buf);
    }
    else if (pexp->nCallingConvention == CC_STDCALL)
    {
        fprintf(fileDest, "PUBLIC __stub_%.*s@%d\n__stub_%.*s@%d: nop\n",
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes,
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes);
    }
    else if (pexp->nCallingConvention == CC_FASTCALL)
    {
        fprintf(fileDest, "PUBLIC @_stub_%.*s@%d\n@_stub_%.*s@%d: nop\n",
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes,
                pexp->strName.len, pexp->strName.buf, pexp->nStackBytes);
    }
    else if (pexp->nCallingConvention == CC_CDECL ||
             pexp->nCallingConvention == CC_STUB)
    {
        fprintf(fileDest, "PUBLIC __stub_%.*s\n__stub_%.*s: nop\n",
                pexp->strName.len, pexp->strName.buf,
                pexp->strName.len, pexp->strName.buf);
    }
    else if (pexp->nCallingConvention == CC_EXTERN)
    {
        fprintf(fileDest, "PUBLIC __stub_%.*s\n__stub_%.*s:\n",
                pexp->strName.len, pexp->strName.buf,
                pexp->strName.len, pexp->strName.buf);
    }

    return 1;
}

void
OutputHeader_def(FILE *file, char *libname)
{
    fprintf(file,
            "; File generated automatically, do not edit!\n\n"
            "NAME %s\n\n"
            "EXPORTS\n",
            libname);
}

void
PrintName(FILE *fileDest, EXPORT *pexp, PSTRING pstr, int fDeco)
{
    const char *pcName = pstr->buf;
    int nNameLength = pstr->len;
    const char* pcDot, *pcAt;

    /* Check for non-x86 first */
    if (giArch != ARCH_X86)
    {
        /* Does the string already have stdcall decoration? */
        pcAt = ScanToken(pcName, '@');
        if (pcAt && (pcAt < (pcName + nNameLength)) && (pcName[0] == '_'))
        {
            /* Skip leading underscore and remove trailing decoration */
            pcName++;
            nNameLength = pcAt - pcName;
        }

        /* Print the undecorated function name */
        fprintf(fileDest, "%.*s", nNameLength, pcName);
    }
    else if (fDeco &&
        ((pexp->nCallingConvention == CC_STDCALL) ||
         (pexp->nCallingConvention == CC_FASTCALL)))
    {
        /* Scan for a dll forwarding dot */
        pcDot = ScanToken(pcName, '.');
        if (pcDot)
        {
            /* First print the dll name, followed by a dot */
            nNameLength = pcDot - pcName;
            fprintf(fileDest, "%.*s.", nNameLength, pcName);

            /* Now the actual function name */
            pcName = pcDot + 1;
            nNameLength = pexp->strTarget.len - nNameLength - 1;
        }

        /* Does the string already have decoration? */
        pcAt = ScanToken(pcName, '@');
        if (pcAt && (pcAt < (pcName + nNameLength)))
        {
            /* On GCC, we need to remove the leading stdcall underscore */
            if (!gbMSComp && (pexp->nCallingConvention == CC_STDCALL))
            {
                pcName++;
                nNameLength--;
            }

            /* Print the already decorated function name */
            fprintf(fileDest, "%.*s", nNameLength, pcName);
        }
        else
        {
            /* Print the prefix, but skip it for (GCC && stdcall) */
            if (gbMSComp || (pexp->nCallingConvention != CC_STDCALL))
            {
                fprintf(fileDest, "%c", pexp->nCallingConvention == CC_FASTCALL ? '@' : '_');
            }

            /* Print the name with trailing decoration */
            fprintf(fileDest, "%.*s@%d", nNameLength, pcName, pexp->nStackBytes);
        }
    }
    else
    {
        /* Print the undecorated function name */
        fprintf(fileDest, "%.*s", nNameLength, pcName);
    }
}

void
OutputLine_def_MS(FILE *fileDest, EXPORT *pexp)
{
    PrintName(fileDest, pexp, &pexp->strName, 0);

    if (gbImportLib)
    {
        /* Redirect to a stub function, to get the right decoration in the lib */
        fprintf(fileDest, "=_stub_%.*s", pexp->strName.len, pexp->strName.buf);
    }
    else if (pexp->strTarget.buf)
    {
        if (pexp->strName.buf[0] == '?')
        {
            fprintf(stderr, "warning: ignoring C++ redirection %.*s -> %.*s\n",
                    pexp->strName.len, pexp->strName.buf, pexp->strTarget.len, pexp->strTarget.buf);
        }
        else
        {
            fprintf(fileDest, "=");

            /* If the original name was decorated, use decoration in the forwarder as well */
            if ((giArch == ARCH_X86) && ScanToken(pexp->strName.buf, '@') &&
                !ScanToken(pexp->strTarget.buf, '@') &&
                ((pexp->nCallingConvention == CC_STDCALL) ||
                (pexp->nCallingConvention == CC_FASTCALL)) )
            {
                PrintName(fileDest, pexp, &pexp->strTarget, 1);
            }
            else
            {
                /* Write the undecorated redirection name */
                fprintf(fileDest, "%.*s", pexp->strTarget.len, pexp->strTarget.buf);
            }
        }
    }
    else if (((pexp->uFlags & FL_STUB) || (pexp->nCallingConvention == CC_STUB)) &&
             (pexp->strName.buf[0] == '?'))
    {
        /* C++ stubs are forwarded to C stubs */
        fprintf(fileDest, "=stub_function%d", pexp->nNumber);
    }
}

void
OutputLine_def_GCC(FILE *fileDest, EXPORT *pexp)
{
    /* Print the function name, with decoration for export libs */
    PrintName(fileDest, pexp, &pexp->strName, gbImportLib);
    DbgPrint("Generating def line for '%.*s'\n", pexp->strName.len, pexp->strName.buf);

    /* Check if this is a forwarded export */
    if (pexp->strTarget.buf)
    {
        int fIsExternal = !!ScanToken(pexp->strTarget.buf, '.');
        DbgPrint("Got redirect '%.*s'\n", pexp->strTarget.len, pexp->strTarget.buf);

        /* print the target name, don't decorate if it is external */
        fprintf(fileDest, "=");
        PrintName(fileDest, pexp, &pexp->strTarget, !fIsExternal);
    }
    else if (((pexp->uFlags & FL_STUB) || (pexp->nCallingConvention == CC_STUB)) &&
             (pexp->strName.buf[0] == '?'))
    {
        /* C++ stubs are forwarded to C stubs */
        fprintf(fileDest, "=stub_function%d", pexp->nNumber);
    }

    /* Special handling for stdcall and fastcall */
    if ((giArch == ARCH_X86) &&
        ((pexp->nCallingConvention == CC_STDCALL) ||
         (pexp->nCallingConvention == CC_FASTCALL)))
    {
        /* Is this the import lib? */
        if (gbImportLib)
        {
            /* Is the name in the spec file decorated? */
            const char* pcDeco = ScanToken(pexp->strName.buf, '@');
            if (pcDeco && (pcDeco < pexp->strName.buf + pexp->strName.len))
            {
                /* Write the name including the leading @  */
                fprintf(fileDest, "==%.*s", pexp->strName.len, pexp->strName.buf);
            }
        }
        else if (!pexp->strTarget.buf)
        {
            /* Write a forwarder to the actual decorated symbol */
            fprintf(fileDest, "=");
            PrintName(fileDest, pexp, &pexp->strName, 1);
        }
    }
}

int
OutputLine_def(FILE *fileDest, EXPORT *pexp)
{
    DbgPrint("OutputLine_def: '%.*s'...\n", pexp->strName.len, pexp->strName.buf);
    fprintf(fileDest, " ");

    if (gbMSComp)
        OutputLine_def_MS(fileDest, pexp);
    else
        OutputLine_def_GCC(fileDest, pexp);

    if (pexp->uFlags & FL_ORDINAL)
    {
        fprintf(fileDest, " @%d", pexp->nOrdinal);
    }

    if (pexp->uFlags & FL_NONAME)
    {
        fprintf(fileDest, " NONAME");
    }

    if (pexp->uFlags & FL_PRIVATE)
    {
        fprintf(fileDest, " PRIVATE");
    }

    if (pexp->nCallingConvention == CC_EXTERN)
    {
        fprintf(fileDest, " DATA");
    }

    fprintf(fileDest, "\n");

    return 1;
}

int
ParseFile(char* pcStart, FILE *fileDest, PFNOUTLINE OutputLine)
{
    char *pc, *pcLine;
    int nLine;
    EXPORT exp;
    int included;
    char namebuffer[16];

    //fprintf(stderr, "info: line %d, pcStart:'%.30s'\n", nLine, pcStart);

    /* Loop all lines */
    nLine = 1;
    exp.nNumber = 0;
    for (pcLine = pcStart; *pcLine; pcLine = NextLine(pcLine), nLine++)
    {
        pc = pcLine;

        exp.nArgCount = 0;
        exp.uFlags = 0;
        exp.nNumber++;


        //if (!strncmp(pcLine, "22 stdcall @(long) MPR_Alloc",28))
        //    gbDebug = 1;

        //fprintf(stderr, "info: line %d, token:'%d, %.20s'\n",
        //        nLine, TokenLength(pcLine), pcLine);

        /* Skip white spaces */
        while (*pc == ' ' || *pc == '\t') pc++;

        /* Skip empty lines, stop at EOF */
        if (*pc == ';' || *pc <= '#') continue;
        if (*pc == 0) return 0;

        //fprintf(stderr, "info: line %d, token:'%.*s'\n",
        //        nLine, TokenLength(pc), pc);

        /* Now we should get either an ordinal or @ */
        if (*pc == '@')
            exp.nOrdinal = -1;
        else
        {
            exp.nOrdinal = atol(pc);
            exp.uFlags |= FL_ORDINAL;
        }

        /* Go to next token (type) */
        if (!(pc = NextToken(pc)))
        {
            fprintf(stderr, "error: line %d, unexpected end of line\n", nLine);
            return -10;
        }

        //fprintf(stderr, "info: Token:'%.*s'\n", TokenLength(pc), pc);

        /* Now we should get the type */
        if (CompareToken(pc, "stdcall"))
        {
            exp.nCallingConvention = CC_STDCALL;
        }
        else if (CompareToken(pc, "cdecl") ||
                 CompareToken(pc, "varargs"))
        {
            exp.nCallingConvention = CC_CDECL;
        }
        else if (CompareToken(pc, "fastcall"))
        {
            exp.nCallingConvention = CC_FASTCALL;
        }
        else if (CompareToken(pc, "thiscall"))
        {
            exp.nCallingConvention = CC_THISCALL;
        }
        else if (CompareToken(pc, "extern"))
        {
            exp.nCallingConvention = CC_EXTERN;
        }
        else if (CompareToken(pc, "stub"))
        {
            exp.nCallingConvention = CC_STUB;
        }
        else
        {
            fprintf(stderr, "error: line %d, expected callconv, got '%.*s' %d\n",
                    nLine, TokenLength(pc), pc, *pc);
            return -11;
        }

        //fprintf(stderr, "info: nCallingConvention: %d\n", exp.nCallingConvention);

        /* Go to next token (options or name) */
        if (!(pc = NextToken(pc)))
        {
            fprintf(stderr, "fail2\n");
            return -12;
        }

        /* Handle options */
        included = 1;
        while (*pc == '-')
        {
            if (CompareToken(pc, "-arch"))
            {
                /* Default to not included */
                included = 0;
                pc += 5;

                /* Look if we are included */
                while (*pc == '=' || *pc == ',')
                {
                    pc++;
                    if (CompareToken(pc, pszArchString) ||
                        CompareToken(pc, pszArchString2))
                    {
                        included = 1;
                    }

                    /* Skip to next arch or end */
                    while (*pc > ',') pc++;
                }
            }
            else if (CompareToken(pc, "-i386"))
            {
                if (giArch != ARCH_X86) included = 0;
            }
            else if (CompareToken(pc, "-private"))
            {
                exp.uFlags |= FL_PRIVATE;
            }
            else if (CompareToken(pc, "-noname"))
            {
                exp.uFlags |= FL_ORDINAL | FL_NONAME;
            }
            else if (CompareToken(pc, "-ordinal"))
            {
                exp.uFlags |= FL_ORDINAL;
            }
            else if (CompareToken(pc, "-stub"))
            {
                exp.uFlags |= FL_STUB;
            }
            else if (CompareToken(pc, "-norelay") ||
                     CompareToken(pc, "-register") ||
                     CompareToken(pc, "-ret64"))
            {
                /* silently ignore these */
            }
            else
            {
                fprintf(stderr, "info: ignored option: '%.*s'\n",
                        TokenLength(pc), pc);
            }

            /* Go to next token */
            pc = NextToken(pc);
        }

        //fprintf(stderr, "info: Name:'%.10s'\n", pc);

        /* If arch didn't match ours, skip this entry */
        if (!included) continue;

        /* Get name */
        exp.strName.buf = pc;
        exp.strName.len = TokenLength(pc);
        DbgPrint("Got name: '%.*s'\n", exp.strName.len, exp.strName.buf);

        /* Check for autoname */
        if ((exp.strName.len == 1) && (exp.strName.buf[0] == '@'))
        {
            sprintf(namebuffer, "ordinal%d", exp.nOrdinal);
            exp.strName.len = strlen(namebuffer);
            exp.strName.buf = namebuffer;
            exp.uFlags |= FL_ORDINAL | FL_NONAME;
        }

        /* Handle parameters */
        exp.nStackBytes = 0;
        if (exp.nCallingConvention != CC_EXTERN &&
            exp.nCallingConvention != CC_STUB)
        {
            /* Go to next token */
            if (!(pc = NextToken(pc)))
            {
                fprintf(stderr, "fail4\n");
                return -13;
            }

            /* Verify syntax */
            if (*pc++ != '(')
            {
                fprintf(stderr, "error: line %d, expected '('\n", nLine);
                return -14;
            }

            /* Skip whitespaces */
            while (*pc == ' ' || *pc == '\t') pc++;

            exp.nStackBytes = 0;
            while (*pc >= '0')
            {
                if (CompareToken(pc, "long"))
                {
                    exp.nStackBytes += 4;
                    exp.anArgs[exp.nArgCount] = ARG_LONG;
                }
                else if (CompareToken(pc, "double"))
                {
                    exp.nStackBytes += 8;
                    exp.anArgs[exp.nArgCount] = ARG_DBL;
                }
                else if (CompareToken(pc, "ptr") ||
                         CompareToken(pc, "str") ||
                         CompareToken(pc, "wstr"))
                {
                    exp.nStackBytes += 4; // sizeof(void*) on x86
                    exp.anArgs[exp.nArgCount] = ARG_PTR; // FIXME: handle strings
                }
                else if (CompareToken(pc, "int64"))
                {
                    exp.nStackBytes += 8;
                    exp.anArgs[exp.nArgCount] = ARG_INT64;
                }
                else if (CompareToken(pc, "int128"))
                {
                    exp.nStackBytes += 16;
                    exp.anArgs[exp.nArgCount] = ARG_INT128;
                }
                else if (CompareToken(pc, "float"))
                {
                    exp.nStackBytes += 4;
                    exp.anArgs[exp.nArgCount] = ARG_FLOAT;
                }
                else
                    fprintf(stderr, "error: line %d, expected type, got: %.10s\n", nLine, pc);

                exp.nArgCount++;

                /* Go to next parameter */
                if (!(pc = NextToken(pc)))
                {
                    fprintf(stderr, "fail5\n");
                    return -15;
                }
            }

            /* Check syntax */
            if (*pc++ != ')')
            {
                fprintf(stderr, "error: line %d, expected ')'\n", nLine);
                return -16;
            }
        }

        /* Handle special stub cases */
        if (exp.nCallingConvention == CC_STUB)
        {
            /* Check for c++ mangled name */
            if (pc[0] == '?')
            {
                //printf("Found c++ mangled name...\n");
                //
            }
            else
            {
                /* Check for stdcall name */
                const char *p = ScanToken(pc, '@');
                if (p && (p - pc < exp.strName.len))
                {
                    int i;

                    /* Truncate the name to before the @ */
                    exp.strName.len = (int)(p - pc);
                    if (exp.strName.len < 1)
                    {
                        fprintf(stderr, "error, @ in line %d\n", nLine);
                        return -1;
                    }
                    exp.nStackBytes = atoi(p + 1);
                    exp.nArgCount =  exp.nStackBytes / 4;
                    exp.nCallingConvention = CC_STDCALL;
                    exp.uFlags |= FL_STUB;
                    for (i = 0; i < exp.nArgCount; i++)
                        exp.anArgs[i] = ARG_LONG;
                }
            }
        }

        /* Get optional redirection */
        pc = NextToken(pc);
        if (pc)
        {
            exp.strTarget.buf = pc;
            exp.strTarget.len = TokenLength(pc);

            /* Check syntax (end of line) */
            if (NextToken(pc))
            {
                 fprintf(stderr, "error: line %d, additional tokens after ')'\n", nLine);
                 return -17;
            }
        }
        else
        {
            exp.strTarget.buf = 0;
            exp.strTarget.len = 0;
        }

        /* Check for no-name without ordinal */
        if ((exp.uFlags & FL_ORDINAL) && (exp.nOrdinal == -1))
        {
            fprintf(stderr, "error: line %d, ordinal export without ordinal!\n", nLine);
            return -1;
        }

        OutputLine(fileDest, &exp);
        gbDebug = 0;
    }

    return 0;
}


void usage(void)
{
    printf("syntax: spec2pdef [<options> ...] <spec file>\n"
           "Possible options:\n"
           "  -h --help   prints this screen\n"
           "  -l=<file>   generates an asm lib stub\n"
           "  -d=<file>   generates a def file\n"
           "  -s=<file>   generates a stub file\n"
           "  --ms        msvc compatibility\n"
           "  -n=<name>   name of the dll\n"
           "  --implib    generate a def file for an import library\n"
           "  -a=<arch>   Set architecture to <arch>. (i386, x86_64, arm)\n");
}

int main(int argc, char *argv[])
{
    size_t nFileSize;
    char *pszSource, *pszDefFileName = 0, *pszStubFileName = 0, *pszLibStubName = 0;
    char achDllName[40];
    FILE *file;
    int result = 0, i;

    if (argc < 2)
    {
        usage();
        return -1;
    }

    /* Read options */
    for (i = 1; i < argc && *argv[i] == '-'; i++)
    {
        if ((strcasecmp(argv[i], "--help") == 0) ||
            (strcasecmp(argv[i], "-h") == 0))
        {
            usage();
            return 0;
        }
        else if (argv[i][1] == 'd' && argv[i][2] == '=')
        {
            pszDefFileName = argv[i] + 3;
        }
        else if (argv[i][1] == 'l' && argv[i][2] == '=')
        {
            pszLibStubName = argv[i] + 3;
        }
        else if (argv[i][1] == 's' && argv[i][2] == '=')
        {
            pszStubFileName = argv[i] + 3;
        }
        else if (argv[i][1] == 'n' && argv[i][2] == '=')
        {
            pszDllName = argv[i] + 3;
        }
        else if ((strcasecmp(argv[i], "--implib") == 0))
        {
            gbImportLib = 1;
        }
        else if ((strcasecmp(argv[i], "--ms") == 0))
        {
            gbMSComp = 1;
        }
        else if (argv[i][1] == 'a' && argv[i][2] == '=')
        {
            pszArchString = argv[i] + 3;
        }
        else
        {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            return -1;
        }
    }

    if (strcasecmp(pszArchString, "i386") == 0)
    {
        giArch = ARCH_X86;
        gpszUnderscore = "_";
    }
    else if (strcasecmp(pszArchString, "x86_64") == 0) giArch = ARCH_AMD64;
    else if (strcasecmp(pszArchString, "ia64") == 0) giArch = ARCH_IA64;
    else if (strcasecmp(pszArchString, "arm") == 0) giArch = ARCH_ARM;
    else if (strcasecmp(pszArchString, "ppc") == 0) giArch = ARCH_PPC;

    if ((giArch == ARCH_AMD64) || (giArch == ARCH_IA64))
    {
        pszArchString2 = "win64";
    }
    else
        pszArchString2 = "win32";

    /* Set a default dll name */
    if (!pszDllName)
    {
        char *p1, *p2;
        size_t len;

        p1 = strrchr(argv[i], '\\');
        if (!p1) p1 = strrchr(argv[i], '/');
        p2 = p1 = p1 ? p1 + 1 : argv[i];

        /* walk up to '.' */
        while (*p2 != '.' && *p2 != 0) p2++;
        len = p2 - p1;
        if (len >= sizeof(achDllName) - 5)
        {
            fprintf(stderr, "name too long: %s\n", p1);
            return -2;
        }

        strncpy(achDllName, p1, len);
        strncpy(achDllName + len, ".dll", sizeof(achDllName) - len);
        pszDllName = achDllName;
    }

    /* Open input file argv[1] */
    file = fopen(argv[i], "r");
    if (!file)
    {
        fprintf(stderr, "error: could not open file %s ", argv[i]);
        return -3;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    nFileSize = ftell(file);
    rewind(file);

    /* Allocate memory buffer */
    pszSource = malloc(nFileSize + 1);
    if (!pszSource)
    {
        fclose(file);
        return -4;
    }

    /* Load input file into memory */
    nFileSize = fread(pszSource, 1, nFileSize, file);
    fclose(file);

    /* Zero terminate the source */
    pszSource[nFileSize] = '\0';

    if (pszDefFileName)
    {
        /* Open output file */
        file = fopen(pszDefFileName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s ", argv[i + 1]);
            return -5;
        }

        OutputHeader_def(file, pszDllName);
        result = ParseFile(pszSource, file, OutputLine_def);
        fclose(file);
    }

    if (pszStubFileName)
    {
        /* Open output file */
        file = fopen(pszStubFileName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s ", argv[i + 1]);
            return -5;
        }

        OutputHeader_stub(file);
        result = ParseFile(pszSource, file, OutputLine_stub);
        fclose(file);
    }

    if (pszLibStubName)
    {
        /* Open output file */
        file = fopen(pszLibStubName, "w");
        if (!file)
        {
            fprintf(stderr, "error: could not open output file %s ", argv[i + 1]);
            return -5;
        }

        OutputHeader_asmstub(file, pszDllName);
        result = ParseFile(pszSource, file, OutputLine_asmstub);
        fprintf(file, "\nEND\n");
        fclose(file);
    }


    return result;
}
