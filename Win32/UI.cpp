// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: Win32 user interface
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
#include "UI.h"

#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cderr.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <VersionHelpers.h>

#include "AtaAdapter.h"
#include "AVI.h"
#include "Clock.h"
#include "CPU.h"
#include "Debug.h"
#include "D3D11.h"
#include "Drive.h"
#include "Expr.h"
#include "Floppy.h"
#include "Frame.h"
#include "GIF.h"
#include "GUIDlg.h"
#include "IDEDisk.h"
#include "Input.h"
#include "Keyin.h"
#include "Main.h"
#include "Memory.h"
#include "Midi.h"
#include "ODmenu.h"
#include "Options.h"
#include "Parallel.h"
#include "Sound.h"
#include "Tape.h"
#include "Video.h"
#include "WAV.h"

#include "resource.h"   // For menu and dialogue box symbols

static const int MOUSE_HIDE_TIME = 2000;   // 2 seconds
static const UINT MOUSE_TIMER_ID = 42;

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe"
#endif

#define PRINTER_PREFIX      "Printer: "

INT_PTR CALLBACK ImportExportDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
INT_PTR CALLBACK NewDiskDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
void CentreWindow(HWND hwnd_, HWND hwndParent_ = nullptr);

static void DisplayOptions();
static bool InitWindow();

static void LoadRecentFiles();
static void SaveRecentFiles();

static bool EjectDisk(DiskDevice& pFloppy_);
static bool EjectTape();

static void SaveWindowPosition(HWND hwnd_);
static bool RestoreWindowPosition(HWND hwnd_);


HINSTANCE __hinstance;
HWND g_hwnd;
static HMENU g_hmenu;
extern HINSTANCE __hinstance;

static HHOOK hWinKeyHook;

static WNDPROC pfnStaticWndProc;           // Old static window procedure (internal value)

static WINDOWPLACEMENT g_wp{};
static int nWindowDx, nWindowDy;

static int nOptionPage = 0;                // Last active option property page
static const int MAX_OPTION_PAGES = 16;    // Maximum number of option propery pages
static bool fCentredOptions;

static Config current_config;
#define Changed(o)  (current_config.o != GetOption(o))

static char szFloppyFilters[] =
#ifdef HAVE_LIBZ
"All Disks (dsk;sad;mgt;sbt;cpm;gz;zip)\0*.dsk;*.sad;*.mgt;*.sbt;*.cpm;*.gz;*.zip\0"
#endif
"Disk Images (dsk;sad;mgt;sbt;cpm)\0*.dsk;*.sad;*.mgt;*.sbt;*.cpm\0"
#ifdef HAVE_LIBZ
"Compressed Files (gz;zip)\0*.gz;*.zip\0"
#endif
"All Files (*.*)\0*.*\0";

static char szNewDiskFilters[] =
"MGT disk image (*.mgt)\0*.mgt\0"
"EDSK disk image (*.dsk)\0*.dsk\0"
"CP/M disk image (*.cpm)\0*.cpm\0";

static char szHDDFilters[] =
"Hard Disk Images (*.hdf)\0*.hdf\0"
"All Files (*.*)\0*.*\0";

static char szRomFilters[] =
"ROM images (*.rom;*.zx82)\0*.rom;*.zx82\0"
"All files (*.*)\0*.*\0";

static char szDataFilters[] =
"Data files (*.bin;*.dat;*.raw;*.txt)\0*.bin;*.dat;*.raw;*.txt\0"
"All files (*.*)\0*.*\0";

static char szTextFilters[] =
"Data files (*.txt)\0*.txt\0"
"All files (*.*)\0*.*\0";

static char szTapeFilters[] =
"Tape Images (*.tzx;*.tap;*.csw)\0*.tzx;*.tap;*.csw\0"
#ifdef HAVE_LIBZ
"Compressed Files (gz;zip)\0*.gz;*.zip\0"
#endif
"All Files (*.*)\0*.*\0";

static const char* aszBorders[] =
{ "No borders", "Small borders", "Short TV area (default)", "TV visible area", "Complete scan area", nullptr };


extern "C" int main(int argc_, char* argv_[]);

int WINAPI WinMain(
    _In_ HINSTANCE hinst_,
    _In_opt_ HINSTANCE hinstPrev_,
    _In_ LPSTR pszCmdLine_,
    _In_ int nCmdShow_)
{
    __hinstance = hinst_;

    return main(__argc, __argv);
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath(char* pszPath_, size_t nLength_);

bool UI::Init()
{
    LoadRecentFiles();
    return InitWindow();
}

void UI::Exit()
{
    if (g_hwnd)
    {
        SaveWindowPosition(g_hwnd);
        DestroyWindow(g_hwnd), g_hwnd = nullptr;
    }

    SaveRecentFiles();
}


std::unique_ptr<IVideoBase> UI::CreateVideo()
{
    if (auto backend = std::make_unique<Direct3D11Video>(g_hwnd); backend->Init())
    {
        return backend;
    }

    return nullptr;
}

// Check and process any incoming messages
bool UI::CheckEvents()
{
    while (1)
    {
        // Loop to process any pending Windows messages
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // App closing down?
            if (msg.message == WM_QUIT)
                return false;

            // Translation for menu shortcuts, but avoid producing keypad symbols
            if (msg.message != WM_KEYDOWN || msg.wParam < VK_NUMPAD0 || msg.wParam > VK_DIVIDE)
                TranslateMessage(&msg);

            DispatchMessage(&msg);
        }

        // If we're not paused, break out to run the next frame
        if (!g_fPaused)
            break;

        WaitMessage();
    }

    return true;
}

void UI::ShowMessage(MsgType type, const std::string& message)
{
    const char* const pcszCaption = WINDOW_CAPTION;
    HWND hwndParent = GetActiveWindow();

    switch (type)
    {
    case MsgType::Warning:
        MessageBox(hwndParent, message.c_str(), pcszCaption, MB_OK | MB_ICONEXCLAMATION);
        break;

    case MsgType::Error:
        MessageBox(hwndParent, message.c_str(), pcszCaption, MB_OK | MB_ICONSTOP);
        break;

        // Something went seriously wrong!
    case MsgType::Fatal:
        MessageBox(hwndParent, message.c_str(), pcszCaption, MB_OK | MB_ICONSTOP);
        break;
    }
}


std::string GetDlgItemText(HWND hdlg_, int ctrl_id = 0)
{
    char sz[MAX_PATH]{};
    HWND hctrl = ctrl_id ? GetDlgItem(hdlg_, ctrl_id) : hdlg_;
    GetWindowText(hctrl, sz, _countof(sz));
    return sz;
}

void SetDlgItemText(HWND hdlg_, int ctrl_id, const std::string& str, bool select = false)
{
    HWND hctrl = ctrl_id ? GetDlgItem(hdlg_, ctrl_id) : hdlg_;
    SetWindowText(hctrl, str.c_str());

    if (select)
    {
        Edit_SetSel(hctrl, 0, -1);
        SetFocus(hctrl);
    }
}

int GetDlgItemValue(HWND hdlg_, int nId_, int default_value = -1)
{
    try
    {
        return std::stoi(GetDlgItemText(hdlg_, nId_));
    }
    catch (...)
    {
        return default_value;
    }
}

// Save changes to a given drive, optionally prompting for confirmation
bool ChangesSaved(DiskDevice& floppy)
{
    if (!floppy.DiskModified())
        return true;

    if (GetOption(saveprompt))
    {
        switch (MessageBox(g_hwnd,
            fmt::format("Save changes to {}?", floppy.DiskFile()).c_str(),
            WINDOW_CAPTION, MB_YESNOCANCEL | MB_ICONQUESTION))
        {
        case IDYES:     break;
        case IDNO:      floppy.SetDiskModified(false); return true;
        default:        return false;
        }
    }

    if (!floppy.Save())
    {
        Message(MsgType::Warning, "Failed to save changes to {}", floppy.DiskFile());
        return false;
    }

    return true;
}

bool GetSaveLoadFile(LPOPENFILENAME lpofn_, bool fLoad_, bool fCheckExisting_ = true)
{
    // Force an Explorer-style dialog, and ensure loaded files exist
    lpofn_->Flags |= OFN_EXPLORER | OFN_PATHMUSTEXIST;
    if (fCheckExisting_) lpofn_->Flags |= (fLoad_ ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);

    // Loop until successful
    while (!(fLoad_ ? GetOpenFileName(lpofn_) : GetSaveFileName(lpofn_)))
    {
        // Invalid paths choke the dialog
        if (CommDlgExtendedError() == FNERR_INVALIDFILENAME)
            *lpofn_->lpstrFile = '\0';
        else
        {
            TRACE("!!! GetSaveLoadFile() failed with {:x}\n", CommDlgExtendedError());
            return false;
        }
    }

    return true;
}


constexpr auto MAX_RECENT_FILES = 6;
static std::vector<std::string> recent_files;

void RemoveRecentFile(const std::string& path)
{
    auto lower_path = tolower(path);
    auto it = std::find_if(recent_files.begin(), recent_files.end(),
        [&](const auto& str) { return tolower(str) == lower_path; });

    if (it != recent_files.end())
        recent_files.erase(it);
}

void AddRecentFile(const std::string& path)
{
    RemoveRecentFile(path);
    recent_files.insert(recent_files.begin(), path);
}

void LoadRecentFiles()
{
    recent_files = {
        GetOption(mru0), GetOption(mru1), GetOption(mru2),
        GetOption(mru3), GetOption(mru4), GetOption(mru5)
    };

    recent_files.erase(
        std::remove_if(recent_files.begin(), recent_files.end(),
            [](const auto& str) { return str.empty(); }),
        recent_files.end());
}

void SaveRecentFiles()
{
    recent_files.resize(MAX_RECENT_FILES);
    SetOption(mru0, recent_files[0]);
    SetOption(mru1, recent_files[1]);
    SetOption(mru2, recent_files[2]);
    SetOption(mru3, recent_files[3]);
    SetOption(mru4, recent_files[4]);
    SetOption(mru5, recent_files[5]);
}

void UpdateRecentFiles(HMENU hmenu_, int nId_, int nOffset_)
{
    for (int i = 0; i < MAX_RECENT_FILES; i++)
        DeleteMenu(hmenu_, nId_ + i, MF_BYCOMMAND);

    if (recent_files.empty())
    {
        InsertMenu(hmenu_, GetMenuItemCount(hmenu_) - nOffset_, MF_STRING | MF_BYPOSITION, nId_, "Recent Files");
        EnableMenuItem(hmenu_, nId_, MF_GRAYED);
    }
    else
    {
        int nInsertPos = GetMenuItemCount(hmenu_) - nOffset_;

        for (int i = 0; i < static_cast<int>(recent_files.size()); i++)
        {
            char szItem[MAX_PATH * 2], * psz = szItem;
            psz += wsprintf(szItem, "&%d ", i + 1);

            for (auto p = recent_files[i].c_str(); *p; *psz++ = *p++)
            {
                if (*p == '&')
                    *psz++ = *p;
            }

            *psz = '\0';
            ClipPath(szItem + 3, 32);

            InsertMenu(hmenu_, nInsertPos++, MF_STRING | MF_BYPOSITION, nId_ + i, szItem);
        }
    }
}


bool AttachDisk(AtaAdapter& adapter, const std::string& disk_path, int nDevice_)
{
    if (!adapter.Attach(disk_path, nDevice_))
    {
        // Attempt to determine why we couldn't open the device
        auto disk = std::make_unique<DeviceHardDisk>(disk_path);
        if (!disk->Open(false))
        {
            // Access will be denied if we're running as a regular user, and without SAMdiskHelper
            if (GetLastError() == ERROR_ACCESS_DENIED)
                Message(MsgType::Warning, "Failed to open: {}\n\nAdminstrator access or SAMdiskHelper is required.", disk_path);
            else
                Message(MsgType::Warning, "Invalid or incompatible disk: {}", disk_path);
        }
        return false;
    }

    return true;
}

bool InsertDisk(DiskDevice& floppy, std::optional<std::string> new_path = std::nullopt, bool autoload = true)
{
    int nDrive = (&floppy == pFloppy1.get()) ? 1 : 2;

    // Save any changes to the current disk first
    if (!ChangesSaved(floppy))
        return false;

    // Check the floppy drive is present
    if ((nDrive == 1 && GetOption(drive1) != drvFloppy) ||
        (nDrive == 2 && GetOption(drive2) != drvFloppy))
    {
        Message(MsgType::Warning, "Floppy drive {} is not present", nDrive);
        return false;
    }

    // Eject any existing disk if the new path is blank
    if (new_path && (*new_path).empty())
    {
        EjectDisk(floppy);
        return true;
    }

    char szFile[MAX_PATH]{};
    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = szFloppyFilters;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    auto disk_path = floppy.DiskFile();
    std::copy(disk_path.begin(), disk_path.end(), szFile);

    if (!new_path)
    {
        if (!GetSaveLoadFile(&ofn, true))
            return false;

        new_path = szFile;
    }

    bool read_only = !!(ofn.Flags & OFN_READONLY);

    if (!floppy.Insert(*new_path, autoload))
    {
        Message(MsgType::Warning, "Invalid disk: {}", *new_path);
        RemoveRecentFile(*new_path);
        return false;
    }

    Frame::SetStatus("{}  inserted into drive {}{}", floppy.DiskFile(), (&floppy == pFloppy1.get()) ? 1 : 2, read_only ? " (read-only)" : "");
    AddRecentFile(*new_path);
    return true;
}

bool EjectDisk(DiskDevice& floppy)
{
    if (!floppy.HasDisk())
        return true;

    if (ChangesSaved(floppy))
    {
        Frame::SetStatus("{}  ejected from drive {}", floppy.DiskFile(), (&floppy == pFloppy1.get()) ? 1 : 2);
        floppy.Eject();
        return true;
    }

    return false;
}


bool InsertTape(HWND hwndParent_, std::optional<std::string> new_path = std::nullopt)
{
    char szFile[MAX_PATH]{};
    auto tape_path = Tape::GetPath();
    std::copy(tape_path.begin(), tape_path.end(), szFile);

    if (new_path && (*new_path).empty())
    {
        EjectTape();
        return true;
    }

    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwndParent_;
    ofn.lpstrFilter = szTapeFilters;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;

    if (!new_path)
    {
        if (!GetSaveLoadFile(&ofn, true))
            return false;

        new_path = szFile;
    }

    if (!Tape::Insert(*new_path))
    {
        Message(MsgType::Warning, "Invalid tape: {}", *new_path);
        RemoveRecentFile(*new_path);
        return false;
    }

    Frame::SetStatus("{}  inserted", Tape::GetFile());
    AddRecentFile(*new_path);
    return true;
}

bool EjectTape()
{
    if (!Tape::IsInserted())
        return true;

    Frame::SetStatus("{}  ejected", Tape::GetFile());
    Tape::Eject();
    return true;
}

#ifdef HAVE_LIBSPECTRUM

void UpdateTapeToolbar(HWND hdlg_)
{
    libspectrum_tape* tape = Tape::GetTape();
    bool fInserted = tape != nullptr;

    HWND hwndToolbar = GetDlgItem(hdlg_, ID_TAPE_TOOLBAR);

    SendMessage(hwndToolbar, TB_ENABLEBUTTON, ID_TAPE_OPEN, 1);
    SendMessage(hwndToolbar, TB_ENABLEBUTTON, ID_TAPE_EJECT, fInserted);
    SendMessage(hwndToolbar, TB_CHECKBUTTON, ID_TAPE_TURBOLOAD, GetOption(turbotape));
    SendMessage(hwndToolbar, TB_CHECKBUTTON, ID_TAPE_TRAPS, GetOption(tapetraps));
}

void UpdateTapeBlockList(HWND hdlg_)
{
    libspectrum_tape* tape = Tape::GetTape();
    bool fInserted = tape != nullptr;

    // Show the overlay status text for an empty list
    HWND hwndStatus = GetDlgItem(hdlg_, IDS_TAPE_STATUS);
    ShowWindow(hwndStatus, SW_SHOW);

    // Clear the existing list content
    HWND hwndList = GetDlgItem(hdlg_, IDL_TAPE_BLOCKS);
    ListView_DeleteAllItems(hwndList);

    if (fInserted)
    {
        // Hide the status text as a tape is present
        ShowWindow(hwndStatus, SW_HIDE);

        libspectrum_tape_iterator it = nullptr;
        libspectrum_tape_block* block = libspectrum_tape_iterator_init(&it, tape);
        int block_idx = 0;

        // Loop over all blocks in the tape
        for (; block; block = libspectrum_tape_iterator_next(&it), block_idx++)
        {
            char sz[128] = "";
            libspectrum_tape_block_description(sz, sizeof(sz), block);

            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = block_idx;
            lvi.iSubItem = 0;
            lvi.pszText = sz;

            // Insert the new block item, setting the first column to the type text
            int nIndex = ListView_InsertItem(hwndList, &lvi);

            // Set the second column to the block details
            auto details = Tape::GetBlockDetails(block);
            ListView_SetItemText(hwndList, nIndex, 1, const_cast<char*>(details.c_str()));
        }

        // Fetch the current block index
        if (block_idx > 0 && libspectrum_tape_position(&block_idx, tape) == LIBSPECTRUM_ERROR_NONE)
        {
            // Select the current block in the list, and ensure it's visible
            ListView_SetItemState(hwndList, block_idx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(hwndList, block_idx, FALSE);
        }
    }
}

INT_PTR CALLBACK TapeBrowseDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HWND hwndToolbar, hwndList, hwndStatus;
    static HIMAGELIST hImageList;
    static bool fAutoLoad;

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        // Create a flat toolbar with tooltips
        hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "", WS_CHILD | TBSTYLE_FLAT | WS_VISIBLE | TBSTYLE_TOOLTIPS, 0, 0, 0, 0, hdlg_, (HMENU)ID_TAPE_TOOLBAR, __hinstance, nullptr);
        SendMessage(hwndToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0L);

        // Load the image list for our custom icons
        hImageList = ImageList_LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_TAPE_TOOLBAR), 16, 0, CLR_DEFAULT);
        SendMessage(hwndToolbar, TB_SETIMAGELIST, 0, (LPARAM)hImageList);
        int nImageListIcons = ImageList_GetImageCount(hImageList);

        // Append the small icon set from the system common controls DLL
        TBADDBITMAP tbab = { HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
        SendMessage(hwndToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&tbab));

        static TBBUTTON tbb[] =
        {
            { nImageListIcons + STD_FILEOPEN, ID_TAPE_OPEN, TBSTATE_ENABLED, TBSTYLE_BUTTON },
            { 5, ID_TAPE_EJECT, TBSTATE_ENABLED, TBSTYLE_BUTTON },
            { 0, 0, 0, TBSTYLE_SEP },
            { 7, ID_TAPE_TURBOLOAD, TBSTATE_ENABLED, TBSTYLE_CHECK },
            { 8, ID_TAPE_TRAPS, TBSTATE_ENABLED, TBSTYLE_CHECK }
        };

        // Add the toolbar buttons and configure settings
        SendMessage(hwndToolbar, TB_ADDBUTTONS, std::size(tbb), (LPARAM)&tbb);
        SendMessage(hwndToolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(28, 28));
        SendMessage(hwndToolbar, TB_SETINDENT, 6, 0L);
        SendMessage(hwndToolbar, TB_AUTOSIZE, 0, 0);


        // Locate the list control on the dialog
        hwndList = GetDlgItem(hdlg_, IDL_TAPE_BLOCKS);

        // Set full row selection and overflow tooltips
        DWORD dwStyle = ListView_GetExtendedListViewStyle(hwndList);
        ListView_SetExtendedListViewStyle(hwndList, dwStyle | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP);

        LVCOLUMN lvc = { };
        lvc.mask = LVCF_WIDTH | LVCF_TEXT;

        // Add first type column
        lvc.cx = 130;
        lvc.pszText = "Type";
        ListView_InsertColumn(hwndList, 0, &lvc);

        // Add second details column
        lvc.cx = 215;
        lvc.pszText = "Details";
        ListView_InsertColumn(hwndList, 1, &lvc);


        // Locate the status text window for later
        hwndStatus = GetDlgItem(hdlg_, IDS_TAPE_STATUS);

        // Update the toolbar and list view with current settings
        UpdateTapeToolbar(hdlg_);
        UpdateTapeBlockList(hdlg_);

        // Don't auto-load on dialog exit
        fAutoLoad = false;

        CentreWindow(hdlg_);
        return 1;
    }

    case WM_DESTROY:
        DestroyWindow(hwndToolbar), hwndToolbar = nullptr;
        ImageList_Destroy(hImageList), hImageList = nullptr;
        break;

    case WM_CTLCOLORSTATIC:
        // Return a null background brush to make the status text background transparent
        if (reinterpret_cast<HWND>(lParam_) == hwndStatus)
            return reinterpret_cast<INT_PTR>(GetStockBrush(NULL_BRUSH));
        break;

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);

        // Click on list control?
        if (wParam_ == IDL_TAPE_BLOCKS && pnmh->code == NM_CLICK)
        {
            LPNMITEMACTIVATE pnmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam_);
            libspectrum_tape* tape = Tape::GetTape();

            // Select the tape block corresponding to the row clicked
            if (tape && pnmia->iItem >= 0)
                libspectrum_tape_nth_block(tape, pnmia->iItem);
        }
        // Tooltip display request?
        else if (pnmh->code == TTN_GETDISPINFO)
        {
            LPTOOLTIPTEXT pttt = reinterpret_cast<LPTOOLTIPTEXT>(lParam_);

            // Return the appropriate tooltip text
            switch (pttt->hdr.idFrom)
            {
            case ID_TAPE_OPEN: pttt->lpszText = "Open"; break;
            case ID_TAPE_EJECT: pttt->lpszText = "Eject"; break;
            case ID_TAPE_TURBOLOAD: pttt->lpszText = "Fast Loading"; break;
            case ID_TAPE_TRAPS: pttt->lpszText = "Tape Traps"; break;
            }

            return TRUE;
        }

        break;
    }

    case WM_COMMAND:
    {
        switch (wParam_)
        {
        case IDCANCEL:
            EndDialog(hdlg_, 0);
            break;

        case IDCLOSE:
            // Trigger auto-load if required
            if (fAutoLoad && Tape::IsInserted())
                IO::AutoLoad(AutoLoadType::Tape);

            EndDialog(hdlg_, 0);
            break;

        case ID_TAPE_OPEN:
            fAutoLoad |= InsertTape(hdlg_);
            Keyin::Stop();
            UpdateTapeBlockList(hdlg_);
            break;

        case ID_TAPE_EJECT:
            fAutoLoad = false;
            Tape::Eject();
            UpdateTapeBlockList(hdlg_);
            break;

        case ID_TAPE_TURBOLOAD:
            SetOption(turbotape, !GetOption(turbotape));
            break;

        case ID_TAPE_TRAPS:
            SetOption(tapetraps, !GetOption(tapetraps));
            break;

        default:
            return 0;
        }

        // Update the toolbar after all commands
        UpdateTapeToolbar(hdlg_);
        break;
    }

    // Handle a file being dropped on the dialog
    case WM_DROPFILES:
    {
        char szFile[MAX_PATH]{};
        if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof(szFile)))
        {
            InsertTape(hdlg_, szFile);
            UpdateTapeToolbar(hdlg_);
            UpdateTapeBlockList(hdlg_);
        }

        return 0;
    }
    }

    return 0;
}

#endif // HAVE_LIBSPECTRUM


#define CheckOption(id,check)   CheckMenuItem(hmenu, (id), (check) ? MF_CHECKED : MF_UNCHECKED)
#define EnableItem(id,enable)   EnableMenuItem(hmenu, (id), (enable) ? MF_ENABLED : MF_GRAYED)

void UpdateMenuFromOptions()
{
    HMENU hmenu = g_hmenu;
    HMENU hmenuFile = GetSubMenu(hmenu, 0);
    HMENU hmenuFloppy2 = GetSubMenu(hmenuFile, 6);

    bool fFloppy1 = GetOption(drive1) == drvFloppy, fInserted1 = pFloppy1->HasDisk();
    bool fFloppy2 = GetOption(drive2) == drvFloppy, fInserted2 = pFloppy2->HasDisk();

    ModifyMenu(hmenu, IDM_FILE_FLOPPY1_EJECT, MF_STRING, IDM_FILE_FLOPPY1_EJECT,
        fmt::format("&Close {}", fInserted1 ? pFloppy1->DiskFile() : "").c_str());
    ModifyMenu(hmenu, IDM_FILE_FLOPPY2_EJECT, MF_STRING, IDM_FILE_FLOPPY2_EJECT,
        fmt::format("&Close {}", fInserted2 ? pFloppy2->DiskFile() : "").c_str());

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 1 options
    EnableItem(IDM_FILE_NEW_DISK1, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_INSERT, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_EJECT, fInserted1);
    EnableItem(IDM_FILE_FLOPPY1_SAVE_CHANGES, fFloppy1 && pFloppy1->DiskModified());

    // Only enable the floppy device menu item if it's supported
    EnableItem(IDM_FILE_FLOPPY1_DEVICE, FloppyStream::IsSupported());
    CheckOption(IDM_FILE_FLOPPY1_DEVICE, fInserted1 && FloppyStream::IsRecognised(pFloppy1->DiskFile()));

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 2 options
    EnableMenuItem(hmenuFile, 6, MF_BYPOSITION | (fFloppy2 ? MF_ENABLED : MF_GRAYED));
    EnableItem(IDM_FILE_FLOPPY2_EJECT, fInserted2);
    EnableItem(IDM_FILE_FLOPPY2_SAVE_CHANGES, pFloppy2->DiskModified());

    CheckOption(IDM_VIEW_FULLSCREEN, GetOption(fullscreen));
    CheckOption(IDM_VIEW_TVASPECT, GetOption(tvaspect));

    CheckOption(IDM_VIEW_SMOOTH, GetOption(smooth));
    CheckOption(IDM_VIEW_MOTIONBLUR, GetOption(motionblur));

    CheckMenuRadioItem(hmenu, IDM_VIEW_BORDERS0, IDM_VIEW_BORDERS4, IDM_VIEW_BORDERS0 + GetOption(borders), MF_BYCOMMAND);

#ifndef HAVE_LIBPNG
    EnableItem(IDM_RECORD_SCREEN_PNG, FALSE);
#endif

    EnableItem(IDM_RECORD_AVI_START, !AVI::IsRecording());
    EnableItem(IDM_RECORD_AVI_HALF, !AVI::IsRecording());
    EnableItem(IDM_RECORD_AVI_STOP, AVI::IsRecording());

    EnableItem(IDM_RECORD_GIF_START, !GIF::IsRecording());
    EnableItem(IDM_RECORD_GIF_LOOP, !GIF::IsRecording());
    EnableItem(IDM_RECORD_GIF_STOP, GIF::IsRecording());

    EnableItem(IDM_RECORD_WAV_START, !WAV::IsRecording());
    EnableItem(IDM_RECORD_WAV_SEGMENT, !WAV::IsRecording());
    EnableItem(IDM_RECORD_WAV_STOP, WAV::IsRecording());

    CheckOption(IDM_SYSTEM_PAUSE, g_fPaused);

    int speedid = IDM_SYSTEM_SPEED_100;
    switch (GetOption(speed))
    {
    case 50:   speedid = IDM_SYSTEM_SPEED_50;   break;
    case 100:  speedid = IDM_SYSTEM_SPEED_100;  break;
    case 200:  speedid = IDM_SYSTEM_SPEED_200;  break;
    case 300:  speedid = IDM_SYSTEM_SPEED_300;  break;
    case 500:  speedid = IDM_SYSTEM_SPEED_500;  break;
    case 1000: speedid = IDM_SYSTEM_SPEED_1000; break;
    }
    CheckMenuRadioItem(hmenu, IDM_SYSTEM_SPEED_50, IDM_SYSTEM_SPEED_1000, speedid, MF_BYCOMMAND);

    // The built-in GUI prevents some items from being used, so disable them if necessary
    EnableItem(IDM_TOOLS_OPTIONS, !GUI::IsActive());
    EnableItem(IDM_TOOLS_DEBUGGER, !g_fPaused && (Debug::IsActive() || !GUI::IsActive()));
    CheckOption(IDM_TOOLS_DEBUGGER, Debug::IsActive());
    CheckOption(IDM_TOOLS_RASTER_DEBUG, GetOption(rasterdebug));

    // Enable the Flush printer item if there's buffered data in either printer
    bool fPrinter1 = GetOption(parallel1) == 1, fPrinter2 = GetOption(parallel2) == 1;
    bool fFlushable = pPrinterFile->IsFlushable();
    EnableItem(IDM_TOOLS_FLUSH_PRINTER, fFlushable);

    // Enable the online option if a printer is active, and check it if it's online
    EnableItem(IDM_TOOLS_PRINTER_ONLINE, fPrinter1 || fPrinter2);
    CheckOption(IDM_TOOLS_PRINTER_ONLINE, (fPrinter1 || fPrinter2) && GetOption(printeronline));

    // Enable clipboard pasting if Unicode text data is available
    EnableItem(IDM_TOOLS_PASTE_CLIPBOARD, Keyin::CanType() && IsClipboardFormatAvailable(CF_UNICODETEXT));

#ifdef HAVE_LIBSPECTRUM
    EnableItem(IDM_TOOLS_TAPE_BROWSER, true);
#endif

    UpdateRecentFiles(hmenuFile, IDM_FILE_RECENT1, 2);
    UpdateRecentFiles(hmenuFloppy2, IDM_FLOPPY2_RECENT1, 0);
}


bool UI::DoAction(Action action, bool pressed)
{
    // Key being pressed?
    if (pressed)
    {
        switch (action)
        {
        case Action::ToggleFullscreen:
            SetOption(fullscreen, !GetOption(fullscreen));

            if (GetOption(fullscreen))
            {
                g_wp.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(g_hwnd, &g_wp);

                SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP);
                SetMenu(g_hwnd, nullptr);
                SetWindowPos(g_hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW  | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetMenu(g_hwnd, g_hmenu);
                SetWindowPos(g_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

                if (g_wp.length)
                    SetWindowPlacement(g_hwnd, &g_wp);
            }
            break;

        case Action::InsertDisk1:
            InsertDisk(*pFloppy1);
            break;

        case Action::EjectDisk1:
            EjectDisk(*pFloppy1);
            break;

        case Action::InsertDisk2:
            InsertDisk(*pFloppy2);
            break;

        case Action::EjectDisk2:
            EjectDisk(*pFloppy2);
            break;

        case Action::NewDisk1:
            if (ChangesSaved(*pFloppy1))
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 1);
            break;

        case Action::NewDisk2:
            if (ChangesSaved(*pFloppy2))
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 2);
            break;

#ifdef HAVE_LIBSPECTRUM
        case Action::InsertTape:
            InsertTape(g_hwnd);
            break;

        case Action::TapeBrowser:
            DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_TAPE_BROWSER), g_hwnd, TapeBrowseDlgProc, 2);
            break;
#endif
        case Action::ImportData:
            DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_IMPORT), g_hwnd, ImportExportDlgProc, 1);
            break;

        case Action::ExportData:
            DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_EXPORT), g_hwnd, ImportExportDlgProc, 0);
            break;

        case Action::Options:
            if (!GUI::IsActive())
                DisplayOptions();
            break;

        case Action::ExitApp:
            PostMessage(g_hwnd, WM_CLOSE, 0, 0L);
            break;

        case Action::Pause:
        {
            // Reverse logic as we've not done the default processing yet
            SetWindowText(g_hwnd, g_fPaused ? WINDOW_CAPTION : WINDOW_CAPTION " - Paused");

            // Perform default processing
            return false;
        }

        case Action::Paste:
        {
            // Open the clipboard, preventing anyone from modifying its contents
            if (OpenClipboard(g_hwnd))
            {
                // Request the content as Unicode text
                HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
                if (hClip != nullptr)
                {
                    LPCWSTR pwsz = reinterpret_cast<LPCWSTR>(GlobalLock(hClip));
                    if (pwsz)
                    {
                        int nSizeWide = lstrlenW(pwsz);
                        LPWSTR pwsz2 = new WCHAR[nSizeWide * 3]; // <= 3 chars transliterated
                        LPWSTR pw = pwsz2;

                        // Count the number or Cyrillic characters in a block, and the number of those capitalised
                        int nCyrillic = 0, nCaps = 0;

                        // Process character by character
                        for (WCHAR wch; wch = *pwsz; pwsz++)
                        {
                            // Cyrillic character?
                            bool fCyrillic = wch == 0x0401 || wch >= 0x0410 && wch <= 0x044f || wch == 0x0451;
                            nCyrillic = fCyrillic ? nCyrillic + 1 : 0;

                            // Map GBP and (c) directly to SAM codes
                            if (wch == 0x00a3)  // GBP
                                *pw++ = 0x60;
                            else if (wch == 0x00a9) // (c)
                                *pw++ = 0x7f;

                            // Cyrillic?
                            else if (fCyrillic)
                            {
                                // Determine XOR value to preserve case of the source character
                                char chCase = ~(wch - 0x03d0) & 0x20;
                                nCaps = chCase ? nCaps + 1 : 0;

                                // Is the next character Cyrillic too?
                                bool fCyrillic1 = (pwsz[1] == 0x0401 || pwsz[1] >= 0x0410 && pwsz[1] <= 0x044f || pwsz[1] == 0x0451);

                                // If the next character is Cyrillic, match the case for any extra translit letters
                                // Otherwise if >1 character and all capitals so far, continue as capitals
                                char chCase1 = fCyrillic1 ? (~(pwsz[1] - 0x03d0) & 0x20) :
                                    (nCyrillic > 1 && nCyrillic == nCaps) ? chCase : 0;

                                // Special-case Cyrillic characters not in the main range
                                if (wch == 0x0401)
                                {
                                    *pw++ = 'Y';
                                    *pw++ = 'o' ^ chCase1;
                                }
                                else if (wch == 0x0451)
                                {
                                    *pw++ = 'y';
                                    *pw++ = 'o' ^ chCase1;
                                }
                                else
                                {
                                    // Unicode to transliterated Latin, starting from 0x410
                                    static const char* aszConv[] =
                                    {
                                        "a", "b", "v", "g", "d", "e", "zh", "z",
                                        "i", "j", "k", "l", "m", "n", "o", "p",
                                        "r", "s", "t", "u", "f", "h", "c", "ch",
                                        "sh", "shh", "\"", "y", "'", "e", "yu", "ya"
                                    };

                                    // Look up the transliterated string
                                    const char* psz = aszConv[(wch - 0x0410) & 0x1f];

                                    for (char ch; (ch = *psz); psz++)
                                    {
                                        // Toggle case of alphabetic characters only
                                        if (ch >= 'a' && ch <= 'z')
                                            *pw++ = ch ^ chCase;
                                        else
                                            *pw++ = ch;

                                        // For the remaining characters, use the case of the next character
                                        chCase = chCase1;
                                    }
                                }
                            }
                            else
                            {
                                // Copy anything else as-is
                                *pw++ = wch;
                            }
                        }

                        // Terminate the new string
                        *pw++ = 0;

                        // Convert to US-ASCII, stripping diacritic marks as we go
                        int nSize = WideCharToMultiByte(CP_ACP, 0, pwsz2, -1, nullptr, 0, nullptr, nullptr);
                        char* pcsz = new char[nSize + 1];
                        nSize = WideCharToMultiByte(20127, 0, pwsz2, -1, pcsz, nSize, nullptr, nullptr);

                        // Type the string
                        Keyin::String(pcsz);

                        // Clean up
                        delete[] pcsz;
                        delete[] pwsz2;
                        GlobalUnlock(hClip);
                    }
                }

                CloseClipboard();
            }
            break;
        }

        // Not processed
        default:
            return false;
        }
    }

    // Key released (no processing needed)
    else
        return false;

    // Action processed
    return true;
}


void CentreWindow(HWND hwnd_, HWND hwndParent_/*=nullptr*/)
{
    // If a window isn't specified get the parent window
    if ((hwndParent_ && IsIconic(hwndParent_)) || (!hwndParent_ && !(hwndParent_ = GetParent(hwnd_))))
        hwndParent_ = GetDesktopWindow();

    // Get the rectangles of the window to be centred and the parent window
    RECT rWindow, rParent;
    GetWindowRect(hwnd_, &rWindow);
    GetWindowRect(hwndParent_, &rParent);

    // Work out the new position of the window
    int nX = rParent.left + ((rParent.right - rParent.left) - (rWindow.right - rWindow.left)) / 2;
    int nY = rParent.top + ((rParent.bottom - rParent.top) - (rWindow.bottom - rWindow.top)) * 5 / 12;

    // Move the window to its new position
    SetWindowPos(hwnd_, nullptr, nX, nY, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
}


LRESULT CALLBACK URLWndProc(HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HCURSOR hHand = LoadCursor(nullptr, MAKEINTRESOURCE(32649));    // IDC_HAND, which may not be available

    // Cursor query with a valid hand cursor?
    if (uMsg_ == WM_SETCURSOR && hHand)
    {
        // Set the hand
        SetCursor(hHand);
        return TRUE;
    }

    // Pass unhandled messages to the old handler
    return CallWindowProc(pfnStaticWndProc, hwnd_, uMsg_, wParam_, lParam_);
}

INT_PTR CALLBACK AboutDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HFONT hfont;
    static HWND hwndURL;

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        // Append extra details to the version string
        auto version = GetDlgItemText(hdlg_, IDS_VERSION);
#ifdef _WIN64
        version += " x64";
#endif
        SetDlgItemText(hdlg_, IDS_VERSION, version);

        // Grab the attributes of the current GUI font
        LOGFONT lf;
        GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

        // Add underline, and create it as a font to use for URLs
        lf.lfUnderline = TRUE;
        hfont = CreateFontIndirect(&lf);

        // Fetch the URL handle for later, and set the underline font
        hwndURL = GetDlgItem(hdlg_, ID_HOMEPAGE);
        SendMessage(hwndURL, WM_SETFONT, reinterpret_cast<WPARAM>(hfont), 0L);

        // Subclass the static URL control, so we can set a custom cursor
#ifdef _WIN64
        pfnStaticWndProc = (WNDPROC)(LONG_PTR)SetWindowLongPtr(hwndURL, GWLP_WNDPROC, (LONG_PTR)URLWndProc);
#else
        pfnStaticWndProc = (WNDPROC)(LONG_PTR)SetWindowLongPtr(hwndURL, GWLP_WNDPROC, (LONG)(LONG_PTR)URLWndProc);
#endif

        CentreWindow(hdlg_);
        return 1;
    }

    case WM_DESTROY:
        if (hfont)
        {
            DeleteObject(hfont);
            hfont = nullptr;
        }
        break;

    case WM_CTLCOLORSTATIC:
        // Make the text blue if it's the URL
        if (hwndURL == reinterpret_cast<HWND>(lParam_))
            SetTextColor(reinterpret_cast<HDC>(wParam_), RGB(0, 0, 255));

        // Fall through...

    case WM_CTLCOLORDLG:
        // Force a white background on the dialog (and statics, from above)
        return (BOOL)(LONG_PTR)(GetStockObject(WHITE_BRUSH));

    case WM_COMMAND:
        // Esc or the X closes the dialog
        if (wParam_ == IDCANCEL)
            EndDialog(hdlg_, 0);

        // Clicking the URL launches the homepage in the default browser
        else if (wParam_ == ID_HOMEPAGE)
        {
            char szURL[128];
            GetDlgItemText(hdlg_, ID_HOMEPAGE, szURL, sizeof(szURL));

            if (ShellExecute(nullptr, nullptr, szURL, nullptr, "", SW_SHOWMAXIMIZED) <= reinterpret_cast<HINSTANCE>(32))
                Message(MsgType::Warning, "Failed to launch SimCoupe homepage");
            break;
        }
        break;
    }

    return 0;
}


// Mapping from menu ID to the zero-based image to use in the IDT_MENU toolbar
static MENUICON aMenuIcons[] =
{
    { IDM_FILE_NEW_DISK1, 0 },
    { IDM_FILE_FLOPPY1_INSERT, 1},
    { IDM_FILE_FLOPPY1_SAVE_CHANGES, 2 },
    { IDM_FILE_NEW_DISK2, 0 },
    { IDM_FILE_FLOPPY2_INSERT, 1},
    { IDM_FILE_FLOPPY2_SAVE_CHANGES, 2 },
    { IDM_HELP_ABOUT, 4 },
    { IDM_TOOLS_OPTIONS, 6},
    { IDM_SYSTEM_RESET, 7}
};


// Hook function for catching the Windows key
LRESULT CALLBACK WinKeyHookProc(int nCode_, WPARAM wParam_, LPARAM lParam_)
{
    // Is this a full-screen Windows key press?
    if (nCode_ >= 0 && GetOption(fullscreen) && lParam_ >= 0 && (wParam_ == VK_LWIN || wParam_ == VK_RWIN))
    {
        // Press control, release control, release Windows
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(LOBYTE(wParam_), 0, KEYEVENTF_KEYUP, 0);
        return 0;
    }

    return CallNextHookEx(hWinKeyHook, nCode_, wParam_, lParam_);
}


// Handle messages for the main window
LRESULT CALLBACK WindowProc(HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static bool fInMenu = false, fHideCursor = false;
    static UINT_PTR ulMouseTimer = 0;

    static OwnerDrawnMenu odmenu(nullptr, IDT_MENU, aMenuIcons);

    LRESULT lResult;
    if (odmenu.WindowProc(hwnd_, uMsg_, wParam_, lParam_, &lResult))
        return lResult;

    // Input module has first go at processing any messages
    if (Input::FilterMessage(hwnd_, uMsg_, wParam_, lParam_))
        return 0;

    switch (uMsg_)
    {
        // Main window being created
    case WM_CREATE:
        // Allow files to be dropped onto the main window if the first drive is present
        DragAcceptFiles(hwnd_, GetOption(drive1) == drvFloppy);

        // Hook keyboard input to our thread
        hWinKeyHook = SetWindowsHookEx(WH_KEYBOARD, WinKeyHookProc, nullptr, GetCurrentThreadId());
        return 0;

        // Application close request
    case WM_CLOSE:
        // Ensure both drives are saved before we exit
        if (!ChangesSaved(*pFloppy1) || !ChangesSaved(*pFloppy2))
            return 0;

        UnhookWindowsHookEx(hWinKeyHook), hWinKeyHook = nullptr;

        PostQuitMessage(0);
        return 0;

        // System shutting down or using logging out
    case WM_QUERYENDSESSION:
        // Save without prompting, to avoid data loss
        if (pFloppy1) pFloppy1->Save();
        if (pFloppy2) pFloppy2->Save();
        return TRUE;

    case WM_GETMINMAXINFO:
    {
        RECT rect{ 0, 0, Frame::Width() / 2, Frame::Height() / 2 };
        AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));

        auto pMMI = reinterpret_cast<MINMAXINFO*>(lParam_);
        pMMI->ptMinTrackSize.x = rect.right - rect.left;
        pMMI->ptMinTrackSize.y = rect.bottom - rect.top;
        return 0;
    }

    // File has been dropped on our window
    case WM_DROPFILES:
    {
        char szFile[MAX_PATH]{};
        if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof(szFile)))
        {
            auto file_path = szFile;
            if (Tape::IsRecognised(file_path))
            {
                InsertTape(hwnd_, file_path);
            }
            else
            {
                InsertDisk(*pFloppy1, file_path);
            }

            SetForegroundWindow(hwnd_);
        }

        return 0;
    }


    // Mouse-hide timer has expired
    case WM_TIMER:
        // Make sure the timer is ours
        if (wParam_ != MOUSE_TIMER_ID)
            break;

        // Kill the timer, and flag the mouse as hidden
        KillTimer(hwnd_, MOUSE_TIMER_ID);
        ulMouseTimer = 0;
        fHideCursor = true;

        // Generate a WM_SETCURSOR to update the cursor state
        POINT pt;
        GetCursorPos(&pt);
        SetCursorPos(pt.x, pt.y);
        return 0;

    case WM_SETCURSOR:
        // Hide the cursor unless it's being used for the Win32 GUI or the emulation using using it in windowed mode
        if (fHideCursor || Input::IsMouseAcquired())
        {
            // Only hide the cursor over the client area
            if (LOWORD(lParam_) == HTCLIENT)
            {
                SetCursor(nullptr);
                return TRUE;
            }
        }
        break;

        // Mouse has been moved
    case WM_MOUSEMOVE:
    {
        static POINT ptLast;
        POINT pt = { GET_X_LPARAM(lParam_), GET_Y_LPARAM(lParam_) };
        ClientToScreen(hwnd_, &pt);

        if ((pt.x != ptLast.x || pt.y != ptLast.y) && !Input::IsMouseAcquired())
        {
            fHideCursor = false;
            ulMouseTimer = SetTimer(g_hwnd, MOUSE_TIMER_ID, MOUSE_HIDE_TIME, nullptr);
            ptLast = pt;
        }

        return 0;
    }

    case WM_INITMENU:
        UpdateMenuFromOptions();
        break;

    case WM_ENTERMENULOOP:
        fInMenu = true;
        break;

    case WM_EXITMENULOOP:
        // No longer in menu, so start timer to hide the mouse if not used again
        fInMenu = fHideCursor = false;
        ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, 1, nullptr);

        // Purge any menu navigation key presses
        Input::Purge();
        break;


    case WM_ERASEBKGND:
        return 1;

    case WM_SYSCOMMAND:
        // Is this an Alt-key combination?
        if ((wParam_ & 0xfff0) == SC_KEYMENU)
        {
            // Ignore the key if Ctrl is pressed, to avoid Win9x problems with AltGr activating the menu
            if ((GetAsyncKeyState(VK_CONTROL) < 0) || GetAsyncKeyState(VK_RMENU) < 0)
                return 0;

            // Stop Windows processing SAM Cntrl-key combinations (if enabled) and Alt-Enter
            // As well as blocking access to the menu it avoids a beep for the unhandled ones (mainly Alt-Enter)
            if ((GetOption(altforcntrl) && lParam_) || lParam_ == VK_RETURN)
                return 0;

            // Alt-0 to Alt-9 set the zoom level
            if (lParam_ >= '0' && lParam_ <= '9')
                return SendMessage(hwnd_, WM_COMMAND, IDM_VIEW_ZOOM_50 + lParam_ - '0', 0L);
        }
        // Maximize?
        else if (wParam_ == SC_MAXIMIZE)
        {
            // Shift pressed?
            if (GetAsyncKeyState(VK_SHIFT) < 0)
            {
                // Toggle fullscreen instead
                Actions::Do(Action::ToggleFullscreen);
                return 0;
            }
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:

        // Forward the function keys on as regular keys instead of a system keys
        if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            return SendMessage(hwnd_, uMsg_ - WM_SYSKEYDOWN + WM_KEYDOWN, wParam_, lParam_);

        // Alt-Return is used to toggle full-screen (ignore key repeats)
        else if (uMsg_ == WM_SYSKEYDOWN && wParam_ == VK_RETURN && (lParam_ & 0x60000000) == 0x20000000)
            Actions::Do(Action::ToggleFullscreen);

        break;

    case WM_KEYUP:
    case WM_KEYDOWN:
    {
        bool fPress = (uMsg_ == WM_KEYDOWN);

        // If the keyboard is used, simulate early timer expiry to hide the cursor
        if (fPress && ulMouseTimer)
            ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, 1, nullptr);

        // Unpause on key-down so the user doesn't think we've hung
        if (fPress && g_fPaused && wParam_ != VK_PAUSE)
            Actions::Do(Action::Pause);

        // Read the current states of the shift keys
        bool fCtrl = GetAsyncKeyState(VK_CONTROL) < 0;
        bool fAlt = GetAsyncKeyState(VK_MENU) < 0;
        bool fShift = GetAsyncKeyState(VK_SHIFT) < 0;

        // Function key?
        if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
        {
            // Ignore Windows-modified function keys unless the SAM keypad mapping is enabled
            if ((GetAsyncKeyState(VK_LWIN) < 0 || GetAsyncKeyState(VK_RWIN) < 0) && wParam_ <= VK_F10)
                return 0;

            Actions::Key((int)wParam_ - VK_F1 + 1, fPress, fCtrl, fAlt, fShift);
            return 0;
        }

        // Most of the emulator keys are handled above, but we've a few extra fixed mappings of our own (well, mine!)
        switch (wParam_)
        {
        case VK_DIVIDE:     Actions::Do(Action::Debugger, fPress); break;
        case VK_MULTIPLY:   Actions::Do(fCtrl ? Action::Reset : Action::SpeedTurbo, fPress); break;
        case VK_ADD:        Actions::Do(fCtrl ? Action::SpeedTurbo : Action::SpeedFaster, fPress); break;
        case VK_SUBTRACT:   Actions::Do(fCtrl ? Action::SpeedNormal : Action::SpeedSlower, fPress); break;

        case VK_CANCEL:
        case VK_PAUSE:
            if (fPress)
            {
                // Ctrl-Break is used for reset
                if (GetAsyncKeyState(VK_CONTROL) < 0)
                    CPU::Init();

                // Pause toggles pause mode
                else
                    Actions::Do(Action::Pause);
            }
            break;

        case VK_SNAPSHOT:
        case VK_SCROLL:
            if (!fPress)
                Actions::Do(Action::SavePNG);
            break;

            // Use the default behaviour for anything we're not using
        default:
            return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
        }

        // We processed the key
        return 0;
    }
    break;

    // Menu and commands
    case WM_COMMAND:
    {
        WORD wId = LOWORD(wParam_);

        // If Shift is held, use the built-in versions of some dialogs
        if (GetAsyncKeyState(VK_SHIFT) < 0)
        {
            switch (wId)
            {
            case IDM_FILE_IMPORT_DATA:      GUI::Start(new ImportDialog);      return 0;
            case IDM_FILE_EXPORT_DATA:      GUI::Start(new ExportDialog);      return 0;
            case IDM_FILE_FLOPPY1_INSERT:   GUI::Start(new BrowseFloppy(1));   return 0;
            case IDM_FILE_FLOPPY2_INSERT:   GUI::Start(new BrowseFloppy(2));   return 0;
            case IDM_TOOLS_OPTIONS:         GUI::Start(new OptionsDialog);     return 0;
            case IDM_HELP_ABOUT:            GUI::Start(new AboutDialog);       return 0;

            case IDM_FILE_NEW_DISK1:        GUI::Start(new NewDiskDialog(1));  return 0;
            case IDM_FILE_NEW_DISK2:        GUI::Start(new NewDiskDialog(2));  return 0;
            case IDM_TOOLS_TAPE_BROWSER:    GUI::Start(new BrowseTape());      return 0; // no tape browser yet
            }
        }

        switch (wId)
        {
        case IDM_FILE_NEW_DISK1:        Actions::Do(Action::NewDisk1);          break;
        case IDM_FILE_NEW_DISK2:        Actions::Do(Action::NewDisk2);          break;
        case IDM_FILE_IMPORT_DATA:      Actions::Do(Action::ImportData);        break;
        case IDM_FILE_EXPORT_DATA:      Actions::Do(Action::ExportData);        break;
        case IDM_FILE_EXIT:             Actions::Do(Action::ExitApp);   break;

        case IDM_RECORD_AVI_START:      Actions::Do(Action::RecordAvi);         break;
        case IDM_RECORD_AVI_HALF:       Actions::Do(Action::RecordAviHalf);     break;
        case IDM_RECORD_AVI_STOP:       Actions::Do(Action::RecordAviStop);     break;

        case IDM_RECORD_GIF_START:      Actions::Do(Action::RecordGif);         break;
        case IDM_RECORD_GIF_LOOP:       Actions::Do(Action::RecordGifLoop);     break;
        case IDM_RECORD_GIF_STOP:       Actions::Do(Action::RecordGifStop);     break;

        case IDM_RECORD_WAV_START:      Actions::Do(Action::RecordWav);         break;
        case IDM_RECORD_WAV_SEGMENT:    Actions::Do(Action::RecordWavSegment);  break;
        case IDM_RECORD_WAV_STOP:       Actions::Do(Action::RecordWavStop);     break;

        case IDM_RECORD_SCREEN_PNG:     Actions::Do(Action::SavePNG);           break;
        case IDM_RECORD_SCREEN_SSX:     Actions::Do(Action::SaveSSX);           break;

        case IDM_TOOLS_OPTIONS:         Actions::Do(Action::Options);           break;
        case IDM_TOOLS_PASTE_CLIPBOARD: Actions::Do(Action::Paste);             break;
        case IDM_TOOLS_PRINTER_ONLINE:  Actions::Do(Action::TogglePrinter);     break;
        case IDM_TOOLS_FLUSH_PRINTER:   Actions::Do(Action::FlushPrinter);      break;
        case IDM_TOOLS_TAPE_BROWSER:    Actions::Do(Action::TapeBrowser);       break;
        case IDM_TOOLS_DEBUGGER:        Actions::Do(Action::Debugger);          break;
        case IDM_TOOLS_RASTER_DEBUG:    Actions::Do(Action::ToggleRasterDebug); break;

        case IDM_FILE_FLOPPY1_DEVICE:
            if (!FloppyStream::IsAvailable())
            {
                if (MessageBox(hwnd_, "Real floppy disk support requires a 3rd party driver.\n"
                    "\n"
                    "Visit the website to to download it?",
                    "fdrawcmd.sys not found",
                    MB_ICONQUESTION | MB_YESNO) == IDYES)
                    ShellExecute(nullptr, nullptr, "http://simonowen.com/fdrawcmd/", nullptr, "", SW_SHOWMAXIMIZED);
            }
            else if (GetOption(drive1) == drvFloppy && ChangesSaved(*pFloppy1) && pFloppy1->Insert("A:"))
                Frame::SetStatus("Using floppy drive {}", pFloppy1->DiskFile());
            break;

        case IDM_FILE_FLOPPY1_INSERT:       Actions::Do(Action::InsertDisk1); break;
        case IDM_FILE_FLOPPY1_EJECT:        Actions::Do(Action::EjectDisk1);  break;
        case IDM_FILE_FLOPPY1_SAVE_CHANGES: Actions::Do(Action::SaveDisk1);   break;

        case IDM_FILE_FLOPPY2_INSERT:       Actions::Do(Action::InsertDisk2); break;
        case IDM_FILE_FLOPPY2_EJECT:        Actions::Do(Action::EjectDisk2);  break;
        case IDM_FILE_FLOPPY2_SAVE_CHANGES: Actions::Do(Action::SaveDisk2);   break;

        case IDM_VIEW_FULLSCREEN:           Actions::Do(Action::ToggleFullscreen); break;
        case IDM_VIEW_TVASPECT:             Actions::Do(Action::ToggleTV);        break;
        case IDM_VIEW_SMOOTH:               Actions::Do(Action::ToggleSmoothing);  break;
        case IDM_VIEW_MOTIONBLUR:           Actions::Do(Action::ToggleMotionBlur); break;

        case IDM_VIEW_ZOOM_50:
        case IDM_VIEW_ZOOM_100:
        case IDM_VIEW_ZOOM_150:
        case IDM_VIEW_ZOOM_200:
        case IDM_VIEW_ZOOM_250:
        case IDM_VIEW_ZOOM_300:
        case IDM_VIEW_ZOOM_350:
        case IDM_VIEW_ZOOM_400:
        case IDM_VIEW_ZOOM_450:
        case IDM_VIEW_ZOOM_500:
        {
            auto scale_2x = wId - IDM_VIEW_ZOOM_50 + 1;
            Video::ResizeWindow(Frame::Height()* scale_2x / 2);
            break;
        }

        case IDM_VIEW_BORDERS0:
        case IDM_VIEW_BORDERS1:
        case IDM_VIEW_BORDERS2:
        case IDM_VIEW_BORDERS3:
        case IDM_VIEW_BORDERS4:
            SetOption(borders, wId - IDM_VIEW_BORDERS0);
            Frame::Init();
            break;

        case IDM_SYSTEM_PAUSE:      Actions::Do(Action::Pause);           break;
        case IDM_SYSTEM_NMI:        Actions::Do(Action::Nmi);       break;
        case IDM_SYSTEM_RESET:      Actions::Do(Action::Reset); Actions::Do(Action::Reset, false); break;

        case IDM_HELP_GENERAL:
        {
            auto help_path = OSD::MakeFilePath(PathType::Resource, "ReadMe.md");
            if (fs::exists(help_path))
                ShellExecute(hwnd_, nullptr, "notepad.exe", help_path.c_str(), "", SW_SHOWNORMAL);
            else
                Message(MsgType::Warning, "Help not found:\n\n{}", help_path.string());
            break;
        }

        case IDM_HELP_ABOUT:    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd_, AboutDlgProc, 0);   break;


        case IDM_SYSTEM_SPEED_50:
        case IDM_SYSTEM_SPEED_100:
        case IDM_SYSTEM_SPEED_200:
        case IDM_SYSTEM_SPEED_300:
        case IDM_SYSTEM_SPEED_500:
        case IDM_SYSTEM_SPEED_1000:
        {
            static const std::vector<int> speeds{ 50, 100, 200, 300, 500, 1000 };
            SetOption(speed, speeds[wId - IDM_SYSTEM_SPEED_50]);
            Frame::SetStatus("{}% Speed", GetOption(speed));
            break;
        }


        default:
            if ((wId >= IDM_FILE_RECENT1 && wId <= IDM_FILE_RECENT9) ||
                (wId >= IDM_FLOPPY2_RECENT1 && wId <= IDM_FLOPPY2_RECENT9))
            {
                auto drive1 = (wId <= IDM_FILE_RECENT9);
                auto file_index = wId - (drive1 ? IDM_FILE_RECENT1 : IDM_FLOPPY2_RECENT1);
                auto file_path = recent_files[file_index];

                if (Tape::IsRecognised(file_path))
                {
                    InsertTape(hwnd_, file_path);
                }
                else
                {
                    InsertDisk(drive1 ? *pFloppy1 : *pFloppy2, file_path);
                }
            }
            break;
        }
        break;
    }
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}


void SaveWindowPosition(HWND hwnd_)
{
    bool fullscreen = GetOption(fullscreen);
    if (fullscreen)
        Actions::Do(Action::ToggleFullscreen);

    WINDOWPLACEMENT wp = { sizeof(wp) };
    GetWindowPlacement(hwnd_, &wp);

    auto& rect = wp.rcNormalPosition;
    SetOption(windowpos,
        fmt::format("{},{},{},{},{}",
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            (wp.showCmd == SW_SHOWMAXIMIZED) ? 1 : 0));

    SetOption(fullscreen, fullscreen);
}

bool RestoreWindowPosition(HWND hwnd_)
{
    int x, y, width, height, maximised;
    if (sscanf(GetOption(windowpos).c_str(), "%d,%d,%d,%d,%d", &x, &y, &width, &height, &maximised) != 5)
        return false;

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    SetRect(&wp.rcNormalPosition, x, y, x + width, y + height);
    wp.showCmd = maximised ? SW_MAXIMIZE : GetOption(fullscreen) ? SW_HIDE : SW_SHOW;
    SetWindowPlacement(hwnd_, &wp);

    return true;
}


bool InitWindow()
{
    // Set up and register window class
    WNDCLASS wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = __hinstance;
    wc.hIcon = LoadIcon(__hinstance, MAKEINTRESOURCE(IDI_MAIN));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SimCoupeClass";
    RegisterClass(&wc);

    g_hmenu = LoadMenu(wc.hInstance, MAKEINTRESOURCE(IDR_MENU));

    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    auto width = static_cast<int>(std::round(Frame::Width() * 3 * aspect_ratio / 2));
    auto height = Frame::Height() * 3 / 2;

    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) * 5 / 12;

    g_hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, WINDOW_CAPTION, WS_OVERLAPPEDWINDOW,
        x, y, width, height, nullptr, g_hmenu, wc.hInstance, nullptr);
    if (!g_hwnd)
        return false;

    // Restore the window position, falling back on the current options to determine its size
    if (!RestoreWindowPosition(g_hwnd))
    {
        RECT rect{ 0, 0, width, height };
        AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));
        SetWindowPos(g_hwnd, HWND_TOP,
            0, 0, rect.right - rect.left, rect.bottom - rect.top,
            SWP_SHOWWINDOW | SWP_NOMOVE);
    }

    if (GetOption(fullscreen))
    {
        SetOption(fullscreen, false);
        Actions::Do(Action::ToggleFullscreen);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath(char* pszPath_, size_t nLen_)
{
    char* psz1 = nullptr, * psz2 = nullptr;

    // Accept regular and UNC paths only
    if (lstrlen(pszPath_) < 3)
        return;
    else if (!memcmp(pszPath_ + 1, ":\\", 2))
        psz1 = pszPath_ + 2;
    else if (memcmp(pszPath_, "\\\\", 2))
        return;
    else
    {
        for (psz1 = pszPath_ + 2; *psz1 && *psz1 != '\\'; psz1++);
        for (psz1++; *psz1 && *psz1 != '\\'; psz1++);

        if (!*psz1)
            return;
    }

    // Adjust the length for the prefix we skipped
    nLen_ -= (psz1 - pszPath_);

    // Search the rest of the path string
    for (char* p = psz1; *p; p = CharNext(p))
    {
        // Keep going until we find a backslash
        if (*p != '\\')
            continue;

        // Remember the position for a possible clipping location, and
        // if we have room for the remaining segment, stop now
        else if (strlen(psz2 = p) <= nLen_)
            break;
    }

    // Do we have a clip segment longer than the ellipsis overhead?
    if (psz2 && (psz2 - psz1) > 4)
    {
        // Clip and join the remaining sections
        lstrcpy(psz1 + 1, "...");
        lstrcpy(psz1 + 4, psz2);
    }
}

// Helper function for filling a combo-box with strings and selecting one
void SetComboStrings(HWND hdlg_, UINT uID_, const std::vector<std::string>& strings, int nDefault_/*=0*/)
{
    auto hwndCombo = GetDlgItem(hdlg_, uID_);
    ComboBox_ResetContent(hwndCombo);

    for (auto& str : strings)
        ComboBox_AddString(hwndCombo, str.c_str());

    ComboBox_SetCurSel(hwndCombo, (nDefault_ == -1) ? 0 : nDefault_);
}

std::string GetComboText(HWND hdlg, UINT ctrl_id)
{
    auto hwndCombo = GetDlgItem(hdlg, ctrl_id);
    auto index = ComboBox_GetCurSel(hwndCombo);

    auto len = ComboBox_GetLBTextLen(hwndCombo, index);
    std::vector<char> text(len + 1);
    ComboBox_GetLBText(hwndCombo, index, text.data());

    auto str = text.data();
    return (str == "None" || str == "<None>") ? "" : str;
}

void AddComboString(HWND hdlg, UINT ctrl_id, const char* pcsz_)
{
    ComboBox_AddString(GetDlgItem(hdlg, ctrl_id), pcsz_);
}

void AddComboExString(HWND hdlg_, UINT uID_, const char* pcsz_)
{
    COMBOBOXEXITEM cbei{};
    cbei.iItem = -1;
    cbei.mask = CBEIF_TEXT;
    cbei.pszText = const_cast<char*>(pcsz_);

    SendDlgItemMessage(hdlg_, uID_, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&cbei));
}


void FillMidiOutCombo(HWND hwndCombo)
{
    ComboBox_ResetContent(hwndCombo);

    int nDevs = midiOutGetNumDevs();
    ComboBox_AddString(hwndCombo, nDevs ? "None" : "<None>");

    for (int i = 0; i < nDevs; i++)
    {
        MIDIOUTCAPS mc{};
        if (midiOutGetDevCaps(i, &mc, sizeof(mc)) == MMSYSERR_NOERROR)
            ComboBox_AddString(hwndCombo, mc.szPname);
    }

    if (ComboBox_SelectString(hwndCombo, -1, GetOption(midioutdev).c_str()) == CB_ERR)
        ComboBox_SetCurSel(hwndCombo, 0);

    ComboBox_Enable(hwndCombo, nDevs != 0);
}


void FillPrintersCombo(HWND hwndCombo)
{
    ComboBox_ResetContent(hwndCombo);
    ComboBox_AddString(hwndCombo, "File: simcNNNN.txt (auto-generated)");
    ComboBox_SetCurSel(hwndCombo, 0);
}


void FillJoystickCombo(HWND hwndCombo, const std::string& selected)
{
    ComboBox_ResetContent(hwndCombo);
    Input::FillJoystickCombo(hwndCombo);

    auto num_items = ComboBox_GetCount(hwndCombo);
    ComboBox_Enable(hwndCombo, num_items != 0);
    ComboBox_InsertString(hwndCombo, 0, num_items ? "None" : "<None>");

    if (ComboBox_SelectString(hwndCombo, -1, selected.c_str()) == CB_ERR)
        ComboBox_SetCurSel(hwndCombo, 0);
}


////////////////////////////////////////////////////////////////////////////////

// Browse for an image, setting a specified filter with the path selected
void BrowseImage(HWND hdlg_, int nControl_, const char* pcszFilters_)
{
    char szFile[MAX_PATH]{};
    auto file_path = GetDlgItemText(hdlg_, nControl_);
    std::copy(file_path.begin(), file_path.end(), szFile);

    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = hdlg_;
    ofn.lpstrFilter = pcszFilters_;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = 0;

    if (GetSaveLoadFile(&ofn, true))
    {
        SetDlgItemText(hdlg_, nControl_, szFile);
    }
}

BOOL BadField(HWND hdlg_, int nId_)
{
    HWND hctrl = GetDlgItem(hdlg_, nId_);
    Edit_SetSel(hctrl, 0, -1);
    SetFocus(hctrl);
    MessageBeep(MB_ICONHAND);
    return FALSE;
}


INT_PTR CALLBACK ImportExportDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static char szFile[MAX_PATH], szAddress[128] = "32768", szPage[128] = "1", szOffset[128] = "0", szLength[128] = "16384";
    static int nType = 0;
    static bool fImport;
    static POINT apt[2];

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        CentreWindow(hdlg_);
        fImport = !!lParam_;

        static const std::vector<std::string> types{ "BASIC Address (0-540671)", "Main Memory (pages 0-31)", "External RAM (pages 0-255)" };
        SetComboStrings(hdlg_, IDC_TYPE, types, nType);

        SendMessage(hdlg_, WM_COMMAND, IDC_TYPE, 0);
        SetDlgItemText(hdlg_, IDE_ADDRESS, szAddress);
        SetDlgItemText(hdlg_, IDE_PAGE, szPage);
        SetDlgItemText(hdlg_, IDE_OFFSET, szOffset);
        SetDlgItemText(hdlg_, IDE_LENGTH, szLength);

        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD wControl = LOWORD(wParam_);
        bool fChange = HIWORD(wParam_) == EN_CHANGE;

        switch (wControl)
        {
        case IDCANCEL:
            EndDialog(hdlg_, 0);
            return TRUE;

        case IDC_TYPE:
        {
            auto type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_TYPE));

            for (auto id : { IDS_ADDRESS, IDE_ADDRESS, IDS_LENGTH2, IDE_LENGTH2 })
                ShowWindow(GetDlgItem(hdlg_, id), !type ? SW_SHOW : SW_HIDE);

            for (auto id : { IDS_PAGE, IDE_PAGE, IDS_OFFSET, IDE_OFFSET, IDS_LENGTH, IDE_LENGTH })
                ShowWindow(GetDlgItem(hdlg_, id), type ? SW_SHOW : SW_HIDE);

            break;
        }

        case IDE_LENGTH:
        case IDE_LENGTH2:
        {
            static bool fUpdating;

            if (fChange && !fUpdating)
            {
                fUpdating = true;
                char sz[256];
                GetDlgItemText(hdlg_, wControl, sz, sizeof(sz));
                SetDlgItemText(hdlg_, wControl ^ IDE_LENGTH ^ IDE_LENGTH2, sz);
                fUpdating = false;
            }
            break;
        }

        case IDOK:
        {
            GetDlgItemText(hdlg_, IDE_ADDRESS, szAddress, sizeof(szAddress));
            GetDlgItemText(hdlg_, IDE_PAGE, szPage, sizeof(szPage));
            GetDlgItemText(hdlg_, IDE_OFFSET, szOffset, sizeof(szOffset));
            GetDlgItemText(hdlg_, IDE_LENGTH, szLength, sizeof(szLength));

            nType = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_TYPE));
            int nAddress = GetDlgItemValue(hdlg_, IDE_ADDRESS);
            int nPage = GetDlgItemValue(hdlg_, IDE_PAGE);
            int nOffset = GetDlgItemValue(hdlg_, IDE_OFFSET);
            int nLength = GetDlgItemValue(hdlg_, IDE_LENGTH);

            // Allow offset to span pages
            if (nType && nOffset > 16384)
            {
                nPage += nOffset / 16384;
                nOffset &= 0x3fff;
            }

            if (!nType && nAddress < 0 || nAddress > 540671)
                return BadField(hdlg_, IDE_ADDRESS);
            else if (nType == 1 && nPage < 0 || nPage > 31 || nType == 2 && nPage > 255)
                return BadField(hdlg_, IDE_PAGE);
            else if (nType && nOffset < 0 || nOffset > 16384)
                return BadField(hdlg_, IDE_OFFSET);
            else if (!fImport && nLength <= 0)
                return BadField(hdlg_, IDE_LENGTH);

            static OPENFILENAME ofn = { sizeof(ofn) };
            ofn.hwndOwner = hdlg_;
            ofn.lpstrFilter = szDataFilters;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.Flags = OFN_HIDEREADONLY;

            if (!GetSaveLoadFile(&ofn, fImport))
            {
                EndDialog(hdlg_, 0);
                return TRUE;
            }

            unique_FILE file = fopen(szFile, fImport ? "rb" : "wb");
            if (!file)
            {
                MessageBox(hdlg_, "Failed to open file", fImport ? "Import" : "Export", MB_ICONEXCLAMATION);
                EndDialog(hdlg_, 0);
                return TRUE;
            }

            if (!nType)
            {
                nPage = (nAddress < 0x4000) ? ROM0 : (nAddress - 0x4000) / 0x4000;
                nOffset = nAddress & 0x3fff;
            }
            else if (nType == 1)
                nPage &= 0x1f;
            else
                nPage += EXTMEM;

            if (fImport) nLength = 0x400000;    // 4MB max import
            size_t nDone = 0;

            if (fImport)
            {
                for (int nChunk; (nChunk = std::min(nLength, (0x4000 - nOffset))); nLength -= nChunk, nOffset = 0)
                {
                    nDone += fread(PageWritePtr(nPage++) + nOffset, 1, nChunk, file);

                    // Wrap to page 0 after ROM0
                    if (nPage == ROM0 + 1)
                        nPage = 0;

                    // Stop at the end of the file or if we've hit the end of a logical block
                    if (feof(file) || nPage == EXTMEM || nPage >= ROM0)
                        break;
                }

                Frame::SetStatus("Imported {} bytes", nDone);
            }
            else
            {
                for (int nChunk; (nChunk = std::min(nLength, (0x4000 - nOffset))); nLength -= nChunk, nOffset = 0)
                {
                    nDone += fwrite(PageReadPtr(nPage++) + nOffset, 1, nChunk, file);

                    if (ferror(file))
                    {
                        MessageBox(hdlg_, "Error writing to file", "Export Data", MB_ICONEXCLAMATION);
                        return FALSE;
                    }

                    // Wrap to page 0 after ROM0
                    if (nPage == ROM0 + 1)
                        nPage = 0;

                    // Stop if we've hit the end of a logical block
                    if (nPage == EXTMEM || nPage == ROM0)
                        break;
                }

                Frame::SetStatus("Exported {} bytes", nDone);
            }

            return EndDialog(hdlg_, 1);
        }
        }
    }
    }

    return FALSE;
}


INT_PTR CALLBACK NewDiskDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static int nType, nDrive;
    static bool fCompress, fFormat = true;

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        CentreWindow(hdlg_);

        char sz[32];
        wsprintf(sz, "New Disk %d", nDrive = static_cast<int>(lParam_));
        SetWindowText(hdlg_, sz);

        static const std::vector<std::string> types{ "MGT disk image (800K)", "EDSK disk image (flexible format)", "DOS CP/M image (720K)" };
        SetComboStrings(hdlg_, IDC_TYPES, types, nType);
        SendMessage(hdlg_, WM_COMMAND, IDC_TYPES, 0L);

        Button_SetCheck(GetDlgItem(hdlg_, IDC_FORMAT), fFormat ? BST_CHECKED : BST_UNCHECKED);

#ifdef HAVE_LIBZ
        Button_SetCheck(GetDlgItem(hdlg_, IDC_COMPRESS), fCompress ? BST_CHECKED : BST_UNCHECKED);
#else
        // Disable the compression check-box if zlib isn't available
        EnableWindow(GetDlgItem(hdlg_, IDC_COMPRESS), FALSE);
#endif
        return TRUE;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDCLOSE:
        case IDCANCEL:
            EndDialog(hdlg_, 0);
            return TRUE;

        case IDC_TYPES:
            nType = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_TYPES));

            // Enable the format checkbox for EDSK only
            EnableWindow(GetDlgItem(hdlg_, IDC_FORMAT), nType == 1);

            // Enable formatting for non-EDSK
            if (nType != 1)
                Button_SetCheck(GetDlgItem(hdlg_, IDC_FORMAT), BST_CHECKED);

            // Enable the compress checkbox for MGT only
            EnableWindow(GetDlgItem(hdlg_, IDC_COMPRESS), nType == 0);

            // Disable compression for non-MGT
            if (nType != 0)
                Button_SetCheck(GetDlgItem(hdlg_, IDC_COMPRESS), BST_UNCHECKED);

            break;

        case IDOK:
        {
            // File extensions for each type, plus an additional extension if compressed
            static const char* aszTypes[] = { ".mgt", ".dsk", ".cpm" };

            nType = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_TYPES));
            fCompress = Button_GetCheck(GetDlgItem(hdlg_, IDC_COMPRESS)) == BST_CHECKED;
            fFormat = Button_GetCheck(GetDlgItem(hdlg_, IDC_FORMAT)) == BST_CHECKED;

            char szFile[MAX_PATH] = "Untitled";

            OPENFILENAME ofn = { sizeof(ofn) };
            ofn.hwndOwner = hdlg_;
            ofn.lpstrFilter = szNewDiskFilters;
            ofn.nFilterIndex = nType + 1;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrDefExt = aszTypes[nType];
            ofn.Flags = OFN_HIDEREADONLY;

            if (!GetSaveLoadFile(&ofn, false))
                break;

            // Fetch the file type, in case it's changed
            nType = ofn.nFilterIndex - 1;

            std::unique_ptr<Stream> stream;
            std::unique_ptr<Disk> pDisk;
#ifdef HAVE_LIBZ
            if (nType == 0 && fCompress)
                stream = std::make_unique<ZLibStream>(nullptr, szFile);
            else
#endif
                stream = std::make_unique<FileStream>(nullptr, szFile);

            switch (nType)
            {
            case 0:
                pDisk = std::make_unique<MGTDisk>(std::move(stream));
                break;
            default:
            case 1:
                pDisk = std::make_unique<EDSKDisk>(std::move(stream));
                break;
            case 2:
                pDisk = std::make_unique<MGTDisk>(std::move(stream), DOS_DISK_SECTORS);
                break;
            }

            // Format the EDSK image ready for use?
            if (nType == 1 && fFormat)
            {
                IDFIELD abIDs[NORMAL_DISK_SECTORS];

                // Create a data track to use during the format
                uint8_t abSector[NORMAL_SECTOR_SIZE], * apbData[NORMAL_DISK_SECTORS];
                memset(abSector, 0, sizeof(abSector));

                // Prepare the tracks across the disk
                for (uint8_t head = 0; head < NORMAL_DISK_SIDES; head++)
                {
                    for (uint8_t cyl = 0; cyl < NORMAL_DISK_TRACKS; cyl++)
                    {
                        for (uint8_t sector = 0; sector < NORMAL_DISK_SECTORS; sector++)
                        {
                            abIDs[sector].bTrack = cyl;
                            abIDs[sector].bSide = head;
                            abIDs[sector].bSector = 1 + ((sector + NORMAL_DISK_SECTORS - (cyl % NORMAL_DISK_SECTORS)) % NORMAL_DISK_SECTORS);
                            abIDs[sector].bSize = 2;
                            abIDs[sector].bCRC1 = abIDs[sector].bCRC2 = 0;

                            // Point all data sectors to our reference sector
                            apbData[sector] = abSector;
                        }

                        pDisk->FormatTrack(cyl, head, abIDs, apbData, NORMAL_DISK_SECTORS);
                    }
                }
            }

            // Save the new disk and close it
            bool fSaved = pDisk->Save();
            pDisk.reset();

            // If the save failed, moan about it
            if (!fSaved)
            {
                Message(MsgType::Warning, "Failed to save to {}\n", szFile);
                break;
            }

            auto& pDrive = (nDrive == 1) ? pFloppy1 : pFloppy2;
            InsertDisk(*pDrive, szFile, false);

            EndDialog(hdlg_, 1);
            break;
        }
        }
    }
    }

    return FALSE;
}


INT_PTR CALLBACK HardDiskDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static UINT uSize = 32;
    static HWND hwndEdit;
    static char szFile[MAX_PATH] = "";

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        CentreWindow(hdlg_);

        // Set the previous size, which may be updated by setting the image path below
        SetDlgItemInt(hdlg_, IDE_SIZE, uSize, FALSE);

        // Locate the parent control holding the old path, and to receive the new one
        hwndEdit = reinterpret_cast<HWND>(lParam_);
        GetDlgItemText(hwndEdit, 0, szFile, sizeof(szFile));
        SetDlgItemText(hdlg_, IDE_FILE, szFile);

        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD wControl = LOWORD(wParam_);
        bool fChange = HIWORD(wParam_) == EN_CHANGE;

        switch (wControl)
        {
        case IDCLOSE:
        case IDCANCEL:
            EndDialog(hdlg_, 0);
            return TRUE;

        case IDE_FILE:
        {
            if (!fChange)
                break;

            GetDlgItemText(hdlg_, IDE_FILE, szFile, sizeof(szFile));

            auto disk = HardDisk::OpenObject(szFile);
            if (disk)
            {
                // Fetch the existing disk geometry
                auto pGeom = disk->GetGeometry();
                uSize = (pGeom->uTotalSectors + (1 << 11) - 1) >> 11;
                SetDlgItemInt(hdlg_, IDE_SIZE, uSize, FALSE);
            }

            // The geometry is read-only for existing images
            EnableWindow(GetDlgItem(hdlg_, IDE_SIZE), !disk);

            // Use an OK button to accept an existing file, or Create for a new one
            SetDlgItemText(hdlg_, IDOK, disk || !szFile[0] ? "OK" : "Create");
            EnableWindow(GetDlgItem(hdlg_, IDOK), szFile[0]);

            break;
        }

        case IDB_BROWSE:
        {
            static OPENFILENAME ofn = { sizeof(ofn) };
            ofn.hwndOwner = hdlg_;
            ofn.lpstrFilter = szHDDFilters;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrDefExt = ".hdf";
            ofn.Flags = OFN_HIDEREADONLY;

            if (GetSaveLoadFile(&ofn, true, false))
                SetDlgItemText(hdlg_, IDE_FILE, szFile, true);

            break;
        }

        case IDOK:
        {
            // If new values have been give, create a new disk using the supplied settings
            if (IsWindowEnabled(GetDlgItem(hdlg_, IDE_SIZE)))
            {
                struct stat st;

                // Fetch the new size in MB, and convert to sectors
                uSize = GetDlgItemValue(hdlg_, IDE_SIZE, 0);
                UINT uTotalSectors = uSize << 11;

                // Check the new geometry is within CHS range
                if (!uTotalSectors || (uTotalSectors > (16383 * 16 * 63)))
                {
                    MessageBox(hdlg_, "Invalid disk size", "Create", MB_OK | MB_ICONEXCLAMATION);
                    break;
                }

                // Warn before overwriting existing files
                if (!::stat(szFile, &st) &&
                    MessageBox(hdlg_, "Overwrite existing file?", "Create", MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
                    break;

                // Create the new HDF image
                else if (!HDFHardDisk::Create(szFile, uTotalSectors))
                {
                    MessageBox(hdlg_, "Failed to create new disk (disk full?)", "Create", MB_OK | MB_ICONEXCLAMATION);
                    break;
                }
            }

            // Set the new path back in the parent dialog, and close our dialog
            SetDlgItemText(hwndEdit, 0, szFile, true);
            EndDialog(hdlg_, 1);
            return TRUE;
        }
        }
    }
    }

    return FALSE;
}

////////////////////////////////////////////////////////////////////////////////


// Base handler for all options property pages
INT_PTR CALLBACK BasePageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HWND ahwndPages[MAX_OPTION_PAGES];

    BOOL fRet = FALSE;

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        // Set up simple mapping from from page number to hdlg_
        PROPSHEETPAGE* ppage = reinterpret_cast<PROPSHEETPAGE*>(lParam_);
        ahwndPages[ppage->lParam] = hdlg_;

        // If we've not yet centred the property sheet, do so now
        if (!fCentredOptions)
        {
            CentreWindow(GetParent(hdlg_));
            fCentredOptions = true;
        }

        fRet = TRUE;
        break;
    }

    case WM_NOTIFY:
    {
        switch (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code)
        {
            // New page being selected?
        case PSN_SETACTIVE:
            // Find and remember the page number of the new active page
            for (nOptionPage = MAX_OPTION_PAGES - 1; nOptionPage && ahwndPages[nOptionPage] != hdlg_; nOptionPage--);
            break;
        }
    }
    }

    return fRet;
}


INT_PTR CALLBACK SystemPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        CheckRadioButton(hdlg_, IDR_256K, IDR_512K, (GetOption(mainmem) == 256) ? IDR_256K : IDR_512K);
        SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_SETRANGE, 0, MAKELONG(0, 4));
        SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_SETPOS, TRUE, GetOption(externalmem));

        // Set the current custom ROM path, and enable filesystem auto-complete
        SetDlgItemText(hdlg_, IDE_ROM, GetOption(rom));
        SHAutoComplete(GetDlgItem(hdlg_, IDE_ROM), SHACF_FILESYS_ONLY | SHACF_USETAB);
        SendDlgItemMessage(hdlg_, IDE_ROM, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

        Button_SetCheck(GetDlgItem(hdlg_, IDC_ALBOOT_ROM), GetOption(atombootrom) ? BST_CHECKED : BST_UNCHECKED);

        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);
        if (pnmh->idFrom == IDC_EXTERNAL)
        {
            int nExternalMB = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_GETPOS, 0, 0L));
            auto size = fmt::format("{}MB", nExternalMB);
            SetDlgItemText(hdlg_, IDS_EXTERNAL, size);
        }
        else if (pnmh->code == PSN_APPLY)
        {
            SetOption(mainmem, (Button_GetCheck(GetDlgItem(hdlg_, IDR_256K)) == BST_CHECKED) ? 256 : 512);
            SetOption(externalmem, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_GETPOS, 0, 0L)));

            // If the memory configuration has changed, apply the changes
            if (Changed(mainmem) || Changed(externalmem))
                Memory::UpdateConfig();

            SetOption(rom, GetDlgItemText(hdlg_, IDE_ROM));

            SetOption(atombootrom, Button_GetCheck(GetDlgItem(hdlg_, IDC_ALBOOT_ROM)) == BST_CHECKED);

            // If the ROM config has changed, schedule the changes for the next reset
            if (Changed(rom) || Changed(atombootrom))
                Memory::UpdateRom();
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDE_ROM:
        {
            EnableWindow(GetDlgItem(hdlg_, IDC_ALBOOT_ROM), !GetWindowTextLength(GetDlgItem(hdlg_, IDE_ROM)));
            break;
        }

        case IDB_BROWSE:
        {
            BrowseImage(hdlg_, IDE_ROM, szRomFilters);
            break;
        }
        }
        break;
    }
    }

    return fRet;
}


INT_PTR CALLBACK SoundPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        static const std::vector<std::string> sid_types{ "None", "MOS6581 (Default)", "MOS8580" };
        SetComboStrings(hdlg_, IDC_SID_TYPE, sid_types, GetOption(sid));

        static const std::vector<std::string> dac7c_types{ "None", "Blue Alpha Sampler (8-bit mono)", "SAMVox (4 channel 8-bit mono)", "Paula (2 channel 4-bit stereo)" };
        SetComboStrings(hdlg_, IDC_DAC_7C, dac7c_types, GetOption(dac7c));

        FillMidiOutCombo(GetDlgItem(hdlg_, IDC_MIDI_OUT));
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(sid, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_SID_TYPE)));
            SetOption(dac7c, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DAC_7C)));
            SetOption(midi, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_MIDI_OUT)) ? 1 : 0);
            SetOption(midioutdev, GetOption(midi) ? GetComboText(hdlg_, IDC_MIDI_OUT) : "");

            if (Changed(midioutdev) && !pMidi->SetDevice(GetOption(midioutdev)))
                Message(MsgType::Warning, "Failed to open MIDI device\n");
        }

        break;
    }
    }

    return fRet;
}


int FillHDDCombos(HWND hdlg_, int nCombo1_, int nCombo2_)
{
    HWND hwndCombo1 = GetDlgItem(hdlg_, nCombo1_);
    HWND hwndCombo2 = nCombo2_ ? GetDlgItem(hdlg_, nCombo2_) : nullptr;

    ComboBox_ResetContent(hwndCombo1);
    if (hwndCombo2) ComboBox_ResetContent(hwndCombo2);

    auto device_list = DeviceHardDisk::GetDeviceList();
    for (auto& device : device_list)
    {
        ComboBox_AddString(hwndCombo1, device.c_str());
        if (hwndCombo2) ComboBox_AddString(hwndCombo2, device.c_str());
    }

    if (device_list.empty())
    {
        ComboBox_AddString(hwndCombo1, "<None>");
        if (hwndCombo2) ComboBox_AddString(hwndCombo2, "<None>");
    }

    if (ComboBox_GetCurSel(hwndCombo1) < 0)
    {
        ComboBox_SetCurSel(hwndCombo1, 0);
        if (hwndCombo2) ComboBox_SetCurSel(hwndCombo2, 0);
    }

    bool enable = !device_list.empty();
    ComboBox_Enable(hwndCombo1, enable);
    if (hwndCombo2) ComboBox_Enable(hwndCombo2, enable);

    return static_cast<int>(device_list.size());
}


int FillFloppyCombo(HWND hwndCombo_)
{
    int nDevices = 0;

    ComboBox_ResetContent(hwndCombo_);

    for (UINT u = 0; u < 2; u++)
    {
        auto device = fmt::format(R"(\\.\fdraw{})", u);
        GetFileAttributes(device.c_str());
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            device = fmt::format("{}:", 'A' + u);
            ComboBox_AddString(hwndCombo_, device.c_str());
            nDevices++;
        }
    }

    if (!nDevices)
        ComboBox_AddString(hwndCombo_, "<None>");

    if (ComboBox_GetCurSel(hwndCombo_) < 0)
        ComboBox_SetCurSel(hwndCombo_, 0);

    ComboBox_Enable(hwndCombo_, nDevices != 0);

    return nDevices;
}


INT_PTR CALLBACK Drive1PageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        static const std::vector<std::string> types{ "None", "Floppy Drive" };
        SetComboStrings(hdlg_, IDC_DEVICE_TYPE, types, GetOption(drive1));

        int nDevices = FillFloppyCombo(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE));
        EnableWindow(GetDlgItem(hdlg_, IDR_DEVICE), nDevices != 0);
        SendDlgItemMessage(hdlg_, IDE_FLOPPY_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

        if (FloppyStream::IsRecognised(GetOption(disk1)))
        {
            ComboBox_SelectString(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), -1, GetOption(disk1).c_str());
        }
        else
        {
            SetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, GetOption(disk1).c_str());
        }

        SHAutoComplete(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), SHACF_FILESYS_ONLY | SHACF_USETAB);

        SendMessage(hdlg_, WM_COMMAND, MAKELONG(IDC_DEVICE_TYPE, CBN_SELCHANGE), 0L);
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(drive1, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE)));

            if (GetOption(drive1) == drvFloppy)
            {
                bool fImage = Button_GetCheck(GetDlgItem(hdlg_, IDR_DEVICE)) != BST_CHECKED;
                if (fImage)
                {
                    SetOption(disk1, GetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE));
                }
                else
                {
                    SetOption(disk1, GetComboText(hdlg_, IDC_FLOPPY_DEVICE));
                }
            }

            // If the floppy is active and the disk has changed, insert the new one
            if (GetOption(drive1) == drvFloppy && Changed(disk1))
                InsertDisk(*pFloppy1, GetOption(disk1));
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDE_FLOPPY_IMAGE:
        {
            if (HIWORD(wParam_) == EN_CHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_IMAGE);
            break;
        }

        case IDC_FLOPPY_DEVICE:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_DEVICE);

            break;
        }

        case IDC_DEVICE_TYPE:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
            {
                auto type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE));

                ShowWindow(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), (type == 1) ? SW_SHOW : SW_HIDE);
                ShowWindow(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), (type == 1) ? SW_SHOW : SW_HIDE);

                int control_ids[] = { IDS_DEVICE, IDF_MEDIA, IDS_TEXT1, IDR_IMAGE, IDB_BROWSE, IDR_DEVICE, IDC_HDD_DEVICE };
                for (auto id : control_ids)
                    ShowWindow(GetDlgItem(hdlg_, id), (type >= 1) ? SW_SHOW : SW_HIDE);
            }
            break;
        }

        case IDB_BROWSE:
        {
            auto type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE));
            if (type == 1)
                BrowseImage(hdlg_, IDE_FLOPPY_IMAGE, szFloppyFilters);
            break;
        }
        }
        break;
    }
    }

    return fRet;
}

INT_PTR CALLBACK Drive2PageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static UINT_PTR uTimer;
    static ULONG ulSHChangeNotifyRegister;

    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_DESTROY:
        if (uTimer) KillTimer(hdlg_, 1), uTimer = 0;
        if (ulSHChangeNotifyRegister) SHChangeNotifyDeregister(ulSHChangeNotifyRegister);
        break;

    case WM_USER + 1:
        uTimer = SetTimer(hdlg_, 1, 250, nullptr);
        break;

    case WM_TIMER:
    {
        KillTimer(hdlg_, 1), uTimer = 0;

        FillFloppyCombo(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE));
        FillHDDCombos(hdlg_, IDC_HDD_DEVICE, IDC_HDD_DEVICE2);

        int type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE));

        // Set floppy image path or device selection
        if (FloppyStream::IsRecognised(GetOption(disk2)))
        {
            if (type == drvFloppy) CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_DEVICE);
            ComboBox_SelectString(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), -1, GetOption(disk2).c_str());
        }
        else
        {
            CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_IMAGE);
        }

        // Set Atom primary device selection
        if (DeviceHardDisk::IsRecognised(GetOption(atomdisk0)))
        {
            if (type >= drvAtom) CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_DEVICE);
            ComboBox_SelectString(GetDlgItem(hdlg_, IDC_HDD_DEVICE), -1, GetOption(atomdisk0).c_str());
        }

        // Set Atom secondary device selection
        if (DeviceHardDisk::IsRecognised(GetOption(atomdisk1)))
        {
            if (type >= drvAtom) CheckRadioButton(hdlg_, IDR_IMAGE2, IDR_DEVICE2, IDR_DEVICE);
            ComboBox_SelectString(GetDlgItem(hdlg_, IDC_HDD_DEVICE2), -1, GetOption(atomdisk1).c_str());
        }

        break;
    }

    case WM_INITDIALOG:
    {
        // Schedule an immediate update to the device combo boxes
        if (!uTimer && uMsg_ == WM_INITDIALOG)
            uTimer = SetTimer(hdlg_, 1, 1, nullptr);

        // Register for shell notifications when drive/media changes occur
        if (!ulSHChangeNotifyRegister)
        {
            LPITEMIDLIST ppidl;
            if (SUCCEEDED(SHGetSpecialFolderLocation(hdlg_, CSIDL_DESKTOP, &ppidl)))
            {
                SHChangeNotifyEntry entry = { ppidl, TRUE };
                int nSources = SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED | SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED;
                ulSHChangeNotifyRegister = SHChangeNotifyRegister(hdlg_, SHCNRF_ShellLevel | SHCNRF_NewDelivery, nSources, WM_USER + 1, 1, &entry);
                CoTaskMemFree(ppidl);
            }
        }

        static const std::vector<std::string> types{ "None", "Floppy Drive", "Atom Classic", "Atom Lite" };
        SetComboStrings(hdlg_, IDC_DEVICE_TYPE, types, GetOption(drive2));

        SendDlgItemMessage(hdlg_, IDE_FLOPPY_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));
        SendDlgItemMessage(hdlg_, IDE_HDD_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));
        SendDlgItemMessage(hdlg_, IDE_HDD_IMAGE2, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

        // Set floppy image path or device selection
        if (!FloppyStream::IsRecognised(GetOption(disk2)))
            SetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, GetOption(disk2));

        // Set Atom primary image path
        if (!DeviceHardDisk::IsRecognised(GetOption(atomdisk0)))
            SetDlgItemText(hdlg_, IDE_HDD_IMAGE, GetOption(atomdisk0));

        // Set Atom secondary image path
        if (!DeviceHardDisk::IsRecognised(GetOption(atomdisk1)))
            SetDlgItemText(hdlg_, IDE_HDD_IMAGE2, GetOption(atomdisk1));

        SHAutoComplete(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), SHACF_FILESYS_ONLY | SHACF_USETAB);
        SHAutoComplete(GetDlgItem(hdlg_, IDE_HDD_IMAGE), SHACF_FILESYS_ONLY | SHACF_USETAB);
        SHAutoComplete(GetDlgItem(hdlg_, IDE_HDD_IMAGE2), SHACF_FILESYS_ONLY | SHACF_USETAB);

        SendMessage(hdlg_, WM_COMMAND, MAKELONG(IDC_DEVICE_TYPE, CBN_SELCHANGE), 0L);
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(drive2, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE)));

            bool fImage = Button_GetCheck(GetDlgItem(hdlg_, IDR_DEVICE)) != BST_CHECKED;
            bool fImage2 = Button_GetCheck(GetDlgItem(hdlg_, IDR_DEVICE2)) != BST_CHECKED;

            switch (GetOption(drive2))
            {
            case drvFloppy:
                if (fImage)
                    SetOption(disk2, GetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE));
                else
                    SetOption(disk2, GetComboText(hdlg_, IDC_FLOPPY_DEVICE));
                break;

            case drvAtom:
            case drvAtomLite:
                if (fImage)
                    SetOption(atomdisk0, GetDlgItemText(hdlg_, IDE_HDD_IMAGE));
                else
                    SetOption(atomdisk0, GetComboText(hdlg_, IDC_HDD_DEVICE));

                if (fImage2)
                    SetOption(atomdisk1, GetDlgItemText(hdlg_, IDE_HDD_IMAGE2));
                else
                    SetOption(atomdisk1, GetComboText(hdlg_, IDC_HDD_DEVICE2));

                break;
            }

            // If the floppy is active and the disk has changed, insert the new one
            if (GetOption(drive2) == drvFloppy && Changed(disk2))
                InsertDisk(*pFloppy2, GetOption(disk2));

            // If Atom boot ROM is enabled and a drive type has changed, trigger a ROM refresh
            if (GetOption(atombootrom) && (Changed(drive1) || Changed(drive2)))
                Memory::UpdateRom();
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDE_FLOPPY_IMAGE:
        case IDE_HDD_IMAGE:
        {
            if (HIWORD(wParam_) == EN_CHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_IMAGE);
            break;
        }

        case IDE_HDD_IMAGE2:
        {
            if (HIWORD(wParam_) == EN_CHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE2, IDR_DEVICE2, IDR_IMAGE2);
            break;
        }

        case IDC_FLOPPY_DEVICE:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_DEVICE);

            break;
        }

        case IDC_HDD_DEVICE:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE, IDR_DEVICE, IDR_DEVICE);

            break;
        }

        case IDC_HDD_DEVICE2:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
                CheckRadioButton(hdlg_, IDR_IMAGE2, IDR_DEVICE2, IDR_DEVICE2);

            break;
        }

        case IDC_DEVICE_TYPE:
        {
            if (HIWORD(wParam_) == CBN_SELCHANGE)
            {
                int type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE));

                HICON hicon = LoadIcon(__hinstance, MAKEINTRESOURCE((type >= 2) ? IDI_DRIVE : IDI_FLOPPY));
                SendDlgItemMessage(hdlg_, IDS_DEVICE, STM_SETICON, reinterpret_cast<WPARAM>(hicon), 0L);

                SetDlgItemText(hdlg_, IDF_MEDIA, (type >= 2) ? "Primary" : "Media");

                ShowWindow(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), (type == drvFloppy) ? SW_SHOW : SW_HIDE);
                ShowWindow(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), (type == drvFloppy) ? SW_SHOW : SW_HIDE);

                int control_ids[] = { IDS_DEVICE, IDF_MEDIA, IDS_TEXT1, IDR_IMAGE, IDB_BROWSE, IDR_DEVICE, IDC_HDD_DEVICE };
                for (auto id : control_ids)
                    ShowWindow(GetDlgItem(hdlg_, id), (type >= drvFloppy) ? SW_SHOW : SW_HIDE);

                int control_ids2[] = { IDF_MEDIA2, IDS_TEXT2, IDR_IMAGE2, IDE_HDD_IMAGE, IDE_HDD_IMAGE2, IDB_BROWSE2, IDC_HDD_DEVICE, IDR_DEVICE2, IDC_HDD_DEVICE2 };
                for (auto id : control_ids2)
                    ShowWindow(GetDlgItem(hdlg_, id), (type >= drvAtom) ? SW_SHOW : SW_HIDE);

                uTimer = SetTimer(hdlg_, 1, 1, nullptr);
            }
            break;
        }

        case IDB_BROWSE:
        {
            auto type = ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_DEVICE_TYPE));
            if (type == drvFloppy)
                BrowseImage(hdlg_, IDE_FLOPPY_IMAGE, szFloppyFilters);
            else
                BrowseImage(hdlg_, IDE_HDD_IMAGE, szHDDFilters);
            break;
        }

        case IDB_BROWSE2:
        {
            BrowseImage(hdlg_, IDE_HDD_IMAGE2, szHDDFilters);
            break;
        }
        }
        break;
    }
    }

    return fRet;
}


int CALLBACK BrowseFolderCallback(HWND hwnd_, UINT uMsg_, LPARAM lParam_, LPARAM lpData_)
{
    // Once initialised, set the initial browse location
    if (uMsg_ == BFFM_INITIALIZED)
        SendMessage(hwnd_, BFFM_SETSELECTION, TRUE, lpData_);

    return 0;
}

void BrowseFolder(HWND hdlg_, int nControl_, const char* pcszDefDir_)
{
    char sz[MAX_PATH];
    HWND hctrl = GetDlgItem(hdlg_, nControl_);
    GetWindowText(hctrl, sz, sizeof(sz));

    BROWSEINFO bi = { 0 };
    bi.hwndOwner = hdlg_;
    bi.lpszTitle = "Select default path:";
    bi.lpfn = BrowseFolderCallback;
    bi.lParam = reinterpret_cast<LPARAM>(sz);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    if (auto pidl = SHBrowseForFolder(&bi))
    {
        if (SHGetPathFromIDList(pidl, sz))
        {
            SetDlgItemText(hdlg_, nControl_, sz, true);
        }

        CoTaskMemFree(pidl);
    }
}

INT_PTR CALLBACK InputPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        static const std::vector<std::string> mappings{ "Disabled", "Automatic (default)", "SAM Coupé", "ZX Spectrum" };
        SetComboStrings(hdlg_, IDC_KEYBOARD_MAPPING, mappings, GetOption(keymapping));

        Button_SetCheck(GetDlgItem(hdlg_, IDC_ALT_FOR_CNTRL), GetOption(altforcntrl) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_ALTGR_FOR_EDIT), GetOption(altgrforedit) ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(GetDlgItem(hdlg_, IDC_MOUSE_ENABLED), GetOption(mouse) ? BST_CHECKED : BST_UNCHECKED);

        SendMessage(hdlg_, WM_COMMAND, IDC_MOUSE_ENABLED, 0L);
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(keymapping, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_KEYBOARD_MAPPING)));

            SetOption(altforcntrl, Button_GetCheck(GetDlgItem(hdlg_, IDC_ALT_FOR_CNTRL)) == BST_CHECKED);
            SetOption(altgrforedit, Button_GetCheck(GetDlgItem(hdlg_, IDC_ALTGR_FOR_EDIT)) == BST_CHECKED);

            SetOption(mouse, Button_GetCheck(GetDlgItem(hdlg_, IDC_MOUSE_ENABLED)) == BST_CHECKED);

            if (Changed(keymapping) || Changed(mouse))
                Input::Init();
        }
        break;
    }
    }

    return fRet;
}


INT_PTR CALLBACK JoystickPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static UINT_PTR uTimer;

    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_DESTROY:
        if (uTimer)
            KillTimer(hdlg_, 1), uTimer = 0;
        break;

    case WM_TIMER:
        KillTimer(hdlg_, 1), uTimer = 0;

        FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK1), GetOption(joydev1));
        FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK2), GetOption(joydev2));
        SendMessage(hdlg_, WM_COMMAND, IDC_JOYSTICK1, 0L);
        break;

    case WM_DEVICECHANGE:
        uTimer = SetTimer(hdlg_, 1, 1000, nullptr);
        break;

    case WM_INITDIALOG:
    {
        FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK1), GetOption(joydev1));
        FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK2), GetOption(joydev2));

        static const std::vector<std::string> types{ "None", "Joystick 1", "Joystick 2", "Kempston" };
        SetComboStrings(hdlg_, IDC_SAM_JOYSTICK1, types, GetOption(joytype1));
        SetComboStrings(hdlg_, IDC_SAM_JOYSTICK2, types, GetOption(joytype2));

        SendMessage(hdlg_, WM_COMMAND, IDC_JOYSTICK1, 0L);
        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);

        if (pnmh->code == PSN_APPLY)
        {
            SetOption(joydev1, GetComboText(hdlg_, IDC_JOYSTICK1));
            SetOption(joydev2, GetComboText(hdlg_, IDC_JOYSTICK2));
            SetOption(joytype1, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK1)));
            SetOption(joytype2, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK2)));
            Input::Init();
        }

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDC_JOYSTICK1:
        case IDC_JOYSTICK2:
            ComboBox_Enable(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK1), ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_JOYSTICK1)) != 0);
            ComboBox_Enable(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK2), ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_JOYSTICK2)) != 0);
            break;
        }
        break;
    }
    }

    return fRet;
}


INT_PTR CALLBACK ParallelPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        static const std::vector<std::string> types{ "None", "Printer", "DAC (8-bit mono)", "SAMDAC (8-bit stereo)" };
        SetComboStrings(hdlg_, IDC_PARALLEL_1, types, GetOption(parallel1));
        SetComboStrings(hdlg_, IDC_PARALLEL_2, types, GetOption(parallel2));

        Button_SetCheck(GetDlgItem(hdlg_, IDC_AUTO_FLUSH), GetOption(flushdelay) ? BST_CHECKED : BST_UNCHECKED);
        FillPrintersCombo(GetDlgItem(hdlg_, IDC_PRINTERS));

        SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_1, 0L);
        SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_2, 0L);
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(parallel1, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_PARALLEL_1)));
            SetOption(parallel2, ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_PARALLEL_2)));
            SetOption(flushdelay, (Button_GetCheck(GetDlgItem(hdlg_, IDC_AUTO_FLUSH)) == BST_CHECKED) ? 2 : 0);
        }

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDC_PARALLEL_1:
        case IDC_PARALLEL_2:
        {
            bool fPrinter1 = (ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_PARALLEL_1)) == 1);
            bool fPrinter2 = (ComboBox_GetCurSel(GetDlgItem(hdlg_, IDC_PARALLEL_2)) == 1);

            EnableWindow(GetDlgItem(hdlg_, IDC_PRINTERS), fPrinter1 || fPrinter2);
            EnableWindow(GetDlgItem(hdlg_, IDS_PRINTERS), fPrinter1 || fPrinter2);
            EnableWindow(GetDlgItem(hdlg_, IDC_AUTO_FLUSH), fPrinter1 || fPrinter2);
            break;
        }
        }

        break;
    }
    }

    return fRet;
}


INT_PTR CALLBACK MiscPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        Button_SetCheck(GetDlgItem(hdlg_, IDC_SAMBUS_CLOCK), GetOption(sambusclock) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_DALLAS_CLOCK), GetOption(dallasclock) ? BST_CHECKED : BST_UNCHECKED);

        Button_SetCheck(GetDlgItem(hdlg_, IDC_DRIVE_LIGHTS), GetOption(drivelights) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_STATUS), GetOption(status) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_PROFILE), GetOption(profile) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_SAVE_PROMPT), GetOption(saveprompt) ? BST_CHECKED : BST_UNCHECKED);

        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(sambusclock, Button_GetCheck(GetDlgItem(hdlg_, IDC_SAMBUS_CLOCK)) == BST_CHECKED);
            SetOption(dallasclock, Button_GetCheck(GetDlgItem(hdlg_, IDC_DALLAS_CLOCK)) == BST_CHECKED);

            SetOption(drivelights, Button_GetCheck(GetDlgItem(hdlg_, IDC_DRIVE_LIGHTS)) == BST_CHECKED);
            SetOption(status, Button_GetCheck(GetDlgItem(hdlg_, IDC_STATUS)) == BST_CHECKED);
            SetOption(profile, Button_GetCheck(GetDlgItem(hdlg_, IDC_PROFILE)) == BST_CHECKED);
            SetOption(saveprompt, Button_GetCheck(GetDlgItem(hdlg_, IDC_SAVE_PROMPT)) == BST_CHECKED);
        }
        break;
    }
    }

    return fRet;
}


INT_PTR CALLBACK HelperPageDlgProc(HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
    case WM_INITDIALOG:
    {
        Button_SetCheck(GetDlgItem(hdlg_, IDC_TURBO_DISK), GetOption(turbodisk) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_FAST_RESET), GetOption(fastreset) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_AUTOLOAD), GetOption(autoload) ? BST_CHECKED : BST_UNCHECKED);
        Button_SetCheck(GetDlgItem(hdlg_, IDC_DOSBOOT), GetOption(dosboot) ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemText(hdlg_, IDE_DOSDISK, GetOption(dosdisk));
        SendDlgItemMessage(hdlg_, IDE_DOSDISK, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

        SendMessage(hdlg_, WM_COMMAND, IDC_DOSBOOT, 0L);
        break;
    }

    case WM_NOTIFY:
    {
        if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
        {
            SetOption(turbodisk, Button_GetCheck(GetDlgItem(hdlg_, IDC_TURBO_DISK)) == BST_CHECKED);
            SetOption(fastreset, Button_GetCheck(GetDlgItem(hdlg_, IDC_FAST_RESET)) == BST_CHECKED);
            SetOption(autoload, Button_GetCheck(GetDlgItem(hdlg_, IDC_AUTOLOAD)) == BST_CHECKED);
            SetOption(dosboot, Button_GetCheck(GetDlgItem(hdlg_, IDC_DOSBOOT)) == BST_CHECKED);

            std::vector<char> path(MAX_PATH);
            GetDlgItemText(hdlg_, IDE_DOSDISK, path.data(), static_cast<DWORD>(path.size()));
            SetOption(dosdisk, path.data());
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam_))
        {
        case IDC_DOSBOOT:
        {
            bool fDosBoot = Button_GetCheck(GetDlgItem(hdlg_, IDC_DOSBOOT)) == BST_CHECKED;
            EnableWindow(GetDlgItem(hdlg_, IDS_DOSDISK), fDosBoot);
            EnableWindow(GetDlgItem(hdlg_, IDE_DOSDISK), fDosBoot);
            EnableWindow(GetDlgItem(hdlg_, IDB_BROWSE), fDosBoot);
            break;
        }

        case IDB_BROWSE:
        {
            BrowseImage(hdlg_, IDE_DOSDISK, szFloppyFilters);   break;
            break;
        }
        }
        break;
    }
    }

    return fRet;
}


static void AddPage(std::vector<PROPSHEETPAGE>& pages, int dialog_id, DLGPROC pfnDlgProc)
{
    PROPSHEETPAGE page{};
    page.dwSize = sizeof(page);
    page.hInstance = __hinstance;
    page.pszTemplate = MAKEINTRESOURCE(dialog_id);
    page.pfnDlgProc = pfnDlgProc;
    page.lParam = static_cast<LPARAM>(pages.size());
    pages.push_back(page);
}


void DisplayOptions()
{
    // Update floppy path options
    SetOption(disk1, pFloppy1->DiskPath());
    SetOption(disk2, pFloppy2->DiskPath());

    // Initialise the pages to go on the sheet
    std::vector<PROPSHEETPAGE> pages;
    AddPage(pages, IDD_PAGE_SYSTEM, SystemPageDlgProc);
    AddPage(pages, IDD_PAGE_SOUND, SoundPageDlgProc);
    AddPage(pages, IDD_PAGE_PARALLEL, ParallelPageDlgProc);
    AddPage(pages, IDD_PAGE_INPUT, InputPageDlgProc);
    AddPage(pages, IDD_PAGE_JOYSTICK, JoystickPageDlgProc);
    AddPage(pages, IDD_PAGE_MISC, MiscPageDlgProc);
    AddPage(pages, IDD_PAGE_DRIVE1, Drive1PageDlgProc);
    AddPage(pages, IDD_PAGE_DRIVE2, Drive2PageDlgProc);
    AddPage(pages, IDD_PAGE_HELPER, HelperPageDlgProc);

    PROPSHEETHEADER psh{};
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
    psh.hwndParent = g_hwnd;
    psh.hInstance = __hinstance;
    psh.pszIcon = MAKEINTRESOURCE(IDI_MISC);
    psh.pszCaption = "Options";
    psh.nPages = static_cast<UINT>(pages.size());
    psh.nStartPage = nOptionPage;
    psh.ppsp = pages.data();

    // Save the current option state, flag that we've not centred the dialogue box, then display them for editing
    current_config = Options::g_config;
    fCentredOptions = false;

    // Display option property sheet
    if (PropertySheet(&psh) >= 1)
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

        // Save changed options to config file
        Options::Save();
    }
}
