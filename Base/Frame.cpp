// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.cpp: Display frame generation
//
//  Copyright (c) 1999-2006  Simon Owen
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

const unsigned int STATUS_ACTIVE_TIME = 2500;   // Time the status text is visible for (in ms)
const unsigned int FPS_IN_TURBO_MODE = 5;       // Number of FPS to limit to in Turbo mode

int s_nViewTop, s_nViewBottom;
int s_nViewLeft, s_nViewRight;
int s_nViewWidth, s_nViewHeight;

CScreen *pScreen, *pGuiScreen, *pLastScreen;
CFrame *pFrame, *pFrameLow, *pFrameHigh;

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
    { SCREEN_BLOCKS+4, SCREEN_LINES+74 },
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

    UINT uView = GetOption(borders);
    if (uView < 0 || uView >= (sizeof asViews / sizeof asViews[0]))
        uView = 0;

    s_nViewLeft = (WIDTH_BLOCKS - asViews[uView].w) >> 1;
    s_nViewRight = s_nViewLeft + asViews[uView].w;

    // If we're not showing the full scan image, offset the view to centre over the main screen area
    if ((s_nViewTop = (HEIGHT_LINES - asViews[uView].h) >> 1))
        s_nViewTop += (TOP_BORDER_LINES-BOTTOM_BORDER_LINES) >> 1;
    s_nViewBottom = s_nViewTop + asViews[uView].h;

    // Convert the view area dimensions to hi-res pixels
    s_nWidth = (s_nViewRight - s_nViewLeft) << 4;
    s_nHeight = (s_nViewBottom - s_nViewTop) << 1;

    // Create the two screens and two render classes from the template
    if ((pScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pLastScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pGuiScreen = new CScreen(s_nWidth, s_nHeight)) &&
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

    TRACE("<- Frame::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void Frame::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Frame::Exit(%s)\n", fReInit_ ? "reinit" : "");
    Display::Exit(fReInit_);

    if (pFrameHigh != pFrameLow)
        delete pFrameHigh;
    delete pFrameLow;
    pFrame = pFrameHigh = pFrameLow = NULL;

    delete pScreen;
    delete pGuiScreen;
    delete pLastScreen;
    pScreen = pGuiScreen = pLastScreen = NULL;

    TRACE("<- Frame::Exit()\n");
}


CScreen* Frame::GetScreen ()
{
    return pScreen;
}

int Frame::GetWidth ()
{
    return pScreen->GetPitch();
}

int Frame::GetHeight ()
{
    return pScreen->GetHeight();
}

void Frame::SetView (UINT uBlocks_, UINT uLines_)
{
    UINT uView = GetOption(borders);
    if (uView < 0 || uView >= (sizeof asViews / sizeof asViews[0]))
        uView = 0;

    // Overwrite the current view with the supplied dimensions
    asViews[uView].w = uBlocks_;
    asViews[uView].h = uLines_;
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
            pFrame->UpdateLine(nLine, nLastBlock, nBlock);
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
                pFrame->UpdateLine(nLastLine, nLastBlock, WIDTH_BLOCKS);
                nFrom++;
            }

            // Update the last line up to the current scan position
            if (nTo == nLine)
            {
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(nLine);
                pScreen->SetHiRes(nLine-s_nViewTop, fHiRes);
                (pFrame = fHiRes ? pFrameHigh : pFrameLow)->UpdateLine(nLine, 0, nBlock);

                // Exclude the line from the block as we've drawn it now
                nTo--;

                // Save the current render object to leave as set when we're done
                pLast = pFrame;
            }

            // Draw any full lines in between
            for (int i = nFrom ; i <= nTo ; i++)
            {
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(i);
                pScreen->SetHiRes(i-s_nViewTop, fHiRes);
                (pFrame = fHiRes ? pFrameHigh : pFrameLow)->UpdateLine(i, 0, WIDTH_BLOCKS);
            }

            // If the last line drawn was incomplete, restore the rendered used for it
            if (pLast)
                pFrame = pLast;
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
    if (dwCycleCounter == g_dwCycleCounter)
        return;
    else
        dwCycleCounter = g_dwCycleCounter;

    ProfileStart(Gfx);

    // If this frame was being skipped, clear the whole buffer and start drawing from now
    if (!fDrawFrame)
    {
        nLastLine = nLastBlock = 0;
        fDrawFrame = true;
    }

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
            BYTE* pLine = pScreen->GetLine(nTop, fHiRes); // Fetch fHiRes

            int nOffset = nLeft << (fHiRes ? 4 : 3), nWidth = Frame::GetWidth() - nOffset;
            if (nWidth > 0)
                memset(pLine + nOffset, UNDRAWN_COLOUR, nWidth);

            nTop++;
        }

        // Fill the remaining lines
        for (int i = nTop ; i < nBottom ; i++)
            memset(pScreen->GetLine(i), UNDRAWN_COLOUR, Frame::GetWidth());
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
            // ToDo: check debugger option for redraw type
            int i = 0;
            if (i)
                UpdateAll();        // Update the entire display using the current video settings
            else
            {
                Update();           // Draw up to the current raster point
                RasterComplete();   // Grey the undefined area after the raster
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
                swap(pScreen, pLastScreen);

            // Make a double-height copy of the current frame for the GUI to overlay
            for (int i = 0 ; i < GetHeight() ; i++)
            {
                // In scanlines mode we'll fill alternate lines in black
                if ((i & 1) && GetOption(scanlines))
                    memset(pGuiScreen->GetLine(i), 0, GetWidth());
                else
                {
                    // Copy the frame data and hi-res status
                    bool fHiRes;
                    memcpy(pGuiScreen->GetLine(i), pScreen->GetLine(i>>1, fHiRes), GetWidth());
                    pGuiScreen->SetHiRes(i, fHiRes);
                }
            }

            // Overlay the GUI over the SAM display
            GUI::Draw(pGuiScreen);
            Flip(pGuiScreen);
        }
        else
        {
            DrawOSD(pScreen);
            Flip(pScreen);
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
    pScreen->SetHiRes(0, fHiRes);
    pFrame = fHiRes ? pFrameHigh : pFrameLow;

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

    // Determine whether we're running at increased speed during disk activity
    bool fTurboDisk = GetOption(turboload) &&
        ((pDrive1 && pDrive1->IsActive() && (pDrive1->GetType() != dskImage || ((CDrive*)pDrive1)->GetDiskType() != dtFloppy)) ||
         (pDrive2 && pDrive2->IsActive() && (pDrive2->GetType() != dskImage || ((CDrive*)pDrive2)->GetDiskType() != dtFloppy)));

    // Running in Turbo mode?
    if (!GUI::IsActive() && (g_fTurbo || fTurboDisk))
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
            ((nTicks >= EMULATED_FRAMES_PER_SECOND-1) && (nFrame != nDrawnFrames)) ? (nFrame > nTicks) : (nFrame >= nTicks);

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
    pScreen->Clear();
    pLastScreen->Clear();

    // Mark the full display as dirty so it gets redrawn
    Display::SetDirty();
}


void Frame::Redraw ()
{
    // Draw the last complete frame
    Display::Update(pLastScreen);
}


// Flip buffers, so we can start working on the new frame
void Flip (CScreen*& rpScreen_)
{
    ProfileStart(Gfx);

    int nHeight = rpScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);

    DWORD* pdwA = reinterpret_cast<DWORD*>(rpScreen_->GetLine(0));
    DWORD* pdwB = reinterpret_cast<DWORD*>(pLastScreen->GetLine(0));
    int nPitchDW = rpScreen_->GetPitch() >> 2;

    bool *pfHiRes = rpScreen_->GetHiRes(), *pfHiResLast = pLastScreen->GetHiRes();

    // Work out what has changed since the last frame
    for (int i = 0 ; i < nHeight ; i++)
    {
        // Skip lines currently marked as dirty
        if (Display::IsLineDirty(i))
            continue;

        // If they're different resolutions, or have different contents, they're dirty
        if (pfHiRes[i] != pfHiResLast[i] || memcmp(pdwA, pdwB, rpScreen_->GetWidth(i)))
            Display::SetLineDirty(i);

        pdwA += nPitchDW;
        pdwB += nPitchDW;
    }

    swap(pLastScreen, rpScreen_);

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

        pScreen_->DrawString(nX-2, 2, szProfile, 0);
        pScreen_->DrawString(nX-4, 1, szProfile, 127);
    }

    // Any active status line?
    if (GetOption(status) && szStatus[0])
    {
        int nX = nWidth - pScreen_->GetStringWidth(szStatus);

        pScreen_->DrawString(nX-2, nHeight-CHAR_HEIGHT-1, szStatus, 0);
        pScreen_->DrawString(nX-4, nHeight-CHAR_HEIGHT-2, szStatus, 127);
    }

    ProfileEnd();
}


// Save the frame image to a file
void Frame::SaveFrame (const char* pcszPath_/*=NULL*/)
{
#ifdef USE_ZLIB
    static int nNext = 0;

    // If no path is supplied we need to generate a unique one
    if (!pcszPath_)
    {
        char szTemplate[MAX_PATH];
        sprintf(szTemplate, "%ssnap%%04d.png", OSD::GetDirPath(GetOption(datapath)));
        nNext = Util::GetUniqueFile(szTemplate, nNext, szScreenPath, sizeof(szScreenPath));

        // Leave the path in the buffer, so Frame::End() can spot it when the next frame is generated
        return;
    }

    FILE* f = fopen(szScreenPath, "wb");
    if (!f)
        Frame::SetStatus("Failed to open %s for writing!", szScreenPath);
    else
    {
        bool fSaved = SaveImage(f, pScreen);
        fclose(f);

        // If the save failed, delete the empty image
        if (fSaved)
            Frame::SetStatus("Saved snap%04d.png", nNext-1);
        else
        {
            Frame::SetStatus("Failed to save screen to %s!", szScreenPath);
            unlink(szScreenPath);
        }
    }

#else
    Frame::SetStatus("Save screen requires zLib", szScreenPath);
#endif  // USE_ZLIB

    // We've finished with the path now, so prevent it being saved again
    szScreenPath[0] = '\0';
}


// Set a status message, which will remain on screen for a few seconds
void Frame::SetStatus (const char *pcszFormat_, ...)
{
    va_list pcvArgs;
    va_start (pcvArgs, pcszFormat_);
    vsprintf(szStatus, pcszFormat_, pcvArgs);
    va_end(pcvArgs);

    dwStatusTime = OSD::GetTime();
    TRACE("Status: %s\n", szStatus);
}

////////////////////////////////////////////////////////////////////////////////


// Handle screen mode changes, which may require converting low-res data to hi-res
// Changes on the main screen may generate an artefact by using old data in the new mode (described by Dave Laundon)
void Frame::ChangeMode (BYTE bVal_)
{
    // Action only needs to be taken on main screen lines
    if (IsScreenLine(g_nLine))
    {
        int nLine = g_nLine - s_nViewTop, nBlock = g_nLineCycle >> 3;

        // Changes into the right border can't affect appearance, so ignore them
        if (nBlock < (BORDER_BLOCKS+SCREEN_BLOCKS))
        {
            // Are we switching to mode 3 on a lo-res line?
            if (((bVal_ & VMPR_MODE_MASK) == MODE_3) && !pScreen->IsHiRes(nLine))
            {
                // Convert the used part of the line to high resolution, and use the high resolution object
                pScreen->GetHiResLine(nLine, nBlock);
                pFrame = pFrameHigh;
            }

            // Is the mode changing between 1/2 <-> 3/4 on the main screen?
            if (((vmpr_mode ^ bVal_) & VMPR_MDE1_MASK) && nBlock >= BORDER_BLOCKS)
            {
                // Draw the artefact and advance the draw position
                pFrame->ModeChange(bVal_, g_nLine, nBlock);
                nLastBlock += (VIDEO_DELAY >> 3);
            }
        }
    }

    // Update the mode in the rendering objects
    pFrameLow->SetMode(bVal_);
    pFrameHigh->SetMode(bVal_);
}


// Handle the screen being enabled, which causes a border pixel artefact (reported by Andrew Collier)
void Frame::ChangeScreen (BYTE bVal_)
{
    int nBlock = g_nLineCycle >> 3;

    // Only draw if the artefact cell is visible
    if (g_nLine >= s_nViewTop && g_nLine < s_nViewBottom && nBlock >= s_nViewLeft && nBlock < s_nViewRight)
    {
        // Convert the used part of the line to high resolution
        pScreen->GetHiResLine(g_nLine - s_nViewTop, nBlock);
        pFrame = pFrameHigh;

        // Draw the artefact and advance the draw position
        pFrame->ScreenChange(bVal_, g_nLine, nBlock);
        nLastBlock += (VIDEO_DELAY >> 3);
    }
}


// A screen line in a specified range is being written to, so we need to ensure it's up-to-date
void Frame::TouchLines (int nFrom_, int nTo_)
{
    // Is the line being modified in the area since we last update
    if (nTo_ >= nLastLine && nFrom_ <= g_nLine)
        Update();
}
