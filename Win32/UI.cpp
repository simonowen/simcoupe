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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "UI.h"

#include <shlwapi.h>
#include <shlobj.h>
#include <VersionHelpers.h>

#include "Action.h"
#include "AtaAdapter.h"
#include "AVI.h"
#include "Clock.h"
#include "CPU.h"
#include "Debug.h"
#include "Direct3D9.h"
#include "DirectDraw.h"
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
#include "OSD.h"
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

INT_PTR CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
INT_PTR CALLBACK NewDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
void CentreWindow (HWND hwnd_, HWND hwndParent_=nullptr);

static void DisplayOptions ();
static bool InitWindow ();

static void AddRecentFile (const char* pcsz_);
static void LoadRecentFiles ();
static void SaveRecentFiles ();

static bool EjectDisk (CDiskDevice *pFloppy_);
static bool EjectTape ();

static void SaveWindowPosition(HWND hwnd_);
static bool RestoreWindowPosition(HWND hwnd_);
static void ResizeWindow (int nHeight_=0);


HINSTANCE __hinstance;
HWND g_hwnd, hwndCanvas;
static HMENU g_hmenu;
extern HINSTANCE __hinstance;

static HHOOK hWinKeyHook;

static WNDPROC pfnStaticWndProc;           // Old static window procedure (internal value)

static WINDOWPLACEMENT g_wp = { sizeof(WINDOWPLACEMENT) };
static int nWindowDx, nWindowDy;

static int nOptionPage = 0;                // Last active option property page
static const int MAX_OPTION_PAGES = 16;    // Maximum number of option propery pages
static bool fCentredOptions;

static OPTIONS opts;
// Helper macro for detecting options changes
#define Changed(o)        (opts.o != GetOption(o))
#define ChangedString(o)  (strcasecmp(opts.o, GetOption(o)))

static char szFloppyFilters[] =
#ifdef USE_ZLIB
    "All Disks (dsk;sad;mgt;sbt;cpm;gz;zip)\0*.dsk;*.sad;*.mgt;*.sbt;*.cpm;*.gz;*.zip\0"
#endif
    "Disk Images (dsk;sad;mgt;sbt;cpm)\0*.dsk;*.sad;*.mgt;*.sbt;*.cpm\0"
#ifdef USE_ZLIB
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
#ifdef USE_ZLIB
    "Compressed Files (gz;zip)\0*.gz;*.zip\0"
#endif
    "All Files (*.*)\0*.*\0";

static const char* aszBorders[] =
    { "No borders", "Small borders", "Short TV area (default)", "TV visible area", "Complete scan area", nullptr };


extern "C" int main(int argc_, char* argv_[]);

int WINAPI WinMain(HINSTANCE hinst_, HINSTANCE hinstPrev_, LPSTR pszCmdLine_, int nCmdShow_)
{
    __hinstance = hinst_;

    return main(__argc, __argv);
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath (char* pszPath_, size_t nLength_);

bool UI::Init (bool fFirstInit_/*=false*/)
{
    TRACE("UI::Init()\n");

    LoadRecentFiles();

    return InitWindow();
}

void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("UI::Exit()\n");

    if (g_hwnd)
    {
        SaveWindowPosition(g_hwnd);
        DestroyWindow(g_hwnd), g_hwnd = nullptr;
    }

    SaveRecentFiles();
}


// Create a video object to render the display
VideoBase *UI::GetVideo (bool fFirstInit_)
{
    VideoBase *pVideo = nullptr;

    // Is D3D enabled (1), or are we in auto-mode (-1) and running Vista or later?
    if (GetOption(direct3d) > 0 || (GetOption(direct3d) < 0 && IsWindowsVistaOrGreater()))
    {
        // Try for Direct3D9
        pVideo = new Direct3D9Video;
        if (pVideo && !pVideo->Init(fFirstInit_))
            delete pVideo, pVideo = nullptr;
    }

    // Fall back on DirectDraw
    if (!pVideo)
    {
        pVideo = new DirectDrawVideo;
        if (!pVideo->Init(fFirstInit_))
            delete pVideo, pVideo = nullptr;
    }

    return pVideo;
}

// Check and process any incoming messages
bool UI::CheckEvents ()
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

        // Silence the audio and block until something happens
        Sound::Silence();
        WaitMessage();
    }

    return true;
}

void UI::ShowMessage (eMsgType eType_, const char* pcszMessage_)
{
    const char* const pcszCaption = WINDOW_CAPTION;
    HWND hwndParent = GetActiveWindow();

    switch (eType_)
    {
        case msgWarning:
            MessageBox(hwndParent, pcszMessage_, pcszCaption, MB_OK | MB_ICONEXCLAMATION);
            break;

        case msgError:
            MessageBox(hwndParent, pcszMessage_, pcszCaption, MB_OK | MB_ICONSTOP);
            break;

        // Something went seriously wrong!
        case msgFatal:
            MessageBox(hwndParent, pcszMessage_, pcszCaption, MB_OK | MB_ICONSTOP);
            break;
    }
}


// Resize canvas window to a given container view size
void ResizeCanvas (int nWidthView_, int nHeightView_)
{
    int nX = 0;
    int nY = 0;

    // In fullscreen mode with a menu, adjust to exclude it
    if (GetOption(fullscreen) && GetMenu(g_hwnd))
    {
        int nMenu = GetSystemMetrics(SM_CYMENU);
        nY -= nMenu;
        nHeightView_ += nMenu;
    }

    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();
    if (GetOption(ratio5_4)) nWidth = nWidth * 5/4;

    int nWidthFit = MulDiv(nWidth, nHeightView_, nHeight);
    int nHeightFit = MulDiv(nHeight, nWidthView_, nWidth);

    // Fit width to full height?
    if (nWidthFit <= nWidthView_)
    {
        nWidth = nWidthFit;
        nHeight = nHeightView_;
        nX += (nWidthView_ - nWidth) / 2;
    }
    // Fit height to full width
    else
    {
        nWidth = nWidthView_;
        nHeight = nHeightFit;
        nY += (nHeightView_ - nHeight) / 2;
    }

    // Set the new canvas position and size
    MoveWindow(hwndCanvas, nX, nY, nWidth, nHeight, TRUE);
    TRACE("Canvas: %d,%d %dx%d\n", nX, nY, nWidth, nHeight);
}

// Resize the main window to a given height, or update width if height is zero
void ResizeWindow (int nHeight_)
{
    RECT rClient;
    GetClientRect(g_hwnd, &rClient);

    if (GetOption(fullscreen))
    {
        // Change the window style to a visible pop-up, with no caption, border or menu
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
        SetMenu(g_hwnd, nullptr);

        // Force the window to be top-most, and sized to fill the full screen
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
        ResizeCanvas(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        return;
    }

    if (IsMaximized(g_hwnd))
    {
        // Fetch window position details, including the window position when restored
        WINDOWPLACEMENT wp = { sizeof(wp) };
        GetWindowPlacement(g_hwnd, &wp);

        // Calculate the size of a window with 0x0 client area, to determine the frame/title sizes
        RECT rect = { 0,0, 0,0 };
        AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));

        // Calculate the height of the client area in the normalised window
        int nHeight = (wp.rcNormalPosition.bottom - wp.rcNormalPosition.top) - (rect.bottom - rect.top);

        // Calculate the appropriate width matching the height, taking aspect ratio into consideration
        int nWidth = MulDiv(nHeight, Frame::GetWidth(), Frame::GetHeight());
        if (GetOption(ratio5_4)) nWidth = nWidth * 5/4;
        nWidth += (rect.right - rect.left);

        // Update the restored window to be the correct shape once restored
        wp.rcNormalPosition.right = wp.rcNormalPosition.left + nWidth;
        SetWindowPlacement(g_hwnd, &wp);

        // Adjust the canvas to match the new client area
        ResizeCanvas(rClient.right, rClient.bottom);
    }
    else
    {
        // If a specific height wasn't supplied, use the existing height
        if (!nHeight_)
            nHeight_ = rClient.bottom;

        // Calculate the appropriate width matching the height
        int nWidth_ = MulDiv(nHeight_, Frame::GetWidth(), Frame::GetHeight());
        if (GetOption(ratio5_4)) nWidth_ = nWidth_ * 5/4;

        // Calculate the full window size for our given client area
        RECT rect = { 0, 0, nWidth_, nHeight_ };
        AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));

        // Set the new window size
        SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_SHOWWINDOW|SWP_NOMOVE);
    }
}


// Save changes to a given drive, optionally prompting for confirmation
bool ChangesSaved (CDiskDevice* pFloppy_)
{
    if (!pFloppy_->DiskModified())
        return true;

    if (GetOption(saveprompt))
    {
        char sz[MAX_PATH] = {};
        snprintf(sz, MAX_PATH-1, "Save changes to %s?", pFloppy_->DiskFile());

        switch (MessageBox(g_hwnd, sz, WINDOW_CAPTION, MB_YESNOCANCEL|MB_ICONQUESTION))
        {
            case IDYES:     break;
            case IDNO:      pFloppy_->SetDiskModified(false); return true;
            default:        return false;
        }
    }

    if (!pFloppy_->Save())
    {
        Message(msgWarning, "Failed to save changes to %s", pFloppy_->DiskFile());
        return false;
    }

    return true;
}

bool GetSaveLoadFile (LPOPENFILENAME lpofn_, bool fLoad_, bool fCheckExisting_=true)
{
    // Force an Explorer-style dialog, and ensure loaded files exist
    lpofn_->Flags |= OFN_EXPLORER|OFN_PATHMUSTEXIST;
    if (fCheckExisting_) lpofn_->Flags |= (fLoad_ ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);

    // Loop until successful
    while (!(fLoad_ ? GetOpenFileName(lpofn_) : GetSaveFileName(lpofn_)))
    {
        // Invalid paths choke the dialog
        if (CommDlgExtendedError() == FNERR_INVALIDFILENAME)
            *lpofn_->lpstrFile = '\0';
        else
        {
            TRACE("!!! GetSaveLoadFile() failed with %#08lx\n", CommDlgExtendedError());
            return false;
        }
    }

    return true;
}


#define NUM_RECENT_FILES        6
char szRecentFiles[NUM_RECENT_FILES][MAX_PATH];

void LoadRecentFiles ()
{
    lstrcpyn(szRecentFiles[0], GetOption(mru0), sizeof(szRecentFiles[0]));
    lstrcpyn(szRecentFiles[1], GetOption(mru1), sizeof(szRecentFiles[1]));
    lstrcpyn(szRecentFiles[2], GetOption(mru2), sizeof(szRecentFiles[2]));
    lstrcpyn(szRecentFiles[3], GetOption(mru3), sizeof(szRecentFiles[3]));
    lstrcpyn(szRecentFiles[4], GetOption(mru4), sizeof(szRecentFiles[4]));
    lstrcpyn(szRecentFiles[5], GetOption(mru5), sizeof(szRecentFiles[5]));
}

void SaveRecentFiles ()
{
    SetOption(mru0, szRecentFiles[0]);
    SetOption(mru1, szRecentFiles[1]);
    SetOption(mru2, szRecentFiles[2]);
    SetOption(mru3, szRecentFiles[3]);
    SetOption(mru4, szRecentFiles[4]);
    SetOption(mru5, szRecentFiles[5]);
}

void AddRecentFile (const char* pcsz_)
{
    int i;

    char szNew[MAX_PATH];
    lstrcpyn(szNew, pcsz_, sizeof(szNew));

    for (i = 0 ; lstrcmpi(szRecentFiles[i], szNew) && i < NUM_RECENT_FILES-1 ; i++);

    for ( ; i > 0; i--)
        lstrcpy(szRecentFiles[i], szRecentFiles[i-1]);

    lstrcpy(szRecentFiles[0], szNew);
}

void RemoveRecentFile (const char* pcsz_)
{
    int i;

    for (i = 0 ; lstrcmpi(szRecentFiles[i], pcsz_) && i < NUM_RECENT_FILES ; i++);

    if (i == NUM_RECENT_FILES)
        return;

    for ( ; i < NUM_RECENT_FILES-1 ; i++)
        lstrcpy(szRecentFiles[i], szRecentFiles[i+1]);

    szRecentFiles[i][0] = '\0';
}

void UpdateRecentFiles (HMENU hmenu_, int nId_, int nOffset_)
{
    for (int i = 0 ; i < NUM_RECENT_FILES ; i++)
        DeleteMenu(hmenu_, nId_+i, MF_BYCOMMAND);

    if (!szRecentFiles[0][0])
    {
        InsertMenu(hmenu_, GetMenuItemCount(hmenu_)-nOffset_, MF_STRING|MF_BYPOSITION, nId_, "Recent Files");
        EnableMenuItem(hmenu_, nId_, MF_GRAYED);
    }
    else
    {
        int nInsertPos = GetMenuItemCount(hmenu_) - nOffset_;

        for (int i = 0 ; szRecentFiles[i][0] && i < NUM_RECENT_FILES ; i++)
        {
            char szItem[MAX_PATH*2], *psz = szItem;
            psz += wsprintf(szItem, "&%d ", i+1);

            for (char *p = szRecentFiles[i] ; *p ; *psz++ = *p++)
            {
                if (*p == '&')
                    *psz++ = *p;
            }

            *psz = '\0';
            ClipPath(szItem+3, 32);

            InsertMenu(hmenu_, nInsertPos++, MF_STRING|MF_BYPOSITION, nId_+i, szItem);
        }
    }
}


bool AttachDisk (CAtaAdapter *pAdapter_, const char *pcszDisk_, int nDevice_)
{
    if (!pAdapter_->Attach(pcszDisk_, nDevice_))
    {
        // Attempt to determine why we couldn't open the device
        CHardDisk *pDisk = new CDeviceHardDisk(pcszDisk_);
        if (!pDisk->Open(false))
        {
            // Access will be denied if we're running as a regular user, and without SAMdiskHelper
            if (GetLastError() == ERROR_ACCESS_DENIED)
                Message(msgWarning, "Failed to open: %s\n\nAdminstrator access or SAMdiskHelper is required.", pcszDisk_);
            else
                Message(msgWarning, "Failed to open: %s", pcszDisk_);
        }
        delete pDisk;
        return false;
    }

    return true;
}

bool InsertDisk (CDiskDevice* pFloppy_, const char *pcszPath_=nullptr)
{
    char szFile[MAX_PATH] = "";
    int nDrive = (pFloppy_ == pFloppy1) ? 1 : 2;

    // Save any changes to the current disk first
    if (!ChangesSaved(pFloppy_))
        return false;

    // Check the floppy drive is present
    if ((nDrive == 1 && GetOption(drive1) != drvFloppy) ||
        (nDrive == 2 && GetOption(drive2) != drvFloppy))
    {
        Message(msgWarning, "Floppy drive %d is not present", nDrive);
        return false;
    }

    // Eject any existing disk if the new path is blank
    if (pcszPath_ && !*pcszPath_)
    {
        EjectDisk(pFloppy_);
        return true;
    }

    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = szFloppyFilters;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    lstrcpyn(szFile, pFloppy_->DiskFile(), MAX_PATH);

    // Don't use floppy paths for the initial directory
    if (szFile[0] == 'A' || szFile[0] == 'B')
        szFile[0] = '\0';

    // Prompt for the a new disk to insert if we don't have one
    if (!pcszPath_)
    {
        if (!GetSaveLoadFile(&ofn, true))
            return false;

        pcszPath_ = szFile;
    }

    bool fReadOnly = !!(ofn.Flags & OFN_READONLY);

    // Insert the disk to check it's a recognised format
    if (!pFloppy_->Insert(pcszPath_, true))
    {
        Message(msgWarning, "Invalid disk: %s", pcszPath_);
        RemoveRecentFile(pcszPath_);
        return false;
    }

    Frame::SetStatus("%s  inserted into drive %d%s", pFloppy_->DiskFile(), (pFloppy_ == pFloppy1) ? 1 : 2, fReadOnly ? " (read-only)" : "");
    AddRecentFile(pcszPath_);
    return true;
}

bool EjectDisk (CDiskDevice *pFloppy_)
{
    if (!pFloppy_->HasDisk())
        return true;

    if (ChangesSaved(pFloppy_))
    {
        Frame::SetStatus("%s  ejected from drive %d", pFloppy_->DiskFile(), (pFloppy_ == pFloppy1) ? 1 : 2);
        pFloppy_->Eject();
        return true;
    }

    return false;
}


bool InsertTape (HWND hwndParent_, const char *pcszPath_=nullptr)
{
    char szFile[MAX_PATH] = "";
    lstrcpyn(szFile, Tape::GetPath(), sizeof(szFile));

    // Eject any existing disk if the new path is blank
    if (pcszPath_ && !*pcszPath_)
    {
        EjectTape();
        return true;
    }

    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = hwndParent_;
    ofn.lpstrFilter = szTapeFilters;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;

    if (!pcszPath_)
    {
        // Prompt for the a new disk to insert
        if (!GetSaveLoadFile(&ofn, true))
            return false;

        pcszPath_ = szFile;
    }

    if (!Tape::Insert(pcszPath_))
    {
        Message(msgWarning, "Invalid tape: %s", pcszPath_);
        RemoveRecentFile(pcszPath_);
        return false;
    }

    Frame::SetStatus("%s  inserted", Tape::GetFile());
    AddRecentFile(pcszPath_);
    return true;
}

bool EjectTape ()
{
    if (!Tape::IsInserted())
        return true;

    Frame::SetStatus("%s  ejected", Tape::GetFile());
    Tape::Eject();
    return true;
}

#ifdef USE_LIBSPECTRUM

void UpdateTapeToolbar (HWND hdlg_)
{
    libspectrum_tape *tape = Tape::GetTape();
    bool fInserted = tape != nullptr;

    HWND hwndToolbar = GetDlgItem(hdlg_, ID_TAPE_TOOLBAR);

    SendMessage(hwndToolbar, TB_ENABLEBUTTON, ID_TAPE_OPEN, 1);
    SendMessage(hwndToolbar, TB_ENABLEBUTTON, ID_TAPE_EJECT, fInserted);
    SendMessage(hwndToolbar, TB_CHECKBUTTON, ID_TAPE_TURBOLOAD, GetOption(turbotape));
    SendMessage(hwndToolbar, TB_CHECKBUTTON, ID_TAPE_TRAPS, GetOption(tapetraps));
}

void UpdateTapeBlockList (HWND hdlg_)
{
    libspectrum_tape *tape = Tape::GetTape();
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
        libspectrum_tape_block *block = libspectrum_tape_iterator_init(&it, tape);

        // Loop over all blocks in the tape
        for (int nBlock = 0 ; block ; block = libspectrum_tape_iterator_next(&it), nBlock++)
        {
            char sz[128] = "";
            libspectrum_tape_block_description(sz, sizeof(sz), block);

            LVITEM lvi = {};
            lvi.mask = LVIF_TEXT;
            lvi.iItem = nBlock;
            lvi.iSubItem = 0;
            lvi.pszText = sz;

            // Insert the new block item, setting the first column to the type text
            int nIndex = ListView_InsertItem(hwndList, &lvi);

            // Set the second column to the block details
            ListView_SetItemText(hwndList, nIndex, 1, const_cast<char*>(Tape::GetBlockDetails(block)));
        }

        // Fetch the current block index
        int nCurBlock = 0;
        if (libspectrum_tape_position(&nCurBlock, tape) == LIBSPECTRUM_ERROR_NONE)
        {
            // Select the current block in the list, and ensure it's visible
            ListView_SetItemState(hwndList, nCurBlock, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
            ListView_EnsureVisible(hwndList, nCurBlock, FALSE);
        }
    }
}

INT_PTR CALLBACK TapeBrowseDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HWND hwndToolbar, hwndList, hwndStatus;
    static HIMAGELIST hImageList;
    static bool fAutoLoad;

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            // Create a flat toolbar with tooltips
            hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, "", WS_CHILD|TBSTYLE_FLAT|WS_VISIBLE|TBSTYLE_TOOLTIPS, 0,0, 0,0, hdlg_, (HMENU)ID_TAPE_TOOLBAR, __hinstance, nullptr);
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
                { nImageListIcons+STD_FILEOPEN, ID_TAPE_OPEN, TBSTATE_ENABLED, TBSTYLE_BUTTON },
                { 5, ID_TAPE_EJECT, TBSTATE_ENABLED, TBSTYLE_BUTTON },
                { 0, 0, 0, TBSTYLE_SEP },
                { 7, ID_TAPE_TURBOLOAD, TBSTATE_ENABLED, TBSTYLE_CHECK },
                { 8, ID_TAPE_TRAPS, TBSTATE_ENABLED, TBSTYLE_CHECK }
            };

            // Add the toolbar buttons and configure settings
            SendMessage(hwndToolbar, TB_ADDBUTTONS, _countof(tbb), (LPARAM)&tbb);
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
                libspectrum_tape *tape = Tape::GetTape();

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
                        IO::AutoLoad(AUTOLOAD_TAPE);

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

                    // If the tape is playing, toggle the turbo flag state too
                    if (Tape::IsPlaying())
                        g_nTurbo ^= TURBO_TAPE;

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
            char szFile[MAX_PATH]="";

            // Query details of the file dropped
            if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof(szFile)))
            {
                Tape::Insert(szFile);
                UpdateTapeToolbar(hdlg_);
                UpdateTapeBlockList(hdlg_);
            }

            return 0;
        }
    }

    return 0;
}

#endif // USE_LIBSPECTRUM


#define CheckOption(id,check)   CheckMenuItem(hmenu, (id), (check) ? MF_CHECKED : MF_UNCHECKED)
#define EnableItem(id,enable)   EnableMenuItem(hmenu, (id), (enable) ? MF_ENABLED : MF_GRAYED)

void UpdateMenuFromOptions ()
{
    char szEject[MAX_PATH] = {};

    HMENU hmenu = g_hmenu;
    HMENU hmenuFile = GetSubMenu(hmenu, 0);
    HMENU hmenuFloppy2 = GetSubMenu(hmenuFile, 6);
    HMENU hmenuView = GetSubMenu(hmenu, 1);
    HMENU hmenuRecord = GetSubMenu(hmenu, 2);

    bool fFloppy1 = GetOption(drive1) == drvFloppy, fInserted1 = pFloppy1->HasDisk();
    bool fFloppy2 = GetOption(drive2) == drvFloppy, fInserted2 = pFloppy2->HasDisk();

    snprintf(szEject, MAX_PATH-1, "&Close %s", fInserted1 ? pFloppy1->DiskFile() : "");
    ModifyMenu(hmenu, IDM_FILE_FLOPPY1_EJECT, MF_STRING, IDM_FILE_FLOPPY1_EJECT, szEject);
    snprintf(szEject, MAX_PATH-1, "&Close %s", fInserted2 ? pFloppy2->DiskFile() : "");
    ModifyMenu(hmenu, IDM_FILE_FLOPPY2_EJECT, MF_STRING, IDM_FILE_FLOPPY2_EJECT, szEject);

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 1 options
    EnableItem(IDM_FILE_NEW_DISK1, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_INSERT, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_EJECT, fInserted1);
    EnableItem(IDM_FILE_FLOPPY1_SAVE_CHANGES, fFloppy1 && pFloppy1->DiskModified());

    // Only enable the floppy device menu item if it's supported
    EnableItem(IDM_FILE_FLOPPY1_DEVICE, CFloppyStream::IsSupported());
    CheckOption(IDM_FILE_FLOPPY1_DEVICE, fInserted1 && CFloppyStream::IsRecognised(pFloppy1->DiskFile()));

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 2 options
    EnableMenuItem(hmenuFile, 6, MF_BYPOSITION | (fFloppy2 ? MF_ENABLED : MF_GRAYED));
    EnableItem(IDM_FILE_FLOPPY2_EJECT, fInserted2);
    EnableItem(IDM_FILE_FLOPPY2_SAVE_CHANGES, pFloppy2->DiskModified());

    CheckOption(IDM_VIEW_FULLSCREEN, GetOption(fullscreen));
    CheckOption(IDM_VIEW_RATIO54, GetOption(ratio5_4));
    CheckOption(IDM_VIEW_GREYSCALE, GetOption(greyscale));
    CheckOption(IDM_VIEW_SCANLINES, GetOption(scanlines));

    CheckOption(IDM_VIEW_FILTER, GetOption(filter) && Video::CheckCaps(VCAP_FILTER));
    EnableItem(IDM_VIEW_FILTER, Video::CheckCaps(VCAP_FILTER));

    EnableMenuItem(hmenuView, 8, MF_BYPOSITION | (GetOption(scanlines) ? MF_ENABLED : MF_GRAYED));
    EnableItem(IDM_VIEW_SCANHIRES, GetOption(scanlines));

    CheckOption(IDM_VIEW_SCANHIRES, GetOption(scanhires) && Video::CheckCaps(VCAP_SCANHIRES));
    EnableItem(IDM_VIEW_SCANHIRES, GetOption(scanlines) && Video::CheckCaps(VCAP_SCANHIRES));

    CheckMenuRadioItem(hmenu, IDM_VIEW_ZOOM_50, IDM_VIEW_ZOOM_300, IDM_VIEW_ZOOM_50+GetOption(scale)-1, MF_BYCOMMAND);
    CheckMenuRadioItem(hmenu, IDM_VIEW_BORDERS0, IDM_VIEW_BORDERS4, IDM_VIEW_BORDERS0+GetOption(borders), MF_BYCOMMAND);

    EnableMenuItem(hmenuRecord, 3, MF_BYPOSITION | (GetOption(sound) ? MF_ENABLED : MF_GRAYED));

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
    CheckOption(IDM_SYSTEM_MUTESOUND, !GetOption(sound));

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
    EnableItem(IDM_TOOLS_DEBUGGER, !g_fPaused && !GUI::IsActive());

    // Enable the Flush printer item if there's buffered data in either printer
    bool fPrinter1 = GetOption(parallel1) == 1, fPrinter2 = GetOption(parallel2) == 1;
    bool fFlushable = pPrinterFile->IsFlushable();
    EnableItem(IDM_TOOLS_FLUSH_PRINTER, fFlushable);

    // Enable the online option if a printer is active, and check it if it's online
    EnableItem(IDM_TOOLS_PRINTER_ONLINE, fPrinter1 || fPrinter2);
    CheckOption(IDM_TOOLS_PRINTER_ONLINE, (fPrinter1 || fPrinter2) && GetOption(printeronline));

    // Enable clipboard pasting if Unicode text data is available
    EnableItem(IDM_TOOLS_PASTE_CLIPBOARD, Keyin::CanType() && IsClipboardFormatAvailable(CF_UNICODETEXT));

#ifdef USE_LIBSPECTRUM
    EnableItem(IDM_TOOLS_TAPE_BROWSER, true);
#endif

    UpdateRecentFiles(hmenuFile, IDM_FILE_RECENT1, 2);
    UpdateRecentFiles(hmenuFloppy2, IDM_FLOPPY2_RECENT1, 0);
}


bool UI::DoAction (int nAction_, bool fPressed_/*=true*/)
{
    // Key being pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actToggleFullscreen:
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();

                if (GetOption(fullscreen))
                {
                    // Remember the window position, then re-initialise the video system
                    GetWindowPlacement(g_hwnd, &g_wp);
                    ResizeWindow();
                }
                else
                {
                    SetWindowPlacement(g_hwnd, &g_wp);

                    DWORD dwStyle = (GetWindowStyle(g_hwnd) & WS_VISIBLE) | WS_OVERLAPPEDWINDOW;
                    SetWindowLongPtr(g_hwnd, GWL_STYLE, dwStyle);
                    SetMenu(g_hwnd, g_hmenu);

                    ResizeWindow();
                }
                break;

            case actToggle5_4:
                SetOption(ratio5_4, !GetOption(ratio5_4));
                ResizeWindow();
                Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
                break;

            case actInsertFloppy1:
                InsertDisk(pFloppy1);
                break;

            case actEjectFloppy1:
                EjectDisk(pFloppy1);
                break;

            case actInsertFloppy2:
                InsertDisk(pFloppy2);
                break;

            case actEjectFloppy2:
                EjectDisk(pFloppy2);
                break;

            case actNewDisk1:
                if (ChangesSaved(pFloppy1))
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 1);
                break;

            case actNewDisk2:
                if (ChangesSaved(pFloppy2))
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 2);
                break;

#ifdef USE_LIBSPECTRUM
            case actTapeInsert:
                InsertTape(g_hwnd);
                break;

            case actTapeBrowser:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_TAPE_BROWSER), g_hwnd, TapeBrowseDlgProc, 2);
                break;
#endif
            case actImportData:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_IMPORT), g_hwnd, ImportExportDlgProc, 1);
                break;

            case actExportData:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_EXPORT), g_hwnd, ImportExportDlgProc, 0);
                break;

            case actOptions:
                if (!GUI::IsActive())
                    DisplayOptions();
                break;

            case actExitApplication:
                PostMessage(g_hwnd, WM_CLOSE, 0, 0L);
                break;

            case actPause:
            {
                // Reverse logic as we've not done the default processing yet
                SetWindowText(g_hwnd, g_fPaused ? WINDOW_CAPTION : WINDOW_CAPTION " - Paused");

                // Perform default processing
                return false;
            }

            case actToggleScanlines:
                SetOption(scanlines, !GetOption(scanlines));
                Video::SetDirty();
                Frame::SetStatus("Scanlines %s", GetOption(scanlines) ? "enabled" : "disabled");
                break;

            case actPaste:
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
                            LPWSTR pwsz2 = new WCHAR[nSizeWide*3]; // <= 3 chars transliterated
                            LPWSTR pw = pwsz2;

                            // Count the number or Cyrillic characters in a block, and the number of those capitalised
                            int nCyrillic = 0, nCaps = 0;

                            // Process character by character
                            for (WCHAR wch ; wch = *pwsz ; pwsz++)
                            {
                                // Cyrillic character?
                                bool fCyrillic = wch == 0x0401 || wch >= 0x0410 && wch <= 0x044f || wch == 0x0451;
                                nCyrillic = fCyrillic ? nCyrillic+1 : 0;

                                // Map GBP and (c) directly to SAM codes
                                if (wch == 0x00a3)	// GBP
                                    *pw++ = 0x60;
                                else if (wch == 0x00a9) // (c)
                                    *pw++ = 0x7f;

                                // Cyrillic?
                                else if (fCyrillic)
                                {
                                    // Determine XOR value to preserve case of the source character
                                    char chCase = ~(wch - 0x03d0) & 0x20;
                                    nCaps = chCase ? nCaps+1 : 0;

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
                                        static const char *aszConv[] =
                                        {
                                            "a", "b", "v", "g", "d", "e", "zh", "z",
                                            "i", "j", "k", "l", "m", "n", "o", "p",
                                            "r", "s", "t", "u", "f", "h", "c", "ch",
                                            "sh", "shh", "\"", "y", "'", "e", "yu", "ya"
                                        };

                                        // Look up the transliterated string
                                        const char *psz = aszConv[(wch - 0x0410) & 0x1f];

                                        for (char ch ; (ch = *psz) ; psz++)
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
                            char *pcsz = new char[nSize+1];
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


void CentreWindow (HWND hwnd_, HWND hwndParent_/*=nullptr*/)
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
    int nY = rParent.top + ((rParent.bottom - rParent.top) - (rWindow.bottom - rWindow.top)) * 5/12;

    // Move the window to its new position
    SetWindowPos(hwnd_, nullptr, nX, nY, 0, 0, SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER);
}


LRESULT CALLBACK URLWndProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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

INT_PTR CALLBACK AboutDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HFONT hfont;
    static HWND hwndURL;

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            // Append extra details to the version string
            char szVersion[128];
            GetDlgItemText(hdlg_, IDS_VERSION, szVersion, sizeof(szVersion));
#ifdef _WIN64
            lstrcat(szVersion, " x64");
#endif
            SetDlgItemText(hdlg_, IDS_VERSION, szVersion);

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
                SetTextColor(reinterpret_cast<HDC>(wParam_), RGB(0,0,255));

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
                    Message(msgWarning, "Failed to launch SimCoupe homepage");
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
LRESULT CALLBACK WinKeyHookProc (int nCode_, WPARAM wParam_, LPARAM lParam_)
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
LRESULT CALLBACK WindowProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static bool fInMenu = false, fHideCursor = false;
    static UINT_PTR ulMouseTimer = 0;

    static COwnerDrawnMenu odmenu(nullptr, IDT_MENU, aMenuIcons);

//    TRACE("WindowProc(%#08lx,%#04x,%#08lx,%#08lx)\n", hwnd_, uMsg_, wParam_, lParam_);

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
            if (!ChangesSaved(pFloppy1) || !ChangesSaved(pFloppy2))
                return 0;

            UnhookWindowsHookEx(hWinKeyHook), hWinKeyHook = nullptr;

            PostQuitMessage(0);
            return 0;

        // Main window is being destroyed
        case WM_DESTROY:
            DestroyWindow(hwndCanvas), hwndCanvas = nullptr;
            return 0;

        // System shutting down or using logging out
        case WM_QUERYENDSESSION:
            // Save without prompting, to avoid data loss
            if (pFloppy1) pFloppy1->Save();
            if (pFloppy2) pFloppy2->Save();
            return TRUE;


        // Main window activation change
        case WM_ACTIVATE:
        {
            TRACE("WM_ACTIVATE (%#08lx)\n", wParam_);
            HWND hwndActive = reinterpret_cast<HWND>(lParam_);
            bool fActive = LOWORD(wParam_) != WA_INACTIVE;
            bool fChildOpen = !fActive && hwndActive && GetParent(hwndActive) == hwnd_;

            // Inactive?
            if (!fActive)
            {
                // If a child window is open, silence the sound
                if (fChildOpen)
                    Sound::Silence();
                // If we're in fullscreen mode, minimise the app
                else if (GetOption(fullscreen))
                    ShowWindow(hwnd_, SW_SHOWMINNOACTIVE);
            }

            return 0;
        }

        // File has been dropped on our window
        case WM_DROPFILES:
        {
            char szFile[MAX_PATH]="";

            // Query the first (and only?) file dropped
            if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof(szFile)))
            {
                // Bring our window to the front
                SetForegroundWindow(hwnd_);

                // Insert file as tape or disk, depending on file extension
                if (Tape::IsRecognised(szFile))
                    InsertTape(hwnd_, szFile);
                else
                    InsertDisk(pFloppy1, szFile);
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

            if (!fInMenu && GetOption(fullscreen))
                SetMenu(hwnd_, nullptr);

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

            // Has the mouse moved since last time?
            if ((pt.x != ptLast.x || pt.y != ptLast.y) && !Input::IsMouseAcquired())
            {
                // Show the cursor, but set a timer to hide it if not moved for a few seconds
                fHideCursor = false;
                ulMouseTimer = SetTimer(g_hwnd, MOUSE_TIMER_ID, MOUSE_HIDE_TIME, nullptr);

                // In fullscreen mode, show the popup menu
                if (GetOption(fullscreen) && !GetMenu(g_hwnd))
                    SetMenu(g_hwnd, g_hmenu);

                // Remember the new position
                ptLast = pt;
            }

            return 0;
        }


        case WM_SIZE:
            // Resize the canvas to fit the new main window size
            ResizeCanvas(LOWORD(lParam_), HIWORD(lParam_));
            break;

        case WM_SIZING:
        {
            RECT* pRect = reinterpret_cast<RECT*>(lParam_);

            // Determine the size of the current sizing area
            RECT rWindow = *pRect;
            OffsetRect(&rWindow, -rWindow.left, -rWindow.top);

            // Get the screen size, adjusting for 5:4 mode if necessary
            int nWidth = Frame::GetWidth() >> 1;
            int nHeight = Frame::GetHeight() >> 1;
            if (GetOption(ratio5_4))
                nWidth = MulDiv(nWidth, 5, 4);

            // Determine how big the window would be for an nWidth*nHeight client area
            DWORD dwStyle = GetWindowStyle(hwnd_);
            DWORD dwExStyle = GetWindowExStyle(hwnd_);
            RECT rNonClient = { 0, 0, nWidth, nHeight };
            AdjustWindowRectEx(&rNonClient, dwStyle, TRUE, dwExStyle);
            rNonClient.right += nWindowDx;
            rNonClient.bottom += nWindowDy;
            OffsetRect(&rNonClient, -rNonClient.left, -rNonClient.top);

            // Remove the non-client region to leave just the client area
            rWindow.right -= (rNonClient.right -= nWidth);
            rWindow.bottom -= (rNonClient.bottom -= nHeight);


            // The size adjustment depends on which edge is being dragged
            switch (wParam_)
            {
                case WMSZ_TOP:
                case WMSZ_BOTTOM:
                    rWindow.right = MulDiv(rWindow.bottom, nWidth, nHeight);
                    break;

                case WMSZ_LEFT:
                case WMSZ_RIGHT:
                    rWindow.bottom = MulDiv(rWindow.right, nHeight, nWidth);
                    break;

                default:
                    // With a diagonal drag the larger of the axis sizes (width:height adjusted) is used
                    if (MulDiv(rWindow.right, nHeight, nWidth) > rWindow.bottom)
                        rWindow.bottom = MulDiv(rWindow.right, nHeight, nWidth);
                    else
                        rWindow.right = MulDiv(rWindow.bottom, nWidth, nHeight);
                    break;
            }


            // Work out the the nearest scaling option factor  (multiple of half size, with 1 as the minimum)
            int nScale = (rWindow.right + (nWidth >> 1)) / nWidth;
            SetOption(scale, nScale + !nScale);

            // Holding shift permits free scaling
            if (GetAsyncKeyState(VK_SHIFT) < 0)
            {
                // Use the new size if at least 1x
                if (rWindow.right >= nWidth)
                {
                    if (rWindow.bottom != nHeight*GetOption(scale))
                        SetOption(scale, 0);

                    nWidth = rWindow.right;
                    nHeight = rWindow.bottom;
                }
            }
            // Otherwise stick to exact multiples only
            else
            {
                // Form the new client size
                nWidth *= GetOption(scale);
                nHeight *= GetOption(scale);
            }

            // Add the non-client region back on to give a full window size
            nWidth += rNonClient.right;
            nHeight += rNonClient.bottom;

            // Adjust the appropriate part of sizing area, depending on the drag corner again
            switch (wParam_)
            {
                case WMSZ_TOPLEFT:
                    pRect->top = pRect->bottom - nHeight;
                    pRect->left = pRect->right - nWidth;
                    break;

                case WMSZ_TOP:
                case WMSZ_TOPRIGHT:
                    pRect->top = pRect->bottom - nHeight;
                    pRect->right = pRect->left + nWidth;
                    break;

                case WMSZ_LEFT:
                case WMSZ_BOTTOMLEFT:
                    pRect->bottom = pRect->top + nHeight;
                    pRect->left = pRect->right - nWidth;

                case WMSZ_BOTTOM:
                case WMSZ_RIGHT:
                case WMSZ_BOTTOMRIGHT:
                    pRect->bottom = pRect->top + nHeight;
                    pRect->right = pRect->left + nWidth;
                    break;
            }

            return TRUE;
        }


        // Menu is about to be activated
        case WM_INITMENU:
            UpdateMenuFromOptions();
            break;

        // Menu has been opened
        case WM_ENTERMENULOOP:
            fInMenu = true;
            Sound::Silence();
            break;

        case WM_EXITMENULOOP:
            // No longer in menu, so start timer to hide the mouse if not used again
            fInMenu = fHideCursor = false;
            ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, 1, nullptr);

            // Purge any menu navigation key presses
            Input::Purge();
            break;


        // To avoid flicker, don't erase the background
        case WM_ERASEBKGND:
        {
            RECT rMain, rCanvas;
            HDC hdc = reinterpret_cast<HDC>(wParam_);
            HBRUSH hbrush = GetStockBrush(BLACK_BRUSH);

            // Fetch main window client area, and convert canvas client to same coords
            GetClientRect(hwnd_, &rMain);
            GetClientRect(hwndCanvas, &rCanvas);
            MapWindowRect(hwndCanvas, hwnd_, &rCanvas);

            // Determine borders around the canvas window
            RECT rLeft = { 0, 0, rCanvas.left, rMain.bottom };
            RECT rTop = { 0, 0, rMain.right, rCanvas.top };
            RECT rRight = { rCanvas.right, 0, rMain.right, rMain.bottom };
            RECT rBottom = { 0, rCanvas.bottom, rMain.right, rMain.bottom };

            // Clear any that exist with black
            if (!IsRectEmpty(&rLeft)) FillRect(hdc, &rLeft, hbrush);
            if (!IsRectEmpty(&rTop)) FillRect(hdc, &rTop, hbrush);
            if (!IsRectEmpty(&rRight)) FillRect(hdc, &rRight, hbrush);
            if (!IsRectEmpty(&rBottom)) FillRect(hdc, &rBottom, hbrush);

            return 1;
        }

        // Silence the sound during window drags, and other clicks in the non-client area
        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN:
            Sound::Silence();
            break;

        case WM_SYSCOMMAND:
            // Is this an Alt-key combination?
            if ((wParam_ & 0xfff0) == SC_KEYMENU)
            {
                // Ignore the key if Ctrl is pressed, to avoid Win9x problems with AltGr activating the menu
                if ((GetAsyncKeyState(VK_CONTROL) < 0) || GetAsyncKeyState(VK_RMENU) < 0)
                    return 0;

                // If Alt alone is pressed, ensure the menu is visible (for fullscreen)
                if ((!GetOption(altforcntrl) || !lParam_) && !GetMenu(hwnd_))
                    SetMenu(hwnd_, g_hmenu);

                // Stop Windows processing SAM Cntrl-key combinations (if enabled) and Alt-Enter
                // As well as blocking access to the menu it avoids a beep for the unhandled ones (mainly Alt-Enter)
                if ((GetOption(altforcntrl) && lParam_) || lParam_ == VK_RETURN)
                    return 0;

                // Alt-0 to Alt-5 set the zoom level
                if (lParam_ >= '0' && lParam_ <= '5')
                    return SendMessage(hwnd_, WM_COMMAND, IDM_VIEW_ZOOM_50 + lParam_ - '0', 0L);
            }
            // Maximize?
            else if (wParam_ == SC_MAXIMIZE)
            {
                // Shift pressed?
                if (GetAsyncKeyState(VK_SHIFT) < 0)
                {
                    // Toggle fullscreen instead
                    Action::Do(actToggleFullscreen);
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
                Action::Do(actToggleFullscreen);

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
                Action::Do(actPause);

            // Read the current states of the shift keys
            bool fCtrl  = GetAsyncKeyState(VK_CONTROL) < 0;
            bool fAlt   = GetAsyncKeyState(VK_MENU)    < 0;
            bool fShift = GetAsyncKeyState(VK_SHIFT)   < 0;

            // Function key?
            if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            {
                // Ignore Windows-modified function keys unless the SAM keypad mapping is enabled
                if ((GetAsyncKeyState(VK_LWIN) < 0 || GetAsyncKeyState(VK_RWIN) < 0) && wParam_ <= VK_F10)
                    return 0;

                Action::Key((int)wParam_-VK_F1+1, fPress, fCtrl, fAlt, fShift);
                return 0;
            }

            // Most of the emulator keys are handled above, but we've a few extra fixed mappings of our own (well, mine!)
            switch (wParam_)
            {
                case VK_DIVIDE:     Action::Do(actDebugger, fPress); break;
                case VK_MULTIPLY:   Action::Do(fCtrl ? actResetButton : actTempTurbo, fPress); break;
                case VK_ADD:		Action::Do(fCtrl ? actTempTurbo : actSpeedFaster, fPress); break;
                case VK_SUBTRACT:   Action::Do(fCtrl ? actSpeedNormal : actSpeedSlower, fPress); break;

                case VK_CANCEL:
                case VK_PAUSE:
                    if (fPress)
                    {
                        // Ctrl-Break is used for reset
                        if (GetAsyncKeyState(VK_CONTROL) < 0)
                            CPU::Init();

                        // Pause toggles pause mode
                        else
                            Action::Do(actPause);
                    }
                    break;

                case VK_SNAPSHOT:
                case VK_SCROLL:
                    if (!fPress)
                        Action::Do(actSaveScreenshot);
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
                    case IDM_FILE_IMPORT_DATA:      GUI::Start(new CImportDialog);      return 0;
                    case IDM_FILE_EXPORT_DATA:      GUI::Start(new CExportDialog);      return 0;
                    case IDM_FILE_FLOPPY1_INSERT:   GUI::Start(new CInsertFloppy(1));   return 0;
                    case IDM_FILE_FLOPPY2_INSERT:   GUI::Start(new CInsertFloppy(2));   return 0;
                    case IDM_TOOLS_OPTIONS:         GUI::Start(new COptionsDialog);     return 0;
					case IDM_TOOLS_DEBUGGER:        GUI::Start(new CDebugger);          return 0;
                    case IDM_HELP_ABOUT:            GUI::Start(new CAboutDialog);       return 0;

                    case IDM_FILE_NEW_DISK1:        GUI::Start(new CNewDiskDialog(1));  return 0;
                    case IDM_FILE_NEW_DISK2:        GUI::Start(new CNewDiskDialog(2));  return 0;
                    case IDM_TOOLS_TAPE_BROWSER:    GUI::Start(new CInsertTape());      return 0; // no tape browser yet
                }
            }

            switch (wId)
            {
                case IDM_FILE_NEW_DISK1:        Action::Do(actNewDisk1);          break;
                case IDM_FILE_NEW_DISK2:        Action::Do(actNewDisk2);          break;
                case IDM_FILE_IMPORT_DATA:      Action::Do(actImportData);        break;
                case IDM_FILE_EXPORT_DATA:      Action::Do(actExportData);        break;
                case IDM_FILE_EXIT:             Action::Do(actExitApplication);   break;

                case IDM_RECORD_AVI_START:      Action::Do(actRecordAvi);         break;
                case IDM_RECORD_AVI_HALF:       Action::Do(actRecordAviHalf);     break;
                case IDM_RECORD_AVI_STOP:       Action::Do(actRecordAviStop);     break;

                case IDM_RECORD_GIF_START:      Action::Do(actRecordGif);         break;
                case IDM_RECORD_GIF_LOOP:       Action::Do(actRecordGifLoop);     break;
                case IDM_RECORD_GIF_STOP:       Action::Do(actRecordGifStop);     break;

                case IDM_RECORD_WAV_START:      Action::Do(actRecordWav);         break;
                case IDM_RECORD_WAV_SEGMENT:    Action::Do(actRecordWavSegment);  break;
                case IDM_RECORD_WAV_STOP:       Action::Do(actRecordWavStop);     break;

                case IDM_TOOLS_OPTIONS:         Action::Do(actOptions);           break;
                case IDM_TOOLS_PASTE_CLIPBOARD: Action::Do(actPaste);             break;
                case IDM_TOOLS_PRINTER_ONLINE:  Action::Do(actPrinterOnline);     break;
                case IDM_TOOLS_FLUSH_PRINTER:   Action::Do(actFlushPrinter);      break;
                case IDM_TOOLS_TAPE_BROWSER:    Action::Do(actTapeBrowser);       break;
                case IDM_TOOLS_DEBUGGER:        Action::Do(actDebugger);          break;

                case IDM_FILE_FLOPPY1_DEVICE:
                    if (!CFloppyStream::IsAvailable())
                    {
                        if (MessageBox(hwnd_, "Real floppy disk support requires a 3rd party driver.\n"
                                              "\n"
                                              "Visit the website to to download it?",
                                              "fdrawcmd.sys not found",
                                              MB_ICONQUESTION|MB_YESNO) == IDYES)
                            ShellExecute(nullptr, nullptr, "http://simonowen.com/fdrawcmd/", nullptr, "", SW_SHOWMAXIMIZED);
                    }
                    else if (GetOption(drive1) == drvFloppy && ChangesSaved(pFloppy1) && pFloppy1->Insert("A:"))
                        Frame::SetStatus("Using floppy drive %s", pFloppy1->DiskFile());
                    break;

                case IDM_FILE_FLOPPY1_INSERT:       Action::Do(actInsertFloppy1); break;
                case IDM_FILE_FLOPPY1_EJECT:        Action::Do(actEjectFloppy1);  break;
                case IDM_FILE_FLOPPY1_SAVE_CHANGES: Action::Do(actSaveFloppy1);   break;

                case IDM_FILE_FLOPPY2_INSERT:       Action::Do(actInsertFloppy2); break;
                case IDM_FILE_FLOPPY2_EJECT:        Action::Do(actEjectFloppy2);  break;
                case IDM_FILE_FLOPPY2_SAVE_CHANGES: Action::Do(actSaveFloppy2);   break;

                case IDM_VIEW_FULLSCREEN:           Action::Do(actToggleFullscreen); break;
                case IDM_VIEW_RATIO54:              Action::Do(actToggle5_4);        break;
                case IDM_VIEW_SCANLINES:            Action::Do(actToggleScanlines);  break;
                case IDM_VIEW_SCANHIRES:            Action::Do(actToggleScanHiRes);  break;
                case IDM_VIEW_FILTER:               Action::Do(actToggleFilter);     break;
                case IDM_VIEW_GREYSCALE:            Action::Do(actToggleGreyscale);  break;

                case IDM_VIEW_ZOOM_50:
                case IDM_VIEW_ZOOM_100:
                case IDM_VIEW_ZOOM_150:
                case IDM_VIEW_ZOOM_200:
                case IDM_VIEW_ZOOM_250:
                case IDM_VIEW_ZOOM_300:
                    SetOption(scale, wId-IDM_VIEW_ZOOM_50+1);
                    ResizeWindow(Frame::GetHeight()*GetOption(scale)/2);
                    break;

                case IDM_VIEW_BORDERS0:
                case IDM_VIEW_BORDERS1:
                case IDM_VIEW_BORDERS2:
                case IDM_VIEW_BORDERS3:
                case IDM_VIEW_BORDERS4:
                    SetOption(borders, wId-IDM_VIEW_BORDERS0);
                    Frame::Init();
                    ResizeWindow(Frame::GetHeight()*GetOption(scale)/2);
                    break;

                case IDM_SYSTEM_PAUSE:      Action::Do(actPause);           break;
                case IDM_SYSTEM_MUTESOUND:  Action::Do(actToggleMute);      break;
                case IDM_SYSTEM_NMI:        Action::Do(actNmiButton);       break;
                case IDM_SYSTEM_RESET:      Action::Do(actResetButton); Action::Do(actResetButton,false); break;

                // Items from help menu
                case IDM_HELP_GENERAL:
                    if (ShellExecute(hwnd_, nullptr, OSD::MakeFilePath(MFP_RESOURCE, "SimCoupe.txt"), nullptr, "", SW_SHOWMAXIMIZED) <= reinterpret_cast<HINSTANCE>(32))
                        MessageBox(hwnd_, "Can't find SimCoupe.txt", WINDOW_CAPTION, MB_ICONEXCLAMATION);
                    break;
                case IDM_HELP_ABOUT:    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_ABOUT), hwnd_, AboutDlgProc, 0);   break;

                default:
                    if (wId >= IDM_FILE_RECENT1 && wId <= IDM_FILE_RECENT9)
                    {
                        const char *pcszFile = szRecentFiles[wId - IDM_FILE_RECENT1];
                        if (Tape::IsRecognised(pcszFile))
                            InsertTape(hwnd_, pcszFile);
                        else
                            InsertDisk(pFloppy1, pcszFile);
                    }
                    else if (wId >= IDM_FLOPPY2_RECENT1 && wId <= IDM_FLOPPY2_RECENT9)
                    {
                        const char *pcszFile = szRecentFiles[wId - IDM_FLOPPY2_RECENT1];
                        if (Tape::IsRecognised(pcszFile))
                            InsertTape(hwnd_, pcszFile);
                        else
                            InsertDisk(pFloppy2, pcszFile);
                    }
                    else if (wId >= IDM_SYSTEM_SPEED_50 && wId <= IDM_SYSTEM_SPEED_1000)
                    {
                        int anSpeeds[] = { 50, 100, 200, 300, 500, 1000 };
                        SetOption(speed, anSpeeds[wId-IDM_SYSTEM_SPEED_50]);
                        Frame::SetStatus("%u%% Speed", GetOption(speed));
                    }
                    break;
            }
            break;
        }
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}


// Handle messages for the canvas window
LRESULT CALLBACK CanvasWindowProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    // Pass mouse messages to the main window
    if (uMsg_ >= WM_MOUSEFIRST && uMsg_ <= WM_MOUSELAST)
        return WindowProc(hwnd_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_CREATE:
            return 0;

        // To avoid flicker, don't erase the background
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            Frame::Redraw();
            ValidateRect(hwnd_, nullptr);
            break;

        case WM_SIZE:
            Video::UpdateSize();
            break;

    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}



void SaveWindowPosition (HWND hwnd_)
{
    char sz[128];
    WINDOWPLACEMENT wp = { sizeof(wp) };
    RECT *pr = &wp.rcNormalPosition;
    GetWindowPlacement(hwnd_, &wp);

    wsprintf(sz, "%d,%d,%d,%d,%d", pr->left, pr->top, pr->right-pr->left, pr->bottom-pr->top, wp.showCmd == SW_SHOWMAXIMIZED);
    SetOption(windowpos, sz);
}

bool RestoreWindowPosition (HWND hwnd_)
{
    int nX, nY, nW, nH, nMax;
    if (sscanf(GetOption(windowpos), "%d,%d,%d,%d,%d", &nX, &nY, &nW, &nH, &nMax) != 5)
        return false;

    WINDOWPLACEMENT wp = { sizeof(wp) };
    SetRect(&wp.rcNormalPosition, nX, nY, nX+nW, nY+nH);
    wp.showCmd = nMax ? SW_MAXIMIZE : SW_SHOW;
    SetWindowPlacement(hwnd_, &wp);

    return true;
}


bool InitWindow ()
{
    // Set up and register window class
    WNDCLASS wc = { };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = __hinstance;
    wc.hIcon = LoadIcon(__hinstance, MAKEINTRESOURCE(IDI_MAIN));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "SimCoupeClass";
    RegisterClass(&wc);

    g_hmenu = LoadMenu(wc.hInstance, MAKEINTRESOURCE(IDR_MENU));

    int nWidth = Frame::GetWidth() * GetOption(scale) / 2;
    int nHeight = Frame::GetHeight() * GetOption(scale) / 2;
    int nInitX = (GetSystemMetrics(SM_CXSCREEN) - nWidth) / 2;
    int nInitY = (GetSystemMetrics(SM_CYSCREEN) - nHeight) * 5/12;

    // Create a window for the display (initially invisible)
    g_hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, WINDOW_CAPTION, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                            nInitX, nInitY, 1, 1, nullptr, g_hmenu, wc.hInstance, nullptr);

    wc.lpfnWndProc = CanvasWindowProc;
    wc.hIcon = nullptr;
    wc.lpszClassName = "SimCoupeCanvas";
    RegisterClass(&wc);

    // Create the canvas child window to hold the emulation view
    hwndCanvas = CreateWindow(wc.lpszClassName, "", WS_CHILD|WS_VISIBLE, 0,0, 0,0, g_hwnd, nullptr, wc.hInstance, nullptr);

    // Restore the window position, falling back on the current options to determine its size
    if (!RestoreWindowPosition(g_hwnd))
        ResizeWindow(nHeight);

    return g_hwnd && hwndCanvas;
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath (char* pszPath_, size_t nLen_)
{
    char *psz1 = nullptr, *psz2 = nullptr;

    // Accept regular and UNC paths only
    if (lstrlen(pszPath_) < 3)
        return;
    else if (!memcmp(pszPath_+1, ":\\", 2))
        psz1 = pszPath_+2;
    else if (memcmp(pszPath_, "\\\\", 2))
        return;
    else
    {
        for (psz1 = pszPath_+2 ; *psz1 && *psz1 != '\\' ; psz1++);
        for (psz1++ ; *psz1 && *psz1 != '\\' ; psz1++);

        if (!*psz1)
            return;
    }

    // Adjust the length for the prefix we skipped
    nLen_ -= (psz1 - pszPath_);

    // Search the rest of the path string
    for (char* p = psz1 ; *p ; p = CharNext(p))
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
    if (psz2 && (psz2-psz1) > 4)
    {
        // Clip and join the remaining sections
        lstrcpy(psz1+1, "...");
        lstrcpy(psz1+4, psz2);
    }
}

void GetDlgItemPath (HWND hdlg_, int nId_, char* psz_, int nSize_)
{
    HWND hctrl = nId_ ? GetDlgItem(hdlg_, nId_) : hdlg_;
    GetWindowText(hctrl, psz_, nSize_);
}

void SetDlgItemPath (HWND hdlg_, int nId_, const char* pcsz_, bool fSelect_=false)
{
    HWND hctrl = nId_ ? GetDlgItem(hdlg_, nId_) : hdlg_;
    SetWindowText(hctrl, pcsz_);

    if (fSelect_)
    {
        SendMessage(hctrl, EM_SETSEL, 0, -1);
        SetFocus(hctrl);
    }
}

int GetDlgItemValue (HWND hdlg_, int nId_, int nDefault_=-1)
{
    char sz[256], *pEnd = nullptr;
    GetDlgItemText(hdlg_, nId_, sz, sizeof(sz));

    long lValue = strtol(sz, &pEnd, 0);
    return *pEnd ? nDefault_ : static_cast<int>(lValue);
}

void SetDlgItemValue (HWND hdlg_, int nId_, int nValue_, int nBytes_)
{
    char sz[256];
    wsprintf(sz, (nBytes_ == 1) ? "%02X" : (nBytes_ == 2) ? "%04X" : "%06X", nValue_);
    SetDlgItemText(hdlg_, nId_, sz);
}


// Helper function for filling a combo-box with strings and selecting one
void SetComboStrings (HWND hdlg_, UINT uID_, const char** ppcsz_, int nDefault_/*=0*/)
{
    HWND hwndCombo = GetDlgItem(hdlg_, uID_);

    // Clear any existing contents
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);

    // Add each string from the list
    while (*ppcsz_)
        SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(*ppcsz_++));

    // Select the specified default, or the first item if there wasn't one
    SendMessage(hwndCombo, CB_SETCURSEL, (nDefault_ == -1) ? 0 : nDefault_, 0L);
}

const char *GetComboText (HWND hdlg_, UINT uID_, int nMaxLen_)
{
    static char sz[MAX_PATH];
    sz[0] = '\0';

    if (nMaxLen_ > MAX_PATH)
        nMaxLen_ = MAX_PATH;

    // Fetch the current selection
    HWND hwndCombo = GetDlgItem(hdlg_, uID_);
    int nSel = static_cast<int>(SendMessage(hwndCombo, CB_GETCURSEL, 0, 0L));

    // Fetch the item text if it's not too long
    if (SendMessage(hwndCombo, CB_GETLBTEXTLEN, nSel, 0L) < nMaxLen_)
        GetWindowText(hwndCombo, sz, nMaxLen_);

    // Clear the entry if it's a placeholder
    if (!lstrcmpi(sz, "<None>"))
        sz[0] = '\0';

    return sz;
}

void AddComboString (HWND hdlg_, UINT uID_, const char* pcsz_)
{
    SendDlgItemMessage(hdlg_, uID_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pcsz_));
}

void AddComboExString (HWND hdlg_, UINT uID_, const char* pcsz_)
{
    COMBOBOXEXITEM cbei = {};
    cbei.iItem = -1;
    cbei.mask = CBEIF_TEXT;
    cbei.pszText = const_cast<char*>(pcsz_);

    SendDlgItemMessage(hdlg_, uID_, CBEM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&cbei));
}


void FillMidiOutCombo (HWND hwndCombo_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    int nDevs = midiOutGetNumDevs();
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nDevs ? "None" : "<None>"));

    for (int i = 0 ; i < nDevs ; i++)
    {
        MIDIOUTCAPS mc;
        if (midiOutGetDevCaps(i, &mc, sizeof(mc)) == MMSYSERR_NOERROR)
            SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mc.szPname));
    }

    // Select the current device in the list, or the first one if that fails
    if (SendMessage(hwndCombo_, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(GetOption(midioutdev))) == CB_ERR)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);

    // Enable combo if there are devices, disable otherwise
    EnableWindow(hwndCombo_, nDevs != 0);
}


void FillPrintersCombo (HWND hwndCombo_, LPCSTR pcszSelected_)
{
    int lSelected = 0;
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    // The first entry is to allow printing to auto-generated prntNNNN.bin files
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("File: simcNNNN.txt (auto-generated)"));

    // Select the active printer
    SendMessage(hwndCombo_, CB_SETCURSEL, lSelected, 0L);
}


void FillJoystickCombo (HWND hwndCombo_, const char* pcszSelected_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);
    Input::FillJoystickCombo(hwndCombo_);

    int nItems = static_cast<int>(SendMessage(hwndCombo_, CB_GETCOUNT, 0, 0L));
    EnableWindow(hwndCombo_, nItems != 0);
    SendMessage(hwndCombo_, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(nItems ? "None" : "<None>"));

    // Select the current item, falling back on the first entry (None) if it's missing
    if (SendMessage(hwndCombo_, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(pcszSelected_)) == CB_ERR)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);
}


////////////////////////////////////////////////////////////////////////////////

// Browse for an image, setting a specified filter with the path selected
void BrowseImage (HWND hdlg_, int nControl_, const char* pcszFilters_)
{
    char szFile[MAX_PATH] = "";

    GetDlgItemText(hdlg_, nControl_, szFile, sizeof(szFile));

    OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner = hdlg_;
    ofn.lpstrFilter = pcszFilters_;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = 0;

    if (GetSaveLoadFile(&ofn, true))
        SetDlgItemPath(hdlg_, nControl_, szFile);
}

BOOL BadField (HWND hdlg_, int nId_)
{
    HWND hctrl = GetDlgItem(hdlg_, nId_);
    SendMessage(hctrl, EM_SETSEL, 0, -1);
    SetFocus(hctrl);
    MessageBeep(MB_ICONHAND);
    return FALSE;
}


INT_PTR CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static char szFile[MAX_PATH], szAddress[128]="32768", szPage[128]="1", szOffset[128]="0", szLength[128]="16384";
    static int nType = 0;
    static bool fImport;
    static POINT apt[2];

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            CentreWindow(hdlg_);
            fImport = !!lParam_;

            static const char* asz[] = { "BASIC Address (0-540671)", "Main Memory (pages 0-31)", "External RAM (pages 0-255)", nullptr };
            SetComboStrings(hdlg_, IDC_TYPE, asz, nType);

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
                    int nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPE, CB_GETCURSEL, 0, 0L);
                    int nShow1 = !nType ? SW_SHOW : SW_HIDE, nShow2 = nShow1^SW_SHOW, i;

                    int an1[] = { IDS_ADDRESS, IDE_ADDRESS, IDS_LENGTH2, IDE_LENGTH2 };
                    int an2[] = { IDS_PAGE, IDE_PAGE, IDS_OFFSET, IDE_OFFSET, IDS_LENGTH, IDE_LENGTH };

                    for (i = 0 ; i < _countof(an1) ; i++)
                        ShowWindow(GetDlgItem(hdlg_, an1[i]), nShow1);

                    for (i = 0 ; i < _countof(an2) ; i++)
                        ShowWindow(GetDlgItem(hdlg_, an2[i]), nShow2);

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
                        SetDlgItemText(hdlg_, wControl^IDE_LENGTH^IDE_LENGTH2, sz);
                        fUpdating = false;
                    }
                    break;
                }

                case IDOK:
                {
                    GetDlgItemText(hdlg_, IDE_ADDRESS, szAddress, sizeof(szAddress));
                    GetDlgItemText(hdlg_, IDE_PAGE,    szPage,    sizeof(szPage));
                    GetDlgItemText(hdlg_, IDE_OFFSET,  szOffset,  sizeof(szOffset));
                    GetDlgItemText(hdlg_, IDE_LENGTH,  szLength,  sizeof(szLength));

                    nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPE, CB_GETCURSEL, 0, 0L);
                    int nAddress = GetDlgItemValue(hdlg_, IDE_ADDRESS);
                    int nPage    = GetDlgItemValue(hdlg_, IDE_PAGE);
                    int nOffset  = GetDlgItemValue(hdlg_, IDE_OFFSET);
                    int nLength  = GetDlgItemValue(hdlg_, IDE_LENGTH);

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

                    FILE *f;
                    if (!GetSaveLoadFile(&ofn, fImport))
                    {
                        EndDialog(hdlg_, 0);
                        return TRUE;
                    }
                    else if (!(f = fopen(szFile, fImport ? "rb" : "wb")))
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
                        for (int nChunk ; (nChunk = std::min(nLength, (0x4000 - nOffset))) ; nLength -= nChunk, nOffset = 0)
                        {
                            nDone += fread(PageWritePtr(nPage++)+nOffset, 1, nChunk, f);

                            // Wrap to page 0 after ROM0
                            if (nPage == ROM0+1)
                                nPage = 0;

                            // Stop at the end of the file or if we've hit the end of a logical block
                            if (feof(f) || nPage == EXTMEM || nPage >= ROM0)
                                break;
                        }

                        Frame::SetStatus("Imported %d bytes", nDone);
                    }
                    else
                    {
                        for (int nChunk ; (nChunk = std::min(nLength, (0x4000 - nOffset))) ; nLength -= nChunk, nOffset = 0)
                        {
                            nDone += fwrite(PageReadPtr(nPage++)+nOffset, 1, nChunk, f);

                            if (ferror(f))
                            {
                                MessageBox(hdlg_, "Error writing to file", "Export Data", MB_ICONEXCLAMATION);
                                fclose(f);
                                return FALSE;
                            }

                            // Wrap to page 0 after ROM0
                            if (nPage == ROM0+1)
                                nPage = 0;

                            // Stop if we've hit the end of a logical block
                            if (nPage == EXTMEM || nPage == ROM0)
                                break;
                        }

                        Frame::SetStatus("Exported %d bytes", nDone);
                    }

                    fclose(f);
                    return EndDialog(hdlg_, 1);
                }
            }
        }
    }

    return FALSE;
}


INT_PTR CALLBACK NewDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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

            static const char* aszTypes[] = { "MGT disk image (800K)", "EDSK disk image (flexible format)", "DOS CP/M image (720K)", nullptr };
            SetComboStrings(hdlg_, IDC_TYPES, aszTypes, nType);
            SendMessage(hdlg_, WM_COMMAND, IDC_TYPES, 0L);

            SendDlgItemMessage(hdlg_, IDC_FORMAT, BM_SETCHECK, fFormat ? BST_CHECKED : BST_UNCHECKED, 0L);

#ifdef USE_ZLIB
            SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_SETCHECK, fCompress ? BST_CHECKED : BST_UNCHECKED, 0L);
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
                    nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPES, CB_GETCURSEL, 0, 0L);

                    // Enable the format checkbox for EDSK only
                    EnableWindow(GetDlgItem(hdlg_, IDC_FORMAT), nType == 1);

                    // Enable formatting for non-EDSK
                    if (nType != 1)
                        SendDlgItemMessage(hdlg_, IDC_FORMAT, BM_SETCHECK, BST_CHECKED, 0L);

                    // Enable the compress checkbox for MGT only
                    EnableWindow(GetDlgItem(hdlg_, IDC_COMPRESS), nType == 0);

                    // Disable compression for non-MGT
                    if (nType != 0)
                        SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_SETCHECK, BST_UNCHECKED, 0L);

                    break;

                case IDOK:
                {
                    // File extensions for each type, plus an additional extension if compressed
                    static const char* aszTypes[] = { ".mgt", ".dsk", ".cpm" };

                    nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPES, CB_GETCURSEL, 0, 0L);
                    fCompress = SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_GETCHECK, 0, 0L) == BST_CHECKED;
                    fFormat = SendDlgItemMessage(hdlg_, IDC_FORMAT, BM_GETCHECK, 0, 0L) == BST_CHECKED;

                    char szFile [MAX_PATH] = {};
                    snprintf(szFile, MAX_PATH-1, "untitled%s", aszTypes[nType]);

                    OPENFILENAME ofn = { sizeof(ofn) };
                    ofn.hwndOwner = hdlg_;
                    ofn.lpstrFilter = szNewDiskFilters;
                    ofn.nFilterIndex = nType+1;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.Flags = OFN_HIDEREADONLY;

                    if (!GetSaveLoadFile(&ofn, false))
                        break;

                    // Fetch the file type, in case it's changed
                    nType = ofn.nFilterIndex-1;

                    // Append the appropriate file extension if it's not already present
                    const char *pcszExt = strrchr(szFile, '.');
                    if (!pcszExt || (pcszExt && lstrcmpi(pcszExt, aszTypes[nType])))
                        lstrcat(szFile, aszTypes[nType]);

                    CStream* pStream = nullptr;
                    CDisk* pDisk = nullptr;
#ifdef USE_ZLIB
                    if (nType == 0 && fCompress)
                        pStream = new CZLibStream(nullptr, szFile);
                    else
#endif
                        pStream = new CFileStream(nullptr, szFile);

                    switch (nType)
                    {
                        case 0: pDisk = new CMGTDisk(pStream); break;
                        default:
                        case 1: pDisk = new CEDSKDisk(pStream);  break;
                        case 2: pDisk = new CMGTDisk(pStream, DOS_DISK_SECTORS); break;
                    }

                    // Format the EDSK image ready for use?
                    if (nType == 1 && fFormat)
                    {
                        IDFIELD abIDs[NORMAL_DISK_SECTORS];

                        // Create a data track to use during the format
                        BYTE abSector[NORMAL_SECTOR_SIZE], *apbData[NORMAL_DISK_SECTORS];
                        memset(abSector, 0, sizeof(abSector));

                        // Prepare the tracks across the disk
                        for (BYTE head = 0 ; head < NORMAL_DISK_SIDES ; head++)
                        {
                            for (BYTE cyl = 0 ; cyl < NORMAL_DISK_TRACKS ; cyl++)
                            {
                                for (BYTE sector = 0 ; sector < NORMAL_DISK_SECTORS ; sector++)
                                {
                                    abIDs[sector].bTrack = cyl;
                                    abIDs[sector].bSide = head;
                                    abIDs[sector].bSector = 1 + ((sector + NORMAL_DISK_SECTORS - (cyl%NORMAL_DISK_SECTORS)) % NORMAL_DISK_SECTORS);
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
                    delete pDisk;

                    // If the save failed, moan about it
                    if (!fSaved)
                    {
                        Message(msgWarning, "Failed to save to %s\n", szFile);
                        break;
                    }

                    // Insert into the appropriate drive
                    CDiskDevice *pDrive = (nDrive == 1) ? pFloppy1 : pFloppy2;
                    InsertDisk(pDrive, szFile);

                    EndDialog(hdlg_, 1);
                    break;
                }
            }
        }
    }

    return FALSE;
}


INT_PTR CALLBACK HardDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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
            GetDlgItemPath(hwndEdit, 0, szFile, sizeof(szFile));
            SetDlgItemPath(hdlg_, IDE_FILE, szFile);

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

                    GetDlgItemPath(hdlg_, IDE_FILE, szFile, sizeof(szFile));

                    // If we can, open the existing hard disk image to retrieve the geometry
                    CHardDisk* pDisk = CHardDisk::OpenObject(szFile);
                    bool fExists = pDisk != nullptr;

                    if (pDisk)
                    {
                        // Fetch the existing disk geometry
                        const ATA_GEOMETRY* pGeom = pDisk->GetGeometry ();
                        uSize = (pGeom->uTotalSectors + (1 << 11)-1) >> 11;
                        SetDlgItemInt(hdlg_, IDE_SIZE, uSize, FALSE);

                        delete pDisk;
                    }

                    // The geometry is read-only for existing images
                    EnableWindow(GetDlgItem(hdlg_, IDE_SIZE), !fExists);

                    // Use an OK button to accept an existing file, or Create for a new one
                    SetDlgItemText(hdlg_, IDOK, fExists || !szFile[0] ? "OK" : "Create");
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
                        SetDlgItemPath(hdlg_, IDE_FILE, szFile, true);

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
                        if (!uTotalSectors || (uTotalSectors > (16383*16*63)))
                        {
                            MessageBox(hdlg_, "Invalid disk size", "Create", MB_OK|MB_ICONEXCLAMATION);
                            break;
                        }

                        // Warn before overwriting existing files
                        if (!::stat(szFile, &st) &&
                                MessageBox(hdlg_, "Overwrite existing file?", "Create", MB_YESNO|MB_ICONEXCLAMATION) != IDYES)
                            break;

                        // Create the new HDF image
                        else if (!CHDFHardDisk::Create(szFile, uTotalSectors))
                        {
                            MessageBox(hdlg_, "Failed to create new disk (disk full?)", "Create", MB_OK|MB_ICONEXCLAMATION);
                            break;
                        }
                    }

                    // Set the new path back in the parent dialog, and close our dialog
                    SetDlgItemPath(hwndEdit, 0, szFile, true);
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
INT_PTR CALLBACK BasePageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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
                    for (nOptionPage = MAX_OPTION_PAGES-1 ; nOptionPage && ahwndPages[nOptionPage] != hdlg_ ; nOptionPage--);
                    break;
            }
        }
    }

    return fRet;
}


INT_PTR CALLBACK SystemPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            CheckRadioButton(hdlg_, IDR_256K,IDR_512K, (GetOption(mainmem) == 256) ? IDR_256K : IDR_512K);
            SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_SETRANGE, 0, MAKELONG(0, 4));
            SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_SETPOS, TRUE, GetOption(externalmem));

            // Set the current custom ROM path, and enable filesystem auto-complete
            SetDlgItemPath(hdlg_, IDE_ROM, GetOption(rom));
            SHAutoComplete(GetDlgItem(hdlg_, IDE_ROM), SHACF_FILESYS_ONLY|SHACF_USETAB);
            SendDlgItemMessage(hdlg_, IDE_ROM, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

            SendDlgItemMessage(hdlg_, IDC_ALBOOT_ROM, BM_SETCHECK, GetOption(albootrom) ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);
            if (pnmh->idFrom == IDC_EXTERNAL)
            {
                char sz[16];

                int nExternalMB = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_GETPOS, 0, 0L));
                snprintf(sz, sizeof(sz), "%dMB", nExternalMB);
                SetDlgItemText(hdlg_, IDS_EXTERNAL, sz);
            }
            else if (pnmh->code == PSN_APPLY)
            {
                SetOption(mainmem, (SendDlgItemMessage(hdlg_, IDR_256K, BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 256 : 512);
                SetOption(externalmem, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL, TBM_GETPOS, 0, 0L)));

                // If the memory configuration has changed, apply the changes
                if (Changed(mainmem) || Changed(externalmem))
                    Memory::UpdateConfig();

                GetDlgItemPath(hdlg_, IDE_ROM, const_cast<char*>(GetOption(rom)), MAX_PATH);

                SetOption(albootrom, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ALBOOT_ROM, BM_GETCHECK, 0, 0L) == BST_CHECKED));

                // If the ROM config has changed, schedule the changes for the next reset
                if (ChangedString(rom) || Changed(albootrom))
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


INT_PTR CALLBACK DisplayPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);
    bool fPreview = false;

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char *aszRenderer[] = { "Auto-select (Default)", "DirectDraw", "Direct3D (if available)", nullptr };

            SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_SETCHECK, GetOption(hwaccel) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_FILTER, BM_SETCHECK, GetOption(filter) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_SETCHECK, GetOption(scanlines) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_HIRES, BM_SETCHECK, GetOption(scanhires) ? BST_CHECKED : BST_UNCHECKED, 0L);

            int nRenderer = GetOption(direct3d);
            if (nRenderer < -1 || nRenderer > 1) nRenderer = -1;

            int nIntensity = std::max(0, std::min(95, GetOption(scanlevel)));
            SendDlgItemMessage(hdlg_, IDC_INTENSITY, TBM_SETRANGE, 0, MAKELONG(0, 19));
            SendDlgItemMessage(hdlg_, IDC_INTENSITY, TBM_SETPOS, TRUE, nIntensity / 5);

            SetComboStrings(hdlg_, IDC_RENDERER, aszRenderer, nRenderer+1);

            SendMessage(hdlg_, WM_COMMAND, IDC_HWACCEL, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);

            int nIntensity = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_INTENSITY, TBM_GETPOS, 0, 0L)) * 5;

            if (pnmh->idFrom == IDC_INTENSITY)
            {
                char sz[16];
                snprintf(sz, sizeof(sz), "%d%%", nIntensity);
                SetDlgItemText(hdlg_, IDS_INTENSITY_PERCENT, sz);
                fPreview = true;
            }

            if (pnmh->code == PSN_APPLY)
            {
                SetOption(hwaccel, SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(direct3d, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_RENDERER, CB_GETCURSEL, 0, 0L)) - 1);

                SetOption(filter, SendDlgItemMessage(hdlg_, IDC_FILTER, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(scanlines, SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(scanhires, SendDlgItemMessage(hdlg_, IDC_HIRES, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(scanlevel, nIntensity);

                if (Changed(hwaccel) || Changed(direct3d))
                    Video::Init();

                if (Changed(scanlines) || Changed(scanhires) || Changed(scanlevel))
                    Video::UpdatePalette();
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_HWACCEL:
                {
                    bool fAccel = SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_GETCHECK, 0, 0L) != 0;
                    EnableWindow(GetDlgItem(hdlg_, IDC_FILTER), fAccel);
                    EnableWindow(GetDlgItem(hdlg_, IDS_RENDERER), fAccel);
                    EnableWindow(GetDlgItem(hdlg_, IDC_RENDERER), fAccel);
                    EnableWindow(GetDlgItem(hdlg_, IDC_HIRES), fAccel);
                    fPreview = true;
                    break;
                }

                case IDC_SCANLINES:
                {
                    bool fScanlines = SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_GETCHECK, 0, 0L) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hdlg_, IDC_HIRES), fScanlines);
                    EnableWindow(GetDlgItem(hdlg_, IDS_INTENSITY), fScanlines);
                    EnableWindow(GetDlgItem(hdlg_, IDC_INTENSITY), fScanlines);
                    EnableWindow(GetDlgItem(hdlg_, IDS_INTENSITY_PERCENT), fScanlines);
                    fPreview = true;
                    break;
                }

                case IDC_FILTER:
                case IDC_HIRES:
                    fPreview = true;
                    break;
            }
            break;
        }
    }

    // Are we to preview the display option changes?
    if (fPreview)
    {
        // Apply the current dialog settings
        SetOption(filter, SendDlgItemMessage(hdlg_, IDC_FILTER, BM_GETCHECK, 0, 0L) == BST_CHECKED);
        SetOption(scanlines, SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_GETCHECK, 0, 0L) == BST_CHECKED);
        SetOption(scanhires, SendDlgItemMessage(hdlg_, IDC_HIRES, BM_GETCHECK, 0, 0L) == BST_CHECKED);
        SetOption(scanlevel, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_INTENSITY, TBM_GETPOS, 0, 0L)) * 5);

        // Redraw the display to reflect the changes
        Video::UpdatePalette();
        Frame::Redraw();

        // Restore the previous settings
        SetOption(filter, opts.filter);
        SetOption(scanlines, opts.scanlines);
        SetOption(scanlevel, opts.scanlevel);
        SetOption(scanhires, opts.scanhires);
    }

    return fRet;
}


INT_PTR CALLBACK SoundPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char *aszSIDs[] = { "None", "MOS6581 (Default)", "MOS8580", nullptr };
            SetComboStrings(hdlg_, IDC_SID_TYPE, aszSIDs, GetOption(sid));

            static const char *aszDAC7C[] = { "None", "Blue Alpha Sampler (8-bit mono)", "SAMVox (4 channel 8-bit mono)", "Paula (2 channel 4-bit stereo)", nullptr };
            SetComboStrings(hdlg_, IDC_DAC_7C, aszDAC7C, GetOption(dac7c));

            FillMidiOutCombo(GetDlgItem(hdlg_, IDC_MIDI_OUT));
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(sid, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SID_TYPE, CB_GETCURSEL, 0, 0L)));
                SetOption(dac7c, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_DAC_7C, CB_GETCURSEL, 0, 0L)));

                int nMidi = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_MIDI_OUT, CB_GETCURSEL, 0, 0L));
                SetOption(midi, nMidi ? 1 : 0);

                if (nMidi == 0)
                    SetOption(midioutdev, "");
                else if (SendDlgItemMessage(hdlg_, IDC_MIDI_OUT, CB_GETLBTEXTLEN, nMidi, 0L) < sizeof(opts.midioutdev))
                    SendDlgItemMessage(hdlg_, IDC_MIDI_OUT, CB_GETLBTEXT, nMidi, reinterpret_cast<LPARAM>(GetOption(midioutdev)));

                if (ChangedString(midioutdev) && !pMidi->SetDevice(GetOption(midioutdev)))
                    Message(msgWarning, "Failed to open MIDI device\n");
            }

            break;
        }
    }

    return fRet;
}


int FillHDDCombos (HWND hdlg_, int nCombo1_, int nCombo2_)
{
    int nDevices = 0;

    HWND hwndCombo1 = GetDlgItem(hdlg_, nCombo1_);
    HWND hwndCombo2 = nCombo2_ ? GetDlgItem(hdlg_, nCombo2_) : nullptr;

    SendMessage(hwndCombo1, CB_RESETCONTENT, 0, 0L);
    if (hwndCombo2) SendMessage(hwndCombo2, CB_RESETCONTENT, 0, 0L);

    for (const char *pcszList = CDeviceHardDisk::GetDeviceList() ; *pcszList ; pcszList += strlen(pcszList)+1, nDevices++)
    {
        SendMessage(hwndCombo1, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pcszList));
        if (hwndCombo2) SendMessage(hwndCombo2, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pcszList));
    }

    if (!nDevices)
    {
        LPARAM lNone = reinterpret_cast<LPARAM>("<None>");
        SendMessage(hwndCombo1, CB_ADDSTRING, 0, lNone);
        if (hwndCombo2) SendMessage(hwndCombo2, CB_ADDSTRING, 0, lNone);
    }

    if (SendMessage(hwndCombo1, CB_GETCURSEL, 0, 0L) < 0)
    {
        SendMessage(hwndCombo1, CB_SETCURSEL, 0, 0L);
        if (hwndCombo2) SendMessage(hwndCombo2, CB_SETCURSEL, 0, 0L);
    }

    bool fEnable = nDevices != 0;
    EnableWindow(hwndCombo1, fEnable);
    if (hwndCombo2) EnableWindow(hwndCombo2, fEnable);

    return nDevices;
}


int FillFloppyCombo (HWND hwndCombo_)
{
    int nDevices = 0;

    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    for (UINT u = 0 ; u < 2 ; u++)
    {
        char sz[32];
        wsprintf(sz, "\\\\.\\fdraw%u", u);
        GetFileAttributes(sz);

        if (GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            wsprintf(sz, "%c:", 'A'+u);
            SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz));
            nDevices++;
        }
    }

    if (!nDevices)
        SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("<None>"));

    if (SendMessage(hwndCombo_, CB_GETCURSEL, 0, 0L) < 0)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);

    EnableWindow(hwndCombo_, nDevices != 0);

    return nDevices;
}


INT_PTR CALLBACK Drive1PageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            const char* aszDevices[] = { "None", "Floppy Drive", nullptr };
            SetComboStrings(hdlg_, IDC_DEVICE_TYPE, aszDevices, GetOption(drive1));

            int nDevices = FillFloppyCombo(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE));
            EnableWindow(GetDlgItem(hdlg_, IDR_DEVICE), nDevices != 0);
            SendDlgItemMessage(hdlg_, IDE_FLOPPY_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

            if (CFloppyStream::IsRecognised(GetOption(disk1)))
                SendDlgItemMessage(hdlg_, IDC_FLOPPY_DEVICE, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(GetOption(disk1)));
            else
                SetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, GetOption(disk1));

            SHAutoComplete(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), SHACF_FILESYS_ONLY|SHACF_USETAB);

            SendMessage(hdlg_, WM_COMMAND, MAKELONG(IDC_DEVICE_TYPE, CBN_SELCHANGE), 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(drive1, static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_DEVICE_TYPE), CB_GETCURSEL, 0, 0L)));

                if (GetOption(drive1) == drvFloppy)
                {
                    bool fImage = SendDlgItemMessage(hdlg_, IDR_DEVICE, BM_GETCHECK, 0, 0L) != BST_CHECKED;

                    if (fImage)
                        GetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, const_cast<char*>(GetOption(disk1)), MAX_PATH);
                    else
                        SetOption(disk1, GetComboText(hdlg_, IDC_FLOPPY_DEVICE, MAX_PATH));
                }

                // If the floppy is active and the disk has changed, insert the new one
                if (GetOption(drive1) == drvFloppy && ChangedString(disk1))
                    InsertDisk(pFloppy1, GetOption(disk1));
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
                        CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_IMAGE);
                    break;
                }

                case IDC_FLOPPY_DEVICE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                        CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_DEVICE);

                    break;
                }

                case IDC_DEVICE_TYPE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                    {
                        int nType = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_DEVICE_TYPE, CB_GETCURSEL, 0, 0L));

                        ShowWindow(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), (nType == 1) ? SW_SHOW : SW_HIDE);
                        ShowWindow(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), (nType == 1) ? SW_SHOW : SW_HIDE);

                        int anControls[] = { IDS_DEVICE, IDF_MEDIA, IDS_TEXT1, IDR_IMAGE, IDB_BROWSE, IDR_DEVICE, IDC_HDD_DEVICE };
                        for (int i = 0 ; i < _countof(anControls) ; i++)
                            ShowWindow(GetDlgItem(hdlg_, anControls[i]), (nType >= 1) ? SW_SHOW : SW_HIDE);
                    }
                    break;
                }

                case IDB_BROWSE:
                {
                    int nType = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_DEVICE_TYPE, CB_GETCURSEL, 0, 0L));

                    if (nType == 1)
                        BrowseImage(hdlg_, IDE_FLOPPY_IMAGE, szFloppyFilters);
                    break;
                }
            }
            break;
        }
    }

    return fRet;
}

INT_PTR CALLBACK Drive2PageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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

        case WM_USER+1:
            uTimer = SetTimer(hdlg_, 1, 250, nullptr);
            break;

        case WM_TIMER:
        {
            KillTimer(hdlg_, 1), uTimer = 0;

            FillFloppyCombo(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE));
            FillHDDCombos(hdlg_, IDC_HDD_DEVICE, IDC_HDD_DEVICE2);

            int nType = static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_DEVICE_TYPE), CB_GETCURSEL, 0, 0L));

            // Set floppy image path or device selection
            if (CFloppyStream::IsRecognised(GetOption(disk2)))
            {
                if (nType == drvFloppy) CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_DEVICE);
                SendDlgItemMessage(hdlg_, IDC_FLOPPY_DEVICE, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(GetOption(disk2)));
            }
            else
            {
                CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_IMAGE);
            }

            // Set Atom master device selection
            if (CDeviceHardDisk::IsRecognised(GetOption(atomdisk0)))
            {
                if (nType >= drvAtom) CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_DEVICE);
                SendDlgItemMessage(hdlg_, IDC_HDD_DEVICE, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(GetOption(atomdisk0)));
            }

            // Set Atom slave device selection
            if (CDeviceHardDisk::IsRecognised(GetOption(atomdisk1)))
            {
                if (nType >= drvAtom) CheckRadioButton(hdlg_, IDR_IMAGE2,IDR_DEVICE2, IDR_DEVICE);
                SendDlgItemMessage(hdlg_, IDC_HDD_DEVICE2, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(GetOption(atomdisk1)));
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
                    int nSources = SHCNE_DRIVEADD|SHCNE_DRIVEREMOVED|SHCNE_MEDIAINSERTED|SHCNE_MEDIAREMOVED;
                    ulSHChangeNotifyRegister = SHChangeNotifyRegister(hdlg_, SHCNRF_ShellLevel|SHCNRF_NewDelivery, nSources, WM_USER+1, 1, &entry);
                    CoTaskMemFree(ppidl);
                }
            }

            const char* aszDevices[] = { "None", "Floppy Drive", "Atom (Legacy)", "Atom Lite", nullptr };
            SetComboStrings(hdlg_, IDC_DEVICE_TYPE, aszDevices, GetOption(drive2));

            SendDlgItemMessage(hdlg_, IDE_FLOPPY_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));
            SendDlgItemMessage(hdlg_, IDE_HDD_IMAGE, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));
            SendDlgItemMessage(hdlg_, IDE_HDD_IMAGE2, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

            // Set floppy image path or device selection
            if (!CFloppyStream::IsRecognised(GetOption(disk2)))
                SetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, GetOption(disk2));

            // Set Atom master image path
            if (!CDeviceHardDisk::IsRecognised(GetOption(atomdisk0)))
                SetDlgItemText(hdlg_, IDE_HDD_IMAGE, GetOption(atomdisk0));

            // Set Atom slave image path
            if (!CDeviceHardDisk::IsRecognised(GetOption(atomdisk1)))
                SetDlgItemText(hdlg_, IDE_HDD_IMAGE2, GetOption(atomdisk1));

            SHAutoComplete(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), SHACF_FILESYS_ONLY|SHACF_USETAB);
            SHAutoComplete(GetDlgItem(hdlg_, IDE_HDD_IMAGE), SHACF_FILESYS_ONLY|SHACF_USETAB);
            SHAutoComplete(GetDlgItem(hdlg_, IDE_HDD_IMAGE2), SHACF_FILESYS_ONLY|SHACF_USETAB);

            SendMessage(hdlg_, WM_COMMAND, MAKELONG(IDC_DEVICE_TYPE, CBN_SELCHANGE), 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(drive2, static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_DEVICE_TYPE), CB_GETCURSEL, 0, 0L)));

                bool fImage = SendDlgItemMessage(hdlg_, IDR_DEVICE, BM_GETCHECK, 0, 0L) != BST_CHECKED;
                bool fImage2 = SendDlgItemMessage(hdlg_, IDR_DEVICE2, BM_GETCHECK, 0, 0L) != BST_CHECKED;

                switch (GetOption(drive2))
                {
                    case drvFloppy:
                        if (fImage)
                            GetDlgItemText(hdlg_, IDE_FLOPPY_IMAGE, const_cast<char*>(GetOption(disk2)), MAX_PATH);
                        else
                            SetOption(disk2, GetComboText(hdlg_, IDC_FLOPPY_DEVICE, MAX_PATH));
                        break;

                    case drvAtom:
                    case drvAtomLite:
                        if (fImage)
                            GetDlgItemText(hdlg_, IDE_HDD_IMAGE, const_cast<char*>(GetOption(atomdisk0)), MAX_PATH);
                        else
                            SetOption(atomdisk0, GetComboText(hdlg_, IDC_HDD_DEVICE, MAX_PATH));

                        if (fImage2)
                            GetDlgItemText(hdlg_, IDE_HDD_IMAGE2, const_cast<char*>(GetOption(atomdisk1)), MAX_PATH);
                        else
                            SetOption(atomdisk1, GetComboText(hdlg_, IDC_HDD_DEVICE2, MAX_PATH));

                        break;
                }

                // If the floppy is active and the disk has changed, insert the new one
                if (GetOption(drive2) == drvFloppy && ChangedString(disk2))
                    InsertDisk(pFloppy2, GetOption(disk2));

                // If the Atom Lite selection state has changed, trigger a ROM refresh
                if ((GetOption(drive2) == drvAtomLite) ^ (opts.drive2 >= drvAtomLite))
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
                        CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_IMAGE);
                    break;
                }

                case IDE_HDD_IMAGE2:
                {
                    if (HIWORD(wParam_) == EN_CHANGE)
                        CheckRadioButton(hdlg_, IDR_IMAGE2,IDR_DEVICE2, IDR_IMAGE2);
                    break;
                }

                case IDC_FLOPPY_DEVICE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                        CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_DEVICE);

                    break;
                }

                case IDC_HDD_DEVICE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                        CheckRadioButton(hdlg_, IDR_IMAGE,IDR_DEVICE, IDR_DEVICE);

                    break;
                }

                case IDC_HDD_DEVICE2:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                        CheckRadioButton(hdlg_, IDR_IMAGE2,IDR_DEVICE2, IDR_DEVICE2);

                    break;
                }

                case IDC_DEVICE_TYPE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                    {
                        int nType = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_DEVICE_TYPE, CB_GETCURSEL, 0, 0L));

                        HICON hicon = LoadIcon(__hinstance, MAKEINTRESOURCE((nType >= 2) ? IDI_DRIVE : IDI_FLOPPY));
                        SendDlgItemMessage(hdlg_, IDS_DEVICE, STM_SETICON, reinterpret_cast<WPARAM>(hicon), 0L);

                        SetDlgItemText(hdlg_, IDF_MEDIA, (nType >= 2) ? "Master" : "Media");

                        ShowWindow(GetDlgItem(hdlg_, IDE_FLOPPY_IMAGE), (nType == drvFloppy) ? SW_SHOW : SW_HIDE);
                        ShowWindow(GetDlgItem(hdlg_, IDC_FLOPPY_DEVICE), (nType == drvFloppy) ? SW_SHOW : SW_HIDE);

                        int anControls[] = { IDS_DEVICE, IDF_MEDIA, IDS_TEXT1, IDR_IMAGE, IDB_BROWSE, IDR_DEVICE, IDC_HDD_DEVICE };
                        for (int i = 0 ; i < _countof(anControls) ; i++)
                            ShowWindow(GetDlgItem(hdlg_, anControls[i]), (nType >= drvFloppy) ? SW_SHOW : SW_HIDE);

                        int anControls2[] = { IDF_MEDIA2, IDS_TEXT2, IDR_IMAGE2, IDE_HDD_IMAGE, IDE_HDD_IMAGE2, IDB_BROWSE2, IDC_HDD_DEVICE, IDR_DEVICE2, IDC_HDD_DEVICE2 };
                        for (int i = 0 ; i < _countof(anControls2) ; i++)
                            ShowWindow(GetDlgItem(hdlg_, anControls2[i]), (nType >= drvAtom) ? SW_SHOW : SW_HIDE);

                        uTimer = SetTimer(hdlg_, 1, 1, nullptr);
                    }
                    break;
                }

                case IDB_BROWSE:
                {
                    int nType = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_DEVICE_TYPE, CB_GETCURSEL, 0, 0L));
                    if (nType == drvFloppy)
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


int CALLBACK BrowseFolderCallback (HWND hwnd_, UINT uMsg_, LPARAM lParam_, LPARAM lpData_)
{
    // Once initialised, set the initial browse location
    if (uMsg_ == BFFM_INITIALIZED)
        SendMessage(hwnd_, BFFM_SETSELECTION, TRUE, lpData_);

    return 0;
}

void BrowseFolder (HWND hdlg_, int nControl_, const char* pcszDefDir_)
{
    char sz[MAX_PATH];
    HWND hctrl = GetDlgItem(hdlg_, nControl_);
    GetWindowText(hctrl, sz, sizeof(sz));

    BROWSEINFO bi = {0};
    bi.hwndOwner = hdlg_;
    bi.lpszTitle = "Select default path:";
    bi.lpfn = BrowseFolderCallback;
    bi.lParam = reinterpret_cast<LPARAM>(sz);
    bi.ulFlags = BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

    if (pidl)
    {
        if (SHGetPathFromIDList(pidl, sz))
        {
            SetDlgItemPath(hdlg_, nControl_, sz);
            SendMessage(hctrl, EM_SETSEL, 0, -1);
            SetFocus(hctrl);
        }

        CoTaskMemFree(pidl);
    }
}

INT_PTR CALLBACK InputPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMapping[] = { "Disabled", "Automatic (default)", "SAM Coupé", "ZX Spectrum", nullptr };
            SetComboStrings(hdlg_, IDC_KEYBOARD_MAPPING, aszMapping, GetOption(keymapping));

            SendDlgItemMessage(hdlg_, IDC_ALT_FOR_CNTRL, BM_SETCHECK, GetOption(altforcntrl) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_ALTGR_FOR_EDIT, BM_SETCHECK, GetOption(altgrforedit) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_SETCHECK, GetOption(mouse) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendMessage(hdlg_, WM_COMMAND, IDC_MOUSE_ENABLED, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(keymapping, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_KEYBOARD_MAPPING, CB_GETCURSEL, 0, 0L)));

                SetOption(altforcntrl, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ALT_FOR_CNTRL, BM_GETCHECK, 0, 0L)) == BST_CHECKED);
                SetOption(altgrforedit, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ALTGR_FOR_EDIT, BM_GETCHECK, 0, 0L)) == BST_CHECKED);

                SetOption(mouse, SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                if (Changed(keymapping) || Changed(mouse))
                    Input::Init();
            }
            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK JoystickPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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

            static const char* aszJoysticks[] = { "None", "Joystick 1", "Joystick 2", "Kempston", nullptr };
            SetComboStrings(hdlg_, IDC_SAM_JOYSTICK1, aszJoysticks, GetOption(joytype1));
            SetComboStrings(hdlg_, IDC_SAM_JOYSTICK2, aszJoysticks, GetOption(joytype2));

            SendMessage(hdlg_, WM_COMMAND, IDC_JOYSTICK1, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam_);

            if (pnmh->code == PSN_APPLY)
            {
                HWND hwndJoy1 = GetDlgItem(hdlg_, IDC_JOYSTICK1), hwndJoy2 = GetDlgItem(hdlg_, IDC_JOYSTICK2);
                SendMessage(hwndJoy1, CB_GETLBTEXT, SendMessage(hwndJoy1, CB_GETCURSEL, 0, 0L), (LPARAM)GetOption(joydev1));
                SendMessage(hwndJoy2, CB_GETLBTEXT, SendMessage(hwndJoy2, CB_GETCURSEL, 0, 0L), (LPARAM)GetOption(joydev2));

                SetOption(joytype1, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SAM_JOYSTICK1, CB_GETCURSEL, 0, 0L)));
                SetOption(joytype2, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SAM_JOYSTICK2, CB_GETCURSEL, 0, 0L)));

                // Always reinitialise to ensure we pick up connected devices
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
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK1), SendDlgItemMessage(hdlg_, IDC_JOYSTICK1, CB_GETCURSEL, 0, 0L) != 0);
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAM_JOYSTICK2), SendDlgItemMessage(hdlg_, IDC_JOYSTICK2, CB_GETCURSEL, 0, 0L) != 0);
                    break;
            }
            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK ParallelPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszParallel[] = { "None", "Printer", "DAC (8-bit mono)", "SAMDAC (8-bit stereo)", nullptr };
            SetComboStrings(hdlg_, IDC_PARALLEL_1, aszParallel, GetOption(parallel1));
            SetComboStrings(hdlg_, IDC_PARALLEL_2, aszParallel, GetOption(parallel2));

            SendDlgItemMessage(hdlg_, IDC_AUTO_FLUSH, BM_SETCHECK, GetOption(flushdelay) ? BST_CHECKED : BST_UNCHECKED, 0L);
            FillPrintersCombo(GetDlgItem(hdlg_, IDC_PRINTERS), GetOption(printerdev));

            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_1, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_2, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(parallel1, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PARALLEL_1, CB_GETCURSEL, 0, 0L)));
                SetOption(parallel2, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PARALLEL_2, CB_GETCURSEL, 0, 0L)));

                LRESULT lPrinter = SendDlgItemMessage(hdlg_, IDC_PRINTERS, CB_GETCURSEL, 0, 0L);

                // Fetch the printer name unless print-to-file is selected
                if (lPrinter)
                {
                    char szPrinter[256];
                    SendDlgItemMessage(hdlg_, IDC_PRINTERS, CB_GETLBTEXT, lPrinter, reinterpret_cast<LPARAM>(szPrinter));
                    SetOption(printerdev, szPrinter+lstrlen(PRINTER_PREFIX));
                }

                SetOption(flushdelay, (SendDlgItemMessage(hdlg_, IDC_AUTO_FLUSH, BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 2 : 0);
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
                    bool fPrinter1 = (SendDlgItemMessage(hdlg_, IDC_PARALLEL_1, CB_GETCURSEL, 0, 0L) == 1);
                    bool fPrinter2 = (SendDlgItemMessage(hdlg_, IDC_PARALLEL_2, CB_GETCURSEL, 0, 0L) == 1);

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


INT_PTR CALLBACK MiscPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hdlg_, IDC_SAMBUS_CLOCK, BM_SETCHECK, GetOption(sambusclock) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_DALLAS_CLOCK, BM_SETCHECK, GetOption(dallasclock) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_DRIVE_LIGHTS, BM_SETCHECK, GetOption(drivelights) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_STATUS, BM_SETCHECK, GetOption(status) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_PROFILE, BM_SETCHECK, GetOption(profile) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SAVE_PROMPT, BM_SETCHECK, GetOption(saveprompt) ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(sambusclock, SendDlgItemMessage(hdlg_, IDC_SAMBUS_CLOCK, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(dallasclock, SendDlgItemMessage(hdlg_, IDC_DALLAS_CLOCK, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(drivelights, SendDlgItemMessage(hdlg_, IDC_DRIVE_LIGHTS, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(status, SendDlgItemMessage(hdlg_, IDC_STATUS, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(profile, SendDlgItemMessage(hdlg_, IDC_PROFILE, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(saveprompt, SendDlgItemMessage(hdlg_, IDC_SAVE_PROMPT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
            }
            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK HelperPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hdlg_, IDC_TURBO_DISK, BM_SETCHECK, GetOption(turbodisk) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_SETCHECK, GetOption(fastreset) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_AUTOLOAD, BM_SETCHECK, GetOption(autoload) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_DOSBOOT, BM_SETCHECK, GetOption(dosboot) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SetDlgItemPath(hdlg_, IDE_DOSDISK, GetOption(dosdisk));
            SendDlgItemMessage(hdlg_, IDE_DOSDISK, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"<None>"));

            SendMessage(hdlg_, WM_COMMAND, IDC_DOSBOOT, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(turbodisk, SendDlgItemMessage(hdlg_, IDC_TURBO_DISK, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(fastreset, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_GETCHECK, 0, 0L) == BST_CHECKED));
                SetOption(autoload, SendDlgItemMessage(hdlg_, IDC_AUTOLOAD, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(dosboot,  SendDlgItemMessage(hdlg_, IDC_DOSBOOT, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                GetDlgItemPath(hdlg_, IDE_DOSDISK, const_cast<char*>(GetOption(dosdisk)), MAX_PATH);
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_DOSBOOT:
                {
                    bool fDosBoot = SendDlgItemMessage(hdlg_, IDC_DOSBOOT, BM_GETCHECK, 0, 0L) == BST_CHECKED;
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


static void InitPage (PROPSHEETPAGE* pPage_, int nPage_, int nDialogId_, DLGPROC pfnDlgProc_)
{
    pPage_ = &pPage_[nPage_];

    ZeroMemory(pPage_, sizeof(*pPage_));
    pPage_->dwSize = sizeof(*pPage_);
    pPage_->hInstance = __hinstance;
    pPage_->pszTemplate = MAKEINTRESOURCE(nDialogId_);
    pPage_->pfnDlgProc = pfnDlgProc_;
    pPage_->lParam = nPage_;
    pPage_->pfnCallback = nullptr;
}


void DisplayOptions ()
{
    // Update floppy path options
    SetOption(disk1, pFloppy1->DiskPath());
    SetOption(disk2, pFloppy2->DiskPath());

    // Initialise the pages to go on the sheet
    PROPSHEETPAGE aPages[10] = {};
    InitPage(aPages, 0,  IDD_PAGE_SYSTEM,   SystemPageDlgProc);
    InitPage(aPages, 1,  IDD_PAGE_DISPLAY,  DisplayPageDlgProc);
    InitPage(aPages, 2,  IDD_PAGE_SOUND,    SoundPageDlgProc);
    InitPage(aPages, 3,  IDD_PAGE_PARALLEL, ParallelPageDlgProc);
    InitPage(aPages, 4,  IDD_PAGE_INPUT,    InputPageDlgProc);
    InitPage(aPages, 5,  IDD_PAGE_JOYSTICK, JoystickPageDlgProc);
    InitPage(aPages, 6,  IDD_PAGE_DRIVE1,   Drive1PageDlgProc);
    InitPage(aPages, 7,  IDD_PAGE_DRIVE2,   Drive2PageDlgProc);
    InitPage(aPages, 8,  IDD_PAGE_MISC,     MiscPageDlgProc);
    InitPage(aPages, 9,  IDD_PAGE_HELPER,   HelperPageDlgProc);

    PROPSHEETHEADER psh;
    ZeroMemory(&psh, sizeof(psh));
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
    psh.hwndParent = g_hwnd;
    psh.hInstance = __hinstance;
    psh.pszIcon = MAKEINTRESOURCE(IDI_MISC);
    psh.pszCaption = "Options";
    psh.nPages = _countof(aPages);
    psh.nStartPage = nOptionPage;
    psh.ppsp = aPages;

    // Save the current option state, flag that we've not centred the dialogue box, then display them for editing
    opts = Options::s_Options;
    fCentredOptions = false;

    // Display option property sheet
    if (PropertySheet(&psh) >= 1)
    {
        // Detach current disks
        pAtom->Detach();
        pAtomLite->Detach();
        pSDIDE->Detach();

        // Attach new disks
        CAtaAdapter *pActiveAtom = (GetOption(drive2) == drvAtom) ? pAtom : pAtomLite;
        AttachDisk(pActiveAtom, GetOption(atomdisk0), 0);
        AttachDisk(pActiveAtom, GetOption(atomdisk1), 1);
        AttachDisk(pSDIDE, GetOption(sdidedisk), 0);

        // Save changed options to config file
        Options::Save();
    }
}
