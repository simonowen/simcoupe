// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: Win32 user interface
//
//  Copyright (c) 1999-2010  Simon Owen
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
#include "Action.h"
#include "CDrive.h"
#include "Clock.h"
#include "CPU.h"
#include "Debug.h"
#include "Display.h"
#include "Expr.h"
#include "Floppy.h"
#include "Frame.h"
#include "GUIDlg.h"
#include "HardDisk.h"
#include "Input.h"
#include "Main.h"
#include "Mouse.h"
#include "ODmenu.h"
#include "Options.h"
#include "OSD.h"
#include "Parallel.h"
#include "Memory.h"
#include "Sound.h"
#include "Video.h"

// Source argc/argv from either MSVCRT.DLL or the static CRT
#ifdef _DLL
__declspec(dllimport) int __argc;
__declspec(dllimport) char** __argv;
#else
extern int __argc;
extern char** __argv;
#endif

#include "resource.h"   // For menu and dialogue box symbols

const int MOUSE_HIDE_TIME = 2000;   // 2 seconds
const UINT MOUSE_TIMER_ID = 42;

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe"
#endif

#define PRINTER_PREFIX      "Printer: "

INT_PTR CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
INT_PTR CALLBACK NewDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
void CentreWindow (HWND hwnd_, HWND hwndParent_=NULL);

void DisplayOptions ();

bool g_fActive = true;


HINSTANCE __hinstance;
HWND g_hwnd;
HMENU g_hmenu;
extern HINSTANCE __hinstance;

HHOOK g_hFnKeyHook, hWinKeyHook;
HWND hdlgNewFnKey;

WNDPROC pfnStaticWndProc;           // Old static window procedure (internal value)

WINDOWPLACEMENT g_wp;
int nWindowDx, nWindowDy;

int nOptionPage = 0;                // Last active option property page
const int MAX_OPTION_PAGES = 16;    // Maximum number of option propery pages
bool fCentredOptions;

void AddRecentFile (const char* pcsz_);
void LoadRecentFiles ();
void SaveRecentFiles ();

OPTIONS opts;
// Helper macro for detecting options changes
#define Changed(o)        (opts.o != GetOption(o))
#define ChangedString(o)  (strcasecmp(opts.o, GetOption(o)))


static char szFloppyFilters[] =
#ifdef USE_ZLIB
    "All Disks (dsk;sad;mgt;sdf;td0;sbt;cpm;gz;zip)\0*.dsk;*.sad;*.mgt;*.sdf;*.td0;*.sbt;*.cpm;*.gz;*.zip\0"
#endif
    "Disk Images (dsk;sad;mgt;sdf;td0;sbt;cpm)\0*.dsk;*.sad;*.mgt;*.sdf;*.td0;*.sbt;*.cpm\0"
#ifdef USE_ZLIB
    "Compressed Files (gz;zip)\0*.gz;*.zip\0"
#endif
    "All Files (*.*)\0*.*\0";

static char szHDDFilters[] = 
    "Hard Disk Images (*.hdf)\0*.hdf\0"
    "All Files (*.*)\0*.*\0";

static const char* aszBorders[] =
    { "No borders", "Small borders", "Short TV area (default)", "TV visible area", "Complete scan area", NULL };


bool InitWindow ();
void LocaliseMenu (HMENU hmenu_);
void LocaliseWindows (HWND hwnd_);


int WINAPI WinMain(HINSTANCE hinst_, HINSTANCE hinstPrev_, LPSTR pszCmdLine_, int nCmdShow_)
{
    __hinstance = hinst_;

    return main(__argc, __argv);
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath (char* pszPath_, size_t nLength_);

bool UI::Init (bool fFirstInit_/*=false*/)
{
    UI::Exit(true);
    TRACE("-> UI::Init(%s)\n", fFirstInit_ ? "first" : "");

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    if (fFirstInit_)
        LoadRecentFiles();

    bool fRet = InitWindow();
    TRACE("<- UI::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> UI::Exit(%s)\n", fReInit_ ? "reinit" : "");

    // When we reach here during a normal shutdown the window will already have gone, so check first
    if (g_hwnd && IsWindow(g_hwnd))
        DestroyWindow(g_hwnd);
    g_hwnd = NULL;

    if (!fReInit_)
        SaveRecentFiles();

    TRACE("<- UI::Exit()\n");
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    // Re-pause after a single frame-step
    if (g_fFrameStep)
        Action::Do(actFrameStep);

    while (1)
    {
        // Loop to process any pending Windows messages
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // App closing down?
            if (msg.message == WM_QUIT)
                return false;

            // Translation for menu shortcuts, but avoid producing keypad symbols
            if (msg.message != WM_KEYDOWN || msg.wParam < VK_NUMPAD0 || msg.wParam > VK_DIVIDE)
                TranslateMessage(&msg);

            DispatchMessage(&msg);
        }

        // Continue running if we're active or allowed to run in the background
        if (!g_fPaused && (g_fActive || !GetOption(pauseinactive)))
            break;

        // Block until something happens
        WaitMessage();
    }

    return true;
}

void UI::ShowMessage (eMsgType eType_, const char* pcszMessage_)
{
    const char* const pcszCaption = "SimCoupe";
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


void UI::ResizeWindow (bool fUseOption_/*=false*/)
{
    static bool fCentred = false;

    // The default size is called 2x, when it's actually 1x!
    int nWidth = Frame::GetWidth() >> 1;
    int nHeight = Frame::GetHeight() >> 1;

    // Apply 5:4 ratio if enabled
    if (GetOption(ratio5_4))
        nWidth = MulDiv(nWidth, 5, 4);

    RECT rClient;
    GetClientRect(g_hwnd, &rClient);

    if (fUseOption_ || !rClient.bottom)
    {
        if (!GetOption(scale))
            SetOption(scale, 2);

        nWidth *= GetOption(scale);
        nHeight *= GetOption(scale);
    }
    else if (!fUseOption_)
    {
        RECT rClient;
        GetClientRect(g_hwnd, &rClient);

        nWidth = MulDiv(rClient.bottom, nWidth, nHeight);
        nHeight = rClient.bottom;
    }

    if (GetOption(fullscreen))
    {
        // Change the window style to a visible pop-up, with no caption, border or menu
        SetWindowLongPtr(g_hwnd, GWL_STYLE, WS_POPUP|WS_VISIBLE);
        SetMenu(g_hwnd, NULL);

        // Force the window to be top-most, and sized to fill the full screen
        SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), 0);
    }
    else
    {
        WINDOWPLACEMENT wp = { sizeof(wp) };

            // Leave a maximised window as it is
        if (!GetWindowPlacement(g_hwnd, &wp) || (wp.showCmd != SW_SHOWMAXIMIZED))
        {
            DWORD dwStyle = (GetWindowStyle(g_hwnd) & WS_VISIBLE) | WS_OVERLAPPEDWINDOW;
            SetWindowLongPtr(g_hwnd, GWL_STYLE, dwStyle);
            SetMenu(g_hwnd, g_hmenu);

            // Adjust the window rectangle to give the client area size required for the main window
            RECT rect = { 0, 0, nWidth, nHeight };
            AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));
            SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_NOMOVE);

            // Get the actual client rectangle, now that the menu is in place
            RECT rClient;
            GetClientRect(g_hwnd, &rClient);

            // Ensure the client area is exactly what we wanted, and adjust it if not
            // This can happen if a menu wraps because the window is small, or on buggy Vista versions
            if (rClient.right != nWidth || rClient.bottom != nHeight)
            {
                nWindowDx = nWidth-rClient.right;
                nWindowDy = nHeight-rClient.bottom;
                SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, rect.right-rect.left+nWindowDx, rect.bottom-rect.top+nWindowDy, SWP_NOMOVE);
            }

            if (!fCentred)
            {
                nWindowDx = nWindowDy = 0;
                fCentred = true;
                ResizeWindow();
                CentreWindow(g_hwnd);
            }
        }
    }

    // Ensure the window is repainted and the overlay also covers the
    Display::SetDirty();
}

// Save changes to a given drive, optionally prompting for confirmation
bool SaveDriveChanges (CDiskDevice* pDrive_)
{
    if (!pDrive_->IsModified())
        return true;

    if (GetOption(saveprompt))
    {
        char sz[MAX_PATH];
        wsprintf(sz, "Save changes to %s?", pDrive_->GetFile());

        switch (MessageBox(g_hwnd, sz, "SimCoupe", MB_YESNOCANCEL|MB_ICONQUESTION))
        {
            case IDYES:     break;
            case IDNO:      pDrive_->SetModified(false); return true;
            default:        return false;
        }
    }

    if (!pDrive_->Save())
    {
        Message(msgWarning, "Failed to save changes to %s", pDrive_->GetPath());
        return false;
    }

    return true;
}

bool GetSaveLoadFile (LPOPENFILENAME lpofn_, bool fLoad_, bool fCheck_=true)
{
    // Force an Explorer-style dialog, and ensure loaded files exist
    lpofn_->Flags |= OFN_EXPLORER|OFN_PATHMUSTEXIST | (fCheck_ ? (fLoad_ ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT) : 0);

    // Resolve relative paths in a sensible way
    lpofn_->lpstrInitialDir = OSD::GetDirPath(lpofn_->lpstrInitialDir);

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

bool InsertDisk (CDiskDevice* pDrive_)
{
    char szFile[MAX_PATH] = "";
    static OPENFILENAME ofn = { sizeof(ofn) };

    // Save any changes to the current disk first
    if (!SaveDriveChanges(pDrive_))
        return false;

    // Prompt using the current image directory, unless we're using a real drive
    if (reinterpret_cast<CDrive*>(pDrive_)->GetDiskType() == dtFloppy)
        szFile[0] = '\0';
    else if (pDrive_->IsInserted())
        lstrcpyn(szFile, pDrive_->GetPath(), sizeof(szFile));

    ofn.hwndOwner       = g_hwnd;
    ofn.lpstrFilter     = szFloppyFilters;
    ofn.lpstrFile       = szFile;
    ofn.nMaxFile        = sizeof(szFile);
    ofn.lpstrInitialDir = GetOption(floppypath);

    // Prompt for the a new disk to insert
    if (GetSaveLoadFile(&ofn, true))
    {
        bool fReadOnly = !!(ofn.Flags & OFN_READONLY);

        // Insert the disk to check it's a recognised format
        if (!pDrive_->Insert(szFile, fReadOnly))
            Message(msgWarning, "Invalid disk image: %s", szFile);
        else
        {
            Frame::SetStatus("%s  inserted into drive %d%s", pDrive_->GetFile(), (pDrive_ == pDrive1) ? 1 : 2, fReadOnly ? " (read-only)" : "");
            AddRecentFile(szFile);
            return true;
        }
    }

    return false;
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


#define CheckOption(id,check)   CheckMenuItem(hmenu, (id), (check) ? MF_CHECKED : MF_UNCHECKED)
#define EnableItem(id,enable)   EnableMenuItem(hmenu, (id), (enable) ? MF_ENABLED : MF_GRAYED)

void UpdateMenuFromOptions ()
{
    int i;
    char szEject[128];

    HMENU hmenu = g_hmenu, hmenuFile = GetSubMenu(hmenu, 0), hmenuFloppy2 = GetSubMenu(hmenuFile, 6);

    // Only enable the floppy device menu item if it's available
    EnableItem(IDM_FILE_FLOPPY1_DEVICE, CFloppyStream::IsAvailable());
//  EnableItem(IDM_FILE_FLOPPY2_DEVICE, CFloppyStream::IsAvailable());

    bool fFloppy1 = GetOption(drive1) == dskImage, fInserted1 = pDrive1->IsInserted();
    bool fFloppy2 = GetOption(drive2) == dskImage, fInserted2 = pDrive2->IsInserted();

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 1 options
    EnableItem(IDM_FILE_NEW_DISK1, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_INSERT, fFloppy1 && !GUI::IsActive());
    EnableItem(IDM_FILE_FLOPPY1_EJECT, fFloppy1);
    EnableItem(IDM_FILE_FLOPPY1_SAVE_CHANGES, fFloppy1 && pDrive1->IsModified());

    wsprintf(szEject, "&Close %s", pDrive1->GetFile());
    ModifyMenu(hmenu, IDM_FILE_FLOPPY1_EJECT, MF_STRING | (fInserted1 ? MF_ENABLED : MF_GRAYED), IDM_FILE_FLOPPY1_EJECT, szEject);
    CheckOption(IDM_FILE_FLOPPY1_DEVICE, fInserted1 && CFloppyStream::IsRecognised(pDrive1->GetPath()));


    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 2 options
    EnableMenuItem(hmenuFile, 6, MF_BYPOSITION | (fFloppy2 ? MF_ENABLED : MF_GRAYED));
    EnableItem(IDM_FILE_FLOPPY2_SAVE_CHANGES, pDrive2->IsModified());

    wsprintf(szEject, "&Close %s", pDrive2->GetFile());
    ModifyMenu(hmenu, IDM_FILE_FLOPPY2_EJECT, MF_STRING | (fInserted2 ? MF_ENABLED : MF_GRAYED), IDM_FILE_FLOPPY2_EJECT, szEject);
    CheckOption(IDM_FILE_FLOPPY2_DEVICE, fInserted2 && CFloppyStream::IsRecognised(pDrive2->GetPath()));

    CheckOption(IDM_VIEW_FULLSCREEN, GetOption(fullscreen));
    CheckOption(IDM_VIEW_SYNC, GetOption(sync));
    CheckOption(IDM_VIEW_RATIO54, GetOption(ratio5_4));
    CheckOption(IDM_VIEW_SCANLINES, GetOption(scanlines));
    CheckOption(IDM_VIEW_GREYSCALE, GetOption(greyscale));
    for (i = 0 ; i < 4 ; i++) CheckOption(IDM_VIEW_ZOOM_50+i,  i == GetOption(scale)-1);
    for (i = 0 ; i < 5 ; i++) CheckOption(IDM_VIEW_BORDERS0+i, i == GetOption(borders));

    CheckOption(IDM_SYSTEM_PAUSE, g_fPaused);
    CheckOption(IDM_SYSTEM_MUTESOUND, !GetOption(sound));

    // The built-in GUI prevents some items from being used, so disable them if necessary
    EnableItem(IDM_TOOLS_OPTIONS, !GUI::IsActive());
    EnableItem(IDM_TOOLS_DEBUGGER, !g_fPaused && !GUI::IsActive());

    // Enable the Flush printer item if there's buffered data in either printer
    bool fPrinter1 = GetOption(parallel1) == 1, fPrinter2 = GetOption(parallel2) == 1;
    bool fFlush1 = fPrinter1 && reinterpret_cast<CPrintBuffer*>(pParallel1)->IsFlushable();
    bool fFlush2 = fPrinter2 && reinterpret_cast<CPrintBuffer*>(pParallel2)->IsFlushable();
    EnableItem(IDM_TOOLS_FLUSH_PRINTER, fFlush1 || fFlush2);

    // Enable the online option if a printer is active, and check it if it's online
    EnableItem(IDM_TOOLS_PRINTER_ONLINE, fPrinter1 || fPrinter2);
    CheckOption(IDM_TOOLS_PRINTER_ONLINE, (fPrinter1 || fPrinter2) && GetOption(printeronline));

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
                    g_wp.length = sizeof g_wp;
                    GetWindowPlacement(g_hwnd, &g_wp);
                    Frame::Init();
                }
                else
                {
                    // Re-initialise the video system then set the window back how it was before
                    Frame::Init();
                    SetWindowPlacement(g_hwnd, &g_wp);
                    UI::ResizeWindow(true);
                }
                break;

            case actToggle5_4:
                SetOption(ratio5_4, !GetOption(ratio5_4));

                if (!GetOption(fullscreen))
                    UI::ResizeWindow(!GetOption(stretchtofit));
                else if (!GetOption(stretchtofit))
                    Frame::Init();

                Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
                break;

            case actChangeWindowSize:
            {
                SetOption(scale, (GetOption(scale) % 3) + 1);
                UI::ResizeWindow(true);
                Frame::SetStatus("%u%% size", GetOption(scale)*50);
                break;
            }

            case actInsertFloppy1:
                if (GetOption(drive1) != dskImage)
                    Message(msgWarning, "Floppy drive %d is not present", 1);
                else if (SaveDriveChanges(pDrive1))
                    InsertDisk(pDrive1);
                break;

            case actEjectFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsInserted() && SaveDriveChanges(pDrive1))
                {
                    Frame::SetStatus("%s  ejected from drive %d", pDrive1->GetFile(), 1);
                    pDrive1->Eject();
                }
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) != dskImage)
                    Message(msgWarning, "Floppy drive %d is not present", 2);
                else if (SaveDriveChanges(pDrive2))
                    InsertDisk(pDrive2);
                break;

            case actEjectFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsInserted() && SaveDriveChanges(pDrive2))
                {
                    Frame::SetStatus("%s  ejected from drive %d", pDrive2->GetFile(), 2);
                    pDrive2->Eject();
                }
                break;

            case actNewDisk1:
                if (SaveDriveChanges(pDrive1))
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 1);
                break;

            case actNewDisk2:
                if (SaveDriveChanges(pDrive2))
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_DISK), g_hwnd, NewDiskDlgProc, 2);
                break;

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
                Display::SetDirty();
                Frame::SetStatus("Scanlines %s", GetOption(scanlines) ? "enabled" : "disabled");
                break;

            case actChangeBorders:
                SetOption(borders, (GetOption(borders)+1) % 5);
                Frame::Init();
                UI::ResizeWindow(true);
                Frame::SetStatus(aszBorders[GetOption(borders)]);
                break;

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


void CentreWindow (HWND hwnd_, HWND hwndParent_/*=NULL*/)
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
    SetWindowPos(hwnd_, NULL, nX, nY, 0, 0, SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER);
}


LRESULT CALLBACK URLWndProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HCURSOR hHand = LoadCursor(NULL, MAKEINTRESOURCE(32649));    // IDC_HAND, which may not be available

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
            GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof lf, &lf);

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
                hfont = NULL;
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

                if (ShellExecute(NULL, NULL, szURL, NULL, "", SW_SHOWMAXIMIZED) <= reinterpret_cast<HINSTANCE>(32))
                    Message(msgWarning, "Failed to launch SimCoupé homepage");
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
    // Check if we're using an overlay video surface
    DDSURFACEDESC ddsdBack = { sizeof(ddsdBack) };
    bool fOverlay = pddsBack && SUCCEEDED(pddsBack->GetSurfaceDesc(&ddsdBack)) && (ddsdBack.ddsCaps.dwCaps & DDSCAPS_OVERLAY);

    // Is Alt-PrintScrn being release while using an overlay video surface?
    if (fOverlay && lParam_ < 0 && wParam_ == VK_SNAPSHOT && GetAsyncKeyState(VK_LMENU) < 0)
        PostMessage(g_hwnd, WM_USER+0, 1234, 5678L);

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


LRESULT CALLBACK WindowProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static bool fInMenu = false, fHideCursor = false, fSizingOrMoving = false;
    static UINT_PTR ulMouseTimer = 0;

    static COwnerDrawnMenu odmenu(NULL, IDT_MENU, aMenuIcons);

    LRESULT lResult;
    if (odmenu.WindowProc(hwnd_, uMsg_, wParam_, lParam_, &lResult))
        return lResult;

//  TRACE("WindowProc(%#04x,%#08x,%#08lx,%#08lx)\n", hwnd_, uMsg_, wParam_, lParam_);

    // If the keyboard is used, simulate early timer expiry to hide the cursor
    if (uMsg_ == WM_KEYDOWN && ulMouseTimer)
        ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, 1, NULL);

    // Input has first go at processing any messages
    if (Input::FilterMessage(hwnd_, uMsg_, wParam_, lParam_))
        return 0;

    switch (uMsg_)
    {
        // Main window being created
        case WM_CREATE:
            // Allow files to be dropped onto the main window if the first drive is present
            DragAcceptFiles(hwnd_, GetOption(drive1) == dskImage);

            // Hook keyboard input to our thread
            hWinKeyHook = SetWindowsHookEx(WH_KEYBOARD, WinKeyHookProc, NULL, GetCurrentThreadId());
            return 0;

        // Application close request
        case WM_CLOSE:
            Sound::Silence();

            // Ensure both drives are saved before we exit
            if (!SaveDriveChanges(pDrive1) || !SaveDriveChanges(pDrive2))
                return 0;

            DestroyWindow(hwnd_);
            UnhookWindowsHookEx(hWinKeyHook);
            hWinKeyHook = NULL;

            return 0;

        // Main window is being destroyed
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        // System shutting down or using logging out
        case WM_QUERYENDSESSION:
            // Save without prompting, to avoid data loss
            if (pDrive1) pDrive1->Save();
            if (pDrive2) pDrive2->Save();
            return TRUE;


        // Main window being activated or deactivated
        case WM_ACTIVATE:
        {
            TRACE("WM_ACTIVATE (%#08lx)\n", wParam_);
            g_fActive = (LOWORD(wParam_) != WA_INACTIVE) && !IsIconic(hwnd_);
            bool fChildOpen = GetParent(reinterpret_cast<HWND>(lParam_)) == hwnd_;
            TRACE(" g_fActive=%d, fChildOpen=%d\n", g_fActive, fChildOpen);

            // When the main window becomes inactive to a child window, silence the sound and release the mouse
            if (!g_fActive && fChildOpen)
            {
                Sound::Silence();
                Input::Acquire(false, false);
            }

            // Set an appropriate caption if we pause when inactive
            if (GetOption(pauseinactive))
            {
                if (g_fActive && !g_fPaused)
                    SetWindowText(hwnd_, WINDOW_CAPTION);
                else
                {
                    SetWindowText(hwnd_, WINDOW_CAPTION " - Paused");
                    Sound::Silence();
                }
            }

            // Show the palette dimmed if a child dialog is open
            Video::CreatePalettes(!g_fActive && fChildOpen);
            Frame::Redraw();
            break;
        }

        // Application being activated or deactivated
        case WM_ACTIVATEAPP:
        {
            TRACE("WM_ACTIVATEAPP (w=%#08lx l=%#08lx)\n", wParam_, lParam_);

            // Ensure the palette is correct if we're running fullscreen
            if (g_fActive && GetOption(fullscreen))
                Video::CreatePalettes();

            // If the application is now inactive, 
            if (!g_fActive)
            {
                Input::Acquire(false);
                fHideCursor = false;
                ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, MOUSE_HIDE_TIME, NULL);
            }
            break;
        }


        // File has been dropped on our window
        case WM_DROPFILES:
        {
            char szFile[MAX_PATH]="";

            // Query the first (and only?) file dropped
            if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof(szFile)))
            {
                // Bring our window to the front
                SetForegroundWindow(g_hwnd);

                // Insert the image into drive 1, if available
                if (GetOption(drive1) != dskImage)
                    Message(msgWarning, "Floppy drive %d is not present", 1);
                else if (SaveDriveChanges(pDrive1))
                {
                    if (!pDrive1->Insert(szFile))
                        Message(msgWarning, "Invalid disk image: %s", szFile);
                    else
                    {
                        Frame::SetStatus("%s  inserted into drive 1", pDrive1->GetFile());
                        AddRecentFile(szFile);
                    }
                }
            }

            return 0;
        }


        // Reinitialise the video if something changes
        case WM_SYSCOLORCHANGE:
            Display::Init();
            break;

        // Input language has changed - reinitialise input to pick up the new mappings
        case WM_INPUTLANGCHANGE:
            Input::Init();
            return 1;

        // System time has changed - update the SAM time if we're keeping it synchronised
        case WM_TIMECHANGE:
            IO::InitClocks();
            break;


        // Menu is about to be activated
        case WM_INITMENU:
            UpdateMenuFromOptions();
            break;

        case WM_SIZING:
        {
            RECT* pRect = reinterpret_cast<RECT*>(lParam_);

            // We need a screen to resize!
            if (!Frame::GetScreen())
                break;

            // Determine the size of the current sizing area
            RECT rWindow = *pRect;
            OffsetRect(&rWindow, -rWindow.left, -rWindow.top);

            // Get the screen size, adjusting for 5:4 mode if necessary
            int nWidth = Frame::GetWidth() >> 1, nHeight = Frame::GetHeight() >> 1;
            if (GetOption(ratio5_4))
                nWidth = MulDiv(nWidth, 5, 4);

            // Determine how big the window would be for an nWidth*nHeight client area
            RECT rNonClient = { 0, 0, nWidth, nHeight };
            AdjustWindowRectEx(&rNonClient, GetWindowStyle(g_hwnd), TRUE, GetWindowExStyle(g_hwnd));
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

            // If we can't use free scaling, stick to multiples of half the screen size to stop software mode struggling
            if ((GetAsyncKeyState(VK_SHIFT) < 0) ^ !GetOption(stretchtofit))
            {
                // Form the new client size
                nWidth *= GetOption(scale);
                nHeight *= GetOption(scale);
            }
            else
            {
                if (rWindow.bottom != nHeight*GetOption(scale))
                    SetOption(scale, 0);

                nWidth = rWindow.right;
                nHeight = rWindow.bottom;
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

        // Keep track of whether the window is being resized or moved
        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE:
            fSizingOrMoving = (uMsg_ == WM_ENTERSIZEMOVE);
            break;

        // Menu has been opened
        case WM_ENTERMENULOOP:
            fInMenu = true;
            Sound::Silence();
            Input::Acquire(false);
            break;

        // Handle the exit for modal dialogues, when we get enabled
        case WM_ENABLE:
            if (!wParam_)
                break;

            // Fall through to WM_EXITMENULOOP...

        case WM_EXITMENULOOP:
            // No longer in menu, so start timer to hide the mouse if not used again
            fInMenu = fHideCursor = false;
            ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, MOUSE_HIDE_TIME, NULL);
            break;


        // To avoid flicker, don't erase the background
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);

            // Forcibly redraw the screen if in using the menu, sizing, moving, or inactive and paused
            if (fInMenu || fSizingOrMoving || g_fPaused || !g_fActive)
                Frame::Redraw();

            EndPaint(hwnd_, &ps);
            return 0;
        }

        // System palette has changed
        case WM_PALETTECHANGED:
            // Don't react to our own palette changes!
            if (reinterpret_cast<HWND>(wParam_) == hwnd_)
                break;
            // fall through to WM_QUERYNEWPALETTE ...

        // We're about to get the input focus so make sure any palette is updated
        case WM_QUERYNEWPALETTE:
            Video::UpdatePalette();
            return TRUE;


        // Reposition any video overlay if the window is moved
        case WM_MOVING:
            Frame::Redraw();
            break;


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
                SetMenu(g_hwnd, NULL);

            // Generate a WM_SETCURSOR to update the cursor state
            POINT pt;
            GetCursorPos(&pt);
            SetCursorPos(pt.x, pt.y);
            return 0;

        case WM_SETCURSOR:
            // Hide the cursor unless it's being used for the Win32 GUI or the emulation using using it in windowed mode
            if (fHideCursor || Input::IsMouseAcquired() || GUI::IsActive())
            {
                // Only hide the cursor over the client area of the main window
                if (LOWORD(lParam_) == HTCLIENT && reinterpret_cast<HWND>(wParam_) == hwnd_)
                {
                    SetCursor(NULL);
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

//          TRACE("WM_MOUSEMOVE to %ld,%ld\n", pt.x, pt.y);

            // Has the mouse moved since last time?
            if ((pt.x != ptLast.x || pt.y != ptLast.y) && !Input::IsMouseAcquired())
            {
                // Show the cursor, but set a timer to hide it if not moved for a few seconds
                fHideCursor = false;
                ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, MOUSE_HIDE_TIME, NULL);

                if (!GetMenu(g_hwnd))
                    SetMenu(g_hwnd, g_hmenu);

                // Remember the new position
                ptLast = pt;
            }

            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            // Acquire the mouse if it's enabled
            if (GetOption(mouse) && !GUI::IsActive() && !Input::IsMouseAcquired())
            {
                Input::Acquire();
                ulMouseTimer = SetTimer(hwnd_, MOUSE_TIMER_ID, 1, NULL);
            }

            break;
        }

        // Silence the sound during window drags, and other clicks in the non-client area
        case WM_NCLBUTTONDOWN:
            Sound::Silence();
            break;

        case WM_SYSCOMMAND:
            // Is this an Alt-key combination?
            if ((wParam_ & 0xfff0) == SC_KEYMENU)
            {
                // Ignore the key if Ctrl is pressed, to avoid Win9x problems with AltGr activating the menu
                if (GetAsyncKeyState(VK_CONTROL < 0) || GetAsyncKeyState(VK_RMENU))
                    return 0;

                // If Alt alone is pressed, ensure the menu is visible
                if ((!GetOption(altforcntrl) || !lParam_) && !GetMenu(hwnd_))
                    SetMenu(hwnd_, g_hmenu);

                // Stop Windows processing SAM Cntrl-key combinations (if enabled) and Alt-Enter
                // As well as blocking access to the menu it avoids a beep for the unhandled ones (mainly Alt-Enter)
                if ((GetOption(altforcntrl) && lParam_) || lParam_ == VK_RETURN)
                    return 0;
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

            // Function key?
            if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            {
                // Ignore Windows-modified function keys unless the SAM keypad mapping is enabled
                if (GetOption(samfkeys) != (GetAsyncKeyState(VK_LWIN) < 0 || GetAsyncKeyState(VK_RWIN) < 0) && wParam_ <= VK_F10)
                    return 0;

                // Read the current states of the control and shift keys
                bool fCtrl  = GetAsyncKeyState(VK_CONTROL) < 0;
                bool fAlt   = GetAsyncKeyState(VK_MENU)    < 0;
                bool fShift = GetAsyncKeyState(VK_SHIFT)   < 0;

                Action::Key((int)wParam_-VK_F1+1, fPress, fCtrl, fAlt, fShift);
                return 0;
            }

            // Most of the emulator keys are handled above, but we've a few extra fixed mappings of our own (well, mine!)
            switch (wParam_)
            {
                case VK_ESCAPE: 
                {
                    if (GetOption(mouseesc) && Input::IsMouseAcquired())
                        Input::Acquire(false);
                    break;
                }

                // Keypad '-' = Z80 reset (can be held to view reset screens)
                case VK_SUBTRACT:   if (GetOption(keypadreset)) Action::Do(actResetButton, uMsg_ == WM_KEYDOWN);    break;
                case VK_DIVIDE:     if (fPress) Action::Do(actDebugger);    break;
                case VK_MULTIPLY:   if (fPress) Action::Do(actNmiButton);   break;
                case VK_ADD:        Action::Do(actTempTurbo, fPress);       break;

                case VK_CANCEL:
                case VK_PAUSE:
                    if (fPress)
                    {
                        // Ctrl-Break is used for reset
                        if (GetAsyncKeyState(VK_CONTROL) < 0)
                            CPU::Init();

                        // Shift-pause single steps
                        else if (GetAsyncKeyState(VK_SHIFT) < 0)
                            Action::Do(actFrameStep);

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

        // Handler for Alt+PrintScrn being used with an overlay surface - warn the user
        case WM_USER+0:
        {
            // Make sure it's really from us
            if (wParam_ == 1234 && lParam_ == 5678)
                MessageBox(hwnd_, "The Windows screenshot function cannot capture video overlays.\n\n"
                                  "On the Display tab in the options, de-select \"Use RGB/YUV video overlay\", then try again.",
                                  "SimCoupe", MB_ICONEXCLAMATION);
            break;
        }

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
                }
            }

            switch (wId)
            {
                case IDM_FILE_NEW_DISK1:        Action::Do(actNewDisk1);          break;
                case IDM_FILE_NEW_DISK2:        Action::Do(actNewDisk2);          break;
                case IDM_FILE_IMPORT_DATA:      Action::Do(actImportData);        break;
                case IDM_FILE_EXPORT_DATA:      Action::Do(actExportData);        break;
                case IDM_FILE_EXIT:             Action::Do(actExitApplication);   break;

                case IDM_TOOLS_OPTIONS:         Action::Do(actOptions);           break;
                case IDM_TOOLS_PRINTER_ONLINE:  Action::Do(actPrinterOnline);     break;
                case IDM_TOOLS_FLUSH_PRINTER:   Action::Do(actFlushPrinter);      break;
                case IDM_TOOLS_DEBUGGER:        Action::Do(actDebugger);          break;

                case IDM_FILE_FLOPPY1_DEVICE:
                case IDM_FILE_FLOPPY2_DEVICE:
                    if (!CFloppyStream::IsAvailable())
                    {
                        if (MessageBox(g_hwnd, "Real disk support requires a 3rd party driver.\n\nDo you want to download it?",
                            "fdrawcmd.sys not found", MB_ICONQUESTION|MB_YESNO) == IDYES)
                            ShellExecute(NULL, NULL, "http://simonowen.com/fdrawcmd/", NULL, "", SW_SHOWMAXIMIZED);
                    }

                    if (wId == IDM_FILE_FLOPPY1_DEVICE && GetOption(drive1) == dskImage && SaveDriveChanges(pDrive1) && pDrive1->Insert("A:"))
                        Frame::SetStatus("Using floppy drive %s", pDrive1->GetFile());
                    else if (wId == IDM_FILE_FLOPPY2_DEVICE && GetOption(drive2) == dskImage && SaveDriveChanges(pDrive2) && pDrive2->Insert("B:"))
                        Frame::SetStatus("Using floppy drive %s", pDrive2->GetFile());

                    break;

                case IDM_FILE_FLOPPY1_INSERT:       Action::Do(actInsertFloppy1); break;
                case IDM_FILE_FLOPPY1_EJECT:        Action::Do(actEjectFloppy1);  break;
                case IDM_FILE_FLOPPY1_SAVE_CHANGES: Action::Do(actSaveFloppy1);   break;

                case IDM_FILE_FLOPPY2_INSERT:       Action::Do(actInsertFloppy2); break;
                case IDM_FILE_FLOPPY2_EJECT:        Action::Do(actEjectFloppy2);  break;
                case IDM_FILE_FLOPPY2_SAVE_CHANGES: Action::Do(actSaveFloppy2);   break;


                case IDM_VIEW_FULLSCREEN:           Action::Do(actToggleFullscreen); break;
                case IDM_VIEW_SYNC:                 Action::Do(actToggleSync);       break;
                case IDM_VIEW_RATIO54:              Action::Do(actToggle5_4);        break;
                case IDM_VIEW_SCANLINES:            Action::Do(actToggleScanlines);  break;
                case IDM_VIEW_GREYSCALE:            Action::Do(actToggleGreyscale);  break;

                case IDM_VIEW_ZOOM_50:
                case IDM_VIEW_ZOOM_100:
                case IDM_VIEW_ZOOM_150:
                case IDM_VIEW_ZOOM_200:
                    SetOption(scale, wId-IDM_VIEW_ZOOM_50+1);
                    UI::ResizeWindow(true);
                    break;

                case IDM_VIEW_BORDERS0:
                case IDM_VIEW_BORDERS1:
                case IDM_VIEW_BORDERS2:
                case IDM_VIEW_BORDERS3:
                case IDM_VIEW_BORDERS4:
                    SetOption(borders, wId-IDM_VIEW_BORDERS0);
                    Frame::Init();
                    UI::ResizeWindow(true);
                    break;

                case IDM_SYSTEM_PAUSE:      Action::Do(actPause);           break;
                case IDM_SYSTEM_MUTESOUND:  Action::Do(actToggleMute);      break;
                case IDM_SYSTEM_NMI:        Action::Do(actNmiButton);       break;
                case IDM_SYSTEM_RESET:      Action::Do(actResetButton); Action::Do(actResetButton,false); break;

                // Items from help menu
                case IDM_HELP_GENERAL:
                    if (ShellExecute(hwnd_, NULL, OSD::GetFilePath("SimCoupe.txt"), NULL, "", SW_SHOWMAXIMIZED) <= reinterpret_cast<HINSTANCE>(32))
                        MessageBox(hwnd_, "Can't find SimCoupe.txt", "SimCoupe", MB_ICONEXCLAMATION);
                    break;
                case IDM_HELP_ABOUT:    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_ABOUT), g_hwnd, AboutDlgProc, NULL);   break;

                default:
                    if (wId >= IDM_FILE_RECENT1 && wId <= IDM_FILE_RECENT9)
                    {
                        const char* psz = szRecentFiles[wId - IDM_FILE_RECENT1];

                        if (!SaveDriveChanges(pDrive1))
                            break;
                        else if (pDrive1->Insert(psz))
                        {
                            Frame::SetStatus("%s  inserted into drive %d", pDrive1->GetFile(), 1);
                            AddRecentFile(psz);
                        }
                        else
                        {
                            Message(msgWarning, "Failed to open disk image:\n\n%s", psz);
                            RemoveRecentFile(psz);
                        }
                    }
                    else if (wId >= IDM_FLOPPY2_RECENT1 && wId <= IDM_FLOPPY2_RECENT9)
                    {
                        const char* psz = szRecentFiles[wId - IDM_FLOPPY2_RECENT1];

                        if (!SaveDriveChanges(pDrive2))
                            break;
                        else if (pDrive2->Insert(psz))
                        {
                            Frame::SetStatus("%s  inserted into drive %d", pDrive2->GetFile(), 2);
                            AddRecentFile(psz);
                        }
                        else
                        {
                            Message(msgWarning, "Failed to open disk image:\n\n%s", psz);
                            RemoveRecentFile(psz);
                        }
                    }
                    break;
            }
            break;
        }
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}


bool InitWindow ()
{
    // set up and register window class
    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = __hinstance;
    wc.hIcon = LoadIcon(__hinstance, MAKEINTRESOURCE(IDI_MAIN));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.hCursor = LoadCursor(__hinstance, MAKEINTRESOURCE(IDC_CURSOR));
    wc.lpszClassName = "SimCoupeClass";

    g_hmenu = LoadMenu(wc.hInstance, MAKEINTRESOURCE(IDR_MENU));
    LocaliseMenu(g_hmenu);

    // Create a window for the display (initially invisible)
    bool f = (RegisterClass(&wc) && (g_hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, WINDOW_CAPTION, WS_OVERLAPPEDWINDOW,
                                                            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, g_hmenu, wc.hInstance, NULL)));

    return f;
}

////////////////////////////////////////////////////////////////////////////////

void ClipPath (char* pszPath_, size_t nLen_)
{
    char *psz1 = NULL, *psz2 = NULL;

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

void ShortenPath (char* pszPath_, int nSize_)
{
    char sz1[MAX_PATH], sz2[MAX_PATH];

    lstrcpyn(sz1, OSD::GetDirPath(), sizeof(sz1));
    int n1 = lstrlen(sz1);
    sz1[n1-1] = '\0';

    lstrcpyn(sz2, pszPath_, sizeof(sz2));
    int n2 = lstrlen(sz2);
    sz2[n1-1] = sz2[n2+1] = '\0';

    if (!lstrcmpi(sz1, sz2))
        lstrcpyn(pszPath_, sz2+n1, nSize_);
}

void GetDlgItemPath (HWND hdlg_, int nId_, char* psz_, int nSize_)
{
    HWND hctrl = nId_ ? GetDlgItem(hdlg_, nId_) : hdlg_;
    GetWindowText(hctrl, psz_, nSize_);
    if (*psz_) lstrcpyn(psz_, OSD::GetFilePath(psz_), nSize_);
}

void SetDlgItemPath (HWND hdlg_, int nId_, const char* pcsz_, bool fSelect_=false)
{
    char sz[MAX_PATH];
    ShortenPath(lstrcpy(sz, pcsz_), sizeof(sz));

    HWND hctrl = nId_ ? GetDlgItem(hdlg_, nId_) : hdlg_;
    SetWindowText(hctrl, sz);

    if (fSelect_)
    {
        SendMessage(hctrl, EM_SETSEL, 0, -1);
        SetFocus(hctrl);
    }
}

int GetDlgItemValue (HWND hdlg_, int nId_, int nDefault_=-1)
{
    char sz[256];
    GetDlgItemText(hdlg_, nId_, sz, sizeof(sz));

    int nValue;
    return Expr::Eval(sz, nValue, Expr::simple) ? nValue : nDefault_;
}

void SetDlgItemValue (HWND hdlg_, int nId_, int nValue_)
{
    char sz[256];
    wsprintf(sz, "%d", nValue_);
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

void AddComboString(HWND hdlg_, UINT uID_, const char* pcsz_)
{
    SendDlgItemMessage(hdlg_, uID_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pcsz_));
}


void FillMidiInCombo (HWND hwndCombo_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    int nDevs = 0;//midiInGetNumDevs();
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("<not currently supported>"));
//  SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nDevs ? "<default device>" : "<None>"));

    if (nDevs)
    {
        for (int i = 0 ; i < nDevs ; i++)
        {
            MIDIINCAPS mc;
            if (midiInGetDevCaps(i, &mc, sizeof mc) == MMSYSERR_NOERROR)
                SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mc.szPname));
        }
    }

    // Select the current device in the list, or the first one if that fails
    if (!*GetOption(midiindev) || SendMessage(hwndCombo_, CB_SETCURSEL, atoi(GetOption(midiindev))+1, 0L) == CB_ERR)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);
}


void FillMidiOutCombo (HWND hwndCombo_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    int nDevs = midiOutGetNumDevs();
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nDevs ? "<default device>" : "<None>"));

    if (nDevs)
    {
        for (int i = 0 ; i < nDevs ; i++)
        {
            MIDIOUTCAPS mc;
            if (midiOutGetDevCaps(i, &mc, sizeof mc) == MMSYSERR_NOERROR)
                SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mc.szPname));
        }
    }

    // Select the current device in the list, or the first one if that fails
    if (!*GetOption(midioutdev) || SendMessage(hwndCombo_, CB_SETCURSEL, atoi(GetOption(midioutdev))+1, 0L) == CB_ERR)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);
}


void FillPrintersCombo (HWND hwndCombo_, LPCSTR pcszSelected_)
{
    int lSelected = 0;
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    // Dummy call to find out how much space is needed, then allocate it
    DWORD cbNeeded = 0, dwPrinters = 0;
    EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 1, NULL, 0, &cbNeeded, &dwPrinters);
    BYTE* pbPrinterInfo = new BYTE[cbNeeded];

    // Fill in dummy printer names, in the hope this avoids a WINE bug
    static char* cszUnknown = "<unknown printer>";
    for (int i = 0 ; i < static_cast<int>(dwPrinters) ; i++)
        reinterpret_cast<PRINTER_INFO_1*>(pbPrinterInfo)[i].pName = cszUnknown;

    // The first entry is to allow printing to auto-generated prntNNNN.bin files
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("File: prntNNNN.txt (auto-generated)"));

    // Enumerate the printers into the buffer we've allocated
    if (EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 1, pbPrinterInfo, cbNeeded, &cbNeeded, &dwPrinters))
    {
        // Loop through all the printers found, adding each to the printers combo
        for (int i = 0 ; i < static_cast<int>(dwPrinters) ; i++)
        {
            char szPrinter[256];
            wsprintf(szPrinter, PRINTER_PREFIX "%s", reinterpret_cast<PRINTER_INFO_1*>(pbPrinterInfo)[i].pName);
            SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szPrinter));

            // Remember the position of the currently selected printer (+1 since print-to-file is entry 0)
            if (!lstrcmpi(szPrinter+lstrlen(PRINTER_PREFIX), pcszSelected_))
                lSelected = i+1;
        }
    }

    delete[] pbPrinterInfo;

    // Select the active printer
    SendMessage(hwndCombo_, CB_SETCURSEL, lSelected, 0L);
}


void FillJoystickCombo (HWND hwndCombo_, const char* pcszSelected_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("None"));

    Input::FillJoystickCombo(hwndCombo_);

    // Find the position of the item to select, or select the first one (None) if we can't find it
    LRESULT lPos = SendMessage(hwndCombo_, CB_FINDSTRINGEXACT, 0U-1, reinterpret_cast<LPARAM>(pcszSelected_));
    if (lPos == CB_ERR)
        lPos = 0;
    SendMessage(hwndCombo_, CB_SETCURSEL, lPos, 0L);
}


////////////////////////////////////////////////////////////////////////////////

// Browse for an image, setting a specified filter with the path selected
void BrowseImage (HWND hdlg_, int nControl_, const char* pcszFilters_, const char* pcszDefDir_)
{
    char szFile[MAX_PATH] = "";

    GetDlgItemText(hdlg_, nControl_, szFile, sizeof(szFile));
    if (szFile[0]) lstrcpyn(szFile, OSD::GetFilePath(szFile), sizeof(szFile));

    static OPENFILENAME ofn = { sizeof(ofn) };
    ofn.hwndOwner    = hdlg_;
    ofn.lpstrFilter  = pcszFilters_;
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = sizeof(szFile);
    ofn.lpstrInitialDir = pcszDefDir_;
    ofn.Flags        = 0;

    if (GetSaveLoadFile(&ofn, true))
        SetDlgItemPath(hdlg_, nControl_, szFile, true);
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
    static char szFile[MAX_PATH], szAddress[128]="32768", szPage[128]="1", szOffset[128]="0", szLength[128]="0";
    static int nType = 0;
    static bool fImport;
    static POINT apt[2];

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            CentreWindow(hdlg_);
            fImport = !!lParam_;

            static const char* asz[] = { "BASIC Address (0-540671)", "Main Memory (pages 0-31)", "External RAM (pages 0-255)", NULL };
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

                    for (i = 0 ; i < sizeof(an1)/sizeof(an1[0]) ; i++)
                        ShowWindow(GetDlgItem(hdlg_, an1[i]), nShow1);

                    for (i = 0 ; i < sizeof(an2)/sizeof(an2[0]) ; i++)
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
                    ofn.hwndOwner       = hdlg_;
                    ofn.lpstrFilter     = "Data files (*.bin;*.dat;*.raw;*.txt)\0*.bin;*.dat;*.raw;*.txt\0All files (*.*)\0*.*\0";
                    ofn.lpstrFile       = szFile;
                    ofn.nMaxFile        = sizeof(szFile);
                    ofn.lpstrInitialDir = GetOption(datapath);
                    ofn.Flags           = OFN_HIDEREADONLY;

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
                    nPage = (nAddress < 0x4000) ? ROM0 : nPage;
                    size_t nDone = 0;

                    if (fImport)
                    {
                        for (int nChunk ; (nChunk = min(nLength, (0x4000 - nOffset))) ; nLength -= nChunk, nOffset = 0)
                        {
                            nDone += fread(&apbPageWritePtrs[nPage++][nOffset], 1, nChunk, f);

                            // Stop at the end of the file or if we've hit the end of a logical block
                            if (feof(f) || nPage == EXTMEM || nPage == ROM0 || nPage >= N_PAGES_MAIN)
                                break;
                        }

                        Frame::SetStatus("Imported %d bytes", nDone);
                    }
                    else
                    {
                        for (int nChunk ; (nChunk = min(nLength, (0x4000 - nOffset))) ; nLength -= nChunk, nOffset = 0)
                        {
                            nDone += fwrite(&apbPageWritePtrs[nPage++][nOffset], 1, nChunk, f);

                            if (ferror(f))
                            {
                                MessageBox(hdlg_, "Error writing to file", "Export Data", MB_ICONEXCLAMATION);
                                fclose(f);
                                return FALSE;
                            }

                            // Stop if we've hit the end of a logical block
                            if (nPage == EXTMEM || nPage == ROM0 || nPage == N_PAGES_MAIN)
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

            static const char* aszTypes[] = { "Flexible format EDSK image", "Normal format MGT image (800K)", "Normal format SAD image (800K)", "CP/M DOS image (720K)", NULL };
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
                    // Enable the format checkbox for EDSK only
                    nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPES, CB_GETCURSEL, 0, 0L);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FORMAT), !nType);

                    // Non-EDSK formats are fixed-format
                    if (nType)
                        SendDlgItemMessage(hdlg_, IDC_FORMAT, BM_SETCHECK, BST_CHECKED, 0L);

                    break;

                case IDOK:
                {
                    // File extensions for each type, plus an additional extension if compressed
                    static const char* aszTypes[] = { "dsk", "mgt", "sad", "cpm" };
                    static const char* aszCompress[] = { ".gz", ".gz", "", ".gz" };

                    nType = (int)SendDlgItemMessage(hdlg_, IDC_TYPES, CB_GETCURSEL, 0, 0L);
                    fCompress = SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_GETCHECK, 0, 0L) == BST_CHECKED;
                    fFormat = SendDlgItemMessage(hdlg_, IDC_FORMAT, BM_GETCHECK, 0, 0L) == BST_CHECKED;

                    char szFile [MAX_PATH];
                    wsprintf(szFile, "untitled.%s%s", aszTypes[nType], fCompress ? aszCompress[nType] : "");

                    static OPENFILENAME ofn = { sizeof(ofn) };
                    ofn.hwndOwner       = hdlg_;
                    ofn.lpstrFilter     = szFloppyFilters;
                    ofn.lpstrFile       = szFile;
                    ofn.nMaxFile        = sizeof(szFile);
                    ofn.lpstrInitialDir = GetOption(floppypath);
                    ofn.Flags           = OFN_HIDEREADONLY;

                    if (!GetSaveLoadFile(&ofn, false))
                        break;

                    CStream* pStream = NULL;
                    CDisk* pDisk = NULL;
#ifdef USE_ZLIB
                    if (fCompress)
                        pStream = new CZLibStream(NULL, szFile);
                    else
#endif
                        pStream = new CFileStream(NULL, szFile);

                    switch (nType)
                    {
                        case 0: pDisk = new CEDSKDisk(pStream); break;
                        default:
                        case 1: pDisk = new CMGTDisk(pStream);  break;
                        case 2: pDisk = new CSADDisk(pStream, NORMAL_DISK_SIDES, NORMAL_DISK_TRACKS, NORMAL_DISK_SECTORS, NORMAL_SECTOR_SIZE); break;
                        case 3: pDisk = new CMGTDisk(pStream, DOS_DISK_SECTORS); break;
                    }

                    // Format the EDSK image ready for use?
                    if (nType == 0 && fFormat)
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

                                pDisk->FormatTrack(head, cyl, abIDs, apbData, NORMAL_DISK_SECTORS);
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

                    // If we're to insert the new disk into a drive, do so now
                    if (nDrive == 1 && pDrive1->Insert(szFile))
                    {
                        Frame::SetStatus("%s  inserted into drive %d", pDrive1->GetFile(), nDrive);
                        AddRecentFile(szFile);
                    }
                    else if (nDrive == 2 && pDrive2->Insert(szFile))
                    {
                        Frame::SetStatus("%s  inserted into drive %d", pDrive2->GetFile(), nDrive);
                        AddRecentFile(szFile);
                    }
                    else
                        Frame::SetStatus("Failed to insert new disk!?");

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
                    bool fExists = pDisk != NULL;

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
                    ofn.hwndOwner       = hdlg_;
                    ofn.lpstrFilter     = szHDDFilters;
                    ofn.lpstrFile       = szFile;
                    ofn.nMaxFile        = sizeof(szFile);
                    ofn.lpstrInitialDir = GetOption(hddpath);
                    ofn.lpstrDefExt     = ".hdf";
                    ofn.Flags            = OFN_HIDEREADONLY;

                    if (GetSaveLoadFile(&ofn, true, false))
                        SetDlgItemPath(hdlg_, IDE_FILE, szFile, true);

                    break;
                }

                case IDOK:
                {
                    uSize = GetDlgItemValue(hdlg_, IDE_SIZE, 0);
                    UINT uCylinders = (uSize << 2) & 0x3fff;

                    // Check the geometry is within range, since the edit fields can be modified directly
                    if (!uCylinders || (uCylinders > 16383))
                        MessageBox(hdlg_, "Invalid disk geometry.", "Create", MB_OK|MB_ICONEXCLAMATION);
                    else
                    {
                        // If new values have been give, create a new disk using the supplied settings
                        if (IsWindowEnabled(GetDlgItem(hdlg_, IDE_SIZE)))
                        {
                            struct stat st;

                            // Warn before overwriting existing files
                            if (!::stat(szFile, &st) && 
                                    MessageBox(hdlg_, "Overwrite existing file?", "Create", MB_YESNO|MB_ICONEXCLAMATION) != IDYES)
                                break;

                            // Create the new HDF image
                            else if (!CHDFHardDisk::Create(szFile, uCylinders, 16, 32))
                            {
                                MessageBox(hdlg_, "Failed to create new disk (disk full?)", "Create", MB_OK|MB_ICONEXCLAMATION);
                                break;
                            }
                        }

                        // Set the new path back in the parent dialog, and close our dialog
                        SetDlgItemPath(hwndEdit, 0, szFile, true);
                        EndDialog(hdlg_, 1);
                    }

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
                LocaliseWindows(GetParent(hdlg_));
                CentreWindow(GetParent(hdlg_));
                fCentredOptions = true;
            }

            LocaliseWindows(hdlg_);

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
                    for (nOptionPage = MAX_OPTION_PAGES ; nOptionPage && ahwndPages[nOptionPage] != hdlg_ ; nOptionPage--);
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
            static const char* aszMain[] = { "256K", "512K", NULL };
            SetComboStrings(hdlg_, IDC_MAIN_MEMORY, aszMain, (GetOption(mainmem) >> 8) - 1);

            static const char* aszExternal[] = { "None", "1MB", "2MB", "3MB", "4MB", NULL };
            SetComboStrings(hdlg_, IDC_EXTERNAL_MEMORY, aszExternal, GetOption(externalmem));

            SetDlgItemPath(hdlg_, IDE_ROM, GetOption(rom));

            SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_SETCHECK, GetOption(fastreset) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_HDBOOT_ROM, BM_SETCHECK, GetOption(hdbootrom) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_ASIC_DELAY, BM_SETCHECK, GetOption(asicdelay) ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(mainmem, static_cast<int>((SendDlgItemMessage(hdlg_, IDC_MAIN_MEMORY, CB_GETCURSEL, 0, 0L) + 1) << 8));
                SetOption(externalmem, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL_MEMORY, CB_GETCURSEL, 0, 0L)));

                GetDlgItemPath(hdlg_, IDE_ROM, const_cast<char*>(GetOption(rom)), MAX_PATH);

                SetOption(fastreset, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_GETCHECK, 0, 0L) == BST_CHECKED));
                SetOption(hdbootrom, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_HDBOOT_ROM, BM_GETCHECK, 0, 0L) == BST_CHECKED));
                SetOption(asicdelay, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ASIC_DELAY, BM_GETCHECK, 0, 0L) == BST_CHECKED));
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDE_ROM:
                {
                    EnableWindow(GetDlgItem(hdlg_, IDC_HDBOOT_ROM), !GetWindowTextLength(GetDlgItem(hdlg_, IDE_ROM)));
                    break;
                }

                case IDB_BROWSE:
                {
                    static OPENFILENAME ofn = { sizeof(ofn) };

                    char szFile[MAX_PATH] = "";
                    GetDlgItemText(hdlg_, IDE_ROM, szFile, sizeof(szFile));
                    lstrcpyn(szFile, OSD::GetFilePath(szFile), sizeof(szFile));

                    ofn.hwndOwner       = hdlg_;
                    ofn.lpstrFilter     = "ROM images (*.rom;*.zx82)\0*.rom;*.zx82\0All files (*.*)\0*.*\0";
                    ofn.lpstrFile       = szFile;
                    ofn.nMaxFile        = sizeof(szFile);
                    ofn.lpstrInitialDir = GetOption(rompath);
                    ofn.Flags           = OFN_HIDEREADONLY;

                    if (GetSaveLoadFile(&ofn, true))
                        SetDlgItemPath(hdlg_, IDE_ROM, szFile, true);

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

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_SETCHECK, GetOption(hwaccel) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_OVERLAY, BM_SETCHECK, GetOption(overlay) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_STRETCH_TO_FIT, BM_SETCHECK, GetOption(stretchtofit) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_8BIT_FULLSCREEN, BM_SETCHECK, (GetOption(depth) == 8) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_SETCHECK, !GetOption(frameskip) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_FRAMESKIP_AUTOMATIC, 0L);

            HWND hwndCombo = GetDlgItem(hdlg_, IDC_FRAMESKIP);
            SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);
            SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("all frames"));

            for (int i = 2 ; i <= 10 ; i++)
            {
                char sz[32];
                wsprintf(sz, "every %d%s frame", i, (i==2) ? "nd" : (i==3) ? "rd" : "th");
                SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz));
            }

            SendMessage(hwndCombo, CB_SETCURSEL, (!GetOption(frameskip)) ? 0 : GetOption(frameskip) - 1, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_HWACCEL, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_OVERLAY, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            LPPSHNOTIFY ppsn = reinterpret_cast<LPPSHNOTIFY>(lParam_);

            if (ppsn->hdr.code == PSN_APPLY)
            {
                SetOption(hwaccel, SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(overlay, SendDlgItemMessage(hdlg_, IDC_OVERLAY, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(stretchtofit, SendDlgItemMessage(hdlg_, IDC_STRETCH_TO_FIT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(depth, (SendDlgItemMessage(hdlg_, IDC_8BIT_FULLSCREEN, BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 8 : 16);

                int nFrameSkip = SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_GETCHECK, 0, 0L) != BST_CHECKED;
                SetOption(frameskip, nFrameSkip ? static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FRAMESKIP, CB_GETCURSEL, 0, 0L)) + 1 : 0);

                if (Changed(hwaccel) || Changed(overlay) || (Changed(depth) && GetOption(fullscreen)))
                    Frame::Init();

                if (Changed(stretchtofit))
                    UI::ResizeWindow(!GetOption(stretchtofit));
            }

            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_HWACCEL:
                {
                    OSVERSIONINFO osvi = { sizeof osvi };
                    GetVersionEx(&osvi);

                    // Enable the overlay if hardware acceleration is enabled and we're not on Vista or later
                    bool fVistaOrLater = osvi.dwMajorVersion >= 6;
                    bool fHwAccel = (SendDlgItemMessage(hdlg_, IDC_HWACCEL, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                    EnableWindow(GetDlgItem(hdlg_, IDC_OVERLAY), fHwAccel && !fVistaOrLater);
                    break;
                }

                case IDC_OVERLAY:
                {
                    bool fOverlay = (SendDlgItemMessage(hdlg_, IDC_OVERLAY, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                    EnableWindow(GetDlgItem(hdlg_, IDC_8BIT_FULLSCREEN), !fOverlay);
                    break;
                }

                case IDC_FRAMESKIP_AUTOMATIC:
                {
                    bool fAutomatic = (SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FRAMESKIP), !fAutomatic);
                    break;
                }
            }

            break;
        }
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
            static const char* aszLatency[] = { "1 frame (best)", "2 frames", "3 frames", "4 frames", "5 frames (default)",
                                                "10 frames", "15 frames", "20 frames", "25 frames", NULL };
            int nLatency = GetOption(latency);
            nLatency = (nLatency <= 5 ) ? nLatency - 1 : nLatency/5 + 3;
            SetComboStrings(hdlg_, IDC_LATENCY, aszLatency, nLatency);

            SendDlgItemMessage(hdlg_, IDC_SAASOUND, BM_SETCHECK, GetOption(saasound) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_SETCHECK, GetOption(beeper) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_STEREO, BM_SETCHECK, GetOption(stereo) ? BST_CHECKED : BST_UNCHECKED, 0L);

#ifndef USE_SAASOUND
            EnableWindow(GetDlgItem(hdlg_, IDC_SAASOUND), false);
#endif
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(saasound, SendDlgItemMessage(hdlg_, IDC_SAASOUND, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(beeper, SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_GETCHECK,  0, 0L) == BST_CHECKED);
                SetOption(stereo, SendDlgItemMessage(hdlg_, IDC_STEREO, BM_GETCHECK,  0, 0L) == BST_CHECKED);

                int nLatency = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_LATENCY, CB_GETCURSEL, 0, 0L));
                nLatency = (nLatency < 5) ? nLatency + 1 : (nLatency - 3) * 5;
                SetOption(latency, nLatency);

                if (Changed(saasound) || Changed(beeper) || Changed(stereo) || Changed(latency))
                    Sound::Init();

                if (Changed(beeper))
                    IO::InitBeeper();
            }

            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK DrivePageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszDrives1[] = { "None", "Floppy", NULL };
            SetComboStrings(hdlg_, IDC_DRIVE1, aszDrives1, GetOption(drive1));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE1, 0L);

            static const char* aszDrives2[] = { "None", "Floppy", "Atom", "Atom Lite", NULL };
            SetComboStrings(hdlg_, IDC_DRIVE2, aszDrives2, GetOption(drive2));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE2, 0L);

            static const char* aszSensitivity[] = { "Low sensitivity", "Medium sensitivity", "High sensitivity", NULL };
            SetComboStrings(hdlg_, IDC_SENSITIVITY, aszSensitivity,
                !GetOption(turboload) ? 1 : GetOption(turboload) <= 5 ? 2 : GetOption(turboload) <= 50 ? 1 : 0);

            SendDlgItemMessage(hdlg_, IDC_SAVE_PROMPT, BM_SETCHECK, GetOption(saveprompt) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_TURBO_LOAD, BM_SETCHECK, GetOption(turboload) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_AUTOBOOT, BM_SETCHECK, GetOption(autoboot) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_DOSBOOT, BM_SETCHECK, GetOption(dosboot) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SetDlgItemPath(hdlg_, IDE_DOSDISK, GetOption(dosdisk));

            SendMessage(hdlg_, WM_COMMAND, IDC_TURBO_LOAD, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_DOSBOOT, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                static int anSpeeds[] = { 85, 15, 2 };

                if (SendDlgItemMessage(hdlg_, IDC_TURBO_LOAD, BM_GETCHECK, 0, 0L) == BST_CHECKED)
                    SetOption(turboload, anSpeeds[SendDlgItemMessage(hdlg_, IDC_SENSITIVITY, CB_GETCURSEL, 0, 0L)]);
                else
                    SetOption(turboload, 0);

                SetOption(saveprompt, SendDlgItemMessage(hdlg_, IDC_SAVE_PROMPT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(autoboot, SendDlgItemMessage(hdlg_, IDC_AUTOBOOT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(dosboot,  SendDlgItemMessage(hdlg_, IDC_DOSBOOT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                GetDlgItemPath(hdlg_, IDE_DOSDISK, const_cast<char*>(GetOption(dosdisk)), MAX_PATH);

                SetOption(drive1, (int)SendMessage(GetDlgItem(hdlg_, IDC_DRIVE1), CB_GETCURSEL, 0, 0L));
                SetOption(drive2, (int)SendMessage(GetDlgItem(hdlg_, IDC_DRIVE2), CB_GETCURSEL, 0, 0L));

                if (Changed(drive1) || Changed(drive2))
                    IO::InitDrives();
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_TURBO_LOAD:
                {
                    bool fTurboLoad = SendDlgItemMessage(hdlg_, IDC_TURBO_LOAD, BM_GETCHECK, 0, 0L) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hdlg_, IDC_SENSITIVITY), fTurboLoad);
                    break;
                }

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
                    BrowseImage(hdlg_, IDE_DOSDISK, szFloppyFilters, GetOption(floppypath));   break;
                    break;
                }
            }
            break;
        }
    }

    return fRet;
}

INT_PTR CALLBACK DiskPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static UINT_PTR uTimer;
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_CLOSE:
            if (uTimer)
                KillTimer(hdlg_, 1);
            break;

        case WM_DEVICECHANGE:
            // Schdule a refresh of the device list at a safer time
            uTimer = SetTimer(hdlg_, 1, 1000, NULL);
            break;

        case WM_TIMER:
            KillTimer(hdlg_, 1);
            uTimer = 0;

            // Fall through...

        case WM_INITDIALOG:
        {
            // Drop-down defaults for the drives
            AddComboString(hdlg_, IDC_FLOPPY1, "A:");
            AddComboString(hdlg_, IDC_FLOPPY2, "");
            AddComboString(hdlg_, IDC_ATOM, "");
            AddComboString(hdlg_, IDC_SDIDE, "");
            AddComboString(hdlg_, IDC_YATBUS, "");

            // Refresh the options from the active devices
            if (GetOption(drive1) == dskImage) SetOption(disk1, pDrive1->GetPath());
            if (GetOption(drive2) == dskImage) SetOption(disk2, pDrive2->GetPath());
            if (GetOption(drive2) >= dskAtom)  SetOption(atomdisk, pDrive2->GetPath());
            SetOption(sdidedisk, pSDIDE->GetPath());
            SetOption(yatbusdisk, pYATBus->GetPath());

            // Set the edit controls to the current settings
            SetDlgItemPath(hdlg_, IDC_FLOPPY1, GetOption(disk1));
            SetDlgItemPath(hdlg_, IDC_FLOPPY2, GetOption(disk2));
            SetDlgItemPath(hdlg_, IDC_ATOM, GetOption(atomdisk));
            SetDlgItemPath(hdlg_, IDC_SDIDE, GetOption(sdidedisk));
            SetDlgItemPath(hdlg_, IDC_YATBUS, GetOption(yatbusdisk));

            // Look for SAM-compatible physical drives
            for (UINT u = 0 ; u < 10 ; u++)
            {
                char szDrive[32];
                wsprintf(szDrive, "\\\\.\\PhysicalDrive%u", u);

                CHardDisk* pDisk = CHardDisk::OpenObject(szDrive);
                if (pDisk)
                {
                    AddComboString(hdlg_, IDC_ATOM, szDrive);
                    AddComboString(hdlg_, IDC_SDIDE, szDrive);
                    AddComboString(hdlg_, IDC_YATBUS, szDrive);
                    delete pDisk;
                }
            }
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                bool fFloppy1 = GetOption(drive1) == dskImage, fFloppy2 = GetOption(drive2) == dskImage;

                GetDlgItemPath(hdlg_, IDC_FLOPPY1, const_cast<char*>(GetOption(disk1)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDC_FLOPPY2, const_cast<char*>(GetOption(disk2)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDC_ATOM,    const_cast<char*>(GetOption(atomdisk)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDC_SDIDE,   const_cast<char*>(GetOption(sdidedisk)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDC_YATBUS,  const_cast<char*>(GetOption(yatbusdisk)), MAX_PATH);

                if (ChangedString(disk1) && fFloppy1 && SaveDriveChanges(pDrive1) && !pDrive1->Insert(GetOption(disk1)))
                {
                    Message(msgWarning, "Invalid disk: %s", GetOption(disk1));
                    SetWindowLongPtr(hdlg_, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }

                if (ChangedString(disk2) && fFloppy2 && SaveDriveChanges(pDrive2) && !pDrive2->Insert(GetOption(disk2)))
                {
                    Message(msgWarning, "Invalid disk: %s", GetOption(disk2));
                    SetWindowLongPtr(hdlg_, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }

                // If the Atom path has changed, activate it
                if (ChangedString(atomdisk))
                {
                    // If the Atom is active, force it to be remounted
                    if (GetOption(drive2) >= dskAtom)
                    {
                        delete pDrive2;
                        pDrive2 = NULL;
                    }

                    // Set drive 2 to Atom if we've got a path, or floppy otherwise
                    if (!*GetOption(atomdisk))
                        SetOption(drive2, dskImage);
                    else if (GetOption(drive2) != dskAtomLite)
                        SetOption(drive2, dskAtom);

                    // Re-initialise the drives to activate any changes
                    IO::InitDrives();

                    // Ensure it was mounted ok
                    if (*GetOption(atomdisk) && pDrive2->GetType() < dskAtom)
                    {
                        Message(msgWarning, "Invalid Atom disk: %s", GetOption(atomdisk));
                        SetWindowLongPtr(hdlg_, DWLP_MSGRESULT, PSNRET_INVALID);
                        return TRUE;
                    }
                }

                // Re-init the other hard drive interfaces if anything has changed
                if (ChangedString(sdidedisk) || ChangedString(yatbusdisk))
                {
                    if (ChangedString(sdidedisk))
                    {
                        delete pSDIDE;
                        pSDIDE = NULL;
                    }

                    if (ChangedString(yatbusdisk))
                    {
                        delete pYATBus;
                        pYATBus = NULL;
                    }

                    IO::InitHDD();
                }

                // If the SDIDE path changed, check it was mounted ok
                if (ChangedString(sdidedisk) && *GetOption(sdidedisk) && pSDIDE->GetType() != dskSDIDE)
                {
                    Message(msgWarning, "Invalid SDIDE disk: %s", GetOption(sdidedisk));
                    SetWindowLongPtr(hdlg_, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }

                // If the SDIDE path changed, check it was mounted ok
                if (ChangedString(yatbusdisk) && *GetOption(yatbusdisk) && pYATBus->GetType() != dskYATBus)
                {
                    Message(msgWarning, "Invalid YATBUS disk: %s", GetOption(yatbusdisk));
                    SetWindowLongPtr(hdlg_, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDB_FLOPPY1:   BrowseImage(hdlg_, IDC_FLOPPY1, szFloppyFilters, GetOption(floppypath));   break;
                case IDB_FLOPPY2:   BrowseImage(hdlg_, IDC_FLOPPY2, szFloppyFilters, GetOption(floppypath));   break;

                case IDB_ATOM:
                {
                    LPARAM lCtrl = reinterpret_cast<LPARAM>(GetDlgItem(hdlg_, IDC_ATOM));
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_HARDDISK), hdlg_, HardDiskDlgProc, lCtrl);
                    break;
                }

                case IDB_SDIDE:
                {
                    LPARAM lCtrl = reinterpret_cast<LPARAM>(GetDlgItem(hdlg_, IDC_SDIDE));
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_HARDDISK), hdlg_, HardDiskDlgProc, lCtrl);
                    break;
                }

                case IDB_YATBUS:
                {
                    LPARAM lCtrl = reinterpret_cast<LPARAM>(GetDlgItem(hdlg_, IDC_YATBUS));
                    DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_HARDDISK), hdlg_, HardDiskDlgProc, lCtrl);
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
    GetWindowText(hctrl, sz, sizeof sz);

    BROWSEINFO bi = {0};
    bi.hwndOwner = hdlg_;
    bi.lpszTitle = "Select default path:";
    bi.lpfn = BrowseFolderCallback;
    bi.lParam = reinterpret_cast<LPARAM>(OSD::GetDirPath(sz));
    bi.ulFlags = BIF_RETURNONLYFSDIRS|0x00000040;//|BIF_USENEWUI|BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

    if (pidl)
    {
        if (SHGetPathFromIDList(pidl, sz))
        {
            SetDlgItemPath(hdlg_, nControl_, sz);
            SendMessage(hctrl, EM_SETSEL, 0, -1);
            SetFocus(hctrl);

        }

        IMalloc *imalloc;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }
    }
}

INT_PTR CALLBACK PathPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SetDlgItemPath(hdlg_, IDE_FLOPPY_PATH, GetOption(floppypath));
            SetDlgItemPath(hdlg_, IDE_HDD_PATH,    GetOption(hddpath));
            SetDlgItemPath(hdlg_, IDE_ROM_PATH,    GetOption(rompath));
            SetDlgItemPath(hdlg_, IDE_DATA_PATH,   GetOption(datapath));
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                GetDlgItemPath(hdlg_, IDE_FLOPPY_PATH, const_cast<char*>(GetOption(floppypath)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDE_HDD_PATH,    const_cast<char*>(GetOption(hddpath)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDE_ROM_PATH,    const_cast<char*>(GetOption(rompath)), MAX_PATH);
                GetDlgItemPath(hdlg_, IDE_DATA_PATH,   const_cast<char*>(GetOption(datapath)), MAX_PATH);
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDB_FLOPPY_PATH:   BrowseFolder(hdlg_, IDE_FLOPPY_PATH, GetOption(floppypath)); break;
                case IDB_HDD_PATH:      BrowseFolder(hdlg_, IDE_HDD_PATH,    GetOption(hddpath));    break;
                case IDB_ROM_PATH:      BrowseFolder(hdlg_, IDE_ROM_PATH,    GetOption(rompath));    break;
                case IDB_DATA_PATH:     BrowseFolder(hdlg_, IDE_DATA_PATH,   GetOption(datapath));   break;
            }
            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK InputPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMapping[] = { "None (raw)", "SAM Coupé", "Sinclair Spectrum", NULL };
            SetComboStrings(hdlg_, IDC_KEYBOARD_MAPPING, aszMapping, GetOption(keymapping));

            SendDlgItemMessage(hdlg_, IDC_ALT_FOR_CNTRL, BM_SETCHECK, GetOption(altforcntrl) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_ALTGR_FOR_EDIT, BM_SETCHECK, GetOption(altgrforedit) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_KPMINUS_RESET, BM_SETCHECK, GetOption(keypadreset) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SAM_FKEYS, BM_SETCHECK, GetOption(samfkeys) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_SETCHECK, GetOption(mouse) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_MOUSE_SWAP23, BM_SETCHECK, GetOption(swap23) ? BST_CHECKED : BST_UNCHECKED, 0L);

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
                SetOption(keypadreset, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_KPMINUS_RESET, BM_GETCHECK, 0, 0L)) == BST_CHECKED);
                SetOption(samfkeys, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SAM_FKEYS, BM_GETCHECK, 0, 0L)) == BST_CHECKED);

                SetOption(mouse, SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(swap23, SendDlgItemMessage(hdlg_, IDC_MOUSE_SWAP23, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                if (Changed(keymapping) || Changed(mouse))
                    Input::Init();
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_MOUSE_ENABLED:
                {
                    bool fMouse = SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hdlg_, IDC_MOUSE_SWAP23), fMouse);
                    break;
                }
            }

            break;
        }
    }

    return fRet;
}


INT_PTR CALLBACK JoystickPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK1), GetOption(joydev1));
            FillJoystickCombo(GetDlgItem(hdlg_, IDC_JOYSTICK2), GetOption(joydev2));

            static const char* aszDeadZone[] = { "None", "10%", "20%", "30%", "40%", "50%", NULL };
            SetComboStrings(hdlg_, IDC_DEADZONE_1, aszDeadZone, GetOption(deadzone1)/10);
            SetComboStrings(hdlg_, IDC_DEADZONE_2, aszDeadZone, GetOption(deadzone2)/10);

            SendMessage(hdlg_, WM_COMMAND, IDC_JOYSTICK1, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_JOYSTICK2, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                HWND hwndCombo1 = GetDlgItem(hdlg_, IDC_DEADZONE_1), hwndCombo2 = GetDlgItem(hdlg_, IDC_DEADZONE_2);
                SetOption(deadzone1, 10 * static_cast<int>(SendMessage(hwndCombo1, CB_GETCURSEL, 0, 0L)));
                SetOption(deadzone2, 10 * static_cast<int>(SendMessage(hwndCombo2, CB_GETCURSEL, 0, 0L)));

                HWND hwndJoy1 = GetDlgItem(hdlg_, IDC_JOYSTICK1), hwndJoy2 = GetDlgItem(hdlg_, IDC_JOYSTICK2);
                SendMessage(hwndJoy1, CB_GETLBTEXT, SendMessage(hwndJoy1, CB_GETCURSEL, 0, 0L), (LPARAM)GetOption(joydev1));
                SendMessage(hwndJoy2, CB_GETLBTEXT, SendMessage(hwndJoy2, CB_GETCURSEL, 0, 0L), (LPARAM)GetOption(joydev2));

                if (Changed(deadzone1) || Changed(deadzone1) || ChangedString(joydev1) || ChangedString(joydev2))
                    Input::Init();
            }

            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_JOYSTICK1:
                    EnableWindow(GetDlgItem(hdlg_, IDC_DEADZONE_1), SendDlgItemMessage(hdlg_, IDC_JOYSTICK1, CB_GETCURSEL, 0, 0L) != 0);
                    break;

                case IDC_JOYSTICK2:
                    EnableWindow(GetDlgItem(hdlg_, IDC_DEADZONE_2), SendDlgItemMessage(hdlg_, IDC_JOYSTICK2, CB_GETCURSEL, 0, 0L) != 0);
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
            static const char* aszParallel[] = { "None", "Printer", "Mono DAC", "Stereo EDdac/SAMdac", NULL };
            SetComboStrings(hdlg_, IDC_PARALLEL_1, aszParallel, GetOption(parallel1));
            SetComboStrings(hdlg_, IDC_PARALLEL_2, aszParallel, GetOption(parallel2));

            FillPrintersCombo(GetDlgItem(hdlg_, IDC_PRINTERS), GetOption(printerdev));

            static const char* aszFlushDelay[] = { "Disabled", "After 1 second idle", "After 2 seconds idle", "After 3 seconds idle",
                                                    "After 4 seconds idle", "After 5 seconds idle", NULL };
            SetComboStrings(hdlg_, IDC_FLUSHDELAY, aszFlushDelay, GetOption(flushdelay));

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

                SetOption(printerdev, "");
                LRESULT lPrinter = SendDlgItemMessage(hdlg_, IDC_PRINTERS, CB_GETCURSEL, 0, 0L);

                // Fetch the printer name unless print-to-file is selected
                if (lPrinter)
                {
                    char szPrinter[256];
                    SendDlgItemMessage(hdlg_, IDC_PRINTERS, CB_GETLBTEXT, lPrinter, reinterpret_cast<LPARAM>(szPrinter));
                    SetOption(printerdev, szPrinter+lstrlen(PRINTER_PREFIX));
                }

                SetOption(flushdelay, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FLUSHDELAY, CB_GETCURSEL, 0, 0L)));

                if (Changed(parallel1) || Changed(parallel2) || ChangedString(printerdev))
                    IO::InitParallel();
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
                    EnableWindow(GetDlgItem(hdlg_, IDS_FLUSHDELAY), fPrinter1 || fPrinter2);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FLUSHDELAY), fPrinter1 || fPrinter2);
                    break;
                }
           }

            break;
        }
    }

    return fRet;
}

INT_PTR CALLBACK MidiPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMIDI[] = { "None", "Windows MIDI", NULL };
            SetComboStrings(hdlg_, IDC_MIDI, aszMIDI, GetOption(midi));
            SendMessage(hdlg_, WM_COMMAND, IDC_MIDI, 0L);

            FillMidiInCombo(GetDlgItem(hdlg_, IDC_MIDI_IN));
            FillMidiOutCombo(GetDlgItem(hdlg_, IDC_MIDI_OUT));

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                char sz[16];

                SetOption(midi, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_MIDI, CB_GETCURSEL, 0, 0L)));
                
                // Update the MIDI IN and MIDI OUT device numbers
                SetOption(midiindev,  itoa((int)SendDlgItemMessage(hdlg_, IDC_MIDI_IN,  CB_GETCURSEL, 0, 0L)-1, sz, 10));
                SetOption(midioutdev, itoa((int)SendDlgItemMessage(hdlg_, IDC_MIDI_OUT, CB_GETCURSEL, 0, 0L)-1, sz, 10));

                if (Changed(midi) || ChangedString(midiindev) || ChangedString(midioutdev))
                    IO::InitMidi();
            }

            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam_))
            {
                case IDC_MIDI:
                {
                    LRESULT lMidi = SendDlgItemMessage(hdlg_, IDC_MIDI, CB_GETCURSEL, 0, 0L);
                    EnableWindow(GetDlgItem(hdlg_, IDC_MIDI_OUT), lMidi == 1);
                    EnableWindow(GetDlgItem(hdlg_, IDC_MIDI_IN), /*lMidi == 1*/ FALSE);     // No MIDI-In support yet
                    EnableWindow(GetDlgItem(hdlg_, IDE_STATION_ID), /*lMidi == 2*/ FALSE);  // No network support yet
                    break;
                }
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

            SendDlgItemMessage(hdlg_, IDC_PAUSE_INACTIVE, BM_SETCHECK, GetOption(pauseinactive) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_DRIVE_LIGHTS, BM_SETCHECK, GetOption(drivelights) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_STATUS, BM_SETCHECK, GetOption(status) ? BST_CHECKED : BST_UNCHECKED, 0L);

            static const char* aszProfile[] = { "Disabled", "Speed and frame rate", "Detailed percentages", "Detailed timings", NULL };
            SetComboStrings(hdlg_, IDC_PROFILE, aszProfile, GetOption(profile));

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(sambusclock, SendDlgItemMessage(hdlg_, IDC_SAMBUS_CLOCK, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(dallasclock, SendDlgItemMessage(hdlg_, IDC_DALLAS_CLOCK, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(pauseinactive, SendDlgItemMessage(hdlg_, IDC_PAUSE_INACTIVE, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(drivelights, SendDlgItemMessage(hdlg_, IDC_DRIVE_LIGHTS, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(status, SendDlgItemMessage(hdlg_, IDC_STATUS, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(profile, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PROFILE, CB_GETCURSEL, 0, 0L)));

                if (Changed(sambusclock) || Changed(dallasclock))
                    IO::InitClocks();
            }
            break;
        }
    }

    return fRet;
}


// Function to compare two function key list items, for sorting
int CALLBACK FnKeysCompareFunc (LPARAM lParam1_, LPARAM lParam2_, LPARAM lParamSort_)
{
    return static_cast<int>(lParam1_ - lParam2_);
}


// Window message hook handler for catching function keypresses before anything else sees them
// This is about the only way to see the function keys in a modal dialogue box, as we don't control the message pump!
LRESULT CALLBACK GetMsgHookProc (int nCode_, WPARAM wParam_, LPARAM lParam_)
{
    if (nCode_ >= 0)
    {
        MSG* pMsg = reinterpret_cast<MSG*>(lParam_);

        // We need to eat key messages that relate to the function keys (F10 is a system key)
        if ((pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN || pMsg->message == WM_SYSKEYUP) &&
            (pMsg->wParam >= VK_F1 && pMsg->wParam <= VK_F12))
        {
            // Send it directly to the key setup dialogue box if it's a key down
            if (pMsg->message != WM_SYSKEYUP)
                SendMessage(hdlgNewFnKey, WM_KEYDOWN, pMsg->wParam, pMsg->lParam);

            // Stop anything else seeing the keypress
            pMsg->message = WM_NULL;
            pMsg->wParam = pMsg->lParam = 0;
        }
    }

    // Pass on to the next hook
    return CallNextHookEx(g_hFnKeyHook, nCode_, wParam_, lParam_);
}


INT_PTR CALLBACK NewFnKeyProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            CentreWindow(hdlg_);

            // For now we only support F1-F12
            char szKey[32];
            for (int i = VK_F1 ; i <= VK_F12 ; i++)
            {
                GetKeyNameText(MapVirtualKeyEx(i, 0, GetKeyboardLayout(0)) << 16, szKey, sizeof szKey);
                SendDlgItemMessage(hdlg_, IDC_KEY, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szKey));
            }

            // Fill the actions combo with the list of actions
            for (int n = 0 ; n < MAX_ACTION ; n++)
            {
                // Only add defined strings
                if (Action::aszActions[n] && *Action::aszActions[n])
                    SendDlgItemMessage(hdlg_, IDC_ACTION, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Action::aszActions[n]));
            }

            // If we're editing an entry we need to show the current settings
            if (lParam_)
            {
                UINT uKey = static_cast<UINT>(lParam_);

                // Get the name of the key being edited
                GetKeyNameText(MapVirtualKeyEx(uKey >> 16, 0, GetKeyboardLayout(0)) << 16, szKey, sizeof szKey);

                // Locate the key in the list and select it
                HWND hwndCombo = GetDlgItem(hdlg_, IDC_KEY);
                LRESULT lPos = SendMessage(hwndCombo, CB_FINDSTRINGEXACT, 0U-1, reinterpret_cast<LPARAM>(szKey));
                SendMessage(hwndCombo, CB_SETCURSEL, (lPos == CB_ERR) ? 0 : lPos, 0L);

                // Check the appropriate modifier check-boxes
                SendDlgItemMessage(hdlg_, IDC_CTRL,  BM_SETCHECK, (uKey & 0x8000) ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_ALT,   BM_SETCHECK, (uKey & 0x4000) ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_SHIFT, BM_SETCHECK, (uKey & 0x2000) ? BST_CHECKED : BST_UNCHECKED, 0L);

                // Locate the action in the list and select it
                hwndCombo = GetDlgItem(hdlg_, IDC_ACTION);
                UINT uAction = min(uKey & 0xff, MAX_ACTION-1);
                lPos = SendMessage(hwndCombo, CB_FINDSTRINGEXACT, 0U-1, reinterpret_cast<LPARAM>(Action::aszActions[uAction]));
                SendMessage(hwndCombo, CB_SETCURSEL, (lPos == CB_ERR) ? 0 : lPos, 0L);
            }

            // Select the first item in each combo to get them started
            else
            {
                SendDlgItemMessage(hdlg_, IDC_KEY, CB_SETCURSEL, 0, 0L);
                SendDlgItemMessage(hdlg_, IDC_ACTION, CB_SETCURSEL, 0, 0L);
            }

            // Set up a window hook so we can capture any function key presses - this may seem drastic, but it's not
            // possible any other way because the modal dialogue box has its own internal message pump
            hdlgNewFnKey = hdlg_;
            g_hFnKeyHook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgHookProc, NULL, GetCurrentThreadId());

            return TRUE;
        }

        case WM_DESTROY:
            UnhookWindowsHookEx(g_hFnKeyHook);
            g_hFnKeyHook = NULL;
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDOK:
                {
                    LRESULT lKey = SendDlgItemMessage(hdlg_, IDC_KEY, CB_GETCURSEL, 0, 0L);
                    UINT uAction = static_cast<UINT>(SendDlgItemMessage(hdlg_, IDC_ACTION, CB_GETCURSEL, 0, 0L));

                    char szAction[64];
                    SendDlgItemMessage(hdlg_, IDC_ACTION, CB_GETLBTEXT, uAction, (LPARAM)&szAction);

                    // Look for the action in the list to find out the number (as there may be gaps if some are removed)
                    for (uAction = 0 ; uAction < MAX_ACTION ; uAction++)
                    {
                        // Only add defined strings
                        if (Action::aszActions[uAction] && *Action::aszActions[uAction] && !lstrcmpi(szAction, Action::aszActions[uAction]))
                            break;
                    }


                    char szKey[32];
                    SendDlgItemMessage(hdlg_, IDC_KEY, CB_GETLBTEXT, lKey, (LPARAM)&szKey);

                    // Only F1-F12 are supported at the moment
                    if (szKey[0] == 'F')
                    {
                        // Pack the key-code, shift/ctrl states and action into a DWORD for the new item data
                        DWORD dwParam = ((VK_F1 + strtoul(szKey+1, NULL, 0) - 1) << 16) | uAction;
                        dwParam |= (SendDlgItemMessage(hdlg_, IDC_CTRL,   BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 0x8000 : 0;
                        dwParam |= (SendDlgItemMessage(hdlg_, IDC_ALT,    BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 0x4000 : 0;
                        dwParam |= (SendDlgItemMessage(hdlg_, IDC_SHIFT,  BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 0x2000 : 0;

                        // Pass it back to the caller
                        EndDialog(hdlg_, dwParam);
                        break;
                    }
                }

                // Fall through...

                case IDCANCEL:
                    EndDialog(hdlg_, 0);
                    break;
            }

            break;
        }

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            {
                bool fCtrl  = GetAsyncKeyState(VK_CONTROL) < 0;
                bool fAlt   = GetAsyncKeyState(VK_MENU)    < 0;
                bool fShift = GetAsyncKeyState(VK_SHIFT)   < 0;

                SendDlgItemMessage(hdlg_, IDC_KEY, CB_SETCURSEL, wParam_ - VK_F1, 0L);
                SendDlgItemMessage(hdlg_, IDC_CTRL,  BM_SETCHECK, fCtrl  ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_ALT,   BM_SETCHECK, fAlt   ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_SHIFT, BM_SETCHECK, fShift ? BST_CHECKED : BST_UNCHECKED, 0L);

                return 0;
            }
            break;
        }
    }

    return FALSE;
}


INT_PTR CALLBACK FnKeysPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    INT_PTR fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            HWND hwndList = GetDlgItem(hdlg_, IDL_FNKEYS);
            LVCOLUMN lvc = { 0 };
            lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

            // Add the first column for key sequence
            lvc.cx = 70;
            lvc.pszText = "Keypress";
            lvc.cchTextMax = static_cast<int>(strlen(lvc.pszText))+1;
            SendMessage(hwndList, LVM_INSERTCOLUMN, lvc.iSubItem = 0, reinterpret_cast<LPARAM>(&lvc));

            // Add the second column for action
            lvc.cx = 140;
            lvc.pszText = "Action";
            lvc.cchTextMax = static_cast<int>(strlen(lvc.pszText))+1;
            SendMessage(hwndList, LVM_INSERTCOLUMN, lvc.iSubItem = 1, reinterpret_cast<LPARAM>(&lvc));


            LVITEM lvi = { 0 };
            lvi.mask = LVIF_TEXT | LVIF_PARAM;
            lvi.pszText = LPSTR_TEXTCALLBACK;

            // Grab an upper-case copy of the function key definition string
            char szKeys[256];
            CharUpper(lstrcpyn(szKeys, GetOption(fnkeys), sizeof(szKeys)));

            // Process each of the 'key=action' pairs in the string
            for (char* psz = strtok(szKeys, ", \t") ; psz ; psz = strtok(NULL, ", \t"))
            {
                // Leading C/A/S characters indicate that Ctrl/Alt/Shift modifiers are required with the key
                bool fCtrl  = (*psz == 'C');    if (fCtrl)  psz++;
                bool fAlt   = (*psz == 'A');    if (fAlt)   psz++;
                bool fShift = (*psz == 'S');    if (fShift) psz++;

                // Currently we only support function keys F1-F12
                if (*psz++ == 'F')
                {
                    // Pack the key-code, ctrl/alt/shift states and action into a DWORD for the item data, and insert the item
                    lvi.lParam = ((VK_F1 + strtoul(psz, &psz, 0) - 1) << 16) |
                                 (fCtrl ? 0x8000 : 0) | (fAlt ? 0x4000 : 0) | (fShift ? 0x2000 : 0);
                    lvi.lParam |= strtoul(++psz, NULL, 0);
                    SendMessage(hwndList, LVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&lvi));
                }
            }

            SendMessage(hwndList, LVM_SORTITEMS, 0, reinterpret_cast<LPARAM>(FnKeysCompareFunc));
            break;
        }

        case WM_NOTIFY:
        {
            NMLVDISPINFO* pnmv = reinterpret_cast<NMLVDISPINFO*>(lParam_);
            PSHNOTIFY* ppsn = reinterpret_cast<PSHNOTIFY*>(lParam_);

            // Is this a list control notification?
            if (wParam_ == IDL_FNKEYS)
            {
                switch (pnmv->hdr.code)
                {
                    // Double-click with the mouse?
                    case NM_DBLCLK:
                    {
                        // Edit whatever had the focus
                        SendMessage(hdlg_, WM_COMMAND, IDB_EDIT, 0L);
                        break;
                    }

                    // List item changed?
                    case LVN_ITEMCHANGED:
                    {
                        LRESULT lSelected = SendDlgItemMessage(hdlg_, IDL_FNKEYS, LVM_GETSELECTEDCOUNT, 0, 0L);
                        EnableWindow(GetDlgItem(hdlg_, IDB_EDIT), lSelected == 1);
                        EnableWindow(GetDlgItem(hdlg_, IDB_DELETE), lSelected != 0);

                        break;
                    }

                    // List control wants display information for an item?
                    case LVN_GETDISPINFO:
                    {
                        static char* szActions[] = { "Reset", "Generate NMI", "Frame sync", "Frame-skip" };

                        // The second column shows the action name
                        if (pnmv->item.iSubItem)
                        {
                            LPARAM lAction = pnmv->item.lParam & 0xff;
                            pnmv->item.pszText = const_cast<char*>((lAction < MAX_ACTION) ? Action::aszActions[lAction] : Action::aszActions[0]);
                        }

                        // The first column details the key combination
                        else
                        {
                            char szKey[64] = "";
                            if (pnmv->item.lParam & 0x8000)
                                lstrcat(szKey, "Ctrl-");

                            if (pnmv->item.lParam & 0x4000)
                                lstrcat(szKey, "Alt-");

                            if (pnmv->item.lParam & 0x2000)
                                lstrcat(szKey, "Shift-");

                            // Convert from virtual key-code to scan-code so we can get the name
                            DWORD dwScanCode = MapVirtualKeyEx(static_cast<UINT>(pnmv->item.lParam) >> 16, 0, GetKeyboardLayout(0));
                            GetKeyNameText(dwScanCode << 16, szKey+lstrlen(szKey), sizeof szKey - lstrlen(szKey));
                            lstrcpyn(pnmv->item.pszText, szKey, pnmv->item.cchTextMax);
                        }

                        break;
                    }
                }
            }

            // This a property sheet notification?
            else if (ppsn->hdr.hwndFrom == GetParent(hdlg_))
            {
                // Apply settings changes?
                if (ppsn->hdr.code == PSN_APPLY)
                {
                    char szKeys[256] = "";
                    LVITEM lvi = {0};
                    lvi.mask = LVIF_PARAM;

                    HWND hwndList = GetDlgItem(hdlg_, IDL_FNKEYS);
                    LRESULT lItems = SendMessage(hwndList, LVM_GETITEMCOUNT, 0, 0L);

                    // Look through all keys in the list
                    for (int i = 0 ; i < lItems ; i++)
                    {
                        lvi.iItem = i;

                        // Get the item details so we can see what the key is and does
                        if (SendMessage(hwndList, LVM_GETITEM, i, reinterpret_cast<LPARAM>(&lvi)))
                        {
                            // If there is a previous item, use a comma separator
                            if (szKeys[0]) strcat(szKeys, ",");

                            // Add a C/A/S prefixes for Ctrl/Alt/Shift
                            if (lvi.lParam & 0x8000) strcat(szKeys, "C");
                            if (lvi.lParam & 0x4000) strcat(szKeys, "A");
                            if (lvi.lParam & 0x2000) strcat(szKeys, "S");

                            // Add the key name, '=', and the action number
                            DWORD dwScanCode = MapVirtualKeyEx(static_cast<UINT>(lvi.lParam) >> 16, 0, GetKeyboardLayout(0));
                            GetKeyNameText(dwScanCode << 16, szKeys+lstrlen(szKeys), sizeof szKeys - lstrlen(szKeys));
                            strcat(szKeys, "=");
                            wsprintf(szKeys+lstrlen(szKeys), "%d", lvi.lParam & 0xff);
                        }
                    }

                    // Store the new combined string
                    SetOption(fnkeys, szKeys);
                    break;
                }
            }

            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam_))
            {
                case IDB_ADD:
                case IDB_EDIT:
                {
                    LVITEM lvi = {0};
                    int nEdit = -1;
                    LPARAM lEdit = 0;

                    bool fAdd = LOWORD(wParam_) == IDB_ADD, fEdit = !fAdd;
                    HWND hwndList = GetDlgItem(hdlg_, IDL_FNKEYS);
                    int nItems = static_cast<int>(SendMessage(hwndList, LVM_GETITEMCOUNT, 0, 0L));

                    // If this is an edit we need to find the item being edited
                    if (fEdit)
                    {
                        for (nEdit = 0 ; nEdit < nItems ; nEdit++)
                        {
                            lvi.mask = LVIF_PARAM;
                            lvi.iItem = nEdit;

                            // Look for a selected item, and fetch the details on it
                            if ((SendMessage(hwndList, LVM_GETITEMSTATE, nEdit, LVIS_SELECTED) & LVIS_SELECTED) &&
                                 SendMessage(hwndList, LVM_GETITEM, 0, reinterpret_cast<LPARAM>(&lvi)))
                                 break;
                        }

                        // If we didn't find a selected item, give up now
                        if (nEdit == nItems)
                            break;
                    }

                    // Prompt for the key details, give up if they cancelled
                    LPARAM lParam = DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEW_FNKEY), hdlg_, NewFnKeyProc, lEdit = lvi.lParam);
                    if (!lParam)
                        return 0;

                    // Delete any item being edited to make way for its replacement
                    if (fEdit)
                        SendMessage(hwndList, LVM_DELETEITEM, nEdit, 0L);

                    // Iterate thru all the list items in reverse order looking for a conflict
                    for (int i = nItems-1 ; i >= 0 ; i--)
                    {
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = i;

                        // Get the current item details
                        if (SendMessage(hwndList, LVM_GETITEM, 0, reinterpret_cast<LPARAM>(&lvi)))
                        {
                            // Stop looking for conflicts if we've found an identical mapping
                            if (lvi.lParam == lParam)
                                break;

                            // Keep looking if the mapping doesn't match what we're adding
                            if ((lvi.lParam & ~0xff) != (lParam & ~0xff))
                                continue;

                            // If we're adding, confirm the entry replacement before deleting it
                            if (MessageBox(hdlg_, "Key binding already exists\n\nReplace existing entry?", "SimCoupe",
                                MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) == IDYES)
                            {
                                SendMessage(hwndList, LVM_DELETEITEM, i, 0L);
                                break;
                            }

                            // If we were adding, simply cancel now
                            if (fAdd)
                                return 0;

                            // Add the edited entry back
                            lParam = lEdit;
                            break;
                        }
                    }

                    // Insert the new item
                    lvi.mask = LVIF_TEXT | LVIF_PARAM;
                    lvi.pszText = LPSTR_TEXTCALLBACK;
                    lvi.lParam = lParam;
                    SendMessage(hwndList, LVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&lvi));

                    // Sort the list to it appears in the appropriate place
                    SendMessage(hwndList, LVM_SORTITEMS, 0, reinterpret_cast<LPARAM>(FnKeysCompareFunc));

                    break;
                }

                case IDB_DELETE:
                {
                    HWND hwndList = GetDlgItem(hdlg_, IDL_FNKEYS);
                    for (int i = static_cast<int>(SendMessage(hwndList, LVM_GETITEMCOUNT, 0, 0L))-1 ; i >= 0 ; i--)
                    {
                        if (SendMessage(hwndList, LVM_GETITEMSTATE, i, LVIS_SELECTED) & LVIS_SELECTED)
                            SendMessage(hwndList, LVM_DELETEITEM, i, 0L);
                    }

                    SetFocus(hwndList);
                    break;
                }
            }
    }

    return fRet;
}


static void InitPage (PROPSHEETPAGE* pPage_, int nPage_, int nDialogId_, DLGPROC pfnDlgProc_)
{
    pPage_ = &pPage_[nPage_];

    ZeroMemory(pPage_, sizeof *pPage_);
    pPage_->dwSize = sizeof *pPage_;
    pPage_->hInstance = __hinstance;
    pPage_->pszTemplate = MAKEINTRESOURCE(nDialogId_);
    pPage_->pfnDlgProc = pfnDlgProc_;
    pPage_->lParam = nPage_;
    pPage_->pfnCallback = NULL;
}


void DisplayOptions ()
{
    // Initialise the pages to go on the sheet
    PROPSHEETPAGE aPages[12];
    InitPage(aPages, 0,  IDD_PAGE_SYSTEM,   SystemPageDlgProc);
    InitPage(aPages, 1,  IDD_PAGE_DISPLAY,  DisplayPageDlgProc);
    InitPage(aPages, 2,  IDD_PAGE_SOUND,    SoundPageDlgProc);
    InitPage(aPages, 3,  IDD_PAGE_DRIVES,   DrivePageDlgProc);
    InitPage(aPages, 4,  IDD_PAGE_DISKS,    DiskPageDlgProc);
    InitPage(aPages, 5,  IDD_PAGE_PATHS,    PathPageDlgProc);
    InitPage(aPages, 6,  IDD_PAGE_INPUT,    InputPageDlgProc);
    InitPage(aPages, 7,  IDD_PAGE_JOYSTICK, JoystickPageDlgProc);
    InitPage(aPages, 8,  IDD_PAGE_PARALLEL, ParallelPageDlgProc);
    InitPage(aPages, 9,  IDD_PAGE_MIDI,     MidiPageDlgProc);
    InitPage(aPages, 10, IDD_PAGE_MISC,     MiscPageDlgProc);
    InitPage(aPages, 11, IDD_PAGE_FNKEYS,   FnKeysPageDlgProc);

    PROPSHEETHEADER psh;
    ZeroMemory(&psh, sizeof psh);
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOAPPLYNOW /*| PSH_HASHELP*/;
    psh.hwndParent = g_hwnd;
    psh.hInstance = __hinstance;
    psh.pszIcon = MAKEINTRESOURCE(IDI_MISC);
    psh.pszCaption = "Options";
    psh.nPages = sizeof aPages / sizeof aPages[0];
    psh.nStartPage = nOptionPage;
    psh.ppsp = aPages;

    // Save the current option state, flag that we've not centred the dialogue box, then display them for editing
    opts = Options::s_Options;
    fCentredOptions = false;
    PropertySheet(&psh);

    Options::Save();
}


////////////////////////////////////////////////////////////////////////////////

bool LocaliseString (char* psz_, int nLen_)
{
    // ToDo: determine the message to use, and update the string passed in
    return true;
}


void LocaliseMenu (HMENU hmenu_)
{
    char sz[128];

    // Loop through all items on the menu
    for (int nItem = 0, nMax = GetMenuItemCount(hmenu_); nItem < nMax ; nItem++)
    {
        // Fetch the text for the current menu item
        GetMenuString(hmenu_, nItem, sz, sizeof sz, MF_BYPOSITION);

        // Fetch the current state of the item, and submenu if it has one
        UINT uMenuFlags = GetMenuState(hmenu_, nItem, MF_BYPOSITION) & (MF_GRAYED | MF_DISABLED | MF_CHECKED);
        HMENU hmenuSub = GetSubMenu(hmenu_, nItem);

        // Only consider changing items using a string
        if (sz[0])
        {
            LocaliseString(sz, sizeof sz);

            // Modify the menu, preserving the type and flags
            UINT_PTR uptr = hmenuSub ? reinterpret_cast<UINT_PTR>(hmenuSub) : GetMenuItemID(hmenu_, nItem);
            ModifyMenu (hmenu_, nItem, MF_BYPOSITION | uMenuFlags, uptr, sz);
        }

        // If the menu item has a sub-menu, recurively process it
        if (hmenuSub)
            LocaliseMenu(hmenuSub);
    }
}

void LocaliseWindow (HWND hwnd)
{
    char sz[512];

    char szClass[128];
    GetClassName(hwnd, szClass, sizeof szClass);

    // Tab controls need special handling for the text on each tab
    if (!lstrcmpi(szClass, "SysTabControl32"))
    {
        // Find out the number of tabs on the control
        LRESULT lItems = SendMessage(hwnd, TCM_GETITEMCOUNT, 0, 0L);

        // Loop through each tab
        for (int i = 0 ; i < lItems ; i++)
        {
            // Fill in the structure specifying what we want
            TCITEM sItem;
            sItem.mask = TCIF_TEXT;
            sItem.pszText = sz;
            sItem.cchTextMax = sizeof sz;

            if (SendMessage(hwnd, TCM_GETITEM, i, (LPARAM)&sItem))
            {
                if (LocaliseString(sz, sizeof sz))
                {
                    // Shrink the tab size down and set the tab text with the expanded message
                    SendMessage(hwnd, TCM_SETMINTABWIDTH, 0, (LPARAM)1);
                    SendMessage(hwnd, TCM_SETITEM, i, (LPARAM)&sItem);
                }
            }
        }
    }
    else
    {
        // Get the current window text
        GetWindowText (hwnd, sz, sizeof sz);
        if (LocaliseString(sz, sizeof sz))
            SetWindowText(hwnd, sz);
    }
}


BOOL CALLBACK LocaliseEnumProc (HWND hwnd, LPARAM lParam)
{
    // Fill the window text for this window
    LocaliseWindow(hwnd);
    return TRUE;
}

void LocaliseWindows (HWND hwnd_)
{
    EnumChildWindows(hwnd_, LocaliseEnumProc, NULL);
}
