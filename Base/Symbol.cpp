// Part of SimCoupe - A SAM Coupe emulator
//
// Symbol.cpp: Symbol management
//
//  Copyright (c) 1999-2014 Simon Owen
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "Symbol.h"

#include "CPU.h"
#include "Memory.h"

namespace Symbol
{
static const int MAX_SYMBOL_OFFSET = 3;

typedef std::map <uint16_t, std::string> AddrToSym;
typedef std::map <std::string, uint16_t> SymToAddr;

static AddrToSym port_symbols;
static AddrToSym ram_symbols, rom_symbols, samdos2_symbols;
static SymToAddr symbol_values, rom_values, samdos2_values;

// pyz80 symbol files use cPickler format
static void ReadcPickler(FILE* file, AddrToSym& symtab, SymToAddr* valtab_ptr)
{
    std::string name;

    char line[128]{};
    while (std::fgets(line, sizeof(line), file) != nullptr)
    {
        if (auto psz = std::strchr(line, '\''))
        {
            auto end_ptr = std::strchr(++psz, '\'');
            if (end_ptr)
            {
                *end_ptr = '\0';
                name = psz;
            }
        }
        // The symbol values are integers, with an I type marker
        else if (auto psz = std::strchr(line, 'I'); psz && !name.empty())
        {
            auto value = static_cast<uint16_t>(strtoul(psz + 1, nullptr, 0));
            symtab[value] = name;

            if (valtab_ptr)
                (*valtab_ptr)[tolower(name)] = value;

            name.clear();
        }
    }
}

// pyz80 map files use a simple addr=name format
static void ReadSimple(FILE* file_, AddrToSym& symtab, SymToAddr* valtab_ptr)
{
    char line[128]{};
    while (std::fgets(line, sizeof(line), file_) != nullptr)
    {
        if (auto psz = std::strtok(line, " ="))
        {
            if (*psz == ';')
                continue;

            auto addr = static_cast<uint16_t>(std::strtoul(psz, nullptr, 16));

            if ((psz = std::strtok(nullptr, " =")) != nullptr)
            {
                psz[strcspn(psz, "\r\n")] = '\0';
                symtab[addr] = psz;

                if (valtab_ptr && *psz)
                    (*valtab_ptr)[tolower(psz)] = addr;
            }
        }
    }
}

static bool Load(const std::string& path, AddrToSym& symtab, SymToAddr* valtab_ptr = nullptr)
{
    symtab.clear();
    if (valtab_ptr)
        valtab_ptr->clear();

    unique_FILE file = fopen(path.c_str(), "r");
    if (!file)
        return false;

    if (fgetc(file) == '(' || fgetc(file) == 'd')
        ReadcPickler(file, symtab, valtab_ptr);
    else if (fseek(file, 0, SEEK_SET) == 0)
        ReadSimple(file, symtab, valtab_ptr);
    else
        return false;

    return true;
}

static bool LoadComet(AddrToSym& symtab, SymToAddr& valtab)
{
    symtab.clear();
    valtab.clear();

    auto mem = std::string(PageReadPtr(0x1c) + 0x1000, PageReadPtr(0x1c) + 0x1200);
    if (mem.find("COMET Z80 assembler") == mem.npos)
        return false;

    auto sym_ptr = PageReadPtr(0x1b) + 0x3fff;
    if (!*sym_ptr)
        return false;

    while (*sym_ptr)
    {
        auto len = *sym_ptr;
        auto name = std::string(sym_ptr - len, sym_ptr);
        std::reverse(name.begin(), name.end());
        sym_ptr -= static_cast<size_t>(1) + len;

        auto flags = sym_ptr[0];
        if (flags == 0xff)
        {
            auto value = (sym_ptr[-2] << 8) | sym_ptr[-1];
            symtab[value] = name;
            valtab[tolower(name)] = value;
        }

        sym_ptr -= 3;
    }

    return true;
}

void Update(const std::string& path)
{
    if (port_symbols.empty())
        Load(OSD::MakeFilePath(PathType::Resource, "samports.map"), port_symbols);
    if (rom_symbols.empty())
        Load(OSD::MakeFilePath(PathType::Resource, "samrom.map"), rom_symbols, &rom_values);
    if (samdos2_symbols.empty())
        Load(OSD::MakeFilePath(PathType::Resource, "samdos2.map"), samdos2_symbols, &samdos2_values);
    if (!path.empty() && !Load(path, ram_symbols, &symbol_values))
        LoadComet(ram_symbols, symbol_values);
}

std::optional<int> LookupSymbol(const std::string& symbol)
{
    if (auto it = symbol_values.find(tolower(symbol)); it != symbol_values.end())
        return static_cast<int>((*it).second);
    if (auto it = rom_values.find(tolower(symbol)); it != rom_values.end())
        return static_cast<int>((*it).second);
    if (auto it = samdos2_values.find(tolower(symbol)); it != samdos2_values.end())
        return static_cast<int>((*it).second);

    return std::nullopt;
}

std::string LookupAddr(uint16_t addr, uint16_t lookup_context, int max_len, bool allow_offset)
{
    bool is_rom_addr = (addr < 0x4000 && AddrPage(addr) == ROM0) || (addr >= 0xc000 && AddrPage(addr) == ROM1);
    bool rom_context = AddrPage(lookup_context) == ROM0 || AddrPage(lookup_context) == ROM1;

    bool is_sysvars_addr = addr >= 0x4000 && addr < 0x5d00 && AddrPage(addr) == 0;

    bool samdos2_paged = (read_byte(0x4096) == 0x20) &&
        std::string_view(reinterpret_cast<const char*>(AddrReadPtr(0x50af)), 6) == "SAMDOS";
    bool is_samdos2_addr = samdos2_paged && addr >= 0x4000 && addr < 0x8000;
    bool samdos2_context = samdos2_paged && lookup_context >= 0x4000 && lookup_context < 0x8000;

    const auto& symtab =
        (is_samdos2_addr && samdos2_context) ? samdos2_symbols :
        (is_rom_addr && rom_context) ? rom_symbols :
        (is_sysvars_addr) ? rom_symbols :
        ram_symbols;

    auto it = symtab.find(addr);
    if (it == symtab.end() && allow_offset)
    {
        for (int i = 1; i <= MAX_SYMBOL_OFFSET; i++)
        {
            it = symtab.find(addr - i);
            if (it != symtab.end())
                break;
        }

        // Allow space for +N offset
        max_len -= 2;
    }

    if (it != symtab.end())
    {
        auto [base_addr, name] = *it;
        auto symbol = name;

        if (max_len > 0)
            symbol = symbol.substr(0, max_len);

        if (base_addr != addr)
        {
            symbol += '+';
            symbol += '0' + (addr - base_addr);
        }

        return symbol;
    }

    return "";
}

std::string LookupPort(uint8_t port, bool input_port)
{
    uint16_t port_entry = input_port ? port : (0x8000 | port);

    auto it = port_symbols.find(port_entry);
    if (it == port_symbols.end() && !input_port)
        it = port_symbols.find(port);

    return (it != port_symbols.end()) ? (*it).second : "";
}

} // namespace Symbol
