#include "nvProgramToArb.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../log/log.h"

#define NV_MAX_OPERANDS 8
#define NV_MAX_OPERAND_LEN 256
#define NV_MAX_OUTPUTS 16

typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sbAppendN(StrBuf *sb, const char *s, size_t n)
{
    if (sb->len + n + 1 > sb->cap)
    {
        size_t cap = sb->cap ? sb->cap : 4096;
        while (sb->len + n + 1 > cap)
            cap *= 2;
        char *data = (char *)realloc(sb->data, cap);
        if (!data)
            return;
        sb->data = data;
        sb->cap = cap;
    }
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sbAppend(StrBuf *sb, const char *s)
{
    sbAppendN(sb, s, strlen(s));
}

static void sbPrintf(StrBuf *sb, const char *fmt, ...)
{
    char tmp[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0 && (size_t)n < sizeof(tmp))
        sbAppendN(sb, tmp, (size_t)n);
}

typedef enum
{
    OP_DIRECT, // same instruction in ARB
    OP_SWAP,   // ARB instruction with the two sources swapped (SGT, SLE)
    OP_SEQ,
    OP_SNE,
    OP_SFL,
    OP_STR,
    OP_DIV,
    OP_DP2,
    OP_NRM,
    OP_SSG,
    OP_ARL,
    OP_KIL
} NvOpKind;

static const struct
{
    const char *nv;
    const char *arb;
    NvOpKind kind;
} nvOps[] = {
    {"ABS", "ABS", OP_DIRECT}, {"ADD", "ADD", OP_DIRECT}, {"CMP", "CMP", OP_DIRECT}, {"COS", "COS", OP_DIRECT},
    {"DP3", "DP3", OP_DIRECT}, {"DP4", "DP4", OP_DIRECT}, {"DPH", "DPH", OP_DIRECT}, {"DST", "DST", OP_DIRECT},
    {"EX2", "EX2", OP_DIRECT}, {"EXP", "EXP", OP_DIRECT}, {"FLR", "FLR", OP_DIRECT}, {"FRC", "FRC", OP_DIRECT},
    {"LG2", "LG2", OP_DIRECT}, {"LIT", "LIT", OP_DIRECT}, {"LOG", "LOG", OP_DIRECT}, {"LRP", "LRP", OP_DIRECT},
    {"MAD", "MAD", OP_DIRECT}, {"MAX", "MAX", OP_DIRECT}, {"MIN", "MIN", OP_DIRECT}, {"MOV", "MOV", OP_DIRECT},
    {"MUL", "MUL", OP_DIRECT}, {"POW", "POW", OP_DIRECT}, {"RCC", "RCP", OP_DIRECT}, {"RCP", "RCP", OP_DIRECT},
    {"RSQ", "RSQ", OP_DIRECT}, {"SCS", "SCS", OP_DIRECT}, {"SGE", "SGE", OP_DIRECT}, {"SIN", "SIN", OP_DIRECT},
    {"SLT", "SLT", OP_DIRECT}, {"SUB", "SUB", OP_DIRECT}, {"SWZ", "SWZ", OP_DIRECT}, {"XPD", "XPD", OP_DIRECT},
    {"TEX", "TEX", OP_DIRECT}, {"TXB", "TXB", OP_DIRECT}, {"TXP", "TXP", OP_DIRECT}, {"SGT", "SLT", OP_SWAP},
    {"SLE", "SGE", OP_SWAP},   {"SEQ", NULL, OP_SEQ},     {"SNE", NULL, OP_SNE},     {"SFL", NULL, OP_SFL},
    {"STR", NULL, OP_STR},     {"DIV", NULL, OP_DIV},     {"DP2", NULL, OP_DP2},     {"NRM", NULL, OP_NRM},
    {"SSG", NULL, OP_SSG},     {"ARL", NULL, OP_ARL},     {"KIL", NULL, OP_KIL},
};

// Scratch registers added to the program. _nvk holds the constants 0, 1 and -1.
static const char *nvScratchTemps[] = {"_nvcc", "_nvr", "_nvm", "_nvs", "_nvs2", "_nva0", "_nva1", "_nva2", "_nvadr"};

typedef struct
{
    int isFragment;
    StrBuf options;
    StrBuf body;
    char outputs[NV_MAX_OUTPUTS][64];
    int outputCount;
    char addrName[32]; // address register of a vertex program
    char adrComp;      // component of the emulated address vector currently loaded in addrName.x, 0 if none
    int adrBias;       // offset added to that component before loading it
    int ended;
} NvCtx;

typedef struct
{
    char reg[NV_MAX_OPERAND_LEN]; // register without write mask
    char mask[8];                 // ".xy", or "" for all components
    char cond[3];                 // "GT", "NE"... or "" for an unconditional write
    char condSwz[8];              // ".x", or "" for the identity swizzle
} NvDst;

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    return s;
}

static int isIdentChar(char ch)
{
    return isalnum((unsigned char)ch) || ch == '_';
}

static int isSwizzle(const char *s)
{
    size_t n = strlen(s);
    return n >= 1 && n <= 4 && (strspn(s, "xyzw") == n || strspn(s, "rgba") == n);
}

static int usesName(const char *text, const char *name)
{
    size_t n = strlen(name);
    for (const char *p = strstr(text, name); p; p = strstr(p + 1, name))
    {
        if ((p == text || !isIdentChar(p[-1])) && !isIdentChar(p[n]))
            return 1;
    }
    return 0;
}

// Splits an opcode such as DP3R_SAT or SGTRC into the base instruction, the condition code update flag and the
// saturation mode (1 = [0,1], 2 = [-1,1]). The precision suffix (R, H, X) is dropped: everything runs in fp32.
static int findOp(const char *opcode, int *ccUpdate, int *satMode)
{
    char name[32];
    size_t n = strlen(opcode);
    if (n >= sizeof(name))
        return -1;
    strcpy(name, opcode);

    *satMode = 0;
    if (n > 5 && strcmp(name + n - 5, "_SSAT") == 0)
    {
        *satMode = 2;
        name[n - 5] = '\0';
    }
    else if (n > 4 && strcmp(name + n - 4, "_SAT") == 0)
    {
        *satMode = 1;
        name[n - 4] = '\0';
    }

    for (int i = 0; i < (int)(sizeof(nvOps) / sizeof(nvOps[0])); i++)
    {
        size_t baseLen = strlen(nvOps[i].nv);
        if (strncmp(name, nvOps[i].nv, baseLen) != 0)
            continue;
        const char *rest = name + baseLen;
        if (*rest == 'R' || *rest == 'H' || *rest == 'X')
            rest++;
        *ccUpdate = (*rest == 'C');
        if (*ccUpdate)
        {
            rest++;
            if (*rest == '0') // only CC0 is emulated
                rest++;
        }
        if (*rest == '\0')
            return i;
    }
    return -1;
}

static int splitOperands(char *s, char ops[][NV_MAX_OPERAND_LEN])
{
    int count = 0, depth = 0;
    char *start = s;
    if (*trim(s) == '\0')
        return 0;
    for (char *p = s;; p++)
    {
        if (*p == '[' || *p == '{' || *p == '(')
            depth++;
        else if (*p == ']' || *p == '}' || *p == ')')
            depth--;
        if (*p == '\0' || (*p == ',' && depth == 0))
        {
            size_t n = (size_t)(p - start);
            if (count == NV_MAX_OPERANDS || n >= NV_MAX_OPERAND_LEN)
                return -1;
            char tmp[NV_MAX_OPERAND_LEN];
            memcpy(tmp, start, n);
            tmp[n] = '\0';
            strcpy(ops[count++], trim(tmp));
            if (*p == '\0')
                break;
            start = p + 1;
        }
    }
    return count;
}

// Condition such as "GT", "NE.x" or "EQ0.xyzw".
static int parseCond(const char *c, char rel[3], char swz[8])
{
    static const char *rels[] = {"EQ", "NE", "LT", "GT", "LE", "GE", "TR", "FL"};
    int found = 0;
    for (int i = 0; i < (int)(sizeof(rels) / sizeof(rels[0])); i++)
        found |= strncmp(c, rels[i], 2) == 0;
    if (!found)
        return 0;
    memcpy(rel, c, 2);
    rel[2] = '\0';
    c += 2;
    if (*c == '0')
        c++;
    swz[0] = '\0';
    if (*c == '.')
    {
        if (!isSwizzle(c + 1))
            return 0;
        snprintf(swz, 8, "%s", c);
    }
    else if (*c != '\0')
    {
        return 0;
    }
    return 1;
}

static int parseDst(const char *text, NvDst *d)
{
    char buf[NV_MAX_OPERAND_LEN];
    snprintf(buf, sizeof(buf), "%s", text);
    d->cond[0] = d->condSwz[0] = d->mask[0] = '\0';

    char *paren = strchr(buf, '(');
    if (paren)
    {
        char *close = strchr(paren, ')');
        if (!close)
            return 0;
        *close = '\0';
        if (!parseCond(trim(paren + 1), d->cond, d->condSwz))
            return 0;
        *paren = '\0';
    }

    char *reg = trim(buf);
    char *dot = strrchr(reg, '.');
    if (dot && dot != reg && isSwizzle(dot + 1))
    {
        snprintf(d->mask, sizeof(d->mask), "%s", dot);
        *dot = '\0';
    }
    snprintf(d->reg, sizeof(d->reg), "%s", reg);
    return 1;
}

static int isOutput(const NvCtx *c, const char *reg)
{
    if (strncmp(reg, "result.", 7) == 0)
        return 1;
    for (int i = 0; i < c->outputCount; i++)
    {
        if (strcmp(reg, c->outputs[i]) == 0)
            return 1;
    }
    return 0;
}

// ARB has no |x| operand modifier: such operands are loaded into a scratch register with ABS first.
static int extractAbs(NvCtx *c, char *op, int *absCount)
{
    char *p = op;
    const char *sign = "";
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '-' || *p == '+')
    {
        sign = *p == '-' ? "-" : "";
        p++;
        while (isspace((unsigned char)*p))
            p++;
    }
    if (*p != '|')
        return 1;

    char *close = strrchr(p + 1, '|');
    if (!close || *absCount >= 3)
        return 0;
    char inner[NV_MAX_OPERAND_LEN], suffix[NV_MAX_OPERAND_LEN], signCopy[2];
    size_t n = (size_t)(close - (p + 1));
    memcpy(inner, p + 1, n);
    inner[n] = '\0';
    snprintf(suffix, sizeof(suffix), "%s", close + 1);
    snprintf(signCopy, sizeof(signCopy), "%s", sign);

    sbPrintf(&c->body, "ABS _nva%d, %s;\n", *absCount, trim(inner));
    snprintf(op, NV_MAX_OPERAND_LEN, "%s_nva%d%s", signCopy, *absCount, trim(suffix));
    (*absCount)++;
    return 1;
}

// ARB vertex programs only address constants with A0.x and offsets in [-64, 63], whereas NV_vertex_program2/3 has a
// 4-component address register and bigger offsets. The components written by ARL are kept in _nvadr and the one
// used by an instruction (plus a bias for big offsets) is loaded into A0.x just before it.
static int rewriteRelative(NvCtx *c, char ops[][NV_MAX_OPERAND_LEN], int first, int count)
{
    if (!c->addrName[0])
        return 1;

    size_t nameLen = strlen(c->addrName);
    char comp = 0;
    int bias = 0, haveRef = 0;
    for (int i = first; i < count; i++)
    {
        for (char *p = strchr(ops[i], '['); p; p = strchr(p + 1, '['))
        {
            char *q = p + 1;
            while (isspace((unsigned char)*q))
                q++;
            if (strncmp(q, c->addrName, nameLen) != 0 || q[nameLen] != '.')
                continue;
            char refComp = q[nameLen + 1];
            if (refComp == '\0' || !strchr("xyzw", refComp))
                return 0;
            q += nameLen + 2;
            while (isspace((unsigned char)*q))
                q++;
            int offset = 0;
            if (*q == '+' || *q == '-')
            {
                int negative = *q == '-';
                offset = (int)strtol(q + 1, &q, 10);
                if (negative)
                    offset = -offset;
                while (isspace((unsigned char)*q))
                    q++;
            }
            if (*q != ']')
                return 0;

            int refBias = (offset >= -64 && offset <= 63) ? 0 : (int)floor(offset / 64.0) * 64;
            if (haveRef && (refComp != comp || refBias != bias))
                return 0;
            comp = refComp;
            bias = refBias;
            haveRef = 1;

            char rebuilt[NV_MAX_OPERAND_LEN];
            int prefix = (int)(p - ops[i]);
            int rest = offset - refBias;
            if (rest == 0)
                snprintf(rebuilt, sizeof(rebuilt), "%.*s[%s.x]%s", prefix, ops[i], c->addrName, q + 1);
            else
                snprintf(rebuilt, sizeof(rebuilt), "%.*s[%s.x %c %d]%s", prefix, ops[i], c->addrName,
                         rest < 0 ? '-' : '+', abs(rest), q + 1);
            strcpy(ops[i], rebuilt);
            p = ops[i] + prefix;
        }
    }

    if (haveRef && (comp != c->adrComp || bias != c->adrBias))
    {
        if (bias)
        {
            sbPrintf(&c->body, "ADD _nvs.x, _nvadr.%c, { %d };\n", comp, bias);
            sbPrintf(&c->body, "ARL %s.x, _nvs.x;\n", c->addrName);
        }
        else
        {
            sbPrintf(&c->body, "ARL %s.x, _nvadr.%c;\n", c->addrName, comp);
        }
        c->adrComp = comp;
        c->adrBias = bias;
    }
    return 1;
}

// Writes the result of the instruction into target. sat is "_SAT" when the saturation is done natively.
static int emitOp(NvCtx *c, int opIndex, const char *sat, const char *target, char src[][NV_MAX_OPERAND_LEN], int n)
{
    StrBuf *b = &c->body;
    NvOpKind kind = nvOps[opIndex].kind;

    switch (kind)
    {
    case OP_DIRECT:
        sbPrintf(b, "%s%s %s", nvOps[opIndex].arb, sat, target);
        for (int i = 0; i < n; i++)
        {
            sbAppend(b, ", ");
            sbAppend(b, src[i]);
        }
        sbAppend(b, ";\n");
        return 1;
    case OP_SWAP:
        if (n != 2)
            return 0;
        sbPrintf(b, "%s%s %s, %s, %s;\n", nvOps[opIndex].arb, sat, target, src[1], src[0]);
        return 1;
    case OP_SEQ:
    case OP_SNE:
        if (n != 2)
            return 0;
        sbPrintf(b, "SLT _nvs, %s, %s;\n", src[0], src[1]);
        sbPrintf(b, "SLT _nvs2, %s, %s;\n", src[1], src[0]);
        if (kind == OP_SNE)
        {
            sbPrintf(b, "ADD%s %s, _nvs, _nvs2;\n", sat, target);
        }
        else
        {
            sbAppend(b, "ADD _nvs, _nvs, _nvs2;\n");
            sbPrintf(b, "ADD%s %s, _nvk.y, -_nvs;\n", sat, target);
        }
        return 1;
    case OP_SFL:
    case OP_STR:
        sbPrintf(b, "MOV%s %s, %s;\n", sat, target, kind == OP_SFL ? "_nvk.x" : "_nvk.y");
        return 1;
    case OP_DIV:
        if (n != 2)
            return 0;
        sbPrintf(b, "RCP _nvs.x, %s;\n", src[1]);
        sbPrintf(b, "MUL%s %s, %s, _nvs.x;\n", sat, target, src[0]);
        return 1;
    case OP_DP2:
        if (n != 2)
            return 0;
        sbPrintf(b, "MUL _nvs.xy, %s, %s;\n", src[0], src[1]);
        sbPrintf(b, "ADD%s %s, _nvs.x, _nvs.y;\n", sat, target);
        return 1;
    case OP_NRM:
        if (n != 1)
            return 0;
        sbPrintf(b, "DP3 _nvs.w, %s, %s;\n", src[0], src[0]);
        sbAppend(b, "RSQ _nvs.w, _nvs.w;\n");
        sbPrintf(b, "MUL%s %s, %s, _nvs.w;\n", sat, target, src[0]);
        return 1;
    case OP_SSG:
        if (n != 1)
            return 0;
        sbPrintf(b, "SLT _nvs, _nvk.x, %s;\n", src[0]);
        sbPrintf(b, "SLT _nvs2, %s, _nvk.x;\n", src[0]);
        sbPrintf(b, "ADD%s %s, _nvs, -_nvs2;\n", sat, target);
        return 1;
    default:
        return 0;
    }
}

// Conditional write: copies _nvr into the destination where the condition holds on the emulated condition code.
static void emitSelect(NvCtx *c, const NvDst *d)
{
    StrBuf *b = &c->body;
    char cc[16];
    snprintf(cc, sizeof(cc), "_nvcc%s", d->condSwz);
    const char *rel = d->cond;

    if (c->isFragment)
    {
        // CMP dst, a, b, c: dst = a < 0 ? b : c
        if (strcmp(rel, "GT") == 0)
            sbPrintf(b, "CMP %s%s, -%s, _nvr, %s;\n", d->reg, d->mask, cc, d->reg);
        else if (strcmp(rel, "LT") == 0)
            sbPrintf(b, "CMP %s%s, %s, _nvr, %s;\n", d->reg, d->mask, cc, d->reg);
        else if (strcmp(rel, "GE") == 0)
            sbPrintf(b, "CMP %s%s, %s, %s, _nvr;\n", d->reg, d->mask, cc, d->reg);
        else if (strcmp(rel, "LE") == 0)
            sbPrintf(b, "CMP %s%s, -%s, %s, _nvr;\n", d->reg, d->mask, cc, d->reg);
        else
        {
            sbPrintf(b, "ABS _nvm, %s;\n", cc);
            if (strcmp(rel, "NE") == 0)
                sbPrintf(b, "CMP %s%s, -_nvm, _nvr, %s;\n", d->reg, d->mask, d->reg);
            else
                sbPrintf(b, "CMP %s%s, -_nvm, %s, _nvr;\n", d->reg, d->mask, d->reg);
        }
        return;
    }

    // No CMP in vertex programs: build a 0/1 mask and blend exactly with m * new + (1 - m) * old.
    if (strcmp(rel, "GT") == 0)
        sbPrintf(b, "SLT _nvm, _nvk.x, %s;\n", cc);
    else if (strcmp(rel, "LT") == 0)
        sbPrintf(b, "SLT _nvm, %s, _nvk.x;\n", cc);
    else if (strcmp(rel, "GE") == 0)
        sbPrintf(b, "SGE _nvm, %s, _nvk.x;\n", cc);
    else if (strcmp(rel, "LE") == 0)
        sbPrintf(b, "SGE _nvm, _nvk.x, %s;\n", cc);
    else
    {
        sbPrintf(b, "ABS _nvm, %s;\n", cc);
        sbPrintf(b, "%s _nvm, _nvk.x, _nvm;\n", strcmp(rel, "NE") == 0 ? "SLT" : "SGE");
    }
    sbPrintf(b, "MUL _nvr%s, _nvr, _nvm;\n", d->mask);
    sbAppend(b, "ADD _nvm, _nvk.y, -_nvm;\n");
    sbPrintf(b, "MAD %s%s, %s, _nvm, _nvr;\n", d->reg, d->mask, d->reg);
}

static int emitKill(NvCtx *c, char ops[][NV_MAX_OPERAND_LEN], int count)
{
    StrBuf *b = &c->body;
    char rel[3], swz[8];
    if (count != 1)
        return 0;

    if (parseCond(ops[0], rel, swz))
    {
        // KIL kills when a component is negative.
        char cc[16];
        snprintf(cc, sizeof(cc), "_nvcc%s", swz);
        if (strcmp(rel, "GT") == 0)
            sbPrintf(b, "KIL -%s;\n", cc);
        else if (strcmp(rel, "LT") == 0)
            sbPrintf(b, "KIL %s;\n", cc);
        else if (strcmp(rel, "NE") == 0)
            sbPrintf(b, "ABS _nvm, %s;\nKIL -_nvm;\n", cc);
        else if (strcmp(rel, "EQ") == 0)
            sbPrintf(b, "ABS _nvm, %s;\nSGE _nvm, _nvk.x, _nvm;\nKIL -_nvm;\n", cc);
        else if (strcmp(rel, "GE") == 0)
            sbPrintf(b, "SGE _nvm, %s, _nvk.x;\nKIL -_nvm;\n", cc);
        else if (strcmp(rel, "LE") == 0)
            sbPrintf(b, "SGE _nvm, _nvk.x, %s;\nKIL -_nvm;\n", cc);
        else if (strcmp(rel, "TR") == 0)
            sbAppend(b, "KIL -_nvk.y;\n");
        return 1;
    }

    int absCount = 0;
    if (!extractAbs(c, ops[0], &absCount))
        return 0;
    sbPrintf(b, "KIL %s;\n", ops[0]);
    return 1;
}

static int emitInstruction(NvCtx *c, const char *opcode, char *operands)
{
    int ccUpdate = 0, satMode = 0;
    int opIndex = findOp(opcode, &ccUpdate, &satMode);
    if (opIndex < 0)
        return 0;

    char ops[NV_MAX_OPERANDS][NV_MAX_OPERAND_LEN];
    int count = splitOperands(operands, ops);
    if (count <= 0)
        return 0;
    if (nvOps[opIndex].kind == OP_KIL)
        return emitKill(c, ops, count);
    if (count < 2)
        return 0;

    NvDst d;
    if (!parseDst(ops[0], &d))
        return 0;

    int absCount = 0;
    if (nvOps[opIndex].kind == OP_ARL)
    {
        if (!c->addrName[0] || d.cond[0] || count != 2 || !extractAbs(c, ops[1], &absCount))
            return 0;
        sbPrintf(&c->body, "MOV _nvadr%s, %s;\n", d.mask, ops[1]);
        c->adrComp = 0;
        return 1;
    }

    if (!rewriteRelative(c, ops, 1, count))
        return 0;
    for (int i = 1; i < count; i++)
    {
        if (!extractAbs(c, ops[i], &absCount))
            return 0;
    }

    if (strcmp(d.cond, "FL") == 0)
        return 1;
    if (strcmp(d.cond, "TR") == 0)
        d.cond[0] = '\0';

    int dstIsOutput = isOutput(c, d.reg);
    int hasCond = d.cond[0] != '\0';
    int nativeSat = c->isFragment && satMode == 1;
    int emulSat = satMode != 0 && !nativeSat;
    if (hasCond && dstIsOutput) // the select reads the previous value
        return 0;
    int needTemp = hasCond || emulSat || (ccUpdate && dstIsOutput);

    char target[NV_MAX_OPERAND_LEN + 8];
    snprintf(target, sizeof(target), "%s%s", needTemp ? "_nvr" : d.reg, d.mask);
    if (!emitOp(c, opIndex, nativeSat ? "_SAT" : "", target, ops + 1, count - 1))
        return 0;

    if (emulSat)
    {
        sbPrintf(&c->body, "MAX _nvr%s, _nvr, %s;\n", d.mask, satMode == 2 ? "_nvk.z" : "_nvk.x");
        sbPrintf(&c->body, "MIN _nvr%s, _nvr, _nvk.y;\n", d.mask);
    }
    if (hasCond)
        emitSelect(c, &d);
    else if (needTemp)
        sbPrintf(&c->body, "MOV %s%s, _nvr;\n", d.reg, d.mask);

    if (ccUpdate)
        sbPrintf(&c->body, "MOV _nvcc%s, %s;\n", d.mask, needTemp && !hasCond ? "_nvr" : d.reg);
    return 1;
}

static int processStatement(NvCtx *c, char *stmt)
{
    char *s = trim(stmt);

    // Labels are only targets of branches, which are not supported: drop them.
    for (;;)
    {
        char *p = s;
        while (isIdentChar(*p))
            p++;
        char *q = p;
        while (isspace((unsigned char)*q))
            q++;
        if (p == s || *q != ':')
            break;
        s = trim(q + 1);
    }
    if (*s == '\0')
        return 1;

    char keyword[32];
    char *p = s;
    while (isIdentChar(*p))
        p++;
    size_t n = (size_t)(p - s);
    if (n == 0 || n >= sizeof(keyword))
        return 0;
    memcpy(keyword, s, n);
    keyword[n] = '\0';
    char *rest = trim(p);

    if (strcmp(keyword, "END") == 0)
    {
        c->ended = 1;
        return 1;
    }
    if (strcmp(keyword, "OPTION") == 0)
    {
        if (strncmp(rest, "NV_", 3) != 0)
            sbPrintf(&c->options, "OPTION %s;\n", rest);
        return 1;
    }
    if (strcmp(keyword, "SHORT") == 0 || strcmp(keyword, "LONG") == 0) // NV_fragment_program2 precision qualifiers
        return processStatement(c, rest);

    if (strcmp(keyword, "OUTPUT") == 0)
    {
        if (c->outputCount == NV_MAX_OUTPUTS)
            return 0;
        char *name = c->outputs[c->outputCount++];
        size_t len = 0;
        while (isIdentChar(rest[len]) && len < sizeof(c->outputs[0]) - 1)
            len++;
        memcpy(name, rest, len);
        name[len] = '\0';
    }
    else if (strcmp(keyword, "ADDRESS") == 0)
    {
        if (c->addrName[0] || strchr(rest, ',')) // a single address register is emulated
            return 0;
        snprintf(c->addrName, sizeof(c->addrName), "%s", rest);
    }

    if (strcmp(keyword, "TEMP") == 0 || strcmp(keyword, "PARAM") == 0 || strcmp(keyword, "ATTRIB") == 0 ||
        strcmp(keyword, "OUTPUT") == 0 || strcmp(keyword, "ADDRESS") == 0 || strcmp(keyword, "ALIAS") == 0)
    {
        sbAppend(&c->body, s);
        sbAppend(&c->body, ";\n");
        return 1;
    }

    return emitInstruction(c, keyword, rest);
}

char *nvProgramToArb(const char *program, int len, int isFragment)
{
    if (!program)
        return NULL;

    size_t size = len >= 0 ? (size_t)len : strlen(program);
    char *src = (char *)malloc(size + 1);
    if (!src)
        return NULL;
    memcpy(src, program, size);
    src[size] = '\0';

    const char *header = isFragment ? "!!ARBfp1.0" : "!!ARBvp1.0";
    char *p = src;
    while (isspace((unsigned char)*p))
        p++;
    if (strncmp(p, header, strlen(header)) != 0 || !strstr(p, "OPTION NV_"))
    {
        free(src);
        return NULL;
    }
    p += strlen(header);

    NvCtx c;
    memset(&c, 0, sizeof(c));
    c.isFragment = isFragment;
    StrBuf stmt = {0};
    char empty[1] = "";
    int ok = 1;

    for (; *p && ok && !c.ended; p++)
    {
        if (*p == '#')
        {
            while (p[1] && p[1] != '\n')
                p++;
            sbAppendN(&stmt, " ", 1);
        }
        else if (*p == ';')
        {
            ok = processStatement(&c, stmt.data ? stmt.data : empty);
            if (ok)
            {
                stmt.len = 0;
                stmt.data[0] = '\0';
            }
        }
        else
        {
            sbAppendN(&stmt, p, 1);
        }
    }
    if (ok && !c.ended) // END has no semicolon
        ok = processStatement(&c, stmt.data ? stmt.data : empty) && c.ended;

    char *result = NULL;
    if (!ok)
    {
        log_warn("nvProgramToArb: cannot translate \"%s\", program left untouched\n", stmt.data ? trim(stmt.data) : "");
    }
    else
    {
        const char *body = c.body.data ? c.body.data : "";
        StrBuf out = {0};
        sbAppend(&out, header);
        sbAppend(&out, "\n");
        if (c.options.data)
            sbAppend(&out, c.options.data);
        if (isFragment && strstr(body, "SHADOW") && !(c.options.data && strstr(c.options.data, "ARB_fragment_program_shadow")))
            sbAppend(&out, "OPTION ARB_fragment_program_shadow;\n");

        int first = 1;
        for (int i = 0; i < (int)(sizeof(nvScratchTemps) / sizeof(nvScratchTemps[0])); i++)
        {
            if (!usesName(body, nvScratchTemps[i]))
                continue;
            sbAppend(&out, first ? "TEMP " : ", ");
            sbAppend(&out, nvScratchTemps[i]);
            first = 0;
        }
        if (!first)
            sbAppend(&out, ";\n");
        if (usesName(body, "_nvk"))
            sbAppend(&out, "PARAM _nvk = { 0, 1, -1, 0 };\n");

        sbAppend(&out, body);
        sbAppend(&out, "END\n");
        result = out.data;
    }

    free(stmt.data);
    free(c.options.data);
    free(c.body.data);
    free(src);
    return result;
}
