// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.cpp: Display frame generation
//
//  Copyright (c) 1999-2002  Simon Owen
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

// Notes:
//  This module generates a display-independant representation of a single
//  TV frame in a CScreen object.  The platform-specific conversion to the
//  native display format is done in Display.cpp
//
//  The actual drawing work is done by a template class in Frame.h, depending
//  on whether or not the current line is high resolution.

// ToDo:
//  - change from dirty lines to dirty rectangles, to reduce rendering further
//  - maybe move away from the template class, as it's not as useful anymore

#include "SimCoupe.h"
#include <sys/stat.h>
#include "Frame.h"

#include "Debug.h"
#include "CDrive.h"
#include "Display.h"
#include "GUI.h"
#include "Options.h"
#include "OSD.h"
#include "PNG.h"
#include "Util.h"
#include "UI.h"


// SAM palette colours to use for the floppy drive LED states
const BYTE FLOPPY_LED_ON_COLOUR = GREEN_4;    // Light green floppy light on colour
const BYTE ATOM_LED_ON_COLOUR   = RED_3;      // Red hard disk light colour
const BYTE LED_OFF_COLOUR       = GREY_2;     // Dark grey light off colour
const BYTE UNDRAWN_COLOUR       = GREY_3;     // Mid grey for undrawn screen background

const unsigned int STATUS_ACTIVE_TIME = 2000;   // Time the status text is visible for (in ms)
const unsigned int FPS_IN_TURBO_MODE = 5;       // Number of FPS to limit to in Turbo mode

int s_nViewTop, s_nViewBottom;
int s_nViewLeft, s_nViewRight;
int s_nViewWidth, s_nViewHeight;

CScreen *g_pScreen, *g_pGuiScreen, *g_pLastScreen;
CFrame *g_pFrame, *pFrameLow, *pFrameHigh;

bool fDrawFrame, g_fFlashPhase;
int nFrame;

int nLastLine, nLastBlock;      // Line and block we've drawn up to so far this frame
int nDrawnFrames;               // Frame number in last second and number of those actually drawn

DWORD dwStatusTime;             // Time the status line was made visible

int s_nWidth, s_nHeight;

char szStatus[128], szProfile[128];
char szScreenPath[MAX_PATH];


typedef struct
{
    int w, h;
}
REGION;

REGION asViews[] =
{
    { SCREEN_BLOCKS, SCREEN_LINES },
    { SCREEN_BLOCKS+2, SCREEN_LINES+20 },
    { SCREEN_BLOCKS+4, SCREEN_LINES+48 },
    { SCREEN_BLOCKS+4, SCREEN_LINES+72 },
    { WIDTH_BLOCKS, HEIGHT_LINES },
};

void DrawOSD (CScreen* pScreen_);
void Flip (CScreen*& rpScreen_);

bool Frame::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("-> Frame::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Set the last line and block draw to the start of the display
    nLastLine = nLastBlock = 0;

    int nBorders = min(GetOption(borders), (int)(sizeof asViews / sizeof asViews[0]) - 1);
    s_nViewLeft = (WIDTH_BLOCKS - asViews[nBorders].w) >> 1;
    s_nViewRight = s_nViewLeft + asViews[nBorders].w;

    // If we're not showing the full scan image, offset the view to centre over the main screen area
    if ((s_nViewTop = (HEIGHT_LINES - asViews[nBorders].h) >> 1))
        s_nViewTop += (TOP_BORDER_LINES-BOTTOM_BORDER_LINES) >> 1;
    s_nViewBottom = s_nViewTop + asViews[nBorders].h;

    // Convert the view area dimensions to hi-res pixels
    s_nWidth = (s_nViewRight - s_nViewLeft) << 4;
    s_nHeight = (s_nViewBottom - s_nViewTop) << 1;

    // Create the two screens and two render classes from the template
    if ((g_pScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (g_pLastScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (g_pGuiScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pFrameLow = new CFrameXx1<false>) && (pFrameHigh = new CFrameXx1<true>))
    {
        Start();
        ChangeMode(vmpr);
        fRet = Display::Init(fFirstInit_);
    }
    else
    {
        // Bah!
        Message(msgFatal, "Out of memory!");
        fRet = false;
    }

    if (!fRet)
        Exit();

    TRACE("<- Frame::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void Frame::Exit (bool fReInit_/*=false*/)
{
    Display::Exit(fReInit_);
    TRACE("-> Frame::Exit(%s)\n", fReInit_ ? "reinit" : "");

    delete pFrameLow; pFrameLow = NULL;
    delete pFrameHigh; pFrameHigh = NULL;
    g_pFrame = NULL;

    delete g_pScreen; g_pScreen = NULL;
    delete g_pGuiScreen; g_pGuiScreen = NULL;
    delete g_pLastScreen; g_pLastScreen = NULL;

    TRACE("<- Frame::Exit()\n");
}


CScreen* Frame::GetScreen ()
{
    return g_pScreen;
}

int Frame::GetWidth ()
{
    return g_pScreen->GetPitch();
}

int Frame::GetHeight ()
{
    return g_pScreen->GetHeight();
}


// Update the frame image to the current raster position
void Frame::Update ()
{
    // Don't do anything if the current frame is being skipped
    if (!fDrawFrame)
        return;

    ProfileStart(Gfx);

    // Work out the line and block for the current position
    int nLine = g_nLine, nBlock = g_nLineCycle >> 3;

    // The line-cycle value has not been adjusted at this point, and may need wrapping if it's too large
    if (nBlock >= WIDTH_BLOCKS)
    {
        nBlock -= WIDTH_BLOCKS;
        nLine++;
    }

    // If we're still on the same line as last time we've only got a part line to draw
    if (nLine == nLastLine)
    {
        if (nBlock > nLastBlock)
        {
            g_pFrame->UpdateLine(nLine, nLastBlock, nBlock);
            nLastBlock = nBlock;
        }
    }

    // We've got multiple lines to update
    else
    {
        CFrame* pLast = NULL;

        // Restrict the range of lines to the visible area
        int nFrom = max(nLastLine, s_nViewTop), nTo = min(nLine, s_nViewBottom-1);

        // Is any part of the block visible?
        if (nFrom <= nTo)
        {
            // Finish the rest of the last line updated
            if (nFrom == nLastLine)
            {
                // Finish the line using the current renderer, and exclude it from the draw range
                g_pFrame->UpdateLine(nLastLine, nLastBlock, WIDTH_BLOCKS);
                nFrom++;
            }

            // Update the last line up to the current scan position
            if (nTo == nLine)
            {
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(nLine);
                g_pScreen->SetHiRes(nLine-s_nViewTop, fHiRes);
                (g_pFrame = fHiRes ? pFrameHigh : pFrameLow)->UpdateLine(nLine, 0, nBlock);

                // Exclude the line from the block as we've drawn it now
                nTo--;

                // Save the current render object to leave as set when we're done
                pLast = g_pFrame;
            }

            // Draw any full lines in between
            for (int i = nFrom ; i <= nTo ; i++)
            {
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(i);
                g_pScreen->SetHiRes(i-s_nViewTop, fHiRes);
                (g_pFrame = fHiRes ? pFrameHigh : pFrameLow)->UpdateLine(i, 0, WIDTH_BLOCKS);
            }

            // If the last line drawn was incomplete, restore the rendered used for it
            if (pLast)
                g_pFrame = pLast;
        }

        // Remember the current scan position so we can continue from it next time
        nLastLine = nLine;
        nLastBlock = nBlock;
    }

    ProfileEnd();
}

// Update the full frame image using the current video settings
void Frame::UpdateAll ()
{
    // Keep the current position and last raster settings safe
    int nSafeLastLine = nLastLine, nSafeLastBlock = nLastBlock;
    int nSafeLine = g_nLine, nSafeLineCycle = g_nLineCycle;

    // Set up an update region that covers the entire display
    nLastLine = nLastBlock = 0;
    g_nLine = HEIGHT_LINES;
    g_nLineCycle = WIDTH_BLOCKS;

    // Redraw using the current video/palette/border settings
    Update();

    // Restore the saved settings
    g_nLine = nSafeLine;
    g_nLineCycle = nSafeLineCycle;
    nLastLine = nSafeLastLine;
    nLastBlock = nSafeLastBlock;
}


// Fill the display after current raster position, currently with a dark grey
void RasterComplete ()
{
    static DWORD dwCycleCounter;

    // Don't do anything if the current frame is being skipped or we've already completed the area
    if (!fDrawFrame || (dwCycleCounter == g_dwCycleCounter))
        return;
    else
        dwCycleCounter = g_dwCycleCounter;

    ProfileStart(Gfx);

    // Work out the range that within the visible area
    int nLeft = max(s_nViewLeft, nLastBlock) - s_nViewLeft;
    int nTop = max(nLastLine, s_nViewTop) - s_nViewTop, nBottom = s_nViewBottom - s_nViewTop;

    // If there anything to clear?
    if (nTop <= nBottom)
    {
        // Complete the undrawn section of the current line, if any
        if (nTop == (nLastLine-s_nViewTop))
        {
            bool fHiRes;
            BYTE* pLine = g_pScreen->GetLine(nTop, fHiRes); // Fetch fHiRes

            int nOffset = nLeft << (fHiRes ? 4 : 3), nWidth = Frame::GetWidth() - nOffset;
            if (nWidth > 0)
                memset(pLine + nOffset, UNDRAWN_COLOUR, nWidth);

            nTop++;
        }

        // Fill the remaining lines
        for (int i = nTop ; i < nBottom ; i++)
            memset(g_pScreen->GetLine(i), UNDRAWN_COLOUR, Frame::GetWidth());
    }

    ProfileEnd();
}


// Complete the displayed frame at the end of an emulated frame
void Frame::Complete ()
{
    nFrame++;

    ProfileStart(Gfx);

    // Was the current frame drawn?
    if (fDrawFrame)
    {
        nDrawnFrames++;

        if (!GUI::IsModal())
        {
            if (0)  // ToDo: check debugger option for redraw type
                UpdateAll();
            else
            {
                Update();
                RasterComplete();
            }
        }

        // Save the screen if we have a path
        if (szScreenPath[0])
            SaveFrame(szScreenPath);

        static bool fLastActive = false;
        if (GUI::IsActive())
        {
            // If this is the first time we're showing the GUI, swap back the last complete SAM frame
            if (!fLastActive)
                swap(g_pScreen, g_pLastScreen);

            // Make a double-height copy of the current frame for the GUI to overlay
            for (int i = 0 ; i < GetHeight() ; i++)
            {
                // In scanlines mode we'll fill alternate lines in black
                if ((i & 1) && GetOption(scanlines))
                    memset(g_pGuiScreen->GetLine(i), 0, GetWidth());
                else
                {
                    // Copy the frame data and hi-res status
                    bool fHiRes;
                    memcpy(g_pGuiScreen->GetLine(i), g_pScreen->GetLine(i>>1, fHiRes), GetWidth());
                    g_pGuiScreen->SetHiRes(i, fHiRes);
                }
            }

            // Overlay the GUI over the SAM display
            GUI::Draw(g_pGuiScreen);
            Flip(g_pGuiScreen);
        }
        else
        {
            DrawOSD(g_pScreen);
            Flip(g_pScreen);
        }

        // Redraw what's new
        Redraw();

        fLastActive = GUI::IsActive();
    }

    ProfileEnd();

    // Unless we're fast booting, sync to 50Hz and decide whether we should draw the next frame
    if (!g_nFastBooting)
        Sync();
}

// Start of frame
void Frame::Start ()
{
    // Last drawn position is the start of the frame
    nLastLine = nLastBlock = 0;

    // Set up for drawing with the appropriate render object
    bool fHiRes = (vmpr_mode == MODE_3) && (s_nViewTop >= TOP_BORDER_LINES);
    g_pScreen->SetHiRes(0, fHiRes);
    g_pFrame = fHiRes ? pFrameHigh : pFrameLow;

    // Toggle paper/ink colours every 16 emulated frames for the flash attribute in modes 1 and 2
    static int nFlash = 0;
    if (!(++nFlash % 16))
        g_fFlashPhase = !g_fFlashPhase;

    // If the status line has been visible long enough, hide it
    if (szStatus[0] && ((OSD::GetTime() - dwStatusTime) > STATUS_ACTIVE_TIME))
        szStatus[0] = '\0';
}


void Frame::Sync ()
{
    DWORD dwNow = OSD::GetTime();

    // Fetch the frame number we should be up to, according to the running timer
    int nTicks = OSD::FrameSync(false);

    // Determine whether a disk is currently active
    bool fDiskActive = (pDrive1 && pDrive1->IsActive()) || (pDrive2 && pDrive2->IsActive());

    // Running in Turbo mode?
    if (!GUI::IsActive() && (g_fTurbo || (fDiskActive && GetOption(turboload))))
    {
        static DWORD dwLastFrame;

        // Draw 5 frames a second in turbo mode
        if (fDrawFrame = ((dwNow - dwLastFrame) >= 1000/FPS_IN_TURBO_MODE))
            dwLastFrame = dwNow;
    }
    else
    {
        // Whether the next frame gets draw depends on frame-skip setting...
        // In auto-skip mode we'll draw unless we're behind, but leave some slack at the end of the frame just in case
        fDrawFrame = GetOption(frameskip) ? !(nFrame % GetOption(frameskip)) :
            ((nTicks >= EMULATED_FRAMES_PER_SECOND-2) && (nFrame != nDrawnFrames)) ? (nFrame > nTicks) : (nFrame >= nTicks);

        // Sync if the option is enabled and we're not behind
        ProfileStart(Idle);
        if (GetOption(sync) && (nFrame >= nTicks))
            nTicks = OSD::FrameSync(true);
        ProfileEnd();
    }


    // Show the profiler stats once a second
    if (nTicks >= EMULATED_FRAMES_PER_SECOND)
    {
        // Format the profile string and reset it
        sprintf(szProfile, "%3d%%:%2dfps%s", nFrame * 2, nDrawnFrames, Profile::GetStats());
        TRACE("%s   %d ticks  %d frames  %d drawn\n", szProfile, nTicks, nFrame, nDrawnFrames);
        Profile::Reset();

        // Reset frame counters to wait for the next second
        nFrame = nDrawnFrames = 0;

        // If we've fallen too far behind, don't even attempt to catch up.  This avoids a quick burst of
        // speed after unpausing, which can be a real problem when playing games!
        if ((nTicks - nFrame) > 5)
            OSD::s_nTicks = 0;
        else
            OSD::s_nTicks %= EMULATED_FRAMES_PER_SECOND;
    }
}


// Clear and invalidate the frame buffers
void Frame::Clear ()
{
    // Clear the frame buffers
    g_pScreen->Clear();
    g_pLastScreen->Clear();

    // Mark the full display as dirty so it gets redrawn
    Display::SetDirty();
}


void Frame::Redraw ()
{
    // Draw the last complete frame
    Display::Update(g_pLastScreen);
}


// Flip buffers, so we can start working on the new frame
void Flip (CScreen*& rpScreen_)
{
    ProfileStart(Gfx);

    int nHeight = rpScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);

    DWORD* pdwA = reinterpret_cast<DWORD*>(rpScreen_->GetLine(0));
    DWORD* pdwB = reinterpret_cast<DWORD*>(g_pLastScreen->GetLine(0));
    int nPitchDW = rpScreen_->GetPitch() >> 2;

    bool *pfHiRes = rpScreen_->GetHiRes(), *pfHiResLast = g_pLastScreen->GetHiRes();

    // Work out what has changed since the last frame
    for (int i = 0 ; i < nHeight ; i++)
    {
        // Are the lines different resolutions?
        if (pfHiRes[i] != pfHiResLast[i])
            Display::SetLineDirty(i);
        else
        {
            int nWidth = rpScreen_->GetWidth(i) >> 2;
            DWORD *pA = pdwA, *pB = pdwB;

            // Scan the line width
            for (int j = 0 ; j < nWidth ; j += 4)
            {
                // Check for differences 4 DWORDs at a time
                if ((pA[0] ^ pB[0]) | (pA[1] ^ pB[1]) | (pA[2] ^ pB[2]) | (pA[3] ^ pB[3]))
                {
                    Display::SetLineDirty(i);
                    break;
                }

                pA += 4;
                pB += 4;
            }
        }

        pdwA += nPitchDW;
        pdwB += nPitchDW;
    }

    swap(g_pLastScreen, rpScreen_);

    ProfileEnd();
}


// Draw on-screen display indicators, such as the floppy LEDs and the status text
void DrawOSD (CScreen* pScreen_)
{
    ProfileStart(Gfx);

    int nWidth = pScreen_->GetPitch(), nHeight = pScreen_->GetHeight() >> 1;

    // Drive LEDs enabled?
    if (GetOption(drivelights))
    {
        int nX = (GetOption(fullscreen) && GetOption(ratio5_4)) ? 20 : 2;
        int nY = ((GetOption(drivelights)-1) & 1) ? nHeight-4 : 2;

        // Floppy 1 light
        if (GetOption(drive1))
            pScreen_->FillRect(nX, nY, 14, 2, pDrive1->IsLightOn() ? FLOPPY_LED_ON_COLOUR : LED_OFF_COLOUR);

        // Floppy 2 or Atom drive light
        if (GetOption(drive2))
        {
            BYTE bColour = (GetOption(drive2) == 1) ? FLOPPY_LED_ON_COLOUR : ATOM_LED_ON_COLOUR;
            pScreen_->FillRect(nX + 18, nY, 14, 2, pDrive2->IsLightOn() ? bColour : LED_OFF_COLOUR);
        }
    }

    // We'll use the old font for the simple on-screen text
    CScreen::SetFont(&sOldFont);

    // Show the profiling statistics?
    if (GetOption(profile))
    {
        int nX = nWidth - pScreen_->GetStringWidth(szProfile);

        pScreen_->DrawString(nX-1, 2, szProfile, 0);
        pScreen_->DrawString(nX-2, 1, szProfile, 127);
    }

    // Any active status line?
    if (GetOption(status) && szStatus[0])
    {
        int nX = nWidth - pScreen_->GetStringWidth(szStatus);

        pScreen_->DrawString(nX-1, nHeight-CHAR_HEIGHT-1, szStatus, 0);
        pScreen_->DrawString(nX-2, nHeight-CHAR_HEIGHT-2, szStatus, 127);
    }

    ProfileEnd();
}


// Save the frame image to a file
void Frame::SaveFrame (const char* pcszPath_/*=NULL*/)
{
#ifdef USE_ZLIB
    // If no path is supplied we need to generate a unique one
    if (!pcszPath_)
    {
        struct stat st;
        static int nNext = 0;

        // Get the location to store the image
        char* pszBase = szScreenPath + strlen(strcpy(szScreenPath, OSD::GetFilePath("")));

        // Find the next file in the sequence that doesn't already exist
        do
        {
            sprintf(pszBase, "snap%04d.png", nNext);
            nNext++;
        }
        while (!::stat(szScreenPath, &st));

        // We'll leave the path in the buffer, so Frame::End() can spot it when the next frame is generated
        return;
    }

    // Get the save path
    const char* pcszFile = OSD::GetFilePath("");

    // If the file is in the save path, we can refer to the file by it's name, to avoid a verbose path
    if (!strncmp(szScreenPath, pcszFile, strlen(pcszFile)))
        pcszFile = szScreenPath + strlen(pcszFile);

    FILE* f = fopen(szScreenPath, "wb");
    if (!f)
        Frame::SetStatus("Failed to open %s for writing!", szScreenPath);
    else
    {
        bool fSaved = SaveImage(f, g_pScreen);
        fclose(f);

        // If the save failed, delete the empty image
        if (fSaved)
            Frame::SetStatus("Saved screen to %s", pcszFile);
        else
        {
            Frame::SetStatus("Failed to save screen to %s!", szScreenPath);
            remove(szScreenPath);
        }
    }

#else
    Frame::SetStatus("Save screen not available without zLib", szScreenPath);
#endif  // USE_ZLIB

    // We've finished with the path now, so prevent it being saved again
    szScreenPath[0] = '\0';
}


// Set a status message, which will remain on screen for a few seconds
void Frame::SetStatus (const char *pcszFormat_, ...)
{
    va_list pcvArgs;
    va_start (pcvArgs, pcszFormat_);
    vsprintf(szStatus, pcszFormat_, reinterpret_cast<va_list>(pcvArgs));
    va_end(pcvArgs);

    dwStatusTime = OSD::GetTime();
    TRACE("Status: %s\n", szStatus);
}

////////////////////////////////////////////////////////////////////////////////


// Handle screen mode changes, which need special attention to implement ASIC artefacts caused by an 8 pixel
// block being drawn in a new mode, but using the data already fetched from the display memory for the old mode
void Frame::ChangeMode (BYTE bVal_)
{
    // Is the current line in the main display area?
    if (IsScreenLine(g_nLine))
    {
        // Calculate the position of the change
        int nLine = g_nLine - s_nViewTop, nBlock = g_nLineCycle >> 3;

        // Are we before the right-border?
        if (nBlock < (BORDER_BLOCKS+SCREEN_BLOCKS))
        {
            // Are we switching to mode 3 on a lo-res line?
            if (((bVal_ & VMPR_MODE_MASK) == MODE_3) && !g_pScreen->IsHiRes(nLine))
            {
                // Convert the used part of the line to high resolution, and use the high resolution object
                g_pScreen->GetHiResLine(nLine, nBlock);
                g_pFrame = pFrameHigh;
            }

            // Is the mode changing between 1/2 <-> 3/4 on the main screen?
            if (((vmpr_mode ^ bVal_) & VMPR_MDE1_MASK) && nBlock >= BORDER_BLOCKS)
            {
                // Draw the appropriate ASIC artefact caused by the mode change (discovered by Dave Laundon)
                g_pFrame->ModeChange(bVal_, g_nLine, nBlock);
                nLastBlock += (VIDEO_DELAY >> 3);
            }
        }
    }

    // Update the mode in the rendering objects
    pFrameLow->SetMode(bVal_);
    pFrameHigh->SetMode(bVal_);
}


// A screen line in a specified range is being written to, so we need to ensure it's up-to-date
void Frame::TouchLines (int nFrom_, int nTo_)
{
    // Is the line being modified in the area since we last update
    if (nTo_ >= nLastLine && nFrom_ <= g_nLine)
        Update();
}
