// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: WinCE user interface
//
//  Copyright (c) 1999-2003  Simon Owen
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

#include "CDrive.h"
#include "Clock.h"
#include "CPU.h"
#include "Display.h"
#include "Frame.h"
#include "Input.h"
#include "Main.h"
#include "Options.h"
#include "OSD.h"
#include "Memory.h"
#include "Sound.h"
#include "Video.h"


extern int __argc;
extern char** __argv;
extern int __cdecl main (int argc, char *argv[]);


#include "resource.h" // For menu and dialogue box symbols

#ifdef _DEBUG
#define WINDOW_CAPTION      _T("SimCoupé [DEBUG]")
#else
#define WINDOW_CAPTION      _T("SimCoupé")
#endif

BOOL CALLBACK AboutDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
BOOL CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);
void SetComboStrings (HWND hdlg_, UINT uID_, const TCHAR** ppcsz_, int nDefault_=-1);

bool InitWindow ();
void DisplayOptions ();


bool g_fActive = true, g_fFrameStep = false, g_fTestMode = false;

HINSTANCE __hinstance;
HWND g_hwnd;
extern HINSTANCE __hinstance;

int nOptionPage = 0;                // Last active option property page
const int MAX_OPTION_PAGES = 16;    // Maximum number of option propery pages

OPTIONS opts;

// Helper macro for detecting options changes
#define Changed(o)        (opts.o != GetOption(o))
#define ChangedString(o)  (strcasecmp(opts.o, GetOption(o)))


static TCHAR szFloppyFilters [] =
#ifdef USE_ZLIB
    _T("All disks (.dsk;sad;sdf;gz;zip)\0*.dsk;*.sad;*.sdf;*.gz;*.zip\0")
    _T("Uncompressed (.dsk;.sad;.sdf)\0*.dsk;*.sad;*.sdf\0")
    _T("Compressed (.gz;zip)\0*.gz;*.zip\0")
#else
    _T("All disks (*.dsk;sad;sdf)\0*.dsk;*.sad;*.sdf\0")
#endif
    _T("All Files (*.*)\0*.*\0");


// Entry point from Windows
int WINAPI WinMain (HINSTANCE hinst_, HINSTANCE hinstPrev_, LPWSTR pszCmdLine_, int nCmdShow_)
{
    __hinstance = hinst_;

    // Override some defaults
    SetDefault(latency,15);     // 15 frames (lots needed for now)
    SetDefault(bits,8);         // 8-bit sound
    SetDefault(stereo,0);       // Mono

    if (Main::Init(__argc, __argv))
        CPU::Run();

    Main::Exit();

    return 1;
}


bool UI::Init (bool fFirstInit_/*=false*/)
{
    return InitWindow();
}

void UI::Exit (bool fReInit_/*=false*/)
{
    if (g_hwnd)
    {
        // When we reach here during a normal shutdown the window will already have gone, so check first
        if (IsWindow(g_hwnd))
            DestroyWindow(g_hwnd);

        g_hwnd = NULL;
    }
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
/*
    // Re-pause after a single frame-step
    if (g_fFrameStep)
        DoAction(actFrameStep);
*/
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
        if (!g_fPaused && g_fActive)
            break;

        // WinCE lacks WaitMessage(), so just sleep for a frame
        Sleep(200);
    }

    return true;
}

void UI::ShowMessage (eMsgType eType_, const char* pcszMessage_)
{
    USES_CONVERSION;
    PCWSTR pcwszMessage = A2W(pcszMessage_);

    switch (eType_)
    {
        case msgWarning:
            MessageBox(NULL, pcwszMessage, _T("Warning"), MB_OK | MB_ICONEXCLAMATION);
            break;

        case msgError:
            MessageBox(NULL, pcwszMessage, _T("Error"), MB_OK | MB_ICONSTOP);
            break;

        // Something went seriously wrong!
        case msgFatal:
            MessageBox(NULL, pcwszMessage, _T("Fatal Error"), MB_OK | MB_ICONSTOP);
            Video::Exit();
            exit(1);
            break;
    }
}


bool GetSaveLoadFile (HWND hwndParent_, LPCTSTR pcszFilters_, LPCTSTR pcszDefExt_, LPTSTR pszFile_, DWORD cbSize_, bool fLoad_)
{
    USES_CONVERSION;
    static int nFilterIndex = 0;
    *pszFile_ = '\0';

    // Fill in the details for a fax file to select
    OPENFILENAME ofn = { sizeof ofn };
    ofn.hwndOwner       = hwndParent_;
    ofn.lpstrFilter     = pcszFilters_;
    ofn.lpstrFile       = pszFile_;
    ofn.nMaxFile        = cbSize_;
    ofn.lpstrDefExt     = pcszDefExt_;
    ofn.nFilterIndex    = nFilterIndex;
    ofn.Flags           = OFN_PATHMUSTEXIST | (fLoad_ ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);

    // Return if the user cancelled without picking a file
    BOOL fRet = fLoad_ ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn);

    if (!fRet)
    {
        TRACE("%s() failed (%#08lx)\n", fLoad_ ? "GetOpenFileName" : "GetSaveFileName", CommDlgExtendedError());
        return FALSE;
    }

    // Remember the current filter selection for next time
    nFilterIndex = ofn.nFilterIndex;

    // Copy the filename into the supplied string, and return success
    TRACE("GetSaveLoadFile: %s selected\n", W2A(pszFile_));
    return TRUE;
}

bool InsertDisk (CDiskDevice* pDrive_)
{
    USES_CONVERSION;
    static TCHAR szFile[MAX_PATH] = _T("");

    // Prompt for the a new disk to insert
    if (GetSaveLoadFile(g_hwnd, szFloppyFilters, NULL, szFile, MAX_PATH, true))
    {
        // Eject any previous disk, saving if necessary
        pDrive_->Eject();

        // Open the new disk (using the requested read-only mode), and insert it into the drive if successful
        if (pDrive_->Insert(W2A(szFile)))
            return true;

        Message(msgWarning, "Invalid disk image: %s", W2A(szFile));
    }

    return false;
}


bool UI::DoAction (int nAction_, bool fPressed_/*=true*/)
{
    USES_CONVERSION;

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
                Sound::Silence();
                SetOption(fullscreen, !GetOption(fullscreen));
                InvalidateRect(g_hwnd, NULL, TRUE);

                if (GetOption(fullscreen))
                {
                    // Landscape
                    Frame::SetView(SCREEN_BLOCKS+4, SCREEN_LINES+48);
                    ShowWindow(g_hwndSIP, SW_HIDE);
                }
                else
                {
                    // Portrait
                    Frame::SetView(SCREEN_BLOCKS, SCREEN_LINES+66);
                    ShowWindow(g_hwndSIP, SW_SHOW);
                }

                Frame::Init();
                break;

            case actToggle50HzSync:
                SetOption(sync, !GetOption(sync));
                Frame::SetStatus("Frame sync %s", GetOption(sync) ? "enabled" : "disabled");
                break;

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

            case actInsertFloppy1:
                if (GetOption(drive1) != dskImage)
                    Message(msgInfo, "Floppy 1 is not present");
                else
                {
                    InsertDisk(pDrive1);
                    SetOption(disk1, pDrive1->GetImage());
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
                    Frame::SetStatus("Saved changes to disk in drive 1");
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) != dskImage)
                    Message(msgInfo, "Floppy 2 is not present");
                else
                {
                    InsertDisk(pDrive2);
                    SetOption(disk2, pDrive2->GetImage());
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

            case actSaveScreenshot:
                Frame::SaveFrame();
                break;
/*
            case actImportData:
                Video::CreatePalettes(true);
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_IMPORT), g_hwnd, ImportExportDlgProc, 1);
                Video::CreatePalettes();
                break;

            case actExportData:
                Video::CreatePalettes(true);
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_EXPORT), g_hwnd, ImportExportDlgProc, 0);
                Video::CreatePalettes();
                break;
*/
            case actDisplayOptions:
                 Video::CreatePalettes(true);
                 DisplayOptions();
                 Video::CreatePalettes();
                break;

            case actAbout:
                DialogBoxParam(__hinstance, MAKEINTRESOURCE(IDD_ABOUT), g_hwnd, AboutDlgProc, NULL);
                break;

            case actMinimise:
                // Hacky combination of actions to achieve what we want
                ShowWindow(g_hwnd, SW_HIDE);
                SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0,0, 0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOACTIVATE);
                break;

            case actExitApplication:
                ShowWindow(g_hwnd, SW_HIDE);
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
                    SetWindowText(g_hwnd, WINDOW_CAPTION _T(" - Paused"));
                }
                else
                {
                    Input::Purge();
                    Sound::Play();
                    SetWindowText(g_hwnd, WINDOW_CAPTION);
                    g_fFrameStep = (nAction_ == actFrameStep);
                }

                Video::CreatePalettes();
                Display::SetDirty();
                Frame::Redraw();
                break;
            }

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


BOOL CALLBACK AboutDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HFONT hfont;
    static HWND hwndURL;

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            // Show us full-screen with an OK button
            SHINITDLGINFO idi = { SHIDIM_FLAGS, hdlg_, SHIDIF_DONEBUTTON | SHIDIF_FULLSCREENNOMENUBAR | SHIDIF_SIPDOWN };
            SHInitDialog(&idi);

            // Grab the attributes of the current GUI font
            LOGFONT lf;
            GetObject(GetStockObject(SYSTEM_FONT), sizeof lf, &lf);

            // Add underline, and create it as a font to use for URLs
            lf.lfHeight = -11;
            lf.lfUnderline = TRUE;
            hfont = CreateFontIndirect(&lf);

            // Fetch the URL handle for later, and set the underline font
            hwndURL = GetDlgItem(hdlg_, ID_HOMEPAGE);
            SendMessage(hwndURL, WM_SETFONT, reinterpret_cast<WPARAM>(hfont), 0L);

            return 1;
        }

        case WM_DESTROY:
            if (hfont)
            {
                DeleteObject(hfont);
                hfont = NULL;
            }
            break;

        case WM_COMMAND:
            // Clicking the URL launches the homepage in the default browser
            if (wParam_ == ID_HOMEPAGE)
            {
                SHELLEXECUTEINFO sei = { sizeof sei };
                sei.fMask = SEE_MASK_FLAG_NO_UI;
                sei.lpVerb = _T("open");
                sei.lpFile = _T("http://www.simcoupe.org/");
                sei.nShow = SW_SHOWMAXIMIZED;
                ShellExecuteEx(&sei);
            }

            // Otherwise we're to close the dialog
            else
                EndDialog(hdlg_, 0);
            break;

        case WM_CTLCOLORSTATIC:
            // Make the text blue if it's the URL
            if (hwndURL == reinterpret_cast<HWND>(lParam_))
                SetTextColor(reinterpret_cast<HDC>(wParam_), RGB(0,0,255));

            // Fall through...

        case WM_CTLCOLORDLG:
            // Force a white background on the dialog (and statics, from above)
            return (BOOL)GetStockObject(WHITE_BRUSH);
    }

    return 0;
}


long CALLBACK WindowProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static bool fInMenu = false;
    static DWORD dwPressTime;
    static LPARAM lPressPos;

//  TRACE("WindowProc(%#04x,%#08x,%#08lx,%#08lx)\n", hwnd_, uMsg_, wParam_, lParam_);

    if (Input::FilterMessage(hwnd_, uMsg_, wParam_, lParam_))
        return 0;

    switch (uMsg_)
    {
        // Main window being created
        case WM_CREATE:
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

        case WM_KILLFOCUS:
            GXSuspend();
//          Sound::Silence();
            Input::Purge();
            break;

        case WM_SETFOCUS:
            GXResume();
//          Sound::Silence();
            break;

        case WM_PAINT:
            Display::SetDirty();
            break;

        case WM_ENABLE:
            // Prevent us from being disabled, so we can be closed when modal dialogs are active
            if (!IsWindowEnabled(hwnd_))
                EnableWindow(hwnd_, TRUE);
            return 0;

        case WM_MOUSEMOVE:
            if (dwPressTime)
            {
                int dx = HIWORD(lParam_) - HIWORD(lPressPos);
                int dy = LOWORD(lParam_) - LOWORD(lPressPos);
            }
            break;

        // Main window being activated or deactivated
        case WM_ACTIVATE:
        {
            TRACE("WM_ACTIVATEAPP (%#08lx)\n", wParam_);
            g_fActive = LOWORD(wParam_) != WA_INACTIVE;

            // Should be pause the emulation when inactive?
            if (Frame::GetScreen())
            {
                // Silence the sound while we're not running
                if (!g_fActive)
                    Sound::Silence();

                // Dim the display while we're paused, or undim it when we get control again
                Video::CreatePalettes(!g_fActive);
                Display::SetDirty();
                Frame::Redraw();

                SetWindowText(g_hwnd, g_fActive ? WINDOW_CAPTION : WINDOW_CAPTION _T(" - Paused"));
            }
            break;
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
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}

/*
BOOL CALLBACK ImportExportDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    USES_CONVERSION;

    static UINT uAddress = 32768, uLength = 0;
    static int nPage = 1;
    static WORD wOffset = 0;
    static TCHAR szFile [MAX_PATH];
    static bool fBasicAddress = true, fUpdating = false, fImport = false;
    TCHAR sz[32];

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            fImport = (lParam_ != 0);

            SetDlgItemText(hdlg_, IDE_FILE, szFile);
            SendDlgItemMessage(hdlg_, IDE_FILE, EM_SETSEL, MAX_PATH, -1);

            wsprintf(sz, _T("%u"), uAddress); SetDlgItemText(hdlg_, IDE_ADDRESS, sz);
            SendMessage(hdlg_, WM_COMMAND, fBasicAddress ? IDR_BASIC_ADDRESS : IDR_PAGE_OFFSET, 0);

            if (!fImport && uLength)
            {
                wsprintf(sz, _T("%u"), uLength);
                SetDlgItemText(hdlg_, IDE_LENGTH, sz);
            }

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
                            GetDlgItemText(hdlg_, IDE_LENGTH, sz, 32);
                            uLength = (fImport ? -1 : _ttol(sz));
                        }

                        GetDlgItemText(hdlg_, IDE_FILE, szFile, MAX_PATH);
                        SetLastError(NO_ERROR);
                        DWORD dwAttrs = GetFileAttributes(szFile), dwError = GetLastError();

                        // If we're importing, the file must exist and can't be a directory
                        if (fImport)
                            fOK = !dwError && !(dwAttrs & FILE_ATTRIBUTE_DIRECTORY);

                        // When exporting there must be a length, and the file can't be a directory
                        else
                            fOK = _ttol(sz) && (dwError == ERROR_FILE_NOT_FOUND || !(dwAttrs & FILE_ATTRIBUTE_DIRECTORY));

                        EnableWindow(GetDlgItem(hdlg_, IDOK), fOK);
                    }
                    break;

                case IDE_ADDRESS:
                    if (fChange && !fUpdating)
                    {
                        GetDlgItemText(hdlg_, IDE_ADDRESS, sz, MAX_PATH);
                        uAddress = _ttol(sz) & 0x7ffff;
                        nPage = static_cast<int>(uAddress/16384 - 1) & 0x1f;
                        wOffset = static_cast<WORD>(uAddress) & 0x3fff;

                        fUpdating = true;
                        wsprintf(sz, _T("%u"), nPage);      SetDlgItemText(hdlg_, IDE_PAGE, sz);
                        wsprintf(sz, _T("%u"), wOffset);    SetDlgItemText(hdlg_, IDE_OFFSET, sz);
                        fUpdating = false;
                    }
                    break;

                case IDE_PAGE:
                case IDE_OFFSET:
                    if (fChange && !fUpdating)
                    {
                        GetDlgItemText(hdlg_, IDE_PAGE, sz, MAX_PATH);
                        nPage = static_cast<int>(_ttol(sz)) & 0x1f;

                        GetDlgItemText(hdlg_, IDE_OFFSET, sz, MAX_PATH);
                        wOffset = static_cast<WORD>(_ttol(sz)) & 0x3fff;

                        fUpdating = true;
                        uAddress = static_cast<DWORD>(nPage + 1) * 16384 + wOffset;
                        wsprintf(sz, _T("%u"), uAddress);
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
                    if (!GetSaveLoadFile(g_hwnd, _T("Binary files (*.bin)\0*.bin\0Data files (*.dat)\0*.dat\0All files (*.*)\0*.*\0"),
                                    NULL, szFile, MAX_PATH, fImport))
                        break;

                    SetWindowText(GetDlgItem(hdlg_, IDE_FILE), szFile);
                    SendDlgItemMessage(hdlg_, IDE_FILE, EM_SETSEL, MAX_PATH, -1);
                    break;
                }
                case IDOK:
                {
                    // Addresses in the first 16K are taken from ROM0
                    if (uAddress < 0x4000)
                        nPage = ROM0;

                    FILE* hFile;
                    if (!szFile[0] || !(hFile = fopen(W2A(szFile), fImport ? "rb" : "wb")))
                        Message(msgError, "Failed to open %s for %s", szFile, fImport ? "reading" : "writing");
                    else
                    {
                        // Limit the length to 512K as there's no point in reading more
                        UINT uLen = uLength &= 0x7ffff;

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
*/

bool InitWindow ()
{
    // set up and register window class
    WNDCLASS wc = { 0 };
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = __hinstance;
    wc.hIcon = LoadIcon(__hinstance, MAKEINTRESOURCE(IDI_MAIN));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = _T("SimCoupeClass");

    // Check for a running copy of the emulator
    HWND hwnd = FindWindow(wc.lpszClassName, NULL);
    if (hwnd)
    {
        // Activate the previous instance, and fail the initialisation
        SetForegroundWindow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return false;
    }

    // Create a window for the display
    return (RegisterClass(&wc) && (g_hwnd = CreateWindowEx(0, wc.lpszClassName, WINDOW_CAPTION, WS_VISIBLE,
                                                            0, 0, 240, 320, NULL, NULL, __hinstance, NULL)));
}

////////////////////////////////////////////////////////////////////////////////

// Helper function for filling a combo-box with strings and selecting one
void SetComboStrings (HWND hdlg_, UINT uID_, const TCHAR** ppcsz_, int nDefault_)
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


BOOL CALLBACK SystemPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    USES_CONVERSION;
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const TCHAR* aszMain[] = { _T("256K"), _T("512K"), NULL };
            SetComboStrings(hdlg_, IDC_MAIN_MEMORY, aszMain, (GetOption(mainmem) >> 8) - 1);

            static const TCHAR* aszExternal[] = { _T("None"), _T("1MB"), _T("2MB"), _T("3MB"), _T("4MB"), NULL };
            SetComboStrings(hdlg_, IDC_EXTERNAL_MEMORY, aszExternal, GetOption(externalmem));

            SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_SETCHECK, GetOption(fastreset) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SetDlgItemText(hdlg_, IDE_ROM, A2W(GetOption(rom)));

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(mainmem, (SendDlgItemMessage(hdlg_, IDC_MAIN_MEMORY, CB_GETCURSEL, 0, 0L) + 1) << 8);
                SetOption(externalmem, SendDlgItemMessage(hdlg_, IDC_EXTERNAL_MEMORY, CB_GETCURSEL, 0, 0L));

                SetOption(fastreset, SendDlgItemMessage(hdlg_, IDC_FAST_RESET, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                TCHAR szROM[MAX_PATH];
                GetDlgItemText(hdlg_, IDE_ROM, szROM, MAX_PATH);
                SetOption(rom,W2A(szROM));
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam_))
            {
                case IDB_BROWSE:
                {
                    TCHAR szFile[MAX_PATH] = _T("");

                    if (!GetSaveLoadFile(hdlg_, _T("ROM images (*.rom)\0*.rom\0All files (*.*)\0*.*\0"),
                                            NULL, szFile, MAX_PATH, true))
                        break;

                    SetDlgItemText(hdlg_, IDE_ROM, szFile);
                    SendDlgItemMessage(hdlg_, IDE_ROM, EM_SETSEL, MAX_PATH, -1);
                    break;
                }

            }
            break;
        }
    }

    return fRet;
}


BOOL CALLBACK DisplayPageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    USES_CONVERSION;
    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            SendDlgItemMessage(hdlg_, IDC_FULLSCREEN, BM_SETCHECK, GetOption(fullscreen) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_FULLSCREEN, 0L);

            SendDlgItemMessage(hdlg_, IDC_SYNC, BM_SETCHECK, GetOption(sync) ? BST_CHECKED : BST_UNCHECKED, 0L);

            static const TCHAR* aszMode3[] = { _T("Odd pixels"), _T("Even pixels"), _T("Interlaced"), NULL };
            SetComboStrings(hdlg_, IDC_MODE3, aszMode3, GetOption(mode3));

            SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_SETCHECK, !GetOption(frameskip) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_FRAMESKIP_AUTOMATIC, 0L);

            HWND hwndCombo = GetDlgItem(hdlg_, IDC_FRAMESKIP);
            SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0L);
            SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(_T("Show every frame")));

            for (int i = 2 ; i <= 10 ; i++)
            {
                TCHAR sz[32];
                wsprintf(sz, _T("Show every %d%s frame"), i, (i==2) ? _T("nd") : (i==3) ? _T("rd") : _T("th"));
                SendMessage(hwndCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sz));
            }

            SendMessage(hwndCombo, CB_SETCURSEL, (!GetOption(frameskip)) ? 0 : GetOption(frameskip) - 1, 0L);
            break;
        }

        case WM_NOTIFY:
        {
            LPPSHNOTIFY ppsn = reinterpret_cast<LPPSHNOTIFY>(lParam_);

            if (ppsn->hdr.code == PSN_APPLY)
            {
                SetOption(fullscreen, SendDlgItemMessage(hdlg_, IDC_FULLSCREEN, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(sync, SendDlgItemMessage(hdlg_, IDC_SYNC, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(mode3, SendDlgItemMessage(hdlg_, IDC_MODE3, CB_GETCURSEL, 0, 0L));

                int nFrameSkip = SendDlgItemMessage(hdlg_, IDC_FRAMESKIP_AUTOMATIC, BM_GETCHECK, 0, 0L) != BST_CHECKED;
                SetOption(frameskip, nFrameSkip ? SendDlgItemMessage(hdlg_, IDC_FRAMESKIP, CB_GETCURSEL, 0, 0L) + 1 : 0);

                if (Changed(fullscreen))
                {
                    ShowWindow(g_hwndSIP, GetOption(fullscreen) ? SW_HIDE : SW_SHOW);
                    Frame::Init();
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
//                  EnableWindow(GetDlgItem(hdlg_, IDS_LANDSCAPE_RIGHT), fFullscreen);

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
            static const TCHAR* aszFrequency[] = { _T("11025 Hz"), _T("22050 Hz"), _T("44100 Hz"), NULL };
            static const int anFrequency[] = { 0, 1, 2, 2 };
            SetComboStrings(hdlg_, IDC_FREQ, aszFrequency, anFrequency[GetOption(freq)/11025 - 1]);

            static const TCHAR* aszBits[] = { _T("8-bit"), _T("16-bit"), NULL };
            SetComboStrings(hdlg_, IDC_SAMPLE_SIZE, aszBits, (GetOption(bits) >> 3)-1);

            SendDlgItemMessage(hdlg_, IDC_STEREO, BM_SETCHECK, GetOption(stereo) ? BST_CHECKED : BST_UNCHECKED, 0L);

            SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_SETCHECK, GetOption(beeper) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_SETCHECK, GetOption(saasound) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendDlgItemMessage(hdlg_, IDC_SOUND_ENABLED, BM_SETCHECK, GetOption(sound) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_SOUND_ENABLED, 0L);


            static const TCHAR* aszLatency[] = { _T("5 frames"), _T("10 frames"), _T("15 frames (default)"), _T("20 frames"), _T("25 frames"), NULL };
            int nLatency = GetOption(latency);
            nLatency = (nLatency/5) - 1;
            SetComboStrings(hdlg_, IDC_LATENCY, aszLatency, nLatency);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(freq, 11025 * (1 << SendDlgItemMessage(hdlg_, IDC_FREQ, CB_GETCURSEL, 0, 0L)));
                SetOption(bits, (SendDlgItemMessage(hdlg_, IDC_SAMPLE_SIZE, CB_GETCURSEL, 0, 0L) + 1) << 3);

                SetOption(stereo, SendDlgItemMessage(hdlg_, IDC_STEREO, BM_GETCHECK,  0, 0L) == BST_CHECKED);

                SetOption(sound, SendDlgItemMessage(hdlg_, IDC_SOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(beeper, SendDlgItemMessage(hdlg_, IDC_BEEPER, BM_GETCHECK,  0, 0L) == BST_CHECKED);
                SetOption(saasound, SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                int nLatency = SendDlgItemMessage(hdlg_, IDC_LATENCY, CB_GETCURSEL, 0, 0L);
                nLatency = (nLatency + 1) * 5;
                SetOption(latency, nLatency);


                if (Changed(sound) || Changed(saasound) || Changed(beeper) ||
                    Changed(freq) || Changed(bits) || Changed(stereo) || Changed(latency))
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
                    bool fSAA = fSound && (SendDlgItemMessage(hdlg_, IDC_SAASOUND_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                    EnableWindow(GetDlgItem(hdlg_, IDS_LATENCY), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_LATENCY), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_STEREO), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAASOUND_ENABLED), fSound);
                    EnableWindow(GetDlgItem(hdlg_, IDC_BEEPER), fSound);

                    EnableWindow(GetDlgItem(hdlg_, IDS_FREQ), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDC_FREQ), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDS_SAMPLE_SIZE), fSAA);
                    EnableWindow(GetDlgItem(hdlg_, IDC_SAMPLE_SIZE), fSAA);

                    break;
                }
            }
            break;
        }
    }

    return fRet;
}


BOOL CALLBACK DrivePageDlgProc (HWND hdlg_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    USES_CONVERSION;

    BOOL fRet = BasePageDlgProc(hdlg_, uMsg_, wParam_, lParam_);

    switch (uMsg_)
    {
        case WM_INITDIALOG:
        {
            static const TCHAR* aszDrives1[] = { _T("None"), _T("Floppy drive"), NULL };
            SetComboStrings(hdlg_, IDC_DRIVE1, aszDrives1, GetOption(drive1));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE1, 0L);

            static const TCHAR* aszDrives2[] = { _T("None"), _T("Floppy drive"), _T("Atom Hard Disk"), NULL };
            SetComboStrings(hdlg_, IDC_DRIVE2, aszDrives2, GetOption(drive2));
            SendMessage(hdlg_, WM_COMMAND, IDC_DRIVE2, 0L);

            static const TCHAR* aszSensitivity[] = { _T("Low"), _T("Medium"), _T("High"), NULL };
            SetComboStrings(hdlg_, IDC_SENSITIVITY, aszSensitivity,
                !GetOption(turboload) ? 1 : GetOption(turboload) <= 5 ? 2 : GetOption(turboload) <= 50 ? 1 : 0);

            SendDlgItemMessage(hdlg_, IDC_TURBO_LOAD, BM_SETCHECK, GetOption(turboload) ? BST_CHECKED : BST_UNCHECKED, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_TURBO_LOAD, 0L);
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

                SetOption(drive1, SendMessage(GetDlgItem(hdlg_, IDC_DRIVE1), CB_GETCURSEL, 0, 0L));
                SetOption(drive2, SendMessage(GetDlgItem(hdlg_, IDC_DRIVE2), CB_GETCURSEL, 0, 0L));

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
            static const TCHAR* aszMapping[] = { _T("None (raw)"), _T("SAM Coupé"), _T("Sinclair Spectrum"), NULL };
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
                SetOption(keymapping, SendMessage(GetDlgItem(hdlg_, IDC_KEYBOARD_MAPPING), CB_GETCURSEL, 0, 0L));

                SetOption(altforcntrl, SendDlgItemMessage(hdlg_, IDC_ALT_FOR_CNTRL, BM_GETCHECK, 0, 0L) == BST_CHECKED);
                SetOption(altgrforedit, SendDlgItemMessage(hdlg_, IDC_ALTGR_FOR_EDIT, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                SetOption(mouse, (SendDlgItemMessage(hdlg_, IDC_MOUSE_ENABLED, BM_GETCHECK, 0, 0L) == BST_CHECKED));

                if (Changed(keymapping) || Changed(mouse))
                    Input::Init();
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
            static const TCHAR* aszParallel[] = { _T("None"), _T("Printer"), _T("Mono DAC"), _T("Stereo EDdac/SAMdac"), NULL };
            SetComboStrings(hdlg_, IDC_PARALLEL_1, aszParallel, GetOption(parallel1));
            SetComboStrings(hdlg_, IDC_PARALLEL_2, aszParallel, GetOption(parallel2));

            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_1, 0L);
            SendMessage(hdlg_, WM_COMMAND, IDC_PARALLEL_2, 0L);

            SendDlgItemMessage(hdlg_, IDC_PRINTER_ONLINE, BM_SETCHECK, GetOption(printeronline) ? BST_CHECKED : BST_UNCHECKED, 0L);

            break;
        }

        case WM_NOTIFY:
        {
            if (reinterpret_cast<LPPSHNOTIFY>(lParam_)->hdr.code == PSN_APPLY)
            {
                SetOption(parallel1, SendDlgItemMessage(hdlg_, IDC_PARALLEL_1, CB_GETCURSEL, 0, 0L));
                SetOption(parallel2, SendDlgItemMessage(hdlg_, IDC_PARALLEL_2, CB_GETCURSEL, 0, 0L));

                SetOption(printeronline, SendDlgItemMessage(hdlg_, IDC_PRINTER_ONLINE, BM_GETCHECK, 0, 0L) == BST_CHECKED);

                if (Changed(parallel1) || Changed(parallel2))
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

            static const TCHAR* aszProfile[] = { _T("Disabled"), _T("Speed and frame rate"), _T("Detailed percentages"), _T("Detailed timings"), NULL };
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

                SetOption(profile, SendDlgItemMessage(hdlg_, IDC_PROFILE, CB_GETCURSEL, 0, 0L));
            }
            break;
        }
    }

    return fRet;
}


int CALLBACK PropSheetProc (HWND hwnd_, UINT uMsg_, LPARAM lParam_)
{
    if (uMsg_ == PSCB_INITIALIZED)
    {
        // Get tab control
        HWND hwndTabs = GetDlgItem(hwnd_, 0x3020);

        // Set the tab control to use flat tabs, like Settings
        DWORD dwStyle = GetWindowLong (hwndTabs, GWL_STYLE);
        SetWindowLong (hwndTabs, GWL_STYLE, dwStyle | TCS_BOTTOM);

        // Blank menu bar, to cover the gap below the tabs
        SHMENUBARINFO cbi = { sizeof cbi };
        cbi.hwndParent = hwnd_;
        cbi.dwFlags = SHCMBF_EMPTYBAR;
        SHCreateMenuBar (&cbi);
    }

    // We also need to return the correct version number to get flat tabs
    else if (uMsg_ ==  PSCB_GETVERSION)
        return COMCTL32_VERSION;

    return 1;
}

static void InitPage (PROPSHEETPAGE* pPage_, int nPage_, int nDialogId_, DLGPROC pfnDlgProc_)
{
    pPage_ = &pPage_[nPage_];

    ZeroMemory(pPage_, sizeof *pPage_);
    pPage_->dwSize = sizeof *pPage_;
    pPage_->dwFlags = 0;
    pPage_->hInstance = __hinstance;
    pPage_->pszTemplate = MAKEINTRESOURCE(nDialogId_);
    pPage_->pfnDlgProc = pfnDlgProc_;
    pPage_->lParam = nPage_;
    pPage_->pfnCallback = NULL;
}


void DisplayOptions ()
{
    // Initialise the pages to go on the sheet
    PROPSHEETPAGE aPages[7];
    InitPage(aPages, 0, IDD_PAGE_SYSTEM,    SystemPageDlgProc);
    InitPage(aPages, 1, IDD_PAGE_DISPLAY,   DisplayPageDlgProc);
    InitPage(aPages, 2, IDD_PAGE_SOUND,     SoundPageDlgProc);
    InitPage(aPages, 3, IDD_PAGE_DRIVES,    DrivePageDlgProc);
    InitPage(aPages, 4, IDD_PAGE_INPUT,     InputPageDlgProc);
    InitPage(aPages, 5, IDD_PAGE_PARALLEL,  ParallelPageDlgProc);
    InitPage(aPages, 6, IDD_PAGE_MISC,      MiscPageDlgProc);

    PROPSHEETHEADER psh;
    ZeroMemory(&psh, sizeof psh);
    psh.dwSize = sizeof PROPSHEETHEADER;
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_NOAPPLYNOW | PSH_MAXIMIZE | PSH_USECALLBACK;// | PSH_HASHELP;
    psh.hwndParent = g_hwnd;
    psh.hInstance = __hinstance;
    psh.pszCaption = _T("Options");
    psh.nPages = sizeof aPages / sizeof aPages[0];
    psh.nStartPage = nOptionPage;
    psh.ppsp = aPages;
    psh.pfnCallback = PropSheetProc;

    // Save the current option state, flag that we've not centred the dialogue box, then display them for editing
    opts = Options::s_Options;
    int nRet = PropertySheet(&psh);

    Options::Save();
}
