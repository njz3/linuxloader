#pragma once

#include <cstdint>

namespace ELFIO
{
    class elfio;
}

// ELF programs without section headers (Raw Thrills' games, wrapped in a Sentinel HASP envelope): the sections the
// loader reads, rebuilt from the program headers (elfNoSections.cpp)

// .dynstr, .dynsym, .dynamic, .rel.dyn, .rel.plt and .eh_frame from the dynamic segment's tags, when the file has no
// SHT_DYNAMIC section
void elfAddDynamicSections(ELFIO::elfio &elf);

// The imports a HASP envelope hides (Cruis'n Blast): their records turned into a symbol table and a REL section
void elfAddHaspEnvelopeImports(ELFIO::elfio &elf);

// An unwrapped HASP-envelope program has main as its entry point: the _start that calls it, else entry unchanged
uint32_t elfStartCallingMain(ELFIO::elfio &elf, uint32_t entry);
