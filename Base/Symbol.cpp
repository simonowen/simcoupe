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

#include "Memory.h"

namespace Symbol
{

static const int MAX_SYMBOL_OFFSET = 3;

typedef std::map <uint16_t, std::string> AddrToSym;
typedef std::map <std::string, uint16_t> SymToAddr;

static AddrToSym ram_symbols, rom_symbols;
static AddrToSym port_symbols;
static SymToAddr symbol_values;


// Read a cPickler format file, as used by pyz80 for symbols
static void ReadcPickler(FILE* file_, AddrToSym& symtab_, SymToAddr* pValues_)
{
    char szLine[128], * psz;
    std::string sName;

    // Process each line of the file
    while (fgets(szLine, sizeof(szLine), file_) != nullptr)
    {
        // Check for a single quote around the symbol name
        if ((psz = strchr(szLine, '\'')))
        {
            // Skip the opening quote and look for the closing quote
            char* pszEnd = strchr(++psz, '\'');
            if (pszEnd)
            {
                // Remove it and set the symbol name
                *pszEnd = '\0';
                sName = psz;
            }
        }
        // The symbol values are integers, with an I type marker
        else if ((psz = strchr(szLine, 'I')))
        {
            auto wAddr = static_cast<uint16_t>(strtoul(psz + 1, nullptr, 0));

            // If we have a name, set the mapping entries
            if (sName.length())
            {
                symtab_[wAddr] = sName;

                if (pValues_)
                {
                    sName = tolower(sName);
                    (*pValues_)[sName] = wAddr;
                }

                sName.clear();
            }
        }
    }
}

// Read a simple addr=name format text file, as used by pyz80 for map files.
static void ReadSimple(FILE* file_, AddrToSym& symtab_, SymToAddr* pValues_)
{
    char szLine[128], * psz;

    while (fgets(szLine, sizeof(szLine), file_) != nullptr)
    {
        // Find the address token
        psz = strtok(szLine, " =");
        if (psz)
        {
            // Skip comments
            if (*psz == ';')
                continue;

            // Extract address value from the hex string
            auto wAddr = static_cast<uint16_t>(strtoul(psz, nullptr, 16));

            // Find the label name, stripping EOL to preserve empty values
            if ((psz = strtok(nullptr, " =")) != nullptr)
                psz[strcspn(psz, "\r\n")] = '\0';

            // If found, set the mapping entries
            if (psz)
            {
                std::string sName = psz;
                symtab_[wAddr] = sName;

                // Optionally store the reverse mapping for symbol to value look-ups
                if (pValues_ && *psz)
                {
                    sName = tolower(sName);
                    (*pValues_)[sName] = wAddr;
                }
            }
        }
    }
}

// Load a file into a given symbol table
static bool Load(const std::string& path, AddrToSym& symtab_, SymToAddr* pValues_)
{
    // Clear any existing symbols and values
    symtab_.clear();

    unique_FILE file = fopen(path.c_str(), "r");
    if (!file)
        return false;

    // Sniff the start of the file to check for cPickler format
    if (fgetc(file) == '(' || fgetc(file) == 'd')
    {
        ReadcPickler(file, symtab_, pValues_);
    }
    // Fall back a simple ADDR=NAME text file
    else if (fseek(file, 0, SEEK_SET) != 0)
    {
        return false;
    }
    else
    {
        ReadSimple(file, symtab_, pValues_);
    }

    return true;
}

// Update user symbols, loading the ROM and port symbols if not already loaded
void Update(const std::string& path)
{
    symbol_values.clear();

    // Load ROM symbols if not already loaded
    if (rom_symbols.empty())
        Load(OSD::MakeFilePath(PathType::Resource, "samrom.map"), rom_symbols, &symbol_values);

    // Load I/O port symbols if not already loaded
    if (port_symbols.empty())
        Load(OSD::MakeFilePath(PathType::Resource, "samports.map"), port_symbols, nullptr);

    // If a file was supplied, load RAM symbols from it
    if (!path.empty())
        Load(path, ram_symbols, &symbol_values);
}

// Look up the value of a given symbol (user symbols only)
std::optional<int> LookupSymbol(const std::string& symbol)
{
    auto it = symbol_values.find(tolower(symbol));
    if (it != symbol_values.end())
    {
        return static_cast<int>((*it).second);
    }

    return std::nullopt;
}

// Look up an address, with optional maximum length and offset to nearby symbols is no exact match
std::string LookupAddr(uint16_t wAddr_, int nMaxLen_/*=0*/, bool fAllowOffset_/*=false*/)
{
    std::string symbol;

    // Determine if the address is currently paged as ROM, or we're executing in ROM
    bool fROM = AddrPage(wAddr_) == ROM0 || AddrPage(wAddr_) == ROM1;
    bool fInROM = AddrPage(REG_PC) == ROM0 || AddrPage(REG_PC) == ROM1;

    // Select the ROM or user-defined RAM symbol table
    auto& symtab = (fROM || fInROM) ? rom_symbols : ram_symbols;

    // Look up the address
    auto it = symtab.find(wAddr_);

    // If that failed (and we're allowed) look for an offset to a nearby symbol
    if (it == symtab.end() && fAllowOffset_)
    {
        // Search back for a nearby symbol
        for (int i = 1; i <= MAX_SYMBOL_OFFSET; i++)
        {
            it = symtab.find(wAddr_ - i);

            // Stop if we've found one to use as a base
            if (it != symtab.end())
                break;
        }

        // Allow space for +N in output
        nMaxLen_ -= 2;
    }

    // Entry found?
    if (it != symtab.end())
    {
        // Pull out the symbol string
        symbol = (*it).second;

        // Clip the length if required
        if (nMaxLen_ > 0)
            symbol = symbol.substr(0, nMaxLen_);

        // Add the offset to the symbol name if this isn't for the original address
        if ((*it).first != wAddr_)
        {
            // Append the required offset
            symbol += '+';
            symbol += '0' + (wAddr_ - (*it).first);
        }
    }

    // Return the symbol name
    return symbol;
}

// Look up a port symbol for an input or output port (which may have different functions)
std::string LookupPort(uint8_t bPort_, bool fInput_)
{
    std::string symbol;

    // Use as-is for input ports but set bit 15 for output port look-up
    uint16_t wPort = fInput_ ? bPort_ : (0x8000 | bPort_);

    AddrToSym::iterator it = port_symbols.find(wPort);

    // If we couldn't find an output-specific port, try the common read set
    if (it == port_symbols.end() && !fInput_)
        it = port_symbols.find(bPort_);

    if (it != port_symbols.end())
    {
        symbol = (*it).second;
    }

    return symbol;
}

} // namespace Symbol
