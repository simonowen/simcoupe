// Part of SimCoupe - A SAM Coupe emulator
//
// GUIDlg.cpp: Dialog boxes using the GUI controls
//
//  Copyright (c) 1999-2015 Simon Owen
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
#include "GUIDlg.h"

#include "AtaAdapter.h"
#include "Disk.h"
#include "Frame.h"
#include "HardDisk.h"
#include "Input.h"
#include "Memory.h"
#include "MIDI.h"
#include "Options.h"
#include "Sound.h"
#include "Tape.h"
#include "Video.h"


OPTIONS g_opts;

// Helper macro for detecting options changes
#define Changed(o)         (g_opts.o != GetOption(o))
#define ChangedString(o)   (strcasecmp(g_opts.o, GetOption(o)))

////////////////////////////////////////////////////////////////////////////////

AboutDialog::AboutDialog(Window* pParent_/*=nullptr*/)
    : Dialog(pParent_, 305, 220, "About SimCoupe")
{
    char szVersion[128] = "SimCoupe v1.1 alpha";

#if 0
    // Append the date on beta versions, to save us updating the version number each time
    sprintf(szVersion + strlen(szVersion), " beta ("  __DATE__ ")");
#endif

    new IconControl(this, 6, 6, &sSamIcon);
    new TextControl(this, 86, 10, szVersion, BLACK);
    new TextControl(this, 86, 24, "http://simcoupe.org", GREY_3);

    int y = 46;

    new TextControl(this, 41, y, "Win32/SDL/Allegro/Pocket PC versions:", BLUE_5);
    new TextControl(this, 51, y + 13, "Simon Owen <simon.owen@simcoupe.org>", BLACK); y += 32;

#if defined (__AMIGAOS4__)
    new TextControl(this, 41, y, "AmigaOS 4 version:", BLUE_5);
    new TextControl(this, 51, y + 13, "Ventzislav Tzvetkov <drHirudo@Amigascne.org>", BLACK); y += 32;
    m_nHeight += 32;
#endif

    new TextControl(this, 41, y, "Based on original DOS/X versions by:", BLUE_5);
    new TextControl(this, 51, y + 13, "Allan Skillman <allan.skillman@arm.com>", BLACK); y += 32;

    new TextControl(this, 41, y, "CPU contention and sound enhancements:", BLUE_5);
    new TextControl(this, 51, y + 13, "Dave Laundon <dave.laundon@simcoupe.org>", BLACK); y += 32;

    new TextControl(this, 41, y, "Phillips SAA 1099 sound chip emulation:", BLUE_5);
    new TextControl(this, 51, y + 13, "Dave Hooper <dave@rebuzz.org>", BLACK); y += 32;

    new TextControl(this, 41, y + 3, "See README for additional information", RED_3);

    m_pCloseButton = new TextButton(this, (m_nWidth - 55) / 2, m_nHeight - 21, "Close", 55);
}

void AboutDialog::OnNotify(Window* pWindow_, int /*nParam_*/)
{
    if (pWindow_ == m_pCloseButton)
        Destroy();
}

void AboutDialog::EraseBackground(FrameBuffer& fb)
{
    fb.FillRect(m_nX, m_nY, m_nWidth, m_nHeight, WHITE);
}

////////////////////////////////////////////////////////////////////////////////

// Persist show-hidden option between uses, shared by all file selectors
bool FileDialog::s_fShowHidden = false;

FileDialog::FileDialog(const std::string& caption, const std::string& path_str, const FILEFILTER* pcFileFilter_, int* pnFilter_,
    Window* pParent_/*=nullptr*/) : Dialog(pParent_, 527, 339 + 22, caption), m_pcFileFilter(pcFileFilter_), m_pnFilter(pnFilter_)
{
    // Create all the controls for the dialog (the objects are deleted by the GUI when closed)
    m_pFileView = new FileView(this, 2, 2, (7 * 72) + 19, (4 * 72));

    new FrameControl(this, 0, (4 * 72) + 3, m_nWidth, 1, GREY_6);

    new TextControl(this, 3, m_nHeight - 61, "File:", YELLOW_8);
#if 0
    m_pFile = new EditControl(this, 36, m_nHeight - 64, 204, "");
#else
    m_pFile = new TextControl(this, 36, m_nHeight - 61, "");
#endif

    new TextControl(this, 3, m_nHeight - 40, "Path:", YELLOW_8);
    m_pPath = new TextControl(this, 36, m_nHeight - 40, "");

    new TextControl(this, 3, m_nHeight - 19, "Filter:", YELLOW_8);
    m_pFilter = new ComboBox(this, 36, m_nHeight - 22, m_pcFileFilter->pcszDesc, 204);
    if (m_pnFilter) m_pFilter->Select(*m_pnFilter);

    m_pShowHidden = new CheckBox(this, 252, m_nHeight - 19, "Show hidden files");
    m_pShowHidden->SetChecked(s_fShowHidden);

    m_pRefresh = new TextButton(this, m_nWidth - 160, m_nHeight - 21, "Refresh", 56);
    m_pOK = new TextButton(this, m_nWidth - 99, m_nHeight - 21, "OK", 46);
    m_pCancel = new TextButton(this, m_nWidth - 50, m_nHeight - 21, "Cancel", 46);

    // Set the filter and path
    OnNotify(m_pFilter, 0);
    SetPath(path_str);
}

// Set browse path
void FileDialog::SetPath(const std::string& path)
{
    m_pFileView->SetPath(path);
    m_pPath->SetText(m_pFileView->GetPath());
}

// Handle control notifications
void FileDialog::OnNotify(Window* pWindow_, int nParam_)
{
    if (pWindow_ == m_pOK)
        m_pFileView->NotifyParent(1);
    else if (pWindow_ == m_pCancel)
        Destroy();
    else if (pWindow_ == m_pRefresh)
        m_pFileView->Refresh();
    else if (pWindow_ == m_pShowHidden)
    {
        s_fShowHidden = m_pShowHidden->IsChecked();
        m_pFileView->ShowHidden(s_fShowHidden);
    }
    else if (pWindow_ == m_pFilter)
    {
        int nSelected = m_pFilter->GetSelected();
        m_pFileView->SetFilter(m_pcFileFilter->pcszExts[nSelected]);
        if (m_pnFilter) *m_pnFilter = nSelected;
    }
    else if (pWindow_ == m_pFile)
    {
        if (nParam_)
        {
            OnOK();
        }
        else if (auto index = m_pFileView->FindItem(m_pFile->GetText()))
        {
            m_pFileView->Select(*index);
        }
    }
    else if (pWindow_ == m_pFileView)
    {
        const ListViewItem* pItem = m_pFileView->GetItem();

        if (pItem)
        {
            if (std::addressof(pItem->m_pIcon.get()) == std::addressof(sFolderIcon))
            {
                m_pPath->SetText(m_pFileView->GetPath());
                m_pFile->SetText("");
            }
            else
            {
                m_pFile->SetText(pItem->m_label);

                if (nParam_)
                    OnOK();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static int nFloppyFilter = 0;
static const FILEFILTER sFloppyFilter =
{
#ifdef HAVE_LIBZ
    "All Disks (dsk;sad;mgt;sbt;gz;zip)|"
#endif
    "Disk Images (dsk;sad;mgt;sbt)|"
#ifdef HAVE_LIBZ
    "Compressed Files (gz;zip)|"
#endif
    "All Files",

    {
#ifdef HAVE_LIBZ
        ".dsk;.sad;.mgt;.sbt;.cpm;.gz;.zip",
#endif
        ".dsk;.sad;.mgt;.sbt;.cpm",
#ifdef HAVE_LIBZ
        ".gz;.zip",
#endif
        ""
    }
};

BrowseFloppy::BrowseFloppy(int drive, Window* pParent_/*=nullptr*/)
    : FileDialog("", "", &sFloppyFilter, &nFloppyFilter, pParent_), m_nDrive(drive)
{
    // Set the dialog caption to show which drive we're dealing with
    SetText(fmt::format("Insert Floppy {}", drive));

    // Browse from the location of the previous image, or the default directory if none
    auto pcszImage = ((drive == 1) ? pFloppy1 : pFloppy2)->DiskPath();
    SetPath(*pcszImage ? pcszImage : OSD::MakeFilePath(MFP_INPUT));
}

// Handle OK being clicked when a file is selected
void BrowseFloppy::OnOK()
{
    if (auto full_path = m_pFileView->GetFullPath(); !full_path.empty())
    {
        bool fInserted = false;

        // Insert the disk into the appropriate drive
        fInserted = ((m_nDrive == 1) ? pFloppy1 : pFloppy2)->Insert(full_path.c_str(), true);

        // If we succeeded, show a status message and close the file selector
        if (fInserted)
        {
            // Update the status text and close the dialog
            Frame::SetStatus("{}  inserted into drive {}", m_pFileView->GetItem()->m_label, m_nDrive);
            Destroy();
            return;
        }
    }

    // Report any error
    auto err = fmt::format("Invalid disk image:\n\n{}", m_pFileView->GetItem()->m_label);
    new MsgBox(this, err, "Open Failed", mbWarning);
}

////////////////////////////////////////////////////////////////////////////////

static int nTapeFilter = 0;
static const FILEFILTER sTapeFilter =
{
#ifdef HAVE_LIBZ
    "All Tapes (tap;tzx;csw;gz;zip)|"
#endif
    "Tape Images (tap;tzx;csw)|"
#ifdef HAVE_LIBZ
    "Compressed Files (gz;zip)|"
#endif
    "All Files",

    {
#ifdef HAVE_LIBZ
        ".tap;.tzx;.csw;.gz;.zip",
#endif
        ".tap;.tzx;.csw",
#ifdef HAVE_LIBZ
        ".gz;.zip",
#endif
        ""
    }
};

BrowseTape::BrowseTape(Window* pParent_/*=nullptr*/)
    : FileDialog("Insert Tape", "", &sTapeFilter, &nTapeFilter, pParent_)
{
    // Browse from the location of the previous image, or the default directory if none
    const char* pcszImage = Tape::GetPath();
    SetPath(*pcszImage ? pcszImage : OSD::MakeFilePath(MFP_INPUT));
}

// Handle OK being clicked when a file is selected
void BrowseTape::OnOK()
{
    auto full_path = m_pFileView->GetFullPath();
    if (!full_path.empty())
    {
        bool fInserted = Tape::Insert(full_path.c_str());

        // If we succeeded, show a status message and close the file selector
        if (fInserted)
        {
            // Update the status text and close the dialog
            Frame::SetStatus("{}  inserted", m_pFileView->GetItem()->m_label);
            Destroy();
            return;
        }
    }

    // Report any error
    auto err_msg = fmt::format("Invalid tape image:\n\n{}", m_pFileView->GetItem()->m_label);
    new MsgBox(this, err_msg.c_str(), "Open Failed", mbWarning);
}

////////////////////////////////////////////////////////////////////////////////

FileBrowser::FileBrowser(EditControl* pEdit_, Window* pParent_, const std::string& caption, const FILEFILTER* pcsFilter_, int* pnFilter_)
    : FileDialog(caption, "", pcsFilter_, pnFilter_, pParent_), m_pEdit(pEdit_)
{
    // Browse from the location of the previous image, or the default directory if none
    SetPath(!pEdit_->GetText().empty() ? pEdit_->GetText() : OSD::MakeFilePath(MFP_INPUT));
}

// Handle OK being clicked when a file is selected
void FileBrowser::OnOK()
{
    auto full_path = m_pFileView->GetFullPath();
    if (!full_path.empty())
    {
        // Set the edit control text, activate it, and notify the parent of the change
        m_pEdit->SetText(full_path);
        m_pEdit->Activate();
        m_pParent->OnNotify(m_pEdit, 0);
        Destroy();
    }
}

////////////////////////////////////////////////////////////////////////////////

HDDProperties::HDDProperties(EditControl* pEdit_, Window* pParent_, const char* pcszCaption_)
    : Dialog(pParent_, 268, 56, pcszCaption_), m_pEdit(pEdit_)
{
    new TextControl(this, 12, 13, "File:");
    m_pFile = new EditControl(this, 35, 10, 199, pEdit_->GetText());
    m_pBrowse = new TextButton(this, 239, 10, "...", 17);

    new TextControl(this, 12, 37, "Size (MB):");
    m_pSize = new NumberEditControl(this, 68, 34, 30);

    m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
    m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

    // Set a default size of 32MB, but refresh from the current image (if any)
    m_pSize->SetText("32");
    OnNotify(m_pFile, 0);
}

void HDDProperties::OnNotify(Window* pWindow_, int /*nParam_*/)
{
    static int nHardDiskFilter = 0;
    static const FILEFILTER sHardDiskFilter =
    {
        "Hard disk images (*.hdf)|"
        "All Files",

        { ".hdf", "" }
    };

    if (pWindow_ == m_pCancel)
        Destroy();
    else if (pWindow_ == m_pBrowse)
        new FileBrowser(m_pFile, this, "Browse for HDF", &sHardDiskFilter, &nHardDiskFilter);
    else if (pWindow_ == m_pFile)
    {
        // If we can, open the existing hard disk image to retrieve the geometry
        auto disk = HardDisk::OpenObject(m_pFile->GetText().c_str());

        if (disk)
        {
            // Fetch the existing disk geometry
            const ATA_GEOMETRY* pGeom = disk->GetGeometry();

            // Show the current size in decimal
            char szSize[16] = {};
            snprintf(szSize, sizeof(szSize) - 1, "%u", (pGeom->uTotalSectors + (1 << 11) - 1) >> 11);
            m_pSize->SetText(szSize);
        }

        // The geometry is read-only for existing images
        m_pSize->Enable(!disk);

        // Set the text and state of the OK button, depending on the target file
        m_pOK->SetText(disk ? "OK" : "Create");
        m_pOK->Enable(!m_pFile->GetText().empty());
    }
    else if (pWindow_ == m_pOK)
    {
        // If the size control is enabled, we know the image doesn't already exist
        if (m_pSize->IsEnabled())
        {
            char sz[MAX_PATH];
            strncpy(sz, m_pFile->GetText().c_str(), MAX_PATH - 1);
            sz[MAX_PATH - 1] = '\0';

            size_t nLen = strlen(sz);

            // Append a .hdf extension if it doesn't already have one
            if (nLen > 4 && strcasecmp(sz + nLen - 4, ".hdf"))
            {
                strcat(sz, ".hdf");
                m_pFile->SetText(sz);
            }

            // Determine the total sector count from the size
            auto uTotalSectors = static_cast<unsigned int>(std::stoul(m_pSize->GetText(), nullptr, 10) << 11);

            // Check the geometry is within range
            if (!uTotalSectors || (uTotalSectors > (16383 * 16 * 63)))
            {
                new MsgBox(this, "Invalid disk size", "Warning", mbWarning);
                return;
            }

            // Create a new disk of the required size
            if (!HDFHardDisk::Create(m_pFile->GetText().c_str(), uTotalSectors))
            {
                new MsgBox(this, "Failed to create new disk (disk full?)", "Warning", mbWarning);
                return;
            }
        }

        m_pEdit->SetText(m_pFile->GetText());
        m_pEdit->Activate();
        m_pParent->OnNotify(m_pEdit, 0);
        Destroy();
    }
}

////////////////////////////////////////////////////////////////////////////////

OptionsDialog::OptionsDialog(Window* pParent_/*=nullptr*/)
    : Dialog(pParent_, 364, 171, "Options")
{
    Move(m_nX, m_nY - 40);

    m_pOptions = new OptionView(this, 2, 2, 360, 144);
    new FrameControl(this, 0, m_nHeight - 23, m_nWidth, 1);
    m_pStatus = new TextControl(this, 4, m_nHeight - 15, "", GREY_7);
    m_pClose = new TextButton(this, m_nWidth - 57, m_nHeight - 19, "Close", 55);

    std::vector<ListViewItem> items{
        { sChipIcon, "System" },
        { sDisplayIcon, "Display" },
        { sSoundIcon, "Sound" },
        { sMidiIcon, "MIDI" },
        { sKeyboardIcon, "Input" },
        { sHardDiskIcon, "Drives" },
        { sFloppyDriveIcon, "Disks" },
        { sPortIcon, "Parallel" },
        { sHardwareIcon, "Misc" },
        { sSamIcon, "About" } };

    m_pOptions->SetItems(std::move(items));

    // Set the initial status text
    OnNotify(m_pOptions, 0);
}


class SystemOptions final : public Dialog
{
public:
    SystemOptions(Window* pParent_)
        : Dialog(pParent_, 300, 220, "System Settings")
    {
        new IconControl(this, 10, 10, &sChipIcon);

        new FrameControl(this, 50, 17, 238, 42);
        new TextControl(this, 60, 13, "RAM", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 35, "Internal:");
        m_pMain = new ComboBox(this, 103, 32, "256K|512K", 50);
        new TextControl(this, 167, 35, "External:");
        m_pExternal = new ComboBox(this, 217, 32, "None|1MB|2MB|3MB|4MB", 60);

        new FrameControl(this, 50, 77, 238, 80);
        new TextControl(this, 60, 74, "ROM", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 95, "Custom ROM image (32K):");
        m_pROM = new EditControl(this, 63, 108, 196);
        m_pBrowse = new TextButton(this, 262, 108, "...", 17);

        m_pAtomBootRom = new CheckBox(this, 63, 137, "Use Atom boot ROM when Atom is active.");

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pMain->Select((GetOption(mainmem) >> 8) - 1);
        m_pExternal->Select(GetOption(externalmem));
        m_pROM->SetText(GetOption(rom));
        m_pAtomBootRom->SetChecked(GetOption(atombootrom));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pROM, 0);
    }
    SystemOptions(const SystemOptions&) = delete;
    void operator= (const SystemOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        static int nROMFilter = 0;
        static const FILEFILTER sROMFilter =
        {
            "ROM Images (.rom;.bin)|"
            "All Files",

            { ".rom;.bin", "" }
        };

        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pBrowse)
            new FileBrowser(m_pROM, this, "Browse for ROM", &sROMFilter, &nROMFilter);
        else if (pWindow_ == m_pROM)
            m_pAtomBootRom->Enable(m_pROM->GetText().empty());
        else if (pWindow_ == m_pOK)
        {
            SetOption(mainmem, (m_pMain->GetSelected() + 1) << 8);
            SetOption(externalmem, m_pExternal->GetSelected());
            SetOption(rom, m_pROM->GetText().c_str());
            SetOption(atombootrom, m_pAtomBootRom->IsChecked());

            // If Atom boot ROM is enabled and a drive type has changed, trigger a ROM refresh
            if (GetOption(atombootrom) && (Changed(drive1) || Changed(drive2)))
                Memory::UpdateRom();

            Destroy();
        }
    }

protected:
    CheckBox* m_pAtomBootRom = nullptr;
    ComboBox* m_pMain = nullptr;
    ComboBox* m_pExternal = nullptr;
    EditControl* m_pROM = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
    TextButton* m_pBrowse = nullptr;
};


class DisplayOptions final : public Dialog
{
public:
    DisplayOptions(Window* pParent_)
        : Dialog(pParent_, 300, 185, "Display Settings")
    {
        new IconControl(this, 10, 10, &sDisplayIcon);

        new FrameControl(this, 50, 17, 238, 120, WHITE);
        new TextControl(this, 60, 13, "Settings", YELLOW_8, BLUE_2);

        m_pFullScreen = new CheckBox(this, 60, 35, "Full-screen");

        m_pScaleText = new TextControl(this, 85, 57, "Windowed mode zoom:");
        m_pScale = new ComboBox(this, 215, 54, "50%|100%|150%|200%|250%|300%", 55);

        new FrameControl(this, 63, 77, 212, 1, GREY_6);

        m_pRatio54 = new CheckBox(this, 60, 90, "5:4 pixel shape");

        new TextControl(this, 60, 113, "Viewable area:");
        m_pViewArea = new ComboBox(this, 140, 110, "No borders|Small borders|Short TV area (default)|TV visible area|Complete scan area", 140);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pScale->Select(GetOption(scale) - 1);

        m_pFullScreen->SetChecked(GetOption(fullscreen));
        m_pRatio54->SetChecked(GetOption(ratio5_4));

        m_pViewArea->Select(GetOption(borders));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pScale, 0);
    }
    DisplayOptions(const DisplayOptions&) = delete;
    void operator= (const DisplayOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(fullscreen, m_pFullScreen->IsChecked());

            SetOption(scale, m_pScale->GetSelected() + 1);

            SetOption(ratio5_4, m_pRatio54->IsChecked());

            SetOption(borders, m_pViewArea->GetSelected());

            if (Changed(borders) || Changed(fullscreen) || Changed(ratio5_4) || Changed(scale))
            {
                Frame::Init();
                Video::UpdateSize();

                // Re-centre the window, including the parent if that's a dialog
                if (GetParent()->GetType() == ctDialog)
                    reinterpret_cast<Dialog*>(GetParent())->Centre();
                Centre();
            }

            Destroy();
        }
        else
        {
            bool fFullScreen = m_pFullScreen->IsChecked();
            m_pScaleText->Enable(!fFullScreen);
            m_pScale->Enable(!fFullScreen);

            if (!Video::CheckCaps(VCAP_STRETCH))
            {
                m_pScaleText->Enable(false);
                m_pScale->Enable(false);
                m_pRatio54->Enable(false);
            }
        }
    }

protected:
    CheckBox* m_pFullScreen = nullptr;
    CheckBox* m_pRatio54 = nullptr;
    ComboBox* m_pScale = nullptr;
    ComboBox* m_pViewArea = nullptr;
    TextControl* m_pScaleText = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class SoundOptions final : public Dialog
{
public:
    SoundOptions(Window* pParent_)
        : Dialog(pParent_, 300, 193, "Sound Settings")
    {
        new IconControl(this, 10, 10, &sSoundIcon);

        new FrameControl(this, 50, 17, 238, 60, WHITE);
        new TextControl(this, 60, 13, "SID Interface", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 33, "Select the SID chip type installed:");
        m_pSID = new ComboBox(this, 63, 51, "None|MOS6581 (Default)|MOS8580", 125);


        new FrameControl(this, 50, 89, 238, 75, WHITE);
        new TextControl(this, 60, 85, "DAC on Port 7C", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 104, "These devices use the same I/O port, so only\none may be connected at a time.");
        m_pDAC7C = new ComboBox(this, 63, 136, "None|Blue Alpha Sampler (8-bit mono)|SAMVox (4 channel 8-bit mono)|Paula (2 channel 4-bit stereo)", 190);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        m_pSID->Select(GetOption(sid));
        m_pDAC7C->Select(GetOption(dac7c));
    }
    SoundOptions(const SoundOptions&) = delete;
    void operator= (const SoundOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(sid, m_pSID->GetSelected());
            SetOption(dac7c, m_pDAC7C->GetSelected());

            Destroy();
        }
    }

protected:
    ComboBox* m_pSID = nullptr;
    ComboBox* m_pDAC7C = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class MidiOptions final : public Dialog
{
public:
    MidiOptions(Window* pParent_)
        : Dialog(pParent_, 300, 171, "Midi Settings")
    {
        new IconControl(this, 10, 15, &sMidiIcon);
        new FrameControl(this, 50, 17, 238, 40);
        new TextControl(this, 60, 13, "Active Device", YELLOW_8, BLUE_2);
        new TextControl(this, 63, 33, "Device on MIDI port:");
        m_pMidi = new ComboBox(this, 170, 30, "None|Midi device", 90);

        new FrameControl(this, 50, 72, 238, 68);
        new TextControl(this, 60, 68, "Devices", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 88, "MIDI Out:");
        m_pMidiOut = new ComboBox(this, 115, 85, "/dev/midi", 160);

        new TextControl(this, 63, 115, "MIDI In:");
        m_pMidiIn = new ComboBox(this, 115, 113, "/dev/midi", 160);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pMidi->Select(GetOption(midi));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pMidi, 0);
    }
    MidiOptions(const MidiOptions&) = delete;
    void operator= (const MidiOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(midi, m_pMidi->GetSelected());
            SetOption(midioutdev, m_pMidiOut->GetSelectedText().c_str());
            SetOption(midiindev, m_pMidiIn->GetSelectedText().c_str());

            if (Changed(midi) || Changed(midiindev) || Changed(midioutdev))
                pMidi->SetDevice(GetOption(midioutdev));

            Destroy();
        }
        else
        {
            int nType = m_pMidi->GetSelected();
            m_pMidiOut->Enable(nType == 1);
            m_pMidiIn->Enable(nType == 1);
        }
    }

protected:
    ComboBox* m_pMidi = nullptr;
    ComboBox* m_pMidiOut = nullptr;
    ComboBox* m_pMidiIn = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class InputOptions final : public Dialog
{
public:
    InputOptions(Window* pParent_)
        : Dialog(pParent_, 300, 190, "Input Settings")
    {
        new IconControl(this, 10, 10, &sKeyboardIcon);
        new FrameControl(this, 50, 17, 238, 89);
        new TextControl(this, 60, 13, "Keyboard", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 35, "Mapping mode:");
        m_pKeyMapping = new ComboBox(this, 145, 32, "None (raw)|Auto-select|SAM Coupe|ZX Spectrum", 115);

        m_pAltForCntrl = new CheckBox(this, 63, 63, "Use Left-Alt for SAM Cntrl key");
#ifdef __APPLE__
        m_pAltGrForEdit = new CheckBox(this, 63, 85, "Use Right-Alt for SAM Edit");
#else
        m_pAltGrForEdit = new CheckBox(this, 63, 85, "Use Alt-Gr key for SAM Edit");
#endif
        new IconControl(this, 10, 121, &sMouseIcon);
        new FrameControl(this, 50, 123, 238, 37);
        new TextControl(this, 60, 119, "Mouse", YELLOW_8, BLUE_2);

        m_pMouse = new CheckBox(this, 63, 136, "Enable SAM mouse interface");

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pKeyMapping->Select(GetOption(keymapping));
        m_pAltForCntrl->SetChecked(GetOption(altforcntrl));
        m_pAltGrForEdit->SetChecked(GetOption(altgrforedit));
        m_pMouse->SetChecked(GetOption(mouse));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pMouse, 0);
    }
    InputOptions(const InputOptions&) = delete;
    void operator= (const InputOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(keymapping, m_pKeyMapping->GetSelected());
            SetOption(altforcntrl, m_pAltForCntrl->IsChecked());
            SetOption(altgrforedit, m_pAltGrForEdit->IsChecked());
            SetOption(mouse, m_pMouse->IsChecked());

            Destroy();
        }
    }

protected:
    ComboBox* m_pKeyMapping = nullptr;
    CheckBox* m_pAltForCntrl = nullptr;
    CheckBox* m_pAltGrForEdit = nullptr;
    CheckBox* m_pMouse = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class DriveOptions final : public Dialog
{
public:
    DriveOptions(Window* pParent_)
        : Dialog(pParent_, 300, 221, "Drive Settings")
    {
        new IconControl(this, 10, 10, &sHardDiskIcon);

        new FrameControl(this, 50, 16, 238, 42);
        new TextControl(this, 60, 12, "Drives", YELLOW_8, BLUE_2);

        new TextControl(this, 63, 32, "D1:");
        m_pDrive1 = new ComboBox(this, 83, 29, "None|Floppy", 60);

        new TextControl(this, 158, 32, "D2:");
        m_pDrive2 = new ComboBox(this, 178, 29, "None|Floppy|Atom (Legacy)|Atom Lite", 100);

        new FrameControl(this, 50, 71, 238, 120);
        new TextControl(this, 60, 67, "Options", YELLOW_8, BLUE_2);

        m_pTurboDisk = new CheckBox(this, 60, 87, "Fast floppy disk access");

        m_pAutoLoad = new CheckBox(this, 60, 108, "Auto-load media inserted at startup screen");

        m_pDosBoot = new CheckBox(this, 60, 129, "Automagically boot non-bootable disks");
        m_pDosBootText = new TextControl(this, 77, 148, "DOS image (blank for SAMDOS 2.2):");
        m_pDosDisk = new EditControl(this, 77, 164, 182);
        m_pBrowse = new TextButton(this, 262, 164, "...", 17);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pDrive1->Select(GetOption(drive1));
        m_pDrive2->Select(GetOption(drive2));
        m_pTurboDisk->SetChecked(GetOption(turbodisk) != 0);
        m_pAutoLoad->SetChecked(GetOption(autoload) != 0);
        m_pDosBoot->SetChecked(GetOption(dosboot) != 0);
        m_pDosDisk->SetText(GetOption(dosdisk));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pTurboDisk, 0);
        OnNotify(m_pDosBoot, 0);
    }
    DriveOptions(const DriveOptions&) = delete;
    void operator= (const DriveOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            int anDriveTypes[] = { drvNone, drvFloppy, drvAtom, drvAtomLite };
            SetOption(drive1, anDriveTypes[m_pDrive1->GetSelected()]);
            SetOption(drive2, anDriveTypes[m_pDrive2->GetSelected()]);

            SetOption(turbodisk, m_pTurboDisk->IsChecked());
            SetOption(autoload, m_pAutoLoad->IsChecked());

            SetOption(dosboot, m_pDosBoot->IsChecked());
            SetOption(dosdisk, m_pDosDisk->GetText().c_str());

            // Drive 2 type changed?
            if (Changed(drive2))
            {
                // Detach current disks
                pAtom->Detach();
                pAtomLite->Detach();

                // Attach new disks
                auto& pActiveAtom = (GetOption(drive2) == drvAtom) ? pAtom : pAtomLite;
                AttachDisk(*pActiveAtom, GetOption(atomdisk0), 0);
                AttachDisk(*pActiveAtom, GetOption(atomdisk1), 1);
            }

            Destroy();
        }
        else if (pWindow_ == m_pBrowse)
            new FileBrowser(m_pDosDisk, this, "Browse for DOS Image", &sFloppyFilter, &nFloppyFilter);
        else if (pWindow_ == m_pDosBoot)
        {
            m_pDosBootText->Enable(m_pDosBoot->IsChecked());
            m_pDosDisk->Enable(m_pDosBoot->IsChecked());
            m_pBrowse->Enable(m_pDosBoot->IsChecked());
        }
    }

protected:
    void AttachDisk(AtaAdapter& adapter, const char* pcszDisk_, int nDevice_)
    {
        if (!adapter.Attach(pcszDisk_, nDevice_))
        {
            char sz[MAX_PATH + 128];
            snprintf(sz, sizeof(sz), "Open failed: %s", pcszDisk_);
            new MsgBox(this, sz, "Warning", mbWarning);
        }
    }

protected:
    ComboBox* m_pDrive1 = nullptr;
    ComboBox* m_pDrive2 = nullptr;
    CheckBox* m_pTurboDisk = nullptr;
    CheckBox* m_pAutoLoad = nullptr;
    CheckBox* m_pDosBoot = nullptr;
    EditControl* m_pDosDisk = nullptr;
    TextControl* m_pDosBootText = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
    TextButton* m_pBrowse = nullptr;
};


class DiskOptions final : public Dialog
{
public:
    DiskOptions(Window* pParent_)
        : Dialog(pParent_, 300, 160, "Disk Settings")
    {
        SetOption(disk1, pFloppy1->DiskPath());
        SetOption(disk2, pFloppy2->DiskPath());

        new IconControl(this, 10, 10, &sFloppyDriveIcon);

        new FrameControl(this, 50, 10, 238, 34);
        new TextControl(this, 60, 6, "Atom Disk Device 0", YELLOW_8, BLUE_2);
        m_pAtom0 = new EditControl(this, 60, 20, 200, GetOption(atomdisk0));
        m_pBrowseAtom0 = new TextButton(this, 264, 20, "...", 17);

        new FrameControl(this, 50, 53, 238, 34);
        new TextControl(this, 60, 49, "Atom Disk Device 1", YELLOW_8, BLUE_2);
        m_pAtom1 = new EditControl(this, 60, 63, 200, GetOption(atomdisk1));
        m_pBrowseAtom1 = new TextButton(this, 264, 63, "...", 17);

        new FrameControl(this, 50, 96, 238, 34);
        new TextControl(this, 60, 92, "SD-IDE Hard Disk", YELLOW_8, BLUE_2);
        m_pSDIDE = new EditControl(this, 60, 106, 200, GetOption(sdidedisk));
        m_pBrowseSDIDE = new TextButton(this, 264, 106, "...", 17);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);
    }
    DiskOptions(const DiskOptions&) = delete;
    void operator= (const DiskOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            // Set the options from the edit control values
            SetOption(atomdisk0, m_pAtom0->GetText().c_str());
            SetOption(atomdisk1, m_pAtom1->GetText().c_str());
            SetOption(sdidedisk, m_pSDIDE->GetText().c_str());

            // Any path changes?
            if (Changed(atomdisk0) || Changed(atomdisk1) || Changed(sdidedisk))
            {
                // Detach current disks
                pAtom->Detach();
                pAtomLite->Detach();
                pSDIDE->Detach();

                // Attach new disks
                auto& pActiveAtom = (GetOption(drive2) == drvAtom) ? pAtom : pAtomLite;
                AttachDisk(*pActiveAtom, GetOption(atomdisk0), 0);
                AttachDisk(*pActiveAtom, GetOption(atomdisk1), 1);
                AttachDisk(*pSDIDE, GetOption(sdidedisk), 0);
            }

            Destroy();
        }
        else if (pWindow_ == m_pBrowseAtom0)
            new HDDProperties(m_pAtom0, this, "Atom Disk Device 0");
        else if (pWindow_ == m_pBrowseAtom1)
            new HDDProperties(m_pAtom1, this, "Atom Disk Device 1");
        else if (pWindow_ == m_pBrowseSDIDE)
            new HDDProperties(m_pSDIDE, this, "SD-IDE Hard Disk");
    }

protected:
    void AttachDisk(AtaAdapter& adapter, const char* pcszDisk_, int nDevice_)
    {
        if (!adapter.Attach(pcszDisk_, nDevice_))
        {
            char sz[MAX_PATH + 128];
            snprintf(sz, sizeof(sz), "Open failed: %s", pcszDisk_);
            new MsgBox(this, sz, "Warning", mbWarning);
        }
    }

protected:
    EditControl* m_pAtom0 = nullptr;
    EditControl* m_pAtom1 = nullptr;
    EditControl* m_pSDIDE = nullptr;
    TextButton* m_pBrowseAtom0 = nullptr;
    TextButton* m_pBrowseAtom1 = nullptr;
    TextButton* m_pBrowseSDIDE = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


class ParallelOptions final : public Dialog
{
public:
    ParallelOptions(Window* pParent_)
        : Dialog(pParent_, 300, 241, "Parallel Settings")
    {
        new IconControl(this, 10, 10, &sPortIcon);
        new FrameControl(this, 50, 17, 238, 91);
        new TextControl(this, 60, 13, "Parallel Ports", YELLOW_8, BLUE_2);
        new TextControl(this, 63, 33, "Devices connected to the parallel ports:");

        new TextControl(this, 80, 57, "Port 1:");
        m_pPort1 = new ComboBox(this, 125, 54, "None|Printer|Mono DAC|Stereo DAC", 100);

        new TextControl(this, 80, 82, "Port 2:");
        m_pPort2 = new ComboBox(this, 125, 79, "None|Printer|Mono DAC|Stereo DAC", 100);

        new IconControl(this, 10, 113, &sPortIcon);
        new FrameControl(this, 50, 120, 238, 84);

        new TextControl(this, 60, 116, "Printer Device", YELLOW_8, BLUE_2);
        m_pPrinterText = new TextControl(this, 63, 136, "SAM printer output will be sent to:");
        m_pPrinter = new ComboBox(this, 63, 152, "File: prntNNNN.txt (auto-generated)", 213);

        m_pFlushDelayText = new TextControl(this, 63, 181, "Auto-flush data:");
        m_pFlushDelay = new ComboBox(this, 151, 178, "Disabled|After 1 second idle|After 2 seconds idle|"
            "After 3 seconds idle|After 4 seconds idle|After 5 seconds idle", 125);

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pPort1->Select(GetOption(parallel1));
        m_pPort2->Select(GetOption(parallel2));
        m_pFlushDelay->Select(GetOption(flushdelay));

        // Update the state of the controls to reflect the current settings
        OnNotify(m_pPort1, 0);
    }
    ParallelOptions(const ParallelOptions&) = delete;
    void operator= (const ParallelOptions) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(parallel1, m_pPort1->GetSelected());
            SetOption(parallel2, m_pPort2->GetSelected());
            SetOption(flushdelay, m_pFlushDelay->GetSelected());

            Destroy();
        }
        else
        {
            bool fPrinter1 = m_pPort1->GetSelected() == 1, fPrinter2 = m_pPort2->GetSelected() == 1;
            m_pPrinterText->Enable(fPrinter1 || fPrinter2);
            m_pPrinter->Enable(fPrinter1 || fPrinter2);
            m_pFlushDelayText->Enable(fPrinter1 || fPrinter2);
            m_pFlushDelay->Enable(fPrinter1 || fPrinter2);
        }
    }

protected:
    ComboBox* m_pPort1 = nullptr;
    ComboBox* m_pPort2 = nullptr;
    ComboBox* m_pPrinter = nullptr;
    ComboBox* m_pFlushDelay = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
    TextControl* m_pPrinterText = nullptr;
    TextControl* m_pFlushDelayText = nullptr;
};


class MiscOptions final : public Dialog
{
public:
    MiscOptions(Window* pParent_)
        : Dialog(pParent_, 300, 201, "Misc Settings")
    {
        new IconControl(this, 10, 15, &sHardwareIcon);
        new FrameControl(this, 50, 17, 238, 57);
        new TextControl(this, 60, 13, "Clocks", YELLOW_8, BLUE_2);
        m_pSambus = new CheckBox(this, 63, 32, "SAMBUS Clock");
        m_pDallas = new CheckBox(this, 63, 52, "DALLAS Clock");

        new FrameControl(this, 50, 89, 238, 80);
        new TextControl(this, 60, 85, "Miscellaneous", YELLOW_8, BLUE_2);
        m_pDriveLights = new CheckBox(this, 63, 104, "Show disk drive LEDs");
        m_pStatus = new CheckBox(this, 63, 124, "Display status messages");
        m_pProfile = new CheckBox(this, 63, 144, "Display emulation speed");

        m_pOK = new TextButton(this, m_nWidth - 117, m_nHeight - 21, "OK", 50);
        m_pCancel = new TextButton(this, m_nWidth - 62, m_nHeight - 21, "Cancel", 50);

        // Set the initial state from the options
        m_pSambus->SetChecked(GetOption(sambusclock));
        m_pDallas->SetChecked(GetOption(dallasclock));

        m_pDriveLights->SetChecked(GetOption(drivelights) != 0);
        m_pStatus->SetChecked(GetOption(status));
        m_pProfile->SetChecked(GetOption(profile));
    }
    MiscOptions(const MiscOptions&) = delete;
    void operator= (const MiscOptions&) = delete;

public:
    void OnNotify(Window* pWindow_, int /*nParam_*/) override
    {
        if (pWindow_ == m_pCancel)
            Destroy();
        else if (pWindow_ == m_pOK)
        {
            SetOption(sambusclock, m_pSambus->IsChecked());
            SetOption(dallasclock, m_pDallas->IsChecked());

            SetOption(drivelights, m_pDriveLights->IsChecked());
            SetOption(status, m_pStatus->IsChecked());
            SetOption(profile, m_pProfile->IsChecked());

            Destroy();
        }
    }

protected:
    CheckBox* m_pSambus = nullptr;
    CheckBox* m_pDallas = nullptr;
    CheckBox* m_pDriveLights = nullptr;
    CheckBox* m_pStatus = nullptr;
    CheckBox* m_pProfile = nullptr;
    TextButton* m_pOK = nullptr;
    TextButton* m_pCancel = nullptr;
};


void OptionsDialog::OnNotify(Window* pWindow_, int nParam_)
{
    if (pWindow_ == m_pClose)
        Destroy();
    else if (pWindow_ == m_pOptions)
    {
        const ListViewItem* pItem = m_pOptions->GetItem();
        if (pItem)
        {
            // Save the current options for change comparisons
            g_opts = Options::s_Options;
            auto label_lower = tolower(pItem->m_label);

            if (label_lower == "system")
            {
                m_pStatus->SetText("Main/external memory configuration and ROM image paths");
                if (nParam_) new SystemOptions(this);
            }
            else if (label_lower == "display")
            {
                m_pStatus->SetText("Display settings for mode, depth, view size, etc.");
                if (nParam_) new DisplayOptions(this);
            }
            else if (label_lower == "sound")
            {
                m_pStatus->SetText("Sound device settings");
                if (nParam_) new SoundOptions(this);
            }
            else if (label_lower == "midi")
            {
                m_pStatus->SetText("MIDI settings for music and network");
                if (nParam_) new MidiOptions(this);
            }
            else if (label_lower == "input")
            {
                m_pStatus->SetText("Keyboard mapping and mouse settings");
                if (nParam_) new InputOptions(this);
            }
            else if (label_lower == "drives")
            {
                m_pStatus->SetText("Floppy disk drive configuration");
                if (nParam_) new DriveOptions(this);
            }
            else if (label_lower == "disks")
            {
                m_pStatus->SetText("Disks for floppy and hard disk drives");
                if (nParam_) new DiskOptions(this);
            }
            else if (label_lower ==  "parallel")
            {
                m_pStatus->SetText("Parallel port settings for printer and DACs)");
                if (nParam_) new ParallelOptions(this);
            }
            else if (label_lower == "misc")
            {
                m_pStatus->SetText("Clock settings and miscellaneous front-end options");
                if (nParam_) new MiscOptions(this);
            }
            else if (label_lower == "about")
            {
                m_pStatus->SetText("Display SimCoupe version number and credits");
                if (nParam_) new AboutDialog(this);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

std::string ImportDialog::s_filepath;
unsigned int ImportDialog::s_uAddr = 32768, ImportDialog::s_uPage, ImportDialog::s_uOffset;
bool ImportDialog::s_fUseBasic = true;

ImportDialog::ImportDialog(Window* pParent_)
    : Dialog(pParent_, 230, 165, "Import Data")
{
    new TextControl(this, 10, 18, "File:");
    m_pFile = new EditControl(this, 35, 15, 160, s_filepath);
    m_pBrowse = new TextButton(this, 200, 15, "...", 17);

    m_pFrame = new FrameControl(this, 10, 47, 208, 88);
    new TextControl(this, 20, 43, "Data", YELLOW_8, BLUE_2);

    m_pBasic = new RadioButton(this, 33, 65, "BASIC Address:", 45);
    m_pPageOffset = new RadioButton(this, 33, 90, "Page number:", 45);
    new TextControl(this, 50, 110, "Page offset:", WHITE);

    m_pAddr = new NumberEditControl(this, 143, 63, 45, s_uAddr);
    m_pPage = new NumberEditControl(this, 143, 88, 20, s_uPage);
    m_pOffset = new NumberEditControl(this, 143, 108, 35, s_uOffset);

    int nX = (m_nWidth - (50 + 8 + 50)) / 2;
    m_pOK = new TextButton(this, nX, m_nHeight - 21, "OK", 50);
    m_pCancel = new TextButton(this, nX + 50 + 8, m_nHeight - 21, "Cancel", 50);

    (s_fUseBasic ? m_pBasic : m_pPageOffset)->Select();
    OnNotify(m_pBasic, 0);
    OnNotify(s_fUseBasic ? m_pAddr : m_pPage, 0);
}

void ImportDialog::OnNotify(Window* pWindow_, int nParam_)
{
    static int nImportFilter = 0;
    static const FILEFILTER sImportFilter =
    {
        "Binary files (*.bin)|"
        "All Files",

        { ".bin", "" }
    };

    if (pWindow_ == m_pCancel)
        Destroy();
    else if (pWindow_ == m_pBrowse)
        new FileBrowser(m_pFile, this, "Select File", &sImportFilter, &nImportFilter);
    else if (pWindow_ == m_pAddr)
    {
        // Fetch the modified address
        s_uAddr = m_pAddr->GetValue();

        // Calculate (and update) the new page and offset
        s_uPage = (s_uAddr / 16384 - 1) & 0x1f;
        s_uOffset = s_uAddr & 0x3fff;
        m_pPage->SetValue(s_uPage);
        m_pOffset->SetValue(s_uOffset);
    }
    else if (pWindow_ == m_pPage || pWindow_ == m_pOffset)
    {
        // Fetch the modified page or offset
        s_uPage = m_pPage->GetValue() & 0x1f;
        s_uOffset = m_pOffset->GetValue();

        // Calculate (and update) the new address
        s_uAddr = ((s_uPage + 1) * 16384 + s_uOffset) % 0x84000;    // wrap at end of memory
        m_pAddr->SetValue(s_uAddr);

        // Normalise the internal page and offset from the address
        s_uPage = (s_uAddr / 16384 - 1) & 0x1f;
        s_uOffset = s_uAddr & 0x3fff;
    }
    else if (pWindow_ == m_pBasic || pWindow_ == m_pPageOffset)
    {
        // Fetch the radio selection
        s_fUseBasic = m_pBasic->IsSelected();

        // Enable/disable the edit controls depending on the radio selection
        m_pAddr->Enable(s_fUseBasic);
        m_pPage->Enable(!s_fUseBasic);
        m_pOffset->Enable(!s_fUseBasic);
    }
    else if (pWindow_ == m_pOK || nParam_)
    {
        // Fetch/update the stored filename
        s_filepath = m_pFile->GetText();

        FILE* hFile{};
        if (s_filepath.empty() || !(hFile = fopen(s_filepath.c_str(), "rb")))
        {
            new MsgBox(this, "Failed to open file for reading", "Error", mbWarning);
            return;
        }

        unsigned int uPage = (s_uAddr < 0x4000) ? ROM0 : s_uPage;
        unsigned int uOffset = s_uOffset, uLen = 0x400000;  // 4MB max import
        size_t uRead = 0;

        // Loop reading chunk blocks into the relevant pages
        for (unsigned int uChunk; (uChunk = std::min(uLen, (0x4000 - uOffset))); uLen -= uChunk, uOffset = 0)
        {
            // Read directly into system memory
            uRead += fread(PageWritePtr(uPage) + uOffset, 1, uChunk, hFile);

            // Wrap to page 0 after ROM0
            if (uPage == ROM0 + 1)
                uPage = 0;

            // Stop reading if we've hit the end or reached the end of a logical block
            if (feof(hFile) || uPage == EXTMEM || uPage >= ROM0)
                break;
        }

        fclose(hFile);
        Frame::SetStatus("Imported {} bytes", uRead);
        Destroy();
    }
}


unsigned int ExportDialog::s_uLength = 16384;  // show 16K as the initial export length

ExportDialog::ExportDialog(Window* pParent_)
    : ImportDialog(pParent_)
{
    SetText("Export Data");

    // Enlarge the input dialog for the new controls
    int nOffset = 22;
    Offset(0, -nOffset / 2);
    Inflate(0, nOffset);
    m_pFrame->Inflate(0, nOffset);
    m_pOK->Offset(0, nOffset);
    m_pCancel->Offset(0, nOffset);

    // Add the new controls for Export
    new TextControl(this, 50, 135, "Length:", WHITE);
    m_pLength = new NumberEditControl(this, 143, 133, 45, s_uLength);

    // Move the OK and Cancel buttons to the end of the tab order
    m_pOK->SetParent(m_pOK->GetParent());
    m_pCancel->SetParent(m_pCancel->GetParent());
}


void ExportDialog::OnNotify(Window* pWindow_, int nParam_)
{
    if (pWindow_ == m_pLength)
        s_uLength = m_pLength->GetValue();

    else if (pWindow_ == m_pOK || nParam_)
    {
        // Fetch/update the stored filename
        s_filepath = m_pFile->GetText();

        FILE* hFile{};
        if (s_filepath.empty() || !(hFile = fopen(s_filepath.c_str(), "wb")))
        {
            new MsgBox(this, "Failed to open file for writing", "Error", mbWarning);
            return;
        }

        unsigned int uPage = (s_uAddr < 0x4000) ? ROM0 : s_uPage;
        unsigned int uOffset = s_uOffset, uLen = s_uLength;
        size_t uWritten = 0;

        // Loop reading chunk blocks into the relevant pages
        for (unsigned int uChunk; (uChunk = std::min(uLen, (0x4000 - uOffset))); uLen -= uChunk, uOffset = 0)
        {
            // Write directly from system memory
            uWritten += fwrite(PageReadPtr(uPage++) + uOffset, 1, uChunk, hFile);

            if (ferror(hFile))
            {
                new MsgBox(this, "Error writing to file (disk full?)", "Error", mbWarning);
                fclose(hFile);
                return;
            }

            // Wrap to page 0 after ROM0
            if (uPage == ROM0 + 1)
                uPage = 0;

            // If the first block was in ROM0 or we've passed memory end, wrap to page 0
            if (uPage == EXTMEM || uPage == ROM0)
                break;
        }

        fclose(hFile);
        Frame::SetStatus("Exported {} bytes", uWritten);
        Destroy();
    }

    // Pass to the base handler
    else
        ImportDialog::OnNotify(pWindow_, nParam_);
}

////////////////////////////////////////////////////////////////////////////////

std::string NewDiskDialog::s_filepath;
unsigned int NewDiskDialog::s_uType = 0;
bool NewDiskDialog::s_fCompress, NewDiskDialog::s_fFormat = true;

NewDiskDialog::NewDiskDialog(int nDrive_, Window* pParent_/*=nullptr*/)
    : Dialog(pParent_, 355, 100, "New Disk")
{
    SetText(fmt::format("New Disk {}", nDrive_));

    new IconControl(this, 10, 10, &sDiskIcon);

    new TextControl(this, 60, 10, "Select the type of disk to create:");
    m_pType = new ComboBox(this, 60, 29, "MGT disk image (800K)|"
        "EDSK disk image (flexible format)|"
        "DOS CP/M image (720K)", 215);

    m_pCompress = new CheckBox(this, 60, 55, "Compress image to save space");
    (m_pFormat = new CheckBox(this, 60, 76, "Format image ready for use"))->Enable(false);

    m_pType->Select(s_uType);
    m_pFormat->SetChecked(s_fFormat);

#ifdef HAVE_LIBZ
    m_pCompress->SetChecked(s_fCompress);
#else
    m_pCompress->Enable(false);
#endif

    OnNotify(m_pType, 0);

    m_pOK = new TextButton(this, m_nWidth - 65, 10, "OK", 55);
    m_pCancel = new TextButton(this, m_nWidth - 65, 33, "Cancel", 55);
}

void NewDiskDialog::OnNotify(Window* pWindow_, int /*nParam_*/)
{
    if (pWindow_ == m_pCancel)
        Destroy();
    else if (pWindow_ == m_pOK)
        new MsgBox(this, "New Disk isn't finished yet!", "Sorry", mbWarning);
    else if (pWindow_ == m_pType)
    {
        int nType = m_pType->GetSelected();

        m_pCompress->Enable(nType == 0);
        if (nType != 0)
            m_pCompress->SetChecked(false);

        m_pFormat->Enable(nType == 1);
        if (nType != 1)
            m_pFormat->SetChecked();
    }
}
