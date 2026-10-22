// ELF programs without section headers. A Sentinel HASP envelope (Raw Thrills' Cruis'n Blast) strips them, and the
// loader finds the dynamic table, the symbols and the relocations through sections: they are rebuilt here from the
// program headers, the way ld.so reads them. The envelope also hides most of the program's imports; once the program
// is unwrapped, its records become one more symbol table and REL section for the loader's relocation pass.
#if defined(_WIN32) || defined(__MINGW32__)
#include "elfNoSections.hpp"
#include "../log/log.h"
#include <elfio.hpp>
#include <cstring>
#include <string>
#include <vector>

using namespace ELFIO;

#ifndef PT_GNU_EH_FRAME
#define PT_GNU_EH_FRAME 0x6474e550
#endif
#ifndef DT_GNU_HASH
#define DT_GNU_HASH 0x6ffffef5
#endif

// The file's bytes at a virtual address, from the PT_LOAD segment that holds [va, va + size); nullptr otherwise
static const char *bytesAt(elfio &elf, uint32_t va, uint32_t size)
{
    for (Elf_Half i = 0; i < elf.segments.size(); ++i)
    {
        segment *seg = elf.segments[i];
        if (seg->get_type() != PT_LOAD || !seg->get_data())
            continue;
        uint32_t start = (uint32_t)seg->get_virtual_address();
        uint32_t fileSize = (uint32_t)seg->get_file_size();
        if (va >= start && (uint64_t)(va - start) + size <= fileSize)
            return seg->get_data() + (va - start);
    }
    return nullptr;
}

static section *addSection(elfio &elf, const char *name, Elf_Word type, uint32_t address, const char *data, uint32_t size,
                           Elf_Xword entrySize, Elf_Word link)
{
    // ELFIO writes every new section's name into the section-name table: a file with no section at all gets one first
    if (elf.sections.size() == 0)
        elf.sections.add(".shstrtab")->set_type(SHT_STRTAB);
    section *sec = elf.sections.add(name);
    sec->set_type(type);
    sec->set_flags(SHF_ALLOC);
    sec->set_address(address);
    sec->set_addr_align(4);
    sec->set_entry_size(entrySize);
    sec->set_link(link);
    if (data)
        sec->set_data(data, size);
    else
        sec->set_size(size);
    return sec;
}

// The number of dynamic symbols: DT_HASH's chain count, or past the last chain of DT_GNU_HASH
static uint32_t symbolCount(elfio &elf, uint32_t hash, uint32_t gnuHash)
{
    if (hash)
    {
        const uint32_t *h = (const uint32_t *)bytesAt(elf, hash, 8);
        return h ? h[1] : 0;
    }
    const uint32_t *g = gnuHash ? (const uint32_t *)bytesAt(elf, gnuHash, 16) : nullptr;
    if (!g)
        return 0;
    uint32_t buckets = g[0], symOffset = g[1], bloomWords = g[2];
    uint32_t bucketsVa = gnuHash + 16 + bloomWords * 4;
    const uint32_t *bucket = (const uint32_t *)bytesAt(elf, bucketsVa, buckets * 4);
    if (!bucket)
        return 0;
    uint32_t last = 0;
    for (uint32_t i = 0; i < buckets; i++)
        if (bucket[i] > last)
            last = bucket[i];
    if (last < symOffset)
        return symOffset;
    uint32_t chainsVa = bucketsVa + buckets * 4;
    for (;; last++)
    {
        const uint32_t *chain = (const uint32_t *)bytesAt(elf, chainsVa + (last - symOffset) * 4, 4);
        if (!chain || (*chain & 1))
            return last + 1;
    }
}

void elfAddDynamicSections(elfio &elf)
{
    segment *dynamicSeg = nullptr, *ehFrameHdrSeg = nullptr;
    for (Elf_Half i = 0; i < elf.sections.size(); ++i)
        if (elf.sections[i]->get_type() == SHT_DYNAMIC)
            return;
    for (Elf_Half i = 0; i < elf.segments.size(); ++i)
    {
        if (elf.segments[i]->get_type() == PT_DYNAMIC)
            dynamicSeg = elf.segments[i];
        else if (elf.segments[i]->get_type() == PT_GNU_EH_FRAME)
            ehFrameHdrSeg = elf.segments[i];
    }
    if (!dynamicSeg || !dynamicSeg->get_data())
        return;

    uint32_t strTab = 0, strSize = 0, symTab = 0, symEntry = 16, hash = 0, gnuHash = 0;
    uint32_t rel = 0, relSize = 0, jmpRel = 0, pltRelSize = 0;
    uint32_t dynamicSize = (uint32_t)dynamicSeg->get_file_size();
    const int32_t *dyn = (const int32_t *)dynamicSeg->get_data();
    uint32_t count = 0;
    for (; (count + 1) * 8 <= dynamicSize; count++)
    {
        int32_t tag = dyn[count * 2];
        uint32_t value = (uint32_t)dyn[count * 2 + 1];
        if (tag == DT_NULL)
            break;
        switch (tag)
        {
            case DT_STRTAB: strTab = value; break;
            case DT_STRSZ: strSize = value; break;
            case DT_SYMTAB: symTab = value; break;
            case DT_SYMENT: symEntry = value; break;
            case DT_HASH: hash = value; break;
            case DT_GNU_HASH: gnuHash = value; break;
            case DT_REL: rel = value; break;
            case DT_RELSZ: relSize = value; break;
            case DT_JMPREL: jmpRel = value; break;
            case DT_PLTRELSZ: pltRelSize = value; break;
        }
    }
    const char *strData = bytesAt(elf, strTab, strSize);
    uint32_t symbols = symbolCount(elf, hash, gnuHash);
    const char *symData = bytesAt(elf, symTab, symbols * symEntry);
    if (!strData || !symData)
    {
        log_error("ELF without sections: its dynamic string or symbol table is outside the file");
        return;
    }

    Elf_Half dynstr = addSection(elf, ".dynstr", SHT_STRTAB, strTab, strData, strSize, 0, 0)->get_index();
    Elf_Half dynsym = addSection(elf, ".dynsym", SHT_DYNSYM, symTab, symData, symbols * symEntry, symEntry, dynstr)->get_index();
    addSection(elf, ".dynamic", SHT_DYNAMIC, (uint32_t)dynamicSeg->get_virtual_address(), dynamicSeg->get_data(), (count + 1) * 8, 8,
               dynstr);
    if (const char *relData = rel ? bytesAt(elf, rel, relSize) : nullptr)
        addSection(elf, ".rel.dyn", SHT_REL, rel, relData, relSize, 8, dynsym);
    if (const char *pltData = jmpRel ? bytesAt(elf, jmpRel, pltRelSize) : nullptr)
        addSection(elf, ".rel.plt", SHT_REL, jmpRel, pltData, pltRelSize, 8, dynsym);

    // .eh_frame, for C++ exceptions: eh_frame_hdr's second byte encodes the pointer to it that follows the header
    const uint8_t *hdr = ehFrameHdrSeg ? (const uint8_t *)ehFrameHdrSeg->get_data() : nullptr;
    if (hdr && hdr[0] == 1)
    {
        uint32_t hdrVa = (uint32_t)ehFrameHdrSeg->get_virtual_address();
        uint32_t value;
        memcpy(&value, hdr + 4, 4);
        uint32_t ehFrame = 0;
        if (hdr[1] == 0x1b) // DW_EH_PE_pcrel | DW_EH_PE_sdata4
            ehFrame = hdrVa + 4 + value;
        else if (hdr[1] == 0x03 || hdr[1] == 0x0b) // absolute udata4, sdata4
            ehFrame = value;
        if (ehFrame && bytesAt(elf, ehFrame, 4))
            addSection(elf, ".eh_frame", SHT_PROGBITS, ehFrame, nullptr, 4, 0, 0);
    }
    log_info("ELF without sections: %u dynamic symbols, %u + %u relocations and the dynamic table rebuilt from its program headers",
             symbols, relSize / 8, pltRelSize / 8);
}

// --- the HASP envelope's hidden imports

struct HaspRecord // as the envelope stores them
{
    uint32_t type;    // R_386_GLOB_DAT or R_386_JMP_SLOT
    uint32_t name;    // offset in the name table
    uint32_t version; // offset in the name table, 0 for none
    uint32_t address; // the GOT slot
    uint32_t flags[2]; // 0 or 1
};

static bool isIdentifierChar(char c)
{
    return c == '_' || c == '.' || c == '@' || c == '$' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// A name that starts right after a NUL and is an identifier up to the next NUL
static bool isNameAt(const char *data, size_t size, size_t offset)
{
    if (offset == 0 || offset >= size || data[offset - 1] != 0 || !isIdentifierChar(data[offset]))
        return false;
    for (size_t i = offset; i < size; i++)
    {
        if (data[i] == 0)
            return true;
        if (!isIdentifierChar(data[i]))
            return false;
    }
    return false;
}

// The envelope's own symbols carry obfuscated names made of I, l and 1: nothing outside it provides them
static bool isEnvelopeName(const char *name)
{
    size_t length = strlen(name);
    return length >= 8 && strspn(name, "Il1") == length;
}

void elfAddHaspEnvelopeImports(elfio &elf)
{
    // The envelope's PLT: each stub's relocation index replaced by 0xdeadbeef (push 0xdeadbeef; jmp)
    static const uint8_t stubPush[] = {0x68, 0xef, 0xbe, 0xad, 0xde, 0xe9};
    size_t stubs = 0;
    for (Elf_Half i = 0; i < elf.segments.size(); ++i)
    {
        segment *seg = elf.segments[i];
        if (seg->get_type() != PT_LOAD || !seg->get_data())
            continue;
        const uint8_t *data = (const uint8_t *)seg->get_data();
        size_t size = (size_t)seg->get_file_size();
        for (size_t o = 0; o + sizeof(stubPush) <= size; o++)
            if (data[o] == 0x68 && memcmp(data + o, stubPush, sizeof(stubPush)) == 0)
                stubs++;
    }
    if (stubs < 8)
        return;

    // Its records: the longest run of 24-byte entries that look like GOT relocations
    auto mapped = [&](uint32_t va) { return bytesAt(elf, va, 4) != nullptr; };
    segment *recordSeg = nullptr;
    size_t bestStart = 0, bestCount = 0;
    for (Elf_Half i = 0; i < elf.segments.size(); ++i)
    {
        segment *seg = elf.segments[i];
        if (seg->get_type() != PT_LOAD || !seg->get_data())
            continue;
        const char *data = seg->get_data();
        size_t size = (size_t)seg->get_file_size();
        for (size_t o = 0; o + sizeof(HaspRecord) <= size;)
        {
            size_t count = 0;
            for (; o + (count + 1) * sizeof(HaspRecord) <= size; count++)
            {
                HaspRecord r;
                memcpy(&r, data + o + count * sizeof(HaspRecord), sizeof(r));
                if ((r.type != 6 && r.type != 7) || r.name >= 0x10000 || r.version >= 0x10000 || r.flags[0] > 1 || r.flags[1] > 1 ||
                    !mapped(r.address))
                    break;
            }
            if (count > bestCount)
            {
                bestCount = count;
                bestStart = o;
                recordSeg = seg;
            }
            o += count ? count * sizeof(HaspRecord) : 4;
        }
    }
    if (bestCount < 16)
    {
        log_warn("HASP envelope: %zu PLT stubs without relocation, but no table of its hidden imports", stubs);
        return;
    }
    const char *segData = recordSeg->get_data();
    std::vector<HaspRecord> records(bestCount);
    memcpy(records.data(), segData + bestStart, bestCount * sizeof(HaspRecord));

    // Their name table ends where the records start: its base is where the first names land on identifiers
    size_t base = 0;
    bool found = false;
    for (size_t b = bestStart > 0x10000 ? bestStart - 0x10000 : 0; b < bestStart && !found; b++)
    {
        found = true;
        for (size_t k = 0; k < records.size() && k < 40 && found; k++)
            found = isNameAt(segData + b, bestStart - b, records[k].name);
        if (found)
            base = b;
    }
    if (!found)
    {
        log_warn("HASP envelope: %zu hidden imports, but their name table was not found", records.size());
        return;
    }
    const char *names = segData + base;
    size_t namesSize = bestStart - base;

    std::vector<uint8_t> symbols(16, 0); // symbol 0 stays the null symbol
    std::vector<uint32_t> relocations;
    size_t skipped = 0;
    for (const HaspRecord &r : records)
    {
        if (!isNameAt(names, namesSize, r.name) || isEnvelopeName(names + r.name))
        {
            skipped++;
            continue;
        }
        uint32_t index = (uint32_t)(symbols.size() / 16);
        uint8_t sym[16] = {0};
        memcpy(sym, &r.name, 4);                                    // st_name
        sym[12] = (STB_GLOBAL << 4) | (r.type == 6 ? STT_OBJECT : STT_FUNC); // st_info; value, size, shndx 0: undefined
        symbols.insert(symbols.end(), sym, sym + 16);
        relocations.push_back(r.address);
        relocations.push_back((index << 8) | r.type);
    }
    uint32_t namesVa = (uint32_t)recordSeg->get_virtual_address() + (uint32_t)base;
    Elf_Half str = addSection(elf, ".dynstr.hasp", SHT_STRTAB, namesVa, names, (uint32_t)namesSize, 0, 0)->get_index();
    Elf_Half sym = addSection(elf, ".dynsym.hasp", SHT_DYNSYM, 0, (const char *)symbols.data(), (uint32_t)symbols.size(), 16, str)
                       ->get_index();
    addSection(elf, ".rel.hasp", SHT_REL, 0, (const char *)relocations.data(), (uint32_t)(relocations.size() * 4), 8, sym);
    log_info("HASP envelope: %zu hidden imports restored (%zu of the envelope's own left out), records at 0x%08X", relocations.size() / 2,
             skipped, (uint32_t)recordSeg->get_virtual_address() + (uint32_t)bestStart);
}

uint32_t elfStartCallingMain(elfio &elf, uint32_t entry)
{
    // glibc's i386 _start: xor ebp,ebp; pop esi; mov ecx,esp; and esp,-16; push eax; push esp; push edx;
    // push fini; push init; push ecx; push esi; push main; call __libc_start_main
    static const uint8_t start[] = {0x31, 0xed, 0x5e, 0x89, 0xe1, 0x83, 0xe4, 0xf0, 0x50, 0x54, 0x52, 0x68};
    for (Elf_Half i = 0; i < elf.segments.size(); ++i)
    {
        segment *seg = elf.segments[i];
        if (seg->get_type() != PT_LOAD || !(seg->get_flags() & PF_X) || !seg->get_data())
            continue;
        const uint8_t *data = (const uint8_t *)seg->get_data();
        size_t size = (size_t)seg->get_file_size();
        for (size_t o = 0; o + 28 <= size; o++)
        {
            if (data[o] != 0x31 || memcmp(data + o, start, sizeof(start)) != 0 || data[o + 16] != 0x68 || data[o + 21] != 0x51 ||
                data[o + 22] != 0x56 || data[o + 23] != 0x68)
                continue;
            uint32_t main;
            memcpy(&main, data + o + 24, 4);
            uint32_t startVa = (uint32_t)seg->get_virtual_address() + (uint32_t)o;
            if (main == entry && startVa != entry)
            {
                log_info("Entry point 0x%08X is main: starting at its _start, 0x%08X", entry, startVa);
                return startVa;
            }
        }
    }
    return entry;
}
#endif
