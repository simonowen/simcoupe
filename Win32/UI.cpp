// Part of SimCoupe - A SAM Coupé emulator
//
// UI.cpp: Win32 user interface
//
//  Copyright (c) 1999-2001  Simon Owen
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
#include <windowsx.h>
#include <commdlg.h>
#include <cderr.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winspool.h>

#include "UI.h"
#include "CDrive.h"
#include "Clock.h"
#include "CPU.h"
#include "Display.h"
#include "Frame.h"
#include "GUIDlg.h"
#include "Input.h"
#include "Mouse.h"
#include "Options.h"
#include "OSD.h"
#include "Parallel.h"
#include "Memory.h"
#include "Sound.h"
#include "Video.h"

extern int __argc;
extern char** __argv;
extern int main (int argc, char *argv[]);


#include "resource.h"   // For menu and dialogue box symbols

const int MOUSE_HIDE_TIME = 3000;   // 3 seconds

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupé/Win32 [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupé/Win32"
#endif

BOOL CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
BOOL CALLBACK NewDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
void CentreWindow (HWND hwnd_, HWND hwndParent_=NULL);
void SetComboStrings (HWND hdlg_, UINT uID_, const char** ppcsz_, int nDefault_=-1);

void DisplayOptions ();
bool DoAction (int nAction_, bool fPressed_=true);

bool g_fActive = true, g_fFrameStep = false, g_fTestMode = false;


HINSTANCE __hinstance;
HWND g_hwnd;
HMENU g_hmenu;
extern HINSTANCE __hinstance;

HHOOK g_hFnKeyHook;
HWND hdlgNewFnKey;

WINDOWPLACEMENT g_wp;
int nOptionPage = 0;                // Last active option property page
const int MAX_OPTION_PAGES = 16;    // Maximum number of option propery pages
bool fCentredOptions;


OPTIONS opts;
// Helper macro for detecting options changes
#define Changed(o)        (opts.o != GetOption(o))
#define ChangedString(o)  (strcasecmp(opts.o, GetOption(o)))


enum eActions
{
    actNmiButton, actResetButton, actToggleSaaSound, actToggleBeeper, actToggleFullscreen,
    actToggle5_4, actToggle50HzSync, actChangeWindowScale, actChangeFrameSkip, actChangeProfiler, actChangeMouse,
    actChangeKeyMode, actInsertFloppy1, actEjectFloppy1, actSaveFloppy1, actInsertFloppy2, actEjectFloppy2,
    actSaveFloppy2, actNewDisk, actSaveScreenshot, actFlushPrintJob, actDebugger, actImportData, actExportData,
    actDisplayOptions, actExitApplication, actToggleTurbo, actTempTurbo, actReleaseMouse, actPause, actToggleScanlines,
    actChangeBorders, actChangeSurface, actFrameStep, actPrinterOnline, MAX_ACTION
};

const char* aszActions[MAX_ACTION] =
{
    "NMI button", "Reset button", "Toggle SAA 1099 sound", "Toggle beeper sound", "Toggle fullscreen",
    "Toggle 5:4 aspect ratio", "Toggle 50Hz frame sync", "Change window scale", "Change frame-skip mode",
    "Change profiler mode", "Change mouse mode", "Change keyboard mode", "Insert floppy 1", "Eject floppy 1",
    "Save changes to floppy 1", "Insert floppy 2", "Eject floppy 2", "Save changes to floppy 2", "New Disk",
    "Save screenshot", "Flush print job", "Debugger", "Import data", "Export data", "Display options",
    "Exit application", "Toggle turbo speed", "Turbo speed (when held)", "Release mouse capture", "Pause",
    "Toggle scanlines", "Change viewable area", "Change video surface", "Step single frame", "Toggle printer online"
};


static char szDiskFilters [] =
#ifdef USE_ZLIB
    "All Disks (.dsk;.sad;.sdf;.sbt; .gz;.zip)\0*.dsk;*.sad;*.sdf;*.sbt;*.gz;*.zip\0"
    "Disk Files (.dsk;.sad;.sdf;.sbt)\0*.dsk;*.sad;*.sdf;*.sbt\0"
    "Compressed Files (.gz;.zip)\0*.gz;*.zip\0"
#else
    "All Disks (.dsk;.sad;.sdf;.sbt)\0*.dsk;*.sad;*.sdf;*.sbt\0"
#endif

    "All Files (*.*)\0*.*\0";

static const char* aszBorders[] =
    { "No borders", "Small borders", "Short TV area (default)", "TV visible area", "Complete scan area", NULL };

static const char* aszSurfaceType[] =
    { "Software Emulated", "System Memory", "Video Memory", "YUV Overlay", "RGB Overlay", NULL };


bool InitWindow ();
void LocaliseMenu (HMENU hmenu_);
void LocaliseWindows (HWND hwnd_);


int WINAPI WinMain(HINSTANCE hinst_, HINSTANCE hinstPrev_, LPSTR pszCmdLine_, int nCmdShow_)
{
    __hinstance = hinst_;

    return main(__argc, __argv);
}

////////////////////////////////////////////////////////////////////////////////


bool UI::Init (bool fFirstInit_/*=false*/)
{
    UI::Exit(true);
    TRACE("-> UI::Init(%s)\n", fFirstInit_ ? "first" : "");

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // Set up the main window
    bool fRet = InitWindow();
    TRACE("<- UI::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> UI::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (g_hwnd)
    {
        // When we reach here during a normal shutdown the window will already have gone, so check first
        if (IsWindow(g_hwnd))
            DestroyWindow(g_hwnd);

        g_hwnd = NULL;
    }

    TRACE("<- UI::Exit()\n");
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    // Re-pause after a single frame-step
    if (g_fFrameStep)
        DoAction(actFrameStep);

    while (1)
    {
        // Loop to process any pending Windows messages
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // App closing down?
            if (msg.message == WM_QUIT)
                return false;

            // Do keyboard translation for menu shortcuts etc. and dispatch it
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
    switch (eType_)
    {
        case msgWarning:
            MessageBox(NULL, pcszMessage_, "Warning", MB_OK | MB_ICONEXCLAMATION);
            break;

        case msgError:
            MessageBox(NULL, pcszMessage_, "Error", MB_OK | MB_ICONSTOP);
            break;

        // Something went seriously wrong!
        case msgFatal:
            MessageBox(NULL, pcszMessage_, "Fatal Error", MB_OK | MB_ICONSTOP);
            break;
    }
}


void UI::ResizeWindow (bool fUseOption_/*=false*/)
{
    static bool fCentred = false;

    // The default size is called 2x, when it's actually 1x!
    int nWidth = (Frame::GetWidth() >> 1) * GetOption(scale);
    int nHeight = (Frame::GetHeight() >> 1) * GetOption(scale);

    if (GetOption(ratio5_4))
        nWidth = MulDiv(nWidth, 5, 4);

    if (!fUseOption_)
    {
        RECT rClient;
        GetClientRect(g_hwnd, &rClient);

        nWidth = MulDiv(rClient.bottom, nWidth, nHeight);
        nHeight = rClient.bottom;
    }

    if (GetOption(fullscreen))
    {
        DWORD dwStyle = GetWindowStyle(g_hwnd);
        dwStyle = WS_POPUP|WS_VISIBLE;
        SetWindowLong(g_hwnd, GWL_STYLE, dwStyle);

        RECT rect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        SetWindowPos(g_hwnd, HWND_TOPMOST, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top, 0);
    }
    else
    {
        // Leave a maximised window as it is
        WINDOWPLACEMENT wp;
        wp.length = sizeof wp;
        if (!GetWindowPlacement(g_hwnd, &wp) || (wp.showCmd != SW_SHOWMAXIMIZED))
        {
            DWORD dwStyle = (GetWindowStyle(g_hwnd) & WS_VISIBLE) | WS_OVERLAPPEDWINDOW;
            SetWindowLong(g_hwnd, GWL_STYLE, dwStyle);

            // Adjust the window rectangle to give the client area size required for the main window
            RECT rect = { 0, 0, nWidth, nHeight };
            AdjustWindowRectEx(&rect, GetWindowStyle(g_hwnd), GetMenu(g_hwnd) != NULL, GetWindowExStyle(g_hwnd));
            SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_NOMOVE);

            // Get the actual client rectangle, now that the menu is in place
            RECT rClient;
            GetClientRect(g_hwnd, &rClient);

            // If the menu has wrapped the client area won't be big enough, so enlarge if necessary
            if ((rClient.bottom - rClient.top) < nHeight)
            {
                rect.bottom += GetSystemMetrics(SM_CYMENUSIZE);
                SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, rect.right-rect.left, rect.bottom-rect.top, SWP_NOMOVE);
            }

            if (!fCentred)
            {
                fCentred = true;
                ResizeWindow(true);
                CentreWindow(g_hwnd);
            }
        }
    }

    // Ensure the window is repainted and the overlay also covers the
    Display::SetDirty();
}


bool GetSaveLoadFile (HWND hwndParent_, LPCSTR pcszFilters_, LPCSTR pcszDefExt_, LPSTR pszFile_, DWORD cbSize_, bool* pfReadOnly_, bool fLoad_)
{
    static int nFilterIndex = 0;
    *pszFile_ = '\0';

    // Fill in the details for a fax file to select
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof ofn);
    ofn.lStructSize     = sizeof ofn;
    ofn.hwndOwner       = hwndParent_;
    ofn.lpstrFilter     = pcszFilters_;
    ofn.lpstrFile       = pszFile_;
    ofn.nMaxFile        = cbSize_;
    ofn.lpstrDefExt     = pcszDefExt_;
    ofn.nFilterIndex    = nFilterIndex;
    ofn.Flags           = OFN_EXPLORER | (fLoad_ ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);

    // If supplied and set, check the read-only checkbox
    if (pfReadOnly_ && *pfReadOnly_)
        ofn.Flags |= OFN_READONLY;

    // Return if the user cancelled without picking a file
    BOOL fRet = fLoad_ ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn);
    if (!fRet)
    {
        // Invalid paths can choke the dialog, so clear out the file and try again
        if (CommDlgExtendedError() == FNERR_INVALIDFILENAME)
        {
            *pszFile_ = '\0';
            fRet = fLoad_ ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn);
        }
    }

    if (!fRet)
    {
        TRACE("%s() failed (%#08lx)\n", fLoad_ ? "GetOpenFileName" : "GetSaveFileName", CommDlgExtendedError());
        return FALSE;
    }

    // Remember the current filter selection for next time
    nFilterIndex = ofn.nFilterIndex;

    // If required, read the requested read-only state of the input file
    if (pfReadOnly_)
        *pfReadOnly_ = (ofn.Flags & OFN_READONLY) != 0;

    // Copy the filename into the supplied string, and return success
    TRACE("GetSaveLoadFile: %s selected\n", pszFile_);
    return TRUE;
}

bool InsertDisk (CDiskDevice* pDrive_)
{
    static char szFile[_MAX_PATH] = "";
    static bool fReadOnly = false;

    // Eject any current disk, and use the path and read-only status from it for the open dialogue box
    if (pDrive_->IsInserted())
        strcpy(szFile, pDrive_->GetImage());

    // Prompt for the a new disk to insert
    if (GetSaveLoadFile(g_hwnd, szDiskFilters, NULL, szFile, sizeof szFile, &fReadOnly, true))
    {
        // Eject any previous disk, saving if necessary
        pDrive_->Eject();

        // Open the new disk (using the requested read-only mode), and insert it into the drive if successful
        if (pDrive_->Insert(szFile, fReadOnly))
            return true;

        Message(msgWarning, "Invalid disk image: %s", szFile);
    }

    return false;
}


#define CheckOption(id,check)   CheckMenuItem(hmenu, (id), (check) ? MF_CHECKED : MF_UNCHECKED)
#define EnableItem(id,enable)   EnableMenuItem(hmenu, (id), (enable) ? MF_ENABLED : MF_GRAYED)

void UpdateMenuFromOptions ()
{
    HMENU hmenu = GetMenu(g_hwnd), hmenuFile = GetSubMenu(hmenu, 0);

    // If Ctrl-Shift is held when the menu is activated, we'll enable some disabled items
    g_fTestMode |= (GetAsyncKeyState(VK_SHIFT) < 0 && GetAsyncKeyState(VK_CONTROL) < 0);
    EnableItem(IDM_FILE_FLOPPY1_DEVICE, g_fTestMode);
//  EnableItem(IDM_FILE_FLOPPY2_DEVICE, g_fTestMode);

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 1 options
    EnableMenuItem(hmenuFile, 1, (GetOption(drive1) == dskImage) ? MF_ENABLED|MF_BYPOSITION : MF_GRAYED|MF_BYPOSITION);
    EnableItem(IDM_FILE_FLOPPY1_SAVE_CHANGES, pDrive1->IsModified());

    char szEject[128], szName[_MAX_FNAME], szExt[_MAX_EXT];

    bool fInserted = pDrive1->IsInserted();
    _splitpath(fInserted ? pDrive1->GetImage() : "", NULL, NULL, szName, szExt);
    wsprintf(szEject, "&Eject %s%s", szName, szExt);
    ModifyMenu(hmenu, IDM_FILE_FLOPPY1_EJECT, MF_STRING | (fInserted ? MF_ENABLED : MF_GRAYED), IDM_FILE_FLOPPY1_EJECT, szEject);
    CheckOption(IDM_FILE_FLOPPY1_DEVICE, fInserted && CFloppyStream::IsRecognised(pDrive1->GetImage()));

    // Grey the sub-menu for disabled drives, and update the status/text of the other Drive 2 options
    EnableMenuItem(hmenuFile, 2, (GetOption(drive2) == dskImage) ? MF_ENABLED|MF_BYPOSITION : MF_GRAYED|MF_BYPOSITION);
    EnableItem(IDM_FILE_FLOPPY2_SAVE_CHANGES, pDrive2->IsModified());

    fInserted = pDrive2->IsInserted();
    _splitpath(fInserted ? pDrive2->GetImage() : "", NULL, NULL, szName, szExt);
    wsprintf(szEject, "&Eject %s%s", szName, szExt);
    ModifyMenu(hmenu, IDM_FILE_FLOPPY2_EJECT, MF_STRING | (fInserted ? MF_ENABLED : MF_GRAYED), IDM_FILE_FLOPPY2_EJECT, szEject);
    CheckOption(IDM_FILE_FLOPPY2_DEVICE, fInserted && CFloppyStream::IsRecognised(pDrive2->GetImage()));

    CIoDevice* pPrinter = (GetOption(parallel1) == 1) ? pParallel1 :
                          (GetOption(parallel2) == 1) ? pParallel2 : NULL;

    EnableItem(IDM_TOOLS_FLUSH_PRINTER, pPrinter && reinterpret_cast<CPrinterDevice*>(pPrinter)->IsFlushable());
    EnableItem(IDM_TOOLS_PRINTER_ONLINE, pPrinter);
    CheckOption(IDM_TOOLS_PRINTER_ONLINE, GetOption(printeronline));
}


bool DoAction (int nAction_, bool fPressed_/*=true*/)
{
    // Key being pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actNmiButton:
                CPU::NMI();
                break;

            case actResetButton:
                // Simulate the reset button being held by resetting the CPU and I/O, and holding the sound chip
                CPU::Reset(true);
                Sound::Stop();
                break;

            case actToggleSaaSound:
                SetOption(saasound, !GetOption(saasound));
                Sound::Init();
                Frame::SetStatus("SAA 1099 sound chip %s", GetOption(saasound) ? "enabled" : "disabled");
                break;

            case actToggleBeeper:
                SetOption(beeper, !GetOption(beeper));
                Sound::Init();
                Frame::SetStatus("Beeper %s", GetOption(beeper) ? "enabled" : "disabled");
                break;

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
                else //if (!GetOption(stretchtofit))
                    Frame::Init();

                Frame::SetStatus("%s pixel size", GetOption(ratio5_4) ? "5:4" : "1:1");
                break;

            case actToggle50HzSync:
                SetOption(sync, !GetOption(sync));
                Frame::SetStatus("Frame sync %s", GetOption(sync) ? "enabled" : "disabled");
                break;

            case actChangeWindowScale:
            {
                static const char* aszScaling[] = { "0.5", "1", "1.5"};
                SetOption(scale, (GetOption(scale) % 3) + 1);
                UI::ResizeWindow(true);
                Frame::SetStatus("%sx window scaling", aszScaling[GetOption(scale)-1]);
                break;
            }

            case actChangeFrameSkip:
            {
                SetOption(frameskip, (GetOption(frameskip)+1) % 11);

                int n = GetOption(frameskip);
                switch (n)
                {
                    case 0:     Frame::SetStatus("Automatic frame-skip");   break;
                    case 1:     Frame::SetStatus("No frames skipped");      break;
                    default:    Frame::SetStatus("Displaying every %d%s frame", n, (n==2) ? "nd" : (n==3) ? "rd" : "th");   break;
                }
                break;
            }

            case actChangeProfiler:
                SetOption(profile, (GetOption(profile)+1) % 4);
                break;

            case actChangeMouse:
                SetOption(mouse, !GetOption(mouse));
                Input::Acquire(GetOption(mouse) != 0);
                Frame::SetStatus("Mouse %s", !GetOption(mouse) ? "disabled" : "enabled");
                break;

            case actChangeKeyMode:
                SetOption(keymapping, (GetOption(keymapping)+1) % 3);
                Frame::SetStatus(!GetOption(keymapping) ? "Raw keyboard mode" :
                                GetOption(keymapping)==1 ? "SAM Coupe keyboard mode" : "Spectrum keyboard mode");
                break;

            case actInsertFloppy1:
                if (GetOption(drive1) == dskImage)
                {
                    if (GetAsyncKeyState(VK_SHIFT) < 0)
                        GUI::Start(new CInsertFloppy(1));
                    else
                    {
                        InsertDisk(pDrive1);
                        SetOption(disk1, pDrive1->GetImage());
                    }
                }
                break;

            case actEjectFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsInserted())
                {
                    pDrive1->Eject();
                    SetOption(disk1, "");
                    Frame::SetStatus("Ejected disk from drive 1");
                }
                break;

            case actSaveFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsModified() && pDrive1->Flush())
                    Frame::SetStatus("Saved changes to disk in drive 2");
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) == dskImage)
                {
                    if (GetAsyncKeyState(VK_SHIFT) < 0)
                        GUI::Start(new CInsertFloppy(2));
                    else
                    {
                        InsertDisk(pDrive2);
                        SetOption(disk2, pDrive2->GetImage());
                    }
                }
                break;

            case actEjectFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsInserted())
                {
                    pDrive2->Eject();
                    SetOption(disk2, "");
                    Frame::SetStatus("Ejected disk from drive 2");
                }
                break;

            case actSaveFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsModified() && pDrive2->Flush())
                    Frame::SetStatus("Saved changes to disk in drive 2");
                break;

            case actNewDisk:
                DialogBox(__hinstance, MAKEINTRESOURCE(IDD_NEWDISK), g_hwnd, NewDiskDlgProc);
                break;

            case actSaveScreenshot:
                Frame::SaveFrame();
                break;

            case actDebugger:
                GUI::Start(new CMessageBox(NULL, "Debugger not yet implemented", "Sorry!", mbInformation));
                break;

            case actImportData:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_IMPORT), g_hwnd, ImportExportDlgProc, 1);
                break;

            case actExportData:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_EXPORT), g_hwnd, ImportExportDlgProc, 0);
                break;

            case actDisplayOptions:
                if (GetAsyncKeyState(VK_SHIFT) < 0)
                    GUI::Start(new COptionsDialog);
                else if (!GUI::IsActive())
                {
                    Video::CreatePalettes(true);
                    DisplayOptions();
                    Video::CreatePalettes();
                }
                break;

            case actExitApplication:
                PostMessage(g_hwnd, WM_CLOSE, 0, 0L);
                break;

            case actToggleTurbo:
            {
                g_fTurbo = !g_fTurbo;
                Sound::Silence();

                Frame::SetStatus("Turbo mode %s", g_fTurbo ? "enabled" : "disabled");
                break;
            }

            case actTempTurbo:
                if (!g_fTurbo)
                {
                    g_fTurbo = true;
                    Sound::Silence();
                }
                break;

            case actReleaseMouse:
                Input::Acquire(false);
                Frame::SetStatus("Mouse capture released");
                break;

            case actFrameStep:
            {
                // Run for one frame then pause
                static int nFrameSkip = 0;

                // On first entry, save the current frameskip setting
                if (!g_fFrameStep)
                {
                    nFrameSkip = GetOption(frameskip);
                    g_fFrameStep = true;
                }

                SetOption(frameskip, g_fPaused ? 1 : nFrameSkip);
            }
            // Fall through to actPause...

            case actPause:
            {
                g_fPaused = !g_fPaused;

                if (g_fPaused)
                {
                    Sound::Stop();
                    SetWindowText(g_hwnd, WINDOW_CAPTION " - Paused");
                }
                else
                {
                    Sound::Play();
                    SetWindowText(g_hwnd, WINDOW_CAPTION);
                    g_fFrameStep = (nAction_ == actFrameStep);
                }

                Video::CreatePalettes();

                Frame::Redraw();
                Input::Purge();
                break;
            }

            case actToggleScanlines:
                SetOption(scanlines, !GetOption(scanlines));

                if (GetOption(stretchtofit))
                {
                    SetOption(stretchtofit, false);
                    UI::ResizeWindow(true);
                }

//                Video::Init();
                Frame::SetStatus("Scanlines %s", GetOption(scanlines) ? "enabled" : "disabled");
                break;

            case actChangeBorders:
                SetOption(borders, (GetOption(borders)+1) % 5);
                Frame::Init();
                UI::ResizeWindow(true);
                Frame::SetStatus(aszBorders[GetOption(borders)]);
                break;

            case actChangeSurface:
                SetOption(surface, GetOption(surface) ? GetOption(surface)-1 : 4);
                Frame::Init();
                Frame::SetStatus("Using %s surface", aszSurfaceType[GetOption(surface)]);
                break;

            case actFlushPrintJob:
                IO::InitParallel();
                Frame::SetStatus("Flushed active print job");
                break;

            case actPrinterOnline:
                SetOption(printeronline, !GetOption(printeronline));
                Frame::SetStatus("Printer %s", GetOption(printeronline) ? "ONLINE" : "OFFLINE");
                break;

            default:
                return false;
        }
    }

    // Key released
    else
    {
        switch (nAction_)
        {
            case actResetButton:
                // Reset the CPU, and prepare fast reset if necessary
                CPU::Reset(false);

                // Start the sound playing, so the sound chip continues to play the current settings
                Sound::Play();
                break;

            case actTempTurbo:
                if (g_fTurbo)
                {
                    Sound::Silence();
                    g_fTurbo = false;
                }
                break;

            default:
                return false;
        }
    }

    // Action processed
    return true;
}


void CentreWindow (HWND hwnd_, HWND hwndParent_/*=NULL*/)
{
    // If a window isn't specified get the parent window
    if (!hwndParent_ || (IsIconic(hwndParent_) || !(hwndParent_ = GetParent(hwnd_))))
        hwndParent_ = GetDesktopWindow();

    // Get the rectangles of the window to be centred and the parent window
    RECT rWindow, rParent;
    GetWindowRect(hwnd_, &rWindow);
    GetWindowRect(hwndParent_, &rParent);

    // Work out the new position of the window
    int nX = rParent.left + ((rParent.right - rParent.left) - (rWindow.right - rWindow.left)) / 2;
    int nY = rParent.top + ((rParent.bottom - rParent.top) - (rWindow.bottom - rWindow.top)) / 2;

    // Move the window to its new position
    SetWindowPos(hwnd_, NULL, nX, nY, 0, 0, SWP_SHOWWINDOW|SWP_NOSIZE|SWP_NOZORDER);
}


BOOL CALLBACK AboutDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    switch (uMsg_)
    {
        case WM_INITDIALOG:
            CentreWindow(hdlg_);
            return 1;

        case WM_COMMAND:
            if (wParam_ == IDOK || wParam_ == IDCANCEL)
                EndDialog(hdlg_, 0);
            break;
    }

    return 0;
}


LRESULT CALLBACK WindowProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static bool fInMenu = false, fHideCursor = false, fSizingOrMoving = false;
    static UINT_PTR ulMouseTimer = 0;

//  TRACE("WindowProc(%#04x,%#08x,%#08lx,%#08lx)\n", hwnd_, uMsg_, wParam_, lParam_);

    // Input has first go at processing any messages
    if (Input::FilterMessage(hwnd_, uMsg_, wParam_, lParam_))
        return 0;

    switch (uMsg_)
    {
        // Main window being created
        case WM_CREATE:
            // Allow files to be dragged and dropped onto our main window
            DragAcceptFiles(hwnd_, TRUE);
            return 0;

        // Application close request
        case WM_CLOSE:
            TRACE("WM_CLOSE\n");
            Sound::Silence();
            DestroyWindow(hwnd_);
            return 0;

        // Main window is being destroyed
        case WM_DESTROY:
            TRACE("WM_DESTROY\n");
            PostQuitMessage(0);
            return 0;


        // Main window being activated or deactivated
        case WM_ACTIVATE:
        {
            TRACE("WM_ACTIVATE (%#08lx)\n", wParam_);

            // When the main window becomes inactive (possibly due to a dialogue box), silence the sound
            if (LOWORD(wParam_) == WA_INACTIVE && GetParent(reinterpret_cast<HWND>(lParam_)) == hwnd_)
                Sound::Silence();

            break;
        }

        // Application being activated or deactivated
        case WM_ACTIVATEAPP:
        {
            TRACE("WM_ACTIVATEAPP (%#08lx)\n", wParam_);
            g_fActive = (wParam_ != 0);

            // Should be pause the emulation when inactive?
            if (GetOption(pauseinactive))
            {
                // Silence the sound while we're not running
                if (!g_fActive)
                    Sound::Silence();

                // Dim the display while we're paused, or undim it when we get control again
                Video::CreatePalettes();
                Display::SetDirty();
                Frame::Redraw();

                SetWindowText(g_hwnd, g_fActive ? WINDOW_CAPTION : WINDOW_CAPTION " - Paused");
            }

            // Release the mouse and start the mouse hide delay
            Input::Acquire(false);
            ulMouseTimer = SetTimer(hwnd_, 1989, MOUSE_HIDE_TIME, NULL);
            fHideCursor = false;
            break;
        }


        // File has been dropped on our window
        case WM_DROPFILES:
        {
            // Insert the first (or only) dropped file into drive 1
            char szFile[_MAX_PATH]="";
            if (DragQueryFile(reinterpret_cast<HDROP>(wParam_), 0, szFile, sizeof szFile))
                pDrive1->Insert(szFile);

            return 0;
        }

        // Input language has changed
        case WM_INPUTLANGCHANGE:
            // Reinitialise input as the keyboard layout may have changed
            Input::Init();
            return 1;

        // System time has changed
        case WM_TIMECHANGE:
            // If we're keeping the SAM time synchronised with real time, update the SAM clock
            if (GetOption(clocksync))
                Clock::Init();
            break;


        // Menu is about to be activated
        case WM_INITMENU:
            UpdateMenuFromOptions();
            break;


        case WM_SIZING:
        {
            RECT* pRect = reinterpret_cast<RECT*>(lParam_);

            if (Frame::GetScreen())
            {
                // Determine the size of the current sizing area
                RECT rWindow = *pRect;
                OffsetRect(&rWindow, -rWindow.left, -rWindow.top);

                // Get the screen size, adjusting for 5:4 mode if necessary
                int nWidth = Frame::GetWidth() >> 1, nHeight = Frame::GetHeight() >> 1;
                if (GetOption(ratio5_4))
                    nWidth = MulDiv(nWidth, 5, 4);

                // Determine how big the window would be for an nWidth*nHeight client area
                RECT rNonClient = { 0, 0, nWidth, nHeight };
                AdjustWindowRectEx(&rNonClient, GetWindowStyle(g_hwnd), GetMenu(g_hwnd) != NULL, GetWindowExStyle(g_hwnd));
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
            break;
        }


        // Keep track of whether the window is being resized or moved
        case WM_ENTERSIZEMOVE:
        case WM_EXITSIZEMOVE:
            fSizingOrMoving = (uMsg_ == WM_ENTERSIZEMOVE);
            break;

        // Menu has been opened
        case WM_ENTERMENULOOP:
            // Silence the sound while the menu is being used
            Sound::Silence();

            // Release the mouse
            Input::Acquire(false);
            fInMenu = true;
            break;

        // If the window is being resized or moved, avoid flicker by not erasing the background - the CS_VREDRAW
        // and CS_HREDRAW window styles will ensure it's fully repainted anyway
        case WM_ERASEBKGND:
            if (fSizingOrMoving && !g_fTurbo)
                return 1;
            break;

        // Handle the exit for modal dialogues, when we get enabled
        case WM_ENABLE:
            if (!wParam_)
                break;
            // Fall through to WM_EXITMENULOOP...

        case WM_EXITMENULOOP:
            // No longer in menu, so start timer to hide the mouse if not used again
            fInMenu = fHideCursor = false;
            ulMouseTimer = SetTimer(hwnd_, 1, MOUSE_HIDE_TIME, NULL);
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);

            // Make sure the entire display is updated to ensure the dirty areas are redrawn
            Display::SetDirty();

            // Forcibly redraw the screen if in windowed mode, using the menu in full-screen, or inactive and paused
            if ((!GetOption(fullscreen) && !IsWindowEnabled(g_hwnd)) || fInMenu || fSizingOrMoving || g_fPaused || (!g_fActive && GetOption(pauseinactive)))
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


        // Window has been moved
        case WM_MOVE:
            // Reposition any video overlay
            Frame::Redraw();
            return 0;


        // Mouse-hide timer has expired
        case WM_TIMER:
            // Make sure the timer is ours
            if (wParam_ != ulMouseTimer)
                break;

            // Kill the timer, and flag the mouse as hidden
            KillTimer(hwnd_, ulMouseTimer);
            fHideCursor = true;

            // Generate a WM_SETCURSOR to update the cursor state
            POINT pt;
            GetCursorPos(&pt);
            SetCursorPos(pt.x, pt.y);
            return 0;

        case WM_SETCURSOR:
            // Hide the cursor unless it's being used for the Win32 GUI or the emulation using using it in windowed mode
            if (fHideCursor || Input::IsMouseAcquired() || GUI::IsActive() || GetOption(fullscreen))
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
        case WM_NCMOUSEMOVE:
        {
            static LPARAM lLastPos;

            // Has the mouse moved since last time?
            if (lParam_ != lLastPos)
            {
                // Show the cursor, but set a timer to hide it if not moved for a few seconds
                fHideCursor = false;
                ulMouseTimer = SetTimer(hwnd_, 1, MOUSE_HIDE_TIME, NULL);

                // Remember the new position
                lLastPos = lParam_;
            }

            return 0;
        }

        // Silence the sound during window drags, and other clicks in the non-client area
        case WM_NCLBUTTONDOWN:
            Sound::Silence();
            break;

        case WM_SYSCOMMAND:
            TRACE("wParam_ = %#08lx\n", wParam_);

            // If the Alt key is being used as the SAM 'Cntrl' key, stop Alt-key combinations activating the menu
            if (GetOption(altforcntrl) && (wParam_ & 0xfff0) == SC_KEYMENU && lParam_)
                return 0;

            break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            // Alt-Return is used to toggle full-screen
            if (uMsg_ == WM_SYSKEYDOWN && wParam_ == VK_RETURN && (lParam_ & 0x60000000) == 0x20000000)
                DoAction(actToggleFullscreen);

            // Forward F10 on as a regular key instead of a system key
            if (wParam_ == VK_F10)
                return SendMessage(hwnd_, uMsg_ - WM_SYSKEYDOWN + WM_KEYDOWN, wParam_, lParam_);

            break;

        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            // Function key?
            if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            {
                // Read the current states of the control and shift keys
                bool fCtrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool fShift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;

                // Grab an upper-case copy of the function key definition string
                char szKeys[256];
                _strupr(strcpy(szKeys, GetOption(fnkeys)));

                // Process each of the 'key=action' pairs in the string
                for (char* psz = strtok(szKeys, ", \t") ; psz ; psz = strtok(NULL, ", \t"))
                {
                    // Leading C and S characters indicate that Ctrl and/or Shift modifiers are required with the key
                    bool fCtrled = (*psz == 'C');   if (fCtrled) psz++;
                    bool fShifted = (*psz == 'S');  if (fShifted) psz++;

                    // Currently we only support function keys F1-F12
                    if (*psz++ == 'F')
                    {
                        // If we've not found a matching key, keep looking...
                        if (wParam_ != (VK_F1 + strtoul(psz, &psz, 0) - 1))
                            continue;

                        // The Ctrl/Shift states must match too
                        if (fCtrl == fCtrled && fShift == fShifted)
                        {
                            // Perform the action, passing whether this is a key press or release
                            DoAction(strtoul(++psz, NULL, 0), uMsg_ == WM_KEYDOWN);

                            // Signal we've processed the key (mainly to stop F10 activating the menu)
                            return 0;
                        }
                    }
                }
            }


            // Most of the emulator keys are handled above, but we've a few extra fixed mappings of our own (well, mine!)
            switch (wParam_)
            {
                // Keypad '-' = Z80 reset (can be held to view reset screens)
                case VK_SUBTRACT:
                    DoAction(actResetButton, uMsg_ == WM_KEYDOWN);
                    break;

                // Toggle the debugger
                case VK_DIVIDE:
                    if (uMsg_ == WM_KEYDOWN)
                        DoAction(actDebugger);
                    break;

                case VK_MULTIPLY:
                    if (uMsg_ == WM_KEYDOWN)
                        DoAction(actNmiButton);
                    break;

                case VK_ADD:
                    DoAction(actTempTurbo, uMsg_ == WM_KEYDOWN);
                    break;

                case VK_CANCEL:
                case VK_PAUSE:
                    if (uMsg_ == WM_KEYDOWN)
                    {
                        // Ctrl-Break is used for reset
                        if (GetAsyncKeyState(VK_CONTROL) < 0)
                            CPU::Init();

                        // Shift-pause single steps
                        else if (GetAsyncKeyState(VK_SHIFT) < 0)
                            DoAction(actFrameStep);

                        // Pause toggles pause mode
                        else
                            DoAction(actPause);
                    }
                    break;

                case VK_SNAPSHOT:
                case VK_SCROLL:
                    if (uMsg_ == WM_KEYUP)
                        DoAction(actSaveScreenshot);
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
            TRACE("WM_COMMAND\n");

            WORD wId = LOWORD(wParam_);

            switch (wId)
            {
                case IDM_TOOLS_OPTIONS:         DoAction(actDisplayOptions);    break;
                case IDM_TOOLS_FLUSH_PRINTER:   DoAction(actFlushPrintJob);     break;
                case IDM_TOOLS_PRINTER_ONLINE:  DoAction(actPrinterOnline);     break;

                case IDM_FILE_NEW_DISK:         DoAction(actNewDisk);           break;

                case IDM_FILE_FLOPPY1_DEVICE:       if (GetOption(drive1) == dskImage) pDrive1->Insert("A:");  break;
                case IDM_FILE_FLOPPY1_INSERT:       DoAction(actInsertFloppy1); break;
                case IDM_FILE_FLOPPY1_EJECT:        DoAction(actEjectFloppy1);  break;
                case IDM_FILE_FLOPPY1_SAVE_CHANGES: DoAction(actSaveFloppy1);   break;

                case IDM_FILE_FLOPPY2_DEVICE:       if (GetOption(drive2) == dskImage) pDrive2->Insert("B:");  break;
                case IDM_FILE_FLOPPY2_INSERT:       DoAction(actInsertFloppy2); break;
                case IDM_FILE_FLOPPY2_EJECT:        DoAction(actEjectFloppy2);  break;
                case IDM_FILE_FLOPPY2_SAVE_CHANGES: DoAction(actSaveFloppy2);   break;

                case IDM_FILE_IMPORT_DATA:          DoAction(actImportData);    break;
                case IDM_FILE_EXPORT_DATA:          DoAction(actExportData);    break;

                // Alt-F4 = exit
                case IDM_FILE_EXIT:                 DoAction(actExitApplication); break;

                // Items from help menu
                case IDM_HELP_GENERAL:  ShellExecute(hwnd_, NULL, OSD::GetFilePath("ReadMe.txt"), NULL, "", SW_SHOWMAXIMIZED); break;
                case IDM_HELP_ABOUT:
                    if (GetAsyncKeyState(VK_SHIFT) < 0)
                        GUI::Start(new CAboutDialog);
                    else
                        DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_ABOUT), g_hwnd, AboutDlgProc, NULL);
                    break;
            }
            break;
        }
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}


BOOL CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static UINT uAddress = 32768, uLength = 0;
    static int nPage = 1;
    static WORD wOffset = 0;
    static char szFile [_MAX_PATH];
    static bool fBasicAddress = true, fUpdating = false, fImport = false;
    char sz[32];

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            fImport = (lParam_ != 0);

            SetDlgItemText(hdlg_, IDE_FILE, szFile);
            SendDlgItemMessage(hdlg_, IDE_FILE, EM_SETSEL, _MAX_PATH, -1);

            wsprintf(sz, "%u", uAddress); SetDlgItemText(hdlg_, IDE_ADDRESS, sz);
            SendMessage(hdlg_, WM_COMMAND, fBasicAddress ? IDR_BASIC_ADDRESS : IDR_PAGE_OFFSET, 0);

            if (!fImport && uLength)
            {
                wsprintf(sz, "%u", uLength);
                SetDlgItemText(hdlg_, IDE_LENGTH, sz);
            }

            CentreWindow(hdlg_);

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

                case IDE_LENGTH:
                case IDE_FILE:
                    if (fChange)
                    {
                        bool fOK = true;

                        if (!fImport)
                        {
                            GetDlgItemText(hdlg_, IDE_LENGTH, sz, sizeof sz);
                            uLength = (fImport ? -1 : strtoul(sz, NULL, 0));
                        }

                        GetDlgItemText(hdlg_, IDE_FILE, szFile, sizeof szFile);
                        SetLastError(NO_ERROR);
                        DWORD dwAttrs = GetFileAttributes(szFile), dwError = GetLastError();

                        // If we're importing, the file must exist and can't be a directory
                        if (fImport)
                            fOK = !dwError && !(dwAttrs & FILE_ATTRIBUTE_DIRECTORY);

                        // When exporting there must be a length, and the file can't be a directory
                        else
                            fOK = strtoul(sz, NULL, 0) && (dwError == ERROR_FILE_NOT_FOUND || !(dwAttrs & FILE_ATTRIBUTE_DIRECTORY));

                        EnableWindow(GetDlgItem(hdlg_, IDOK), fOK);
                    }
                    break;

                case IDE_ADDRESS:
                    if (fChange && !fUpdating)
                    {
                        GetDlgItemText(hdlg_, IDE_ADDRESS, sz, sizeof sz);
                        uAddress = strtoul(sz, NULL, 0) & 0x7ffff;
                        nPage = static_cast<int>(uAddress/16384 - 1) & 0x1f;
                        wOffset = static_cast<WORD>(uAddress) & 0x3fff;

                        fUpdating = true;
                        wsprintf(sz, "%u", nPage);      SetDlgItemText(hdlg_, IDE_PAGE, sz);
                        wsprintf(sz, "%u", wOffset);    SetDlgItemText(hdlg_, IDE_OFFSET, sz);
                        fUpdating = false;
                    }
                    break;

                case IDE_PAGE:
                case IDE_OFFSET:
                    if (fChange && !fUpdating)
                    {
                        GetDlgItemText(hdlg_, IDE_PAGE, sz, sizeof sz);
                        nPage = static_cast<int>(strtoul(sz, NULL, 0)) & 0x1f;

                        GetDlgItemText(hdlg_, IDE_OFFSET, sz, sizeof sz);
                        wOffset = static_cast<WORD>(strtoul(sz, NULL, 0)) & 0x3fff;

                        fUpdating = true;
                        uAddress = static_cast<DWORD>(nPage + 1) * 16384 + wOffset;
                        wsprintf(sz, "%u", uAddress);
                        SetDlgItemText(hdlg_, IDE_ADDRESS, sz);
                        fUpdating = false;
                    }
                    break;

                case IDR_BASIC_ADDRESS:
                case IDR_PAGE_OFFSET:
                    fBasicAddress = wControl == IDR_BASIC_ADDRESS;
                    SendDlgItemMessage(hdlg_, fBasicAddress ? IDR_BASIC_ADDRESS : IDR_PAGE_OFFSET, BM_SETCHECK, BST_CHECKED, 0);

                    EnableWindow(GetDlgItem(hdlg_, IDE_ADDRESS), fBasicAddress);
                    EnableWindow(GetDlgItem(hdlg_, IDE_PAGE), !fBasicAddress);
                    EnableWindow(GetDlgItem(hdlg_, IDE_OFFSET), !fBasicAddress);
                    break;

                case IDB_BROWSE:
                {
                    bool fReadOnly = true;
                    if (!GetSaveLoadFile(g_hwnd, "Binary files (*.bin)\0*.bin\0Data files (*.dat)\0*.dat\0All files (*.*)\0*.*\0",
                                    NULL, szFile, sizeof szFile, &fReadOnly, fImport))
                        break;

                    SetWindowText(GetDlgItem(hdlg_, IDE_FILE), szFile);
                    SendDlgItemMessage(hdlg_, IDE_FILE, EM_SETSEL, _MAX_PATH, -1);
                    break;
                }
                case IDOK:
                {
                    // Addresses in the first 16K are taken from ROM0
                    if (uAddress < 0x4000)
                        nPage = ROM0;

                    FILE* hFile;
                    if (!szFile[0] || !(hFile = fopen(szFile, fImport ? "rb" : "wb")))
                        Message(msgError, "Failed to open %s for %s", szFile, fImport ? "reading" : "writing");
                    else
                    {
                        // Limit the length to 512K as there's no point in reading more
                        UINT uLen = fImport ? 0x7ffff : (uLength &= 0x7ffff);

                        // Loop while there's still data to
                        while (uLen > 0)
                        {
                            UINT uChunk = min(uLen, (0x4000U - wOffset));

                            if (( fImport && fread(&apbPageWritePtrs[nPage][wOffset], uChunk, 1, hFile)) ||
                                (!fImport && fwrite(&apbPageReadPtrs[nPage][wOffset], uChunk, 1, hFile)))
                            {
                                uLen -= uChunk;
                                wOffset = 0;

                                // If the first block was in ROM0 or we've passed memory end, wrap to page 0
                                if (++nPage >= N_PAGES_MAIN)
                                    nPage = 0;
                                continue;
                            }

                            if (!fImport)
                                Message(msgWarning, "Error writing to %s! (disk full?)", szFile);
                            break;
                        }

                        fclose(hFile);
                        EndDialog(hdlg_, 1);
                    }

                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


BOOL CALLBACK NewDiskDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static int nType = 0, nSides, nTracks, nSectors, nSectorSize;
    static int nMaxTracks, nMaxSectors, nMaxSectorSize;
    static bool fCompress = false, fNonRealWorld = false;
    static int nInsertInto = 1;
    static char szFile [_MAX_PATH] = "empty.dsk";
    static DWORD dwESP;

    char sz[32];


    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            InitCommonControls();
            CentreWindow(hdlg_);

            static const char* aszInsertInfo[] = { "None", "Drive 1", "Drive 2", NULL };
            SetComboStrings(hdlg_, IDC_INSERT_INTO, aszInsertInfo, nInsertInto);

            for (int nSize = MIN_SECTOR_SIZE ; nSize <= MAX_SECTOR_SIZE ; nSize <<= 1)
            {
                wsprintf(sz, "%d", nSize);
                SendDlgItemMessage(hdlg_, IDC_SECTORSIZE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz));
            }

            SendDlgItemMessage(hdlg_, IDS_SIDES, UDM_SETRANGE, 0, MAKELONG(MAX_DISK_SIDES, 1));
            SendDlgItemMessage(hdlg_, IDS_TRACKS, UDM_SETRANGE, 0, MAKELONG(127, 1));
            SendDlgItemMessage(hdlg_, IDS_SECTORS, UDM_SETRANGE, 0, MAKELONG(255, 1));

            SendDlgItemMessage(hdlg_, IDC_NON_REAL_WORLD, BM_SETCHECK, fNonRealWorld ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_SETCHECK, fCompress ? BST_CHECKED : BST_UNCHECKED, 0L);
            SetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile);
            SendMessage(hdlg_, WM_COMMAND, IDR_DISK_TYPE_DSK + nType, 0L);

#ifndef USE_ZLIB
            // If Zlib is not available, hide the compression check-box
            ShowWindow(GetDlgItem(hdlg_, IDC_COMPRESS), SW_HIDE);
#endif

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

                case IDE_NEWFILE:
                {
                    if (HIWORD(wParam_) == EN_CHANGE)
                    {
                        // Fetch the modified text
                        GetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile, sizeof szFile);

                        int nLen = lstrlen(szFile);
                        if (nLen > 4)
                        {
                            // Temporarily remove any .gz suffix
                            bool fDotGz = (nLen >= 3 && !lstrcmpi(szFile + nLen - 3, ".gz"));
                            if (fDotGz)
                                szFile[nLen -= 3] = '\0';

                            // Work out the type
                            int nNewType =  (/*!lstrcmpi(szFile + nLen - 4, ".sdf")) ? 2 :
                                            (*/!lstrcmpi(szFile + nLen - 4, ".sad")) ? 1 :
                                            (!lstrcmpi(szFile + nLen - 4, ".dsk")) ? 0 : nType;

                            // Restore the .gz if we removed it
                            if (fDotGz)
                                lstrcat(szFile, ".gz");

                            // Modify the radio button selection if the type has changed
                            if (nNewType != nType)
                                SendMessage(hdlg_, WM_COMMAND, IDR_DISK_TYPE_DSK + nNewType, 0);
                        }
                    }
                    break;
                }

                case IDC_NON_REAL_WORLD:
                {
                    fNonRealWorld = (SendDlgItemMessage(hdlg_, IDC_NON_REAL_WORLD, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                    nMaxTracks = fNonRealWorld ? 127 : (!nType ? NORMAL_DISK_TRACKS : MAX_DISK_TRACKS);
                    nMaxSectors = fNonRealWorld ? 255 : NORMAL_DISK_SECTORS;
                    nMaxSectorSize = 128 << 2;

                    // Set and set the current values so the limit is applied
                    GetDlgItemText(hdlg_, IDE_SIDES, sz, sizeof sz); SetDlgItemText(hdlg_, IDE_SIDES, sz);
                    GetDlgItemText(hdlg_, IDE_TRACKS, sz, sizeof sz); SetDlgItemText(hdlg_, IDE_TRACKS, sz);
                    GetDlgItemText(hdlg_, IDE_SECTORS, sz, sizeof sz); SetDlgItemText(hdlg_, IDE_SECTORS, sz);

                    break;
                }

                case IDC_COMPRESS:
                {
                    int nLen = lstrlen(szFile);
                    fCompress = (SendDlgItemMessage(hdlg_, IDC_COMPRESS, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                    bool fDotGz = (nLen >= 3 && !lstrcmpi(szFile + nLen - 3, ".gz"));

                    // Remove the .gz suffix when unchecked
                    if (!fCompress && fDotGz)
                    {
                        szFile[nLen - 3] = '\0';
                        SetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile);
                    }

                    // Add the .gz suffix when checked
                    else if (fCompress && !fDotGz)
                    {
                        lstrcat(szFile, ".gz");
                        SetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile);
                    }

                    break;
                }

                case IDC_INSERT_INTO:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                        nInsertInto = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_INSERT_INTO, CB_GETCURSEL, 0, 0L));
                    break;
                }

                case IDB_BROWSE:
                {
                    bool fReadOnly = true;
                    if (!GetSaveLoadFile(g_hwnd, szDiskFilters, NULL, szFile, sizeof szFile, &fReadOnly, false))
                        break;

                    SetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile);
                    SendDlgItemMessage(hdlg_, IDE_NEWFILE, EM_SETSEL, _MAX_PATH, -1);
                    break;
                }

                case IDR_DISK_TYPE_DSK:
                case IDR_DISK_TYPE_SAD:
                case IDR_DISK_TYPE_SDF:
                {
                    nType = wControl - IDR_DISK_TYPE_DSK;

                    // Ensure only one radio button is selected
                    SendDlgItemMessage(hdlg_, IDR_DISK_TYPE_DSK, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hdlg_, IDR_DISK_TYPE_SAD, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hdlg_, IDR_DISK_TYPE_SDF, BM_SETCHECK, BST_UNCHECKED, 0);
                    SendDlgItemMessage(hdlg_, wControl, BM_SETCHECK, BST_CHECKED, 0);

                    int nLen = lstrlen(szFile);

                    // Strip any .gz extension, but remember whether it was present
                    bool fDotGz = (nLen >= 3 && !lstrcmpi(szFile + nLen - 3, ".gz"));
                    if (fDotGz)
                        szFile[nLen -= 3] = '\0';

                    // Strip any type extension
                    if (nLen > 4 && (/*!lstrcmpi(szFile + nLen - 4, ".sdf") ||*/
                        !lstrcmpi(szFile + nLen - 4, ".sad") || !lstrcmpi(szFile + nLen - 4, ".dsk")))
                        szFile[nLen -= 4] = '\0';

                    // Restore the correct file extension type, and an extra .gz if the file is to be compressed
                    lstrcat(szFile, /*nType == 2 ? ".sdf" : */nType == 1 ? ".sad" : ".dsk");
                    lstrcat(szFile, fDotGz ? ".gz" : "");

                    // Update the file edit control, restoring the caret to the same position
                    DWORD dwStart = 0, dwEnd = 0;
                    SendDlgItemMessage(hdlg_, IDE_NEWFILE, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
                    SetWindowText(GetDlgItem(hdlg_, IDE_NEWFILE), szFile);
                    SendDlgItemMessage(hdlg_, IDE_NEWFILE, EM_SETSEL, dwStart, dwEnd);

                    // Enable only the geometry parameters that can be modified for this image type
                    EnableWindow(GetDlgItem(hdlg_, IDE_SIDES), (wControl != IDR_DISK_TYPE_DSK));
                    EnableWindow(GetDlgItem(hdlg_, IDE_TRACKS), (wControl != IDR_DISK_TYPE_DSK));
                    EnableWindow(GetDlgItem(hdlg_, IDE_SECTORS), (wControl == IDR_DISK_TYPE_SAD));
                    EnableWindow(GetDlgItem(hdlg_, IDC_SECTORSIZE), (wControl == IDR_DISK_TYPE_SAD));

                    // Update the maximum geometry values for the new image type
                    SendMessage(hdlg_, WM_COMMAND, IDC_NON_REAL_WORLD, 0L);

                    // Set the default geometry values for the new type
                    nTracks = (nType == 2) ? MAX_DISK_TRACKS : NORMAL_DISK_TRACKS;
                    wsprintf(sz, "%d", nTracks); SetDlgItemText(hdlg_, IDE_TRACKS, sz);

                    nSectorSize = 128 << 2;
                    SendDlgItemMessage(hdlg_, IDC_SECTORSIZE, CB_SETCURSEL, 2, 0L);

                    wsprintf(sz, "%d", nSides = NORMAL_DISK_SIDES); SetDlgItemText(hdlg_, IDE_SIDES, sz);
                    wsprintf(sz, "%d", nSectors = NORMAL_DISK_SECTORS); SetDlgItemText(hdlg_, IDE_SECTORS, sz);

                }
                break;

                case IDE_SIDES:
                    if (HIWORD(wParam_) == EN_CHANGE)
                    {
                        GetDlgItemText(hdlg_, IDE_SIDES, sz, sizeof sz);
                        nSides = strtoul(sz, NULL, 0);

                        if (nSides < 1)
                            nSides = 1;
                        else if (nSides > MAX_DISK_SIDES)
                            nSides = MAX_DISK_SIDES;
                        else
                            break;

                        wsprintf(sz, "%d", nSides);
                        SetDlgItemText(hdlg_, IDE_SIDES, sz);
                        SendDlgItemMessage(hdlg_, IDE_SIDES, EM_SETSEL, 0, -1);
                    }
                    break;

                case IDE_TRACKS:
                    if (HIWORD(wParam_) == EN_CHANGE)
                    {
                        GetDlgItemText(hdlg_, IDE_TRACKS, sz, sizeof sz);
                        nTracks = strtoul(sz, NULL, 0);

                        if (nTracks < 1)
                            nTracks = 1;
                        else if (nMaxTracks && nTracks > nMaxTracks)
                            nTracks = nMaxTracks;
                        else
                            break;

                        wsprintf(sz, "%d", nTracks);
                        SetDlgItemText(hdlg_, IDE_TRACKS, sz);
                        SendDlgItemMessage(hdlg_, IDE_TRACKS, EM_SETSEL, 0, -1);
                    }
                    break;

                case IDE_SECTORS:
                    if (HIWORD(wParam_) == EN_CHANGE)
                    {
                        GetDlgItemText(hdlg_, IDE_SECTORS, sz, sizeof sz);
                        nSectors = strtoul(sz, NULL, 0);
                        int nMax = fNonRealWorld ? nMaxSectors : (MAX_TRACK_SIZE - MIN_TRACK_OVERHEAD) / (nSectorSize + MIN_SECTOR_OVERHEAD);

                        if (nSectors < 1)
                            nSectors = 1;
                        else if (nMax && nSectors > nMax)
                            nSectors = nMax;
                        else
                            break;

                        wsprintf(sz, "%d", nSectors);
                        SetDlgItemText(hdlg_, IDE_SECTORS, sz);
                        SendDlgItemMessage(hdlg_, IDE_SECTORS, EM_SETSEL, 0, -1);
                    }
                    break;

                case IDC_SECTORSIZE:
                {
                    if (HIWORD(wParam_) == CBN_SELCHANGE)
                    {
                        // Update the sector size
                        nSectorSize = 128 << SendDlgItemMessage(hdlg_, IDC_SECTORSIZE, CB_GETCURSEL, 0, 0L);

                        // Write the number of sectors back, and it'll be capped if necessary
                        GetDlgItemText(hdlg_, IDE_SECTORS, sz, sizeof sz);
                        SetDlgItemText(hdlg_, IDE_SECTORS, sz);
                    }
                    break;
                }

                case IDB_SAVE:
                {
                    // Does the file already exist?
                    DWORD dwAttrs = GetFileAttributes(szFile);
                    if (dwAttrs != 0xffffffff)
                    {
                        char szWarning[512];
                        wsprintf(szWarning, "%s already exists!\n\nOverwrite existing file?", szFile);

                        // Prompt before overwriting it
                        if (MessageBox(NULL, szWarning, "Warning", MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
                            break;
                    }

                    // Eject any disk in the device we'll be inserting into
                    if (nInsertInto == 1)
                        pDrive1->Eject();
                    else if (nInsertInto == 2)
                        pDrive2->Eject();

                    // Create the new stream, either compressed or uncompressed
                    CStream* pStream = NULL;
#ifdef USE_ZLIB
                    if (fCompress)
                        pStream = new CZLibStream(NULL, szFile);
                    else
#endif
                        pStream = new CFileStream(NULL, szFile);

                    // Create a new disk of the appropriate type
                    CDisk* pDisk = NULL;
                    if (!nType)
                        pDisk = new CDSKDisk(pStream);
                    else if (nType == 1)
                        pDisk = new CSADDisk(pStream, nSides, nTracks, nSectors, nSectorSize);
//                  else if (nType == 2)
//                      pDisk = new CSDFDisk(pStream, nSides, nTracks, nSectors, nSectorSize);

                    // Save the new disk and close it
                    bool fSaved = pDisk->Save();
                    delete pDisk;

                    // If the save failed, moan about it
                    if (!fSaved)
                    {
                        Message(msgWarning, "Failed to save to %s!\n", szFile);
                        break;
                    }

                    // If we're to insert the new disk into a drive, do so now
                    if ((nInsertInto == 1 && pDrive1->Insert(szFile)) || (nInsertInto == 2 && pDrive2->Insert(szFile)))
                        Frame::SetStatus("New disk inserted into drive %d\n", nInsertInto);
                    else if (nInsertInto)
                        TRACE("!!! Failed to insert new disk!\n");

                    // We're all done
                    EndDialog(hdlg_, 1);
                    break;
                }
            }
        }
    }

    return FALSE;
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

    HMENU g_hMenu = LoadMenu(wc.hInstance, MAKEINTRESOURCE(IDR_MENU));
    LocaliseMenu(g_hMenu);

    // Create a window for the display (initially invisible)
    bool f = (RegisterClass(&wc) && (g_hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, WINDOW_CAPTION, WS_OVERLAPPEDWINDOW,
                                                            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, g_hMenu, wc.hInstance, NULL)));

    return f;
}


////////////////////////////////////////////////////////////////////////////////

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
    if (SendMessage(hwndCombo_, CB_SETCURSEL, atoi(GetOption(midiindev))+1, 0L) == CB_ERR)
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
    if (SendMessage(hwndCombo_, CB_SETCURSEL, atoi(GetOption(midioutdev))+1, 0L) == CB_ERR)
        SendMessage(hwndCombo_, CB_SETCURSEL, 0, 0L);
}


void FillPrintersCombo (HWND hwndCombo_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);

    // Dummy call to find out how much space is needed, then allocate it
    DWORD cbNeeded = 0, dwPrinters = 0;
    EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 1, NULL, 0, &cbNeeded, &dwPrinters);
    BYTE* pbPrinterInfo = new BYTE[cbNeeded];

    // Fill in dummy printer names, in the hope this avoids a WINE bug
    static char* cszUnknown = "<unknown printer>";
    for (int i = 0 ; i < static_cast<int>(dwPrinters) ; i++)
            reinterpret_cast<PRINTER_INFO_1*>(pbPrinterInfo)[i].pName = cszUnknown;

    // Enumerate the printers into the buffer we've allocated
    if (EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 1, pbPrinterInfo, cbNeeded, &cbNeeded, &dwPrinters))
    {
        // Loop through all the printers found, adding each to the printers combo
        for (int i = 0 ; i < static_cast<int>(dwPrinters) ; i++)
            SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(reinterpret_cast<PRINTER_INFO_1*>(pbPrinterInfo)[i].pName));
    }

    delete pbPrinterInfo;

    // Find the position of the item to select, or select the first one (None) if we can't find it
    LRESULT lPos = SendMessage(hwndCombo_, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(GetOption(printerdev)));
    SendMessage(hwndCombo_, CB_SETCURSEL, (lPos == CB_ERR) ? 0 : lPos, 0L);
}


void FillJoystickCombo (HWND hwndCombo_, const char* pcszSelected_)
{
    SendMessage(hwndCombo_, CB_RESETCONTENT, 0, 0L);
    SendMessage(hwndCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("None"));

    Input::FillJoystickCombo(hwndCombo_);

    // Find the position of the item to select, or select the first one (None) if we can't find it
    LRESULT lPos = SendMessage(hwndCombo_, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(pcszSelected_));
    if (lPos == CB_ERR)
        lPos = 0;
    SendMessage(hwndCombo_, CB_SETCURSEL, lPos, 0L);
}


// Helper function for filling a combo-box with strings and selecting one
void SetComboStrings (HWND hdlg_, UINT uID_, const char** ppcsz_, int nDefault_/*=-1*/)
{
    HWND hwndCombo = GetDlgItem(hdlg_, uID_);

    // Clear any existing contents
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);

    // Add each string from the list
    while (*ppcsz_)
        SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(*ppcsz_++));

    // Select the specified default, or the first item if there wasn't one
    SendMessage(hwndCombo, CB_SETCURSEL, (nDefault_ == -1) ? 0 : nDefault_, 0);
}

////////////////////////////////////////////////////////////////////////////////


// Base handler for all options property pages
BOOL CALLBACK BasePageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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
/*
                // Settings being applied?
                case PSN_APPLY:
                    // Hide the property sheet so any errors are reported on a clean background
                    ShowWindow(GetParent(hdlg_), SW_HIDE);
                    break;
*/
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


BOOL CALLBACK SystemPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMain[] = { "256K", "512K", NULL };
            SetComboStrings(hdlg_, IDC_MAIN_MEMORY, aszMain, (GetOption(mainmem) >> 8) - 1);

            static const char* aszExternal[] = { "None", "1MB", "2MB", "3MB", "4MB", NULL };
            SetComboStrings(hdlg_, IDC_EXTERNAL_MEMORY, aszExternal, GetOption(externalmem));

            SetWindowText(GetDlgItem(hdlg_, IDE_ROM0), GetOption(rom0));
            SetWindowText(GetDlgItem(hdlg_, IDE_ROM1), GetOption(rom1));

            SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_SETCHECK, GetOption(fastreset) ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(mainmem, static_cast<int>((SendDlgItemMessage(hdlg_, IDC_MAIN_MEMORY, CB_GETCURSEL, 0, 0L) + 1) << 8));
                SetOption(externalmem, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_EXTERNAL_MEMORY, CB_GETCURSEL, 0, 0L)));

                GetWindowText(GetDlgItem(hdlg_, IDE_ROM0), const_cast<char*>(GetOption(rom0)), MAX_PATH);
                GetWindowText(GetDlgItem(hdlg_, IDE_ROM1), const_cast<char*>(GetOption(rom1)), MAX_PATH);

                SetOption(fastreset, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_GETCHECK, 0, 0L) == BST_CHECKED));

/*
                // Perform an automatic reset if anything changed (if used we'll need a warning message on property sheet!)
                if (Changed(mainmem) || Changed(externalmem) || ChangedString(rom0) || ChangedString(rom1))
                    InterruptType = Z80_reset;
*/
            }

            break;
        }

        case WM_COMMAND:
        {
//          SendMessage(GetParent(hdlg_), PSM_CHANGED, reinterpret_cast<WPARAM>(hdlg_), 0L);
            break;
        }
    }

    return fRet;
}


BOOL CALLBACK DisplayPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszDepth[] = { "8-bit", "16-bit", "32-bit", NULL };
            int anDepths[] = { 0, 1, 2, 2 };
            SetComboStrings(hdlg_, IDC_FULLSCREEN_DEPTH, aszDepth, anDepths[((GetOption(depth) >> 3) - 1) & 3]);

            static const char* aszScaling[] = { "0.5x", "1x", "1.5x", NULL };
            SetComboStrings(hdlg_, IDC_WINDOW_SCALING, aszScaling, GetOption(scale)-1);

            SendDlgItemMessage(hdlg_, IDC_FULLSCREEN, BM_SETCHECK, GetOption(fullscreen) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_FULLSCREEN, 0L);

            SendDlgItemMessage(hdlg_, IDC_SYNC, BM_SETCHECK, GetOption(sync) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_RATIO_5_4, BM_SETCHECK, GetOption(ratio5_4) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_STRETCH_TO_FIT, BM_SETCHECK, GetOption(stretchtofit) ? BST_CHECKED : BST_UNCHECKED, 0L);

            bool fScanlines = GetOption(scanlines) && !GetOption(stretchtofit);
            SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_SETCHECK, fScanlines ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_STRETCH_TO_FIT, 0L);

            SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_SETCHECK, !GetOption(frameskip) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_FRAMESKIP_AUTOMATIC, 0L);


            HWND hwndCombo = GetDlgItem(hdlg_, IDC_FRAMESKIP);
            SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);
            SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Show every frame"));

            for (int i = 2 ; i <= 10 ; i++)
            {
                char sz[32];
                wsprintf(sz, "Show every %d%s frame", i, (i==2) ? "nd" : (i==3) ? "rd" : "th");
                SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz));
            }

            SendMessage(hwndCombo, CB_SETCURSEL, (!GetOption(frameskip)) ? 0 : GetOption(frameskip) - 1, 0L);

            SetComboStrings(hdlg_, IDC_BORDERS, aszBorders, GetOption(borders));
            SetComboStrings(hdlg_, IDC_SURFACE_TYPE, aszSurfaceType, GetOption(surface));

            break;
        }

        case WM_NOTIFY:
        {
            LPPSHNOTIFY ppsn = reinterpret_cast<LPPSHNOTIFY>(lParam_);

            if (ppsn->hdr.code == PSN_APPLY)
            {
                SetOption(fullscreen, SendDlgItemMessage(hdlg_, IDC_FULLSCREEN, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                int anDepths[] = { 8, 16, 32 };
                SetOption(depth, anDepths[SendDlgItemMessage(hdlg_, IDC_FULLSCREEN_DEPTH, CB_GETCURSEL, 0, 0L)]);
                SetOption(scale, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_WINDOW_SCALING, CB_GETCURSEL, 0, 0L) + 1));

                SetOption(sync, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SYNC, BM_GETCHECK, 0, 0L) == BST_CHECKED));
                SetOption(ratio5_4, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_RATIO_5_4, BM_GETCHECK, 0, 0L) == BST_CHECKED));
                SetOption(stretchtofit, SendDlgItemMessage(hdlg_, IDC_STRETCH_TO_FIT, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(scanlines, SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                int nFrameSkip = SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_GETCHECK, 0, 0L) != BST_CHECKED;
                SetOption(frameskip, nFrameSkip ? static_cast<int>(SendDlgItemMessage(hdlg_, IDC_FRAMESKIP, CB_GETCURSEL, 0, 0L)) + 1 : 0);

                SetOption(surface, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_SURFACE_TYPE, CB_GETCURSEL, 0, 0L)));
                SetOption(borders, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_BORDERS, CB_GETCURSEL, 0, 0L)));


                // Switching fullscreen <-> windowed?
                if (Changed(fullscreen))
                {
                    // Windowed -> fullscreen?
                    if (GetOption(fullscreen))
                    {
                        // Save the current windowed position, then switch the video mode
                        g_wp.length = sizeof g_wp;
                        GetWindowPlacement(g_hwnd, &g_wp);
                        Frame::Init();
                    }
                    else
                    {
                        // Restore the video, and the previously saved window position
                        Frame::Init();
                        SetWindowPlacement(g_hwnd, &g_wp);
                        UI::ResizeWindow(Changed(scale) || (Changed(stretchtofit) && !GetOption(stretchtofit)));
                    }
                }
                else
                {
                    // DX surface type or depth (whilst in full-screen mode) changed?
                    if (Changed(surface) || Changed(borders) || Changed(scanlines) || (GetOption(fullscreen) &&
                            (Changed(stretchtofit) || Changed(depth) || (GetOption(stretchtofit) && Changed(ratio5_4)))))
                        Frame::Init();

                    if (Changed(stretchtofit) || Changed(borders) || Changed(scale) || Changed(ratio5_4))
                        UI::ResizeWindow(Changed(borders) || !GetOption(stretchtofit));
                }
            }

            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_FULLSCREEN:
                {
                    bool fFullscreen = (SendDlgItemMessage(hdlg_, IDC_FULLSCREEN, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                    EnableWindow(GetDlgItem(hdlg_, IDS_WINDOW_SCALING), !fFullscreen);
                    EnableWindow(GetDlgItem(hdlg_, IDC_WINDOW_SCALING), !fFullscreen);
                    EnableWindow(GetDlgItem(hdlg_, IDS_FULLSCREEN_DEPTH), fFullscreen);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FULLSCREEN_DEPTH), fFullscreen);

                    break;
                }

                case IDC_STRETCH_TO_FIT:
                {
                    bool fStretch = (SendDlgItemMessage(hdlg_, IDC_STRETCH_TO_FIT, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                    if (fStretch)
                        SendDlgItemMessage(hdlg_, IDC_SCANLINES, BM_SETCHECK, BST_UNCHECKED, 0L);

                    EnableWindow(GetDlgItem(hdlg_, IDC_SCANLINES), !fStretch);
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


BOOL CALLBACK SoundPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszFreq[] = { "11025 Hz", "22050 Hz", "44100 Hz", NULL };
            static const int anFreq[] = { 0, 1, 2, 2 };
            SetComboStrings(hdlg_, IDC_FREQ, aszFreq, anFreq[GetOption(freq)/11025 - 1]);

            static const char* aszBits[] = { "8-bit", "16-bit", NULL };
            SetComboStrings(hdlg_, IDC_SAMPLE_SIZE, aszBits, (GetOption(bits) >> 3)-1);

            SendDlgItemMessage(hdlg_, IDC_STEREO, BM_SETCHECK, GetOption(stereo) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_FILTER, BM_SETCHECK, GetOption(filter) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_SETCHECK, GetOption(beeper) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_SETCHECK, GetOption(saasound) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SOUND_ENABLED, BM_SETCHECK, GetOption(sound) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_SOUND_ENABLED, 0L);


            static const char* aszLatency[] = { "1 frame (best)", "2 frames", "3 frames", "4 frames", "5 frames (default)",
                                                "10 frames", "15 frames", "20 frames", "25 frames", NULL };
            int nLatency = GetOption(latency);
            nLatency = (nLatency <= 5 ) ? nLatency - 1 : nLatency/5 + 3;
            SetComboStrings(hdlg_, IDC_LATENCY, aszLatency, nLatency);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(freq, 11025 * (1 << SendDlgItemMessage(hdlg_, IDC_FREQ, CB_GETCURSEL, 0, 0L)));
                SetOption(bits, static_cast<int>((SendDlgItemMessage(hdlg_, IDC_SAMPLE_SIZE, CB_GETCURSEL, 0, 0L) + 1)) << 3);

                SetOption(stereo, SendDlgItemMessage(hdlg_, IDC_STEREO, BM_GETCHECK,  0, 0L) == BST_CHECKED);
                SetOption(filter, SendDlgItemMessage(hdlg_, IDC_FILTER, BM_GETCHECK,  0, 0L) == BST_CHECKED);

                SetOption(sound, SendDlgItemMessage(hdlg_, IDC_SOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(beeper, SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_GETCHECK,  0, 0L) == BST_CHECKED);
                SetOption(saasound, SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                int nLatency = static_cast<int>(SendDlgItemMessage(hdlg_, IDC_LATENCY, CB_GETCURSEL, 0, 0L));
                nLatency = (nLatency <= 5) ? nLatency + 1 : (nLatency - 3) * 5;
                SetOption(latency, nLatency);


                if (Changed(sound) || Changed(saasound) || Changed(beeper) ||
                    Changed(freq) || Changed(bits) || Changed(stereo) || Changed(filter) || Changed(latency))
                    Sound::Init();

                if (Changed(beeper))
                    IO::InitBeeper();
            }

            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_SOUND_ENABLED:
                case IDC_SAASOUND_ENABLED:
                case IDC_BEEPER:
                {
                    bool fSound = (SendDlgItemMessage(hdlg_, IDC_SOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                    EnableWindow(GetDlgItem(hdlg_, IDS_LATENCY), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_LATENCY), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_STEREO), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAASOUND_ENABLED), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_BEEPER), fSound);

#ifdef USE_SAASOUND
                    bool fSAA = fSound && (SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);
#else
                    bool fSAA = false;
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAASOUND_ENABLED), false);
#endif

                    EnableWindow(GetDlgItem(hdlg_, IDS_FREQ), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FREQ), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDS_SAMPLE_SIZE), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAMPLE_SIZE), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FILTER), fSAA);

                    break;
                }
            }

            break;
        }
    }

    return fRet;
}


BOOL CALLBACK DiskPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszDrives1[] = { "None", "Floppy drive", NULL };
            SetComboStrings(hdlg_, IDC_DRIVE1, aszDrives1, GetOption(drive1));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE1, 0L);

            SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE1), GetOption(disk1));
            SendDlgItemMessage(hdlg_, IDE_IMAGE1, EM_SETSEL, _MAX_PATH, -1);
            EnableWindow(GetDlgItem(hdlg_, IDB_SAVE1), pDrive1->IsModified());


            static const char* aszDrives2[] = { "None", "Floppy drive", "Atom Hard Disk", NULL };
            SetComboStrings(hdlg_, IDC_DRIVE2, aszDrives2, GetOption(drive2));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE2, 0L);

            SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE2), GetOption(disk2));
            SendDlgItemMessage(hdlg_, IDE_IMAGE2, EM_SETSEL, _MAX_PATH, -1);
            EnableWindow(GetDlgItem(hdlg_, IDB_SAVE2), GetOption(drive2) == dskImage && pDrive2->IsModified());

//          bool fDirect1 = CFloppyStream::IsRecognised(GetOption(disk1));
//          SendDlgItemMessage(hdlg_, IDC_DIRECT_FLOPPY1, BM_SETCHECK, fDirect1 ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(drive1, static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_DRIVE1), CB_GETCURSEL, 0, 0L)));
                SetOption(drive2, static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_DRIVE2), CB_GETCURSEL, 0, 0L)));
                GetWindowText(GetDlgItem(hdlg_, IDE_IMAGE1), const_cast<char*>(GetOption(disk1)), MAX_PATH);
                GetWindowText(GetDlgItem(hdlg_, IDE_IMAGE2), const_cast<char*>(GetOption(disk2)), MAX_PATH);

                if (Changed(drive1) || Changed(drive2))
                    IO::InitDrives();

                if (*GetOption(disk1) && strcmpi(opts.disk1, GetOption(disk1)) && !pDrive1->Insert(GetOption(disk1)))
                {
                    Message(msgWarning, "Invalid disk image: %s", GetOption(disk1));
                    SetOption(disk1, "");
                }

                if (*GetOption(disk2) && strcmpi(opts.disk2, GetOption(disk2)) && !pDrive2->Insert(GetOption(disk2)))
                {
                    Message(msgWarning, "Invalid disk image: %s", GetOption(disk2));
                    SetOption(disk2, "");
                }
            }

            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDC_DRIVE1:
                {
                    LRESULT lDrive1 = SendMessage(GetDlgItem(hdlg_, IDC_DRIVE1), CB_GETCURSEL, 0, 0L);
                    EnableWindow(GetDlgItem(hdlg_, IDE_IMAGE1), lDrive1 != 0);
                    EnableWindow(GetDlgItem(hdlg_, IDB_BROWSE1), lDrive1 != 0);
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE1), (GetOption(drive1) == dskImage) && pDrive1->IsModified());
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT1), (GetOption(drive1) == dskImage) && pDrive1->IsInserted());
                    break;
                }

                case IDC_DRIVE2:
                {
                    LRESULT lDrive2 = SendMessage(GetDlgItem(hdlg_, IDC_DRIVE2), CB_GETCURSEL, 0, 0L);
                    EnableWindow(GetDlgItem(hdlg_, IDE_IMAGE2), lDrive2 == 1);
                    EnableWindow(GetDlgItem(hdlg_, IDB_BROWSE2), lDrive2 == 1);
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE2), pDrive2->IsModified());
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT2), 0 && GetOption(drive2) == dskImage && pDrive2->IsInserted());
                    break;
                }

                case IDE_IMAGE1:
                {
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT1), GetWindowTextLength(GetDlgItem(hdlg_, IDE_IMAGE1)));
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE1), false);
                    break;
                }

                case IDE_IMAGE2:
                {
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT2), GetOption(drive2) == dskImage && GetWindowTextLength(GetDlgItem(hdlg_, IDE_IMAGE2)));
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE2), false);
                    break;
                }

                case IDB_BROWSE1:
                {
                    char sz[_MAX_PATH];
                    GetWindowText(GetDlgItem(hdlg_, IDE_IMAGE1), sz, sizeof sz);
                    bool fReadOnly = false;
                    if (GetSaveLoadFile(hdlg_, szDiskFilters, NULL, sz, sizeof sz, &fReadOnly, true))
                    {
                        SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE1), sz);
                        SendDlgItemMessage(hdlg_, IDE_IMAGE1, EM_SETSEL, _MAX_PATH, -1);
                    }

                    break;
                }

                case IDB_BROWSE2:
                {
                    char sz[_MAX_PATH];
                    GetWindowText(GetDlgItem(hdlg_, IDE_IMAGE2), sz, sizeof sz);
                    bool fReadOnly = false;
                    if (GetSaveLoadFile(hdlg_, szDiskFilters, NULL, sz, sizeof sz, &fReadOnly, true))
                    {
                        SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE2), sz);
                        SendDlgItemMessage(hdlg_, IDE_IMAGE2, EM_SETSEL, _MAX_PATH, -1);
                    }

                    break;
                }

                case IDB_SAVE1:
                    pDrive1->Flush();
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE1), false);
                    break;

                case IDB_SAVE2:
                    pDrive2->Flush();
                    EnableWindow(GetDlgItem(hdlg_, IDB_SAVE2), false);
                    break;

                case IDB_EJECT1:
                    SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE1), "");
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT1), false);
                    break;

                case IDB_EJECT2:
                    SetWindowText(GetDlgItem(hdlg_, IDE_IMAGE2), "");
                    EnableWindow(GetDlgItem(hdlg_, IDB_EJECT2), false);
                    break;
            }

            break;
        }
    }

    return fRet;
}


BOOL CALLBACK InputPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMapping[] = { "None (raw)", "SAM Coupé", "Sinclair Spectrum", NULL };
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
                SetOption(keymapping, static_cast<int>(SendMessage(GetDlgItem(hdlg_, IDC_KEYBOARD_MAPPING), CB_GETCURSEL, 0, 0L)));

                SetOption(altforcntrl, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ALT_FOR_CNTRL, BM_GETCHECK, 0, 0L)) == BST_CHECKED);
                SetOption(altgrforedit, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_ALTGR_FOR_EDIT, BM_GETCHECK, 0, 0L)) == BST_CHECKED);

                SetOption(mouse, SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

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
                    break;
                }
            }

            break;
        }
    }

    return fRet;
}


BOOL CALLBACK JoystickPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

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


BOOL CALLBACK ParallelPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszParallel[] = { "None", "Printer", "Mono DAC", "Stereo EDdac/SAMdac", NULL };
            SetComboStrings(hdlg_, IDC_PARALLEL_1, aszParallel, GetOption(parallel1));
            SetComboStrings(hdlg_, IDC_PARALLEL_2, aszParallel, GetOption(parallel2));

            FillPrintersCombo(GetDlgItem(hdlg_, IDC_PRINTERS));
            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_1, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_2, 0L);

            SendDlgItemMessage(hdlg_, IDC_PRINTER_ONLINE, BM_SETCHECK, GetOption(printeronline) ? BST_CHECKED : BST_UNCHECKED, 0L);

            CIoDevice* pPrinter = (GetOption(parallel1) == 1) ? pParallel1 :
                                  (GetOption(parallel2) == 1) ? pParallel2 : NULL;
            EnableWindow(GetDlgItem(hdlg_, IDB_FLUSH_PRINT_JOB),
                pPrinter && reinterpret_cast<CPrinterDevice*>(pPrinter)->IsFlushable());

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(parallel1, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PARALLEL_1, CB_GETCURSEL, 0, 0L)));
                SetOption(parallel2, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PARALLEL_2, CB_GETCURSEL, 0, 0L)));

                SetOption(printerdev, "");
                SendMessage(GetDlgItem(hdlg_, IDC_PRINTERS), CB_GETLBTEXT, SendMessage(GetDlgItem(hdlg_, IDC_PRINTERS),
                            CB_GETCURSEL, 0, 0L),reinterpret_cast<LPARAM>(const_cast<char*>(GetOption(printerdev))));

                SetOption(printeronline, SendDlgItemMessage(hdlg_, IDC_PRINTER_ONLINE, BM_GETCHECK, 0, 0L) == BST_CHECKED);

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
                    EnableWindow(GetDlgItem(hdlg_, IDC_PRINTER_ONLINE), fPrinter1 || fPrinter2);
                    break;
                }

                case IDB_FLUSH_PRINT_JOB:
                    IO::InitParallel();
                    EnableWindow(GetDlgItem(hdlg_, IDB_FLUSH_PRINT_JOB), false);
                    break;
            }

            break;
        }
    }

    return fRet;
}


BOOL CALLBACK MidiPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const char* aszMIDI[] = { "None", "Windows MIDI", "Network", NULL };
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
                SetOption(midiindev,  itoa(SendDlgItemMessage(hdlg_, IDC_MIDI_IN,  CB_GETCURSEL, 0, 0L)-1, sz, 10));
                SetOption(midioutdev, itoa(SendDlgItemMessage(hdlg_, IDC_MIDI_OUT, CB_GETCURSEL, 0, 0L)-1, sz, 10));

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


BOOL CALLBACK MiscPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hdlg_, IDC_SAMBUS_CLOCK, BM_SETCHECK, GetOption(sambusclock) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_DALLAS_CLOCK, BM_SETCHECK, GetOption(dallasclock) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_CLOCK_SYNC, BM_SETCHECK, GetOption(clocksync) ? BST_CHECKED : BST_UNCHECKED, 0L);

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
                SetOption(clocksync, SendDlgItemMessage(hdlg_, IDC_CLOCK_SYNC, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(pauseinactive, SendDlgItemMessage(hdlg_, IDC_PAUSE_INACTIVE, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(drivelights, SendDlgItemMessage(hdlg_, IDC_DRIVE_LIGHTS, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(status, SendDlgItemMessage(hdlg_, IDC_STATUS, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(profile, static_cast<int>(SendDlgItemMessage(hdlg_, IDC_PROFILE, CB_GETCURSEL, 0, 0L)));
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


BOOL CALLBACK NewFnKeyProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
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
                if (aszActions[n] && *aszActions[n])
                    SendDlgItemMessage(hdlg_, IDC_ACTION, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(aszActions[n]));
            }

            // If we're editing an entry we need to show the current settings
            if (lParam_)
            {
                UINT uKey = static_cast<UINT>(lParam_);

                // Get the name of the key being edited
                GetKeyNameText(MapVirtualKeyEx(uKey >> 16, 0, GetKeyboardLayout(0)) << 16, szKey, sizeof szKey);

                // Locate the key in the list and select it
                HWND hwndCombo = GetDlgItem(hdlg_, IDC_KEY);
                LRESULT lPos = SendMessage(hwndCombo, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(szKey));
                SendMessage(hwndCombo, CB_SETCURSEL, (lPos == CB_ERR) ? 0 : lPos, 0L);

                // Check the appropriate modifier check-boxes
                SendDlgItemMessage(hdlg_, IDC_CTRL,  BM_SETCHECK, (uKey & 0x8000) ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_SHIFT, BM_SETCHECK, (uKey & 0x4000) ? BST_CHECKED : BST_UNCHECKED, 0L);

                // Locate the action in the list and select it
                hwndCombo = GetDlgItem(hdlg_, IDC_ACTION);
                UINT uAction = min(uKey & 0xff, MAX_ACTION-1);
                lPos = SendMessage(hwndCombo, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(aszActions[uAction]));
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
                        if (aszActions[uAction] && *aszActions[uAction] && !lstrcmpi(szAction, aszActions[uAction]))
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
                        dwParam |= (SendDlgItemMessage(hdlg_, IDC_SHIFT,  BM_GETCHECK, 0, 0L) == BST_CHECKED) ? 0x4000 : 0;

                        // Unhook any installed hook
                        if (g_hFnKeyHook)
                            UnhookWindowsHookEx(g_hFnKeyHook);

                        // Pass it back to the caller
                        EndDialog(hdlg_, dwParam);
                        break;
                    }

                    // Fall through...
                }

                case IDCANCEL:
                    // Unhook any installed hook
                    if (g_hFnKeyHook)
                        UnhookWindowsHookEx(g_hFnKeyHook);

                    // Tell the caller than the change was cancelled
                    EndDialog(hdlg_, 0);
                    break;
            }

            break;
        }

        case WM_KEYDOWN:
        {
            if (wParam_ >= VK_F1 && wParam_ <= VK_F12)
            {
                bool fCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                bool fShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

                SendDlgItemMessage(hdlg_, IDC_KEY, CB_SETCURSEL, wParam_ - VK_F1, 0L);
                SendDlgItemMessage(hdlg_, IDC_CTRL,  BM_SETCHECK, (GetAsyncKeyState(VK_CONTROL) & 0x8000) ? BST_CHECKED : BST_UNCHECKED, 0L);
                SendDlgItemMessage(hdlg_, IDC_SHIFT, BM_SETCHECK, (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? BST_CHECKED : BST_UNCHECKED, 0L);

                return 0;
            }
            break;
        }
    }

    return FALSE;
}


BOOL CALLBACK FnKeysPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            InitCommonControls();

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
            _strupr(strcpy(szKeys, GetOption(fnkeys)));

            // Process each of the 'key=action' pairs in the string
            for (char* psz = strtok(szKeys, ", \t") ; psz ; psz = strtok(NULL, ", \t"))
            {
                // Leading C and S characters indicate that Ctrl and/or Shift modifiers are required with the key
                bool fCtrl = (*psz == 'C');     if (fCtrl) psz++;
                bool fShift = (*psz == 'S');    if (fShift) psz++;

                // Currently we only support function keys F1-F12
                if (*psz++ == 'F')
                {
                    // Pack the key-code, shift/ctrl states and action into a DWORD for the item data, and insert the item
                    lvi.lParam = ((VK_F1 + strtoul(psz, &psz, 0) - 1) << 16) | (fCtrl ? 0x8000 : 0) | (fShift ? 0x4000 : 0);
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
                            pnmv->item.pszText = const_cast<char*>((lAction < MAX_ACTION) ? aszActions[lAction] : aszActions[0]);
                        }

                        // The first column details the key combination
                        else
                        {
                            char szKey[64] = "";
                            if (pnmv->item.lParam & 0x8000)
                                lstrcat(szKey, "Ctrl-");

                            if (pnmv->item.lParam & 0x4000)
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

                            // Add a C prefix for Ctrl and/or a S prefix for Shift
                            if (lvi.lParam & 0x8000) strcat(szKeys, "C");
                            if (lvi.lParam & 0x4000) strcat(szKeys, "S");

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
                    LVITEM lvi = { 0 };

                    HWND hwndList = GetDlgItem(hdlg_, IDL_FNKEYS);
                    int nItems = static_cast<int>(SendMessage(hwndList, LVM_GETITEMCOUNT, 0, 0L)), i;

                    // If this is an edit we need to find the item being edited
                    if (LOWORD(wParam_) == IDB_EDIT)
                    {
                        for (i = 0 ; i < nItems ; i++)
                        {
                            lvi.mask = LVIF_PARAM;
                            lvi.iItem = i;

                            // Look for a selected item, and fetch the details on it
                            if ((SendMessage(hwndList, LVM_GETITEMSTATE, i, LVIS_SELECTED) & LVIS_SELECTED) &&
                                 SendMessage(hwndList, LVM_GETITEM, 0, reinterpret_cast<LPARAM>(&lvi)))
                                 break;
                        }

                        // If we didn't find a selected item, give up now
                        if (i == nItems)
                            break;
                    }

                    // Prompt for the key details, give up if they cancelled
                    LPARAM lParam = DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_NEWFNKEY), hdlg_, NewFnKeyProc, lvi.lParam);
                    if (!lParam)
                        return 0;

                    // If we're editing, delete the old entry
                    if (LOWORD(wParam_) == IDB_EDIT)
                    {
                        SendMessage(hwndList, LVM_DELETEITEM, i, 0L);
                        nItems--;
                    }

                    // Iterate thru all the list items in reverse order looking for a conflict
                    for (i = nItems-1 ; i >= 0 ; i--)
                    {
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = i;

                        // Get the current item details
                        if (SendMessage(hwndList, LVM_GETITEM, 0, reinterpret_cast<LPARAM>(&lvi)))
                        {
                            // Have we found a mapping the same as the one we're adding?
                            if ((lvi.lParam & ~0xff) == (lParam & ~0xff))
                            {
                                // Yes, so prompt for delete confirmation, and give up now if they don't want to replace it
                                if (MessageBox(hdlg_, "Key binding already exists\n\n" "Replace existing entry?",
                                        "SimCoupe", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) == IDNO)
                                    return 0;

                                // Delete the entry, and stop looping now we've removed the conflict
                                SendMessage(hwndList, LVM_DELETEITEM, i, 0L);
                                break;
                            }
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


static void InitPage (PROPSHEETPAGE* pPage_, int nPage_, int nDialogId_, int nIconID_, DLGPROC pfnDlgProc_)
{
    pPage_ = &pPage_[nPage_];

    ZeroMemory(pPage_, sizeof *pPage_);
    pPage_->dwSize = sizeof *pPage_;
    pPage_->dwFlags = 0;//(nIconID_ ? PSP_USEICONID : 0) /*| PSP_HASHELP*/;
    pPage_->hInstance = __hinstance;
    pPage_->pszTemplate = MAKEINTRESOURCE(nDialogId_);
    pPage_->pszIcon = MAKEINTRESOURCE(nIconID_);
    pPage_->pfnDlgProc = pfnDlgProc_;
    pPage_->lParam = nPage_;
    pPage_->pfnCallback = NULL;
}


void DisplayOptions ()
{
    // Initialise the pages to go on the sheet
    PROPSHEETPAGE aPages[10];
    InitPage(aPages, 0, IDD_PAGE_SYSTEM,    IDI_MEMORY,     SystemPageDlgProc);
    InitPage(aPages, 1, IDD_PAGE_DISPLAY,   IDI_DISPLAY,    DisplayPageDlgProc);
    InitPage(aPages, 2, IDD_PAGE_SOUND,     IDI_SOUND,      SoundPageDlgProc);
    InitPage(aPages, 3, IDD_PAGE_DISKS,     IDI_FLOPPY,     DiskPageDlgProc);
    InitPage(aPages, 4, IDD_PAGE_INPUT,     IDI_KEYBOARD,   InputPageDlgProc);
    InitPage(aPages, 5, IDD_PAGE_JOYSTICK,  IDI_JOYSTICK,   JoystickPageDlgProc);
    InitPage(aPages, 6, IDD_PAGE_PARALLEL,  IDI_PORT,       ParallelPageDlgProc);
    InitPage(aPages, 7, IDD_PAGE_MIDI,      IDI_MIDI,       MidiPageDlgProc);
    InitPage(aPages, 8, IDD_PAGE_MISC,      IDI_MISC,       MiscPageDlgProc);
    InitPage(aPages, 9, IDD_PAGE_FNKEYS,    IDI_FNKEYS,     FnKeysPageDlgProc);

    PROPSHEETHEADER psh;
    ZeroMemory(&psh, sizeof psh);
    psh.dwSize = PROPSHEETHEADER_V1_SIZE;
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOAPPLYNOW /*| PSH_HASHELP*/;
    psh.hwndParent = g_hwnd;
    psh.hInstance = __hinstance;
    psh.pszIcon = MAKEINTRESOURCE(IDI_MISC);
    psh.pszCaption = "SimCoupe Options";
    psh.nPages = sizeof aPages / sizeof aPages[0];
    psh.nStartPage = nOptionPage;
    psh.ppsp = aPages;

    // Save the current option state, flag that we've not centred the dialogue box, then display them for editing
    opts = Options::s_Options;
    fCentredOptions = false;
    INT_PTR nRet = PropertySheet(&psh);

    Options::Save();

/*
    // Serial port changed?
    if (Changed(serial1) || Changed(serial2))
        IO::InitSerial();
*/
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
