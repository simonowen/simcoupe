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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "Symbol.h"

#include "Memory.h"

namespace Symbol
{

static const int MAX_SYMBOL_OFFSET = 3;

typedef std::map <WORD, std::string> AddrToSym;
typedef std::map <std::string, WORD> SymToAddr;

static AddrToSym ram_symbols, rom_symbols;
static AddrToSym port_symbols;
static SymToAddr symbol_values;


// Read a cPickler format file, as used by pyz80 for symbols
static void ReadcPickler (FILE *file_, AddrToSym &symtab_, SymToAddr *pValues_)
{
    char szLine[128], *psz;
    std::string sName;

    // Process each line of the file
    while (fgets(szLine, sizeof(szLine), file_) != nullptr)
    {
        // Check for a single quote around the symbol name
        if ((psz = strchr(szLine, '\'')))
        {
            // Skip the opening quote and look for the closing quote
            char *pszEnd = strchr(++psz, '\'');
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
            WORD wAddr = static_cast<WORD>(strtoul(psz+1, nullptr, 0));

            // If we have a name, set the mapping entries
            if (sName.length())
            {
                symtab_[wAddr] = sName;

                if (pValues_)
                {
                    std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
                    (*pValues_)[sName] = wAddr;
                }

                sName.clear();
            }
        }
    }
}

// Read a simple addr=name format text file, as used by pyz80 for map files.
static void ReadSimple (FILE *file_, AddrToSym &symtab_, SymToAddr *pValues_)
{
    char szLine[128], *psz;

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
            WORD wAddr = static_cast<WORD>(strtoul(psz, nullptr, 16));

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
                    std::transform(sName.begin(), sName.end(), sName.begin(), ::tolower);
                    (*pValues_)[sName] = wAddr;
                }
            }
        }
    }
}

// Load a file into a given symbol table
static bool Load (const char *pcszFile_, AddrToSym &symtab_, SymToAddr *pValues_)
{
    // Clear any existing symbols and values
    symtab_.clear();
    if (pValues_) pValues_->clear();

    FILE *file = fopen(pcszFile_, "r");
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
        fclose(file);
        return false;
    }
    else
    {
        ReadSimple(file, symtab_, pValues_);
    }

    fclose(file);
    return true;
}

// Update user symbols, loading the ROM and port symbols if not already loaded
void Update (const char *pcszFile_)
{
    // Load ROM symbols if not already loaded
    if (rom_symbols.empty())
        Load(OSD::MakeFilePath(MFP_RESOURCE, "samrom.map"), rom_symbols, nullptr);

    // Load I/O port symbols if not already loaded
    if (port_symbols.empty())
        Load(OSD::MakeFilePath(MFP_RESOURCE, "samports.map"), port_symbols, nullptr);

    // If a file was supplied, load RAM symbols from it
    if (pcszFile_)
        Load(pcszFile_, ram_symbols, &symbol_values);
}

// Look up the value of a given symbol (user symbols only)
int LookupSymbol (std::string sSymbol_)
{
    // Convert to lower-case for case-insensitive look-up
    std::transform(sSymbol_.begin(), sSymbol_.end(), sSymbol_.begin(), ::tolower);

    SymToAddr::iterator it = symbol_values.find(sSymbol_);
    if (it != symbol_values.end())
    {
        // Return the symbol value
        return static_cast<int>((*it).second);
    }

    // Symbol not found
    return -1;
}

// Look up an address, with optional maximum length and offset to nearby symbols is no exact match
std::string LookupAddr (WORD wAddr_, int nMaxLen_/*=0*/, bool fAllowOffset_/*=false*/)
{
    std::string symbol;

    // Determine if the address is currently paged as ROM, or we're executing in ROM
    bool fROM = AddrPage(wAddr_) == ROM0 || AddrPage(wAddr_) == ROM1;
    bool fInROM = AddrPage(PC) == ROM0 || AddrPage(PC) == ROM1;

    // Select the ROM or user-defined RAM symbol table
    AddrToSym &symtab = (fROM || fInROM) ? rom_symbols : ram_symbols;

    // Look up the address
    AddrToSym::iterator it = symtab.find(wAddr_);

    // If that failed (and we're allowed) look for an offset to a nearby symbol
    if (it == symtab.end() && fAllowOffset_)
    {
        // Search back for a nearby symbol
        for (int i = 1 ; i <= MAX_SYMBOL_OFFSET ; i++)
        {
            it = symtab.find(wAddr_-i);

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
std::string LookupPort (BYTE bPort_, bool fInput_)
{
    std::string symbol;

    // Use as-is for input ports but set bit 15 for output port look-up
    WORD wPort = fInput_ ? bPort_ : (0x8000 | bPort_);

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
