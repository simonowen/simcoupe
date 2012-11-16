// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.cpp: Display frame generation
//
//  Copyright (c) 1999-2012 Simon Owen
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

#include "Audio.h"
#include "AtaAdapter.h"
#include "AVI.h"
#include "Debug.h"
#include "Drive.h"
#include "GIF.h"
#include "GUI.h"
#include "Memory.h"
#include "Options.h"
#include "OSD.h"
#include "PNG.h"
#include "Sound.h"
#include "Util.h"
#include "UI.h"


// SAM palette colours to use for the floppy drive LED states
const BYTE FLOPPY_LED_COLOUR    = GREEN_5;  // Green for floppy
const BYTE ATOM_LED_COLOUR      = RED_6;    // Red for Atom
const BYTE ATOMLITE_LED_COLOUR  = 89;       // Blue for Atom Lite
const BYTE LED_OFF_COLOUR       = GREY_2;   // Grey for off
const BYTE UNDRAWN_COLOUR       = GREY_2;   // Grey for undrawn screen in debugger

const unsigned int STATUS_ACTIVE_TIME = 2500;   // Time the status text is visible for (in ms)
const unsigned int FPS_IN_TURBO_MODE = 5;       // Number of FPS to limit to in (non-key) Turbo mode

int s_nViewTop, s_nViewBottom;
int s_nViewLeft, s_nViewRight;

CScreen *pScreen, *pLastScreen, *pGuiScreen, *pLastGuiScreen, *pDisplayScreen;
CFrame *pFrameLow, *pFrameHigh;

bool fDrawFrame, g_fFlashPhase, fSaveScreen;
int nFrame;

int nLastLine, nLastBlock;      // Line and block we've drawn up to so far this frame

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

static void DrawOSD (CScreen *pScreen_);
static void Flip (CScreen *pScreen_);


bool Frame::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);
    TRACE("Frame::Init(%d)\n", fFirstInit_);

    UINT uView = GetOption(borders);
    if (uView >= (sizeof(asViews) / sizeof(asViews[0])))
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

    // Create two SAM screens and two GUI screens, to allow for double-buffering
    pScreen = new CScreen(s_nWidth, s_nHeight);
    pLastScreen = new CScreen(s_nWidth, s_nHeight);
    pGuiScreen = new CScreen(s_nWidth, s_nHeight);
    pLastGuiScreen = new CScreen(s_nWidth, s_nHeight);

    // Create the low-res and hi-res rendering objects
    pFrameLow = new CFrameXx1<false>;
    pFrameHigh = new CFrameXx1<true>;

    // Check we created everything successfully
    if (!pScreen || !pLastScreen || !pGuiScreen || !pLastGuiScreen || !pFrameLow || !pFrameHigh)
    {
        Message(msgFatal, "Out of memory!");
        return false;
    }

    // Drawn screen is the last (initially blank) screen
    pDisplayScreen = pLastScreen;

    // Set the renderer display modes
    pFrameLow->SetMode(vmpr);
    pFrameHigh->SetMode(vmpr);

    // Prepare for new frame
    Flyback();

    return true;
}

void Frame::Exit (bool fReInit_/*=false*/)
{
    TRACE("Frame::Exit(%d)\n", fReInit_);

    // Stop any recording
    GIF::Stop();
    AVI::Stop();

    delete pFrameLow, pFrameLow = NULL;
    delete pFrameHigh, pFrameHigh = NULL;

    delete pScreen, pScreen = NULL;
    delete pLastScreen, pLastScreen = NULL;
    delete pGuiScreen, pGuiScreen = NULL;
    delete pLastGuiScreen, pLastGuiScreen = NULL;

    pDisplayScreen = NULL;
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
    if (uView >= (sizeof(asViews) / sizeof(asViews[0])))
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

    // Work out the line and block for the current position
    int nLine, nBlock = GetRasterPos(&nLine) >> 3;

    // Determine the renderer for the last updated line
    bool fHiRes = pScreen->IsHiRes(nLastLine-s_nViewTop);
    CFrame *pFrame = fHiRes ? pFrameHigh : pFrameLow;

    // If we're still on the same line as last time we've only got a part line to draw
    if (nLine == nLastLine)
    {
        if (nBlock > nLastBlock)
        {
            pFrame->UpdateLine(pScreen, nLine, nLastBlock, nBlock);
            nLastBlock = nBlock;
        }
    }

    // We've got multiple lines to update
    else
    {
        // Restrict the range of lines to the visible area
        int nFrom = max(nLastLine, s_nViewTop), nTo = min(nLine, s_nViewBottom-1);

        // Is any part of the block visible?
        if (nFrom <= nTo)
        {
            // Finish the rest of the last line updated
            if (nFrom == nLastLine)
            {
                // Finish the line using the current renderer, and exclude it from the draw range
                pFrame->UpdateLine(pScreen, nLastLine, nLastBlock, WIDTH_BLOCKS);
                nFrom++;
            }

            // Update the last line up to the current scan position
            if (nTo == nLine)
            {
                // Default to low-res unless a mode 3 screen line
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(nLine);
                pScreen->SetHiRes(nLine-s_nViewTop, fHiRes);

                // Determine the appropriate renderer and draw the partial line
                CFrame *pFrame = fHiRes ? pFrameHigh : pFrameLow;
                pFrame->UpdateLine(pScreen, nLine, 0, nBlock);

                // Exclude the line from the block as we've drawn it now
                nTo--;
            }

            // Draw any full lines in between
            for (int i = nFrom ; i <= nTo ; i++)
            {
                // Default to low-res unless a mode 3 screen line
                bool fHiRes = (vmpr_mode == MODE_3) && IsScreenLine(i);
                pScreen->SetHiRes(i-s_nViewTop, fHiRes);

                // Determine the appropriate renderer and draw the complete line
                CFrame *pFrame = fHiRes ? pFrameHigh : pFrameLow;
                pFrame->UpdateLine(pScreen, i, 0, WIDTH_BLOCKS);
            }
        }

        // Remember the current scan position so we can continue from it next time
        nLastLine = nLine;
        nLastBlock = nBlock;
    }
}


// Begin the frame by copying from the previous frame, up to the last cange
static void CopyBeforeLastUpdate ()
{
    // Determine the range in the visible area
    int nBottom = min(nLastLine, s_nViewBottom-1) - s_nViewTop;

    // If there anything to copy?
    if (nBottom > 0)
    {
        // Copy the last partial line first
        if (nBottom == (nLastLine-s_nViewTop))
        {
            BYTE* pLine = pScreen->GetHiResLine(nBottom);
            BYTE* pLastLine = pLastScreen->GetHiResLine(nBottom);

            int nRight = max(nLastBlock, s_nViewRight) - s_nViewLeft;
            if (nRight > 0)
                memcpy(pLine, pLastLine, nRight<<4);

            nBottom--;
        }

        // Copy the remaining full lines
        for (int i = 0 ; i <= nBottom ; i++)
        {
            BYTE* pLine = pScreen->GetHiResLine(i);
            BYTE* pLastLine = pLastScreen->GetHiResLine(i);
            memcpy(pLine, pLastLine, pScreen->GetPitch());
        }
    }
}

// Complete the drawn frame after the raster using data from the previous frame
static void CopyAfterRaster ()
{
    // Work out the range that is within the visible area
    int nTop = max(nLastLine, s_nViewTop) - s_nViewTop;
    int nBottom = s_nViewBottom - s_nViewTop;

    // If there anything to copy?
    if (nTop <= nBottom)
    {
        // Complete the undrawn section of the current line, if any
        if (nTop == (nLastLine-s_nViewTop))
        {
            BYTE* pLine = pScreen->GetHiResLine(nTop);
            BYTE* pLastLine = pLastScreen->GetHiResLine(nTop);

            int nOffset = (max(s_nViewLeft, nLastBlock) - s_nViewLeft) << 4;
            int nWidth = pScreen->GetPitch() - nOffset;
            if (nWidth > 0)
                memcpy(pLine+nOffset, pLastLine+nOffset, nWidth);

            nTop++;
        }

        // Copy the remaining lines
        for (int i = nTop ; i < nBottom ; i++)
        {
            BYTE* pLine = pScreen->GetHiResLine(i);
            BYTE* pLastLine = pLastScreen->GetHiResLine(i);
            memcpy(pLine, pLastLine, pScreen->GetPitch());
        }
    }
}


// Begin the frame by copying from the previous frame, up to the last cange
void Frame::Begin ()
{
    // Return if we're skipping this frame
    if (!fDrawFrame)
        return;

    // If we're debugging, copy up to the last-update position from the previous frame
    CopyBeforeLastUpdate();
}

// Complete the displayed frame at the end of an emulated frame
void Frame::End ()
{
    // Was the current frame drawn?
    if (fDrawFrame)
    {
        // Update the screen to the current raster position
        Update();

        // If we're debugging, copy after the raster from the previous frame
        CopyAfterRaster();

        if (GUI::IsActive())
        {
            // Make a double-height copy of the current frame for the GUI to overlay
            for (int i = 0 ; i < GetHeight() ; i++)
            {
                bool fHiRes;

                // Fetch the source line data and its hi-res status
                BYTE *pbLine = pScreen->GetLine(i>>1, fHiRes);
                int nWidth = pScreen->GetPitch() >> (fHiRes?0:1);

                // Copy the frame data and hi-res status
                memcpy(pGuiScreen->GetLine(i), pbLine, nWidth);
                pGuiScreen->SetHiRes(i, fHiRes);
            }

            // Overlay the GUI widgets
            GUI::Draw(pGuiScreen);

            // Submit the completed frame
            Flip(pGuiScreen);
        }
        else
        {
            // Screenshot required?
            if (fSaveScreen)
            {
                PNG::Save(pScreen);
                fSaveScreen = false;
            }

            // Add the frame to any recordings
            GIF::AddFrame(pScreen);
            AVI::AddFrame(pScreen);

            // Overlay the floppy LEDs and status text
            DrawOSD(pScreen);

            // Submit the completed frame
            Flip(pScreen);
        }

        // Redraw what's new
        Redraw();
    }

    // Decide whether we should draw the next frame
    Sync();
}

// Flyback to start drawing new frame
void Frame::Flyback ()
{
    // Last drawn position is the start of the frame
    nLastLine = nLastBlock = 0;

    // Toggle paper/ink colours every 16 emulated frames for the flash attribute in modes 1 and 2
    static int nFlash = 0;
    if (!(++nFlash % 16))
        g_fFlashPhase = !g_fFlashPhase;

    // If the status line has been visible long enough, hide it
    if (szStatus[0] && ((OSD::GetTime() - dwStatusTime) > STATUS_ACTIVE_TIME))
        szStatus[0] = '\0';

    // New frame
    nFrame++;
}


void Frame::Sync ()
{
    static DWORD dwLastProfile, dwLastDrawn;
    DWORD dwNow = OSD::GetTime();

    // Determine whether we're running at increased speed during disk activity
    if (GetOption(turbodisk) && (pFloppy1->IsActive() || pFloppy2->IsActive()))
        g_nTurbo |= TURBO_DISK;
    else
        g_nTurbo &= ~TURBO_DISK;

    // Default to drawing all frames
    fDrawFrame = true;

    // Running in Turbo mode? (but not with turbo key)
    if (!GUI::IsActive() && g_nTurbo && !(g_nTurbo & TURBO_KEY))
    {
        // Should we draw a frame yet?
        fDrawFrame = (dwNow - dwLastDrawn) >= 1000/FPS_IN_TURBO_MODE;

        // If so, remember the time we drew it
        if (fDrawFrame)
            dwLastDrawn = dwNow;
    }

    // Show the profiler stats once a second
    if ((dwNow - dwLastProfile) >= 1000)
    {
        // Calculate the relative speed from the framerate, even though we're sound synced
        int nPercent = nFrame*2;

        // 100% speed is actually 50.08fps, so the 51fps we see every ~12 seconds is still fine
        if (nFrame == 51) nPercent = 100;

        // Format the profile string and reset it
        sprintf(szProfile, "%d%%", nPercent);
        TRACE("%s  %d frames\n", szProfile, nFrame);

        // Adjust for next time, taking care to preserve any fractional part
        dwLastProfile = dwNow - ((dwNow - dwLastProfile) % 1000);

        // Reset frame counter
        nFrame = 0;
    }

    // Throttle the speed when the GUI is active, as the I/O code isn't doing it
    if (GUI::IsActive())
    {
        // Add a frame's worth of silence
        static BYTE abSilence[SAMPLE_FREQ*SAMPLE_BLOCK/EMULATED_FRAMES_PER_SECOND];
        Audio::AddData(abSilence, sizeof(abSilence));
    }
}


void Frame::Redraw ()
{
    // Draw the last complete frame
    Video::Update(pDisplayScreen);
}


// Determine the frame difference from last time and flip buffers
void Flip (CScreen *pScreen_)
{
    int nHeight = pScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);

    DWORD* pdwA = reinterpret_cast<DWORD*>(pScreen_->GetLine(0));
    DWORD* pdwB = reinterpret_cast<DWORD*>(pDisplayScreen->GetLine(0));
    int nPitchDW = pScreen_->GetPitch() >> 2;

    bool *pfHiRes = pScreen_->GetHiRes();
    bool *pfHiResDisplay = pDisplayScreen->GetHiRes();

    // Work out what has changed since the last frame
    for (int i = 0 ; i < nHeight ; i++)
    {
        // Skip lines currently marked as dirty
        if (Video::IsLineDirty(i))
            continue;

        // If they're different resolutions, or have different contents, they're dirty
        if (pfHiRes[i] != pfHiResDisplay[i] || memcmp(pdwA, pdwB, pScreen_->GetWidth(i)))
            Video::SetLineDirty(i);

        pdwA += nPitchDW;
        pdwB += nPitchDW;
    }

    // Remember the last drawn screen, to compare differences next time
    pDisplayScreen = pScreen_;

    // Flip screen buffers
    swap(pScreen, pLastScreen);
    swap(pGuiScreen, pLastGuiScreen);
}


// Draw on-screen display indicators, such as the floppy LEDs and the status text
void DrawOSD (CScreen* pScreen_)
{
    int nWidth = pScreen_->GetPitch(), nHeight = pScreen_->GetHeight() >> 1;

    // Drive LEDs enabled?
    if (GetOption(drivelights))
    {
        int nX = 2;
        int nY = ((GetOption(drivelights)-1) & 1) ? nHeight-4 : 2;

        // Floppy 1 light
        if (GetOption(drive1))
        {
            BYTE bColour = pFloppy1->IsLightOn() ? FLOPPY_LED_COLOUR : LED_OFF_COLOUR;
            pScreen_->FillRect(nX, nY, 14, 2, bColour);
        }

        // Floppy 2 or Atom drive light
        if (GetOption(drive2))
        {
            bool fAtomActive = pAtom->IsActive() || pAtomLite->IsActive();
            BYTE bAtomColour = pAtom->IsActive() ? ATOM_LED_COLOUR : ATOMLITE_LED_COLOUR;

            BYTE bColour = pFloppy2->IsLightOn() ? FLOPPY_LED_COLOUR : (fAtomActive ? bAtomColour : LED_OFF_COLOUR);
            pScreen_->FillRect(nX + 18, nY, 14, 2, bColour);
        }
    }

    // We'll use the fixed font for the simple on-screen text
    pScreen_->SetFont(&sPropFont);

    // Show the profiling statistics?
    if (GetOption(profile) && !GUI::IsActive())
    {
        int nX = nWidth - pScreen_->GetStringWidth(szProfile);

        pScreen_->DrawString(nX,   2, szProfile, BLACK);
        pScreen_->DrawString(nX-2, 1, szProfile, WHITE);
    }

    // Any active status line?
    if (GetOption(status) && szStatus[0])
    {
        int nX = nWidth - pScreen_->GetStringWidth(szStatus);

        pScreen_->DrawString(nX,   nHeight-CHAR_HEIGHT-1, szStatus, BLACK);
        pScreen_->DrawString(nX-2, nHeight-CHAR_HEIGHT-2, szStatus, WHITE);
    }
}

// Screenshot save request
void Frame::SaveScreenshot ()
{
    fSaveScreen = true;
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

// Fetch the current horizontal raster position (in cycles) and the current line
int GetRasterPos (int *pnLine_)
{
    if (g_dwCycleCounter >= BORDER_PIXELS)
    {
        DWORD dwScreenCycles = g_dwCycleCounter - BORDER_PIXELS;
        *pnLine_ = dwScreenCycles / TSTATES_PER_LINE;
        return dwScreenCycles % TSTATES_PER_LINE;
    }

    // FIXME: the very start of the interrupt frame is from the final line of the display
    *pnLine_ = 0;
    return 0;
}

// Fetch the 4 bytes the ASIC uses to generate the next 8-pixel cell
void Frame::GetAsicData (BYTE *pb0_, BYTE *pb1_, BYTE *pb2_, BYTE *pb3_)
{
    int nLine;
    GetRasterPos(&nLine);

    bool fHiRes = pScreen->IsHiRes(nLine-s_nViewTop);
    CFrame *pFrame = fHiRes ? pFrameHigh : pFrameLow;

    pFrame->GetAsicData(pb0_, pb1_, pb2_, pb3_);
}

// Handle screen mode changes, which may require converting low-res data to hi-res
// Changes on the main screen may generate an artefact by using old data in the new mode (described by Dave Laundon)
void Frame::ChangeMode (BYTE bNewVmpr_)
{
    int nLine, nBlock = GetRasterPos(&nLine) >> 3;

    // Action only needs to be taken on main screen lines
    if (IsScreenLine(nLine))
    {
        // Changes into the right border can't affect appearance, so ignore them
        if (nBlock < (BORDER_BLOCKS+SCREEN_BLOCKS))
        {
            // Are we switching to mode 3 on a lo-res line?
            if (((bNewVmpr_ & VMPR_MODE_MASK) == MODE_3) && !pScreen->IsHiRes(nLine-s_nViewTop))
            {
                // Convert the used part of the line to high resolution
                pScreen->GetHiResLine(nLine-s_nViewTop, nBlock);
            }

            // Is the mode changing between 1/2 <-> 3/4 on the main screen?
            if (((vmpr_mode ^ bNewVmpr_) & VMPR_MDE1_MASK) && nBlock >= BORDER_BLOCKS)
            {
                bool fHiRes;
                BYTE *pbLine = pScreen->GetLine(nLine-s_nViewTop, fHiRes);

                // Draw the artefact and advance the draw position
                CFrame *pFrame = fHiRes ? pFrameHigh : pFrameLow;
                pFrame->ModeChange(pbLine, nLine, nBlock, bNewVmpr_);

                nLastBlock += (VIDEO_DELAY >> 3);
            }
        }
    }

    // Update the mode in the rendering objects
    pFrameLow->SetMode(bNewVmpr_);
    pFrameHigh->SetMode(bNewVmpr_);
}


// Handle the screen being enabled, which causes a border pixel artefact (reported by Andrew Collier)
void Frame::ChangeScreen (BYTE bNewBorder_)
{
    int nLine, nBlock = GetRasterPos(&nLine) >> 3;

    // Only draw if the artefact cell is visible
    if (nLine >= s_nViewTop && nLine < s_nViewBottom && nBlock >= s_nViewLeft && nBlock < s_nViewRight)
    {
        // Convert the used part of the line to high resolution
        BYTE *pbLine = pScreen->GetHiResLine(nLine-s_nViewTop, nBlock);

        // Draw the artefact and advance the draw position
        pFrameHigh->ScreenChange(pbLine, nLine, nBlock, bNewBorder_);
        nLastBlock += (VIDEO_DELAY >> 3);
    }
}

// A screen line in a specified range is being written to, so we need to ensure it's up-to-date
void Frame::TouchLines (int nFrom_, int nTo_)
{
    // Is the line being modified in the area since we last updated
    if (nTo_ >= nLastLine && nFrom_ <= (int)((g_dwCycleCounter - BORDER_PIXELS) / TSTATES_PER_LINE))
        Update();
}


// Set a new screen mode (VMPR value)
void CFrame::SetMode (BYTE bNewVmpr_)
{
    static FNLINEUPDATE apfnLineUpdates[] =
        { &CFrame::Mode1Line, &CFrame::Mode2Line, &CFrame::Mode3Line, &CFrame::Mode4Line };

    m_pLineUpdate = apfnLineUpdates[(bNewVmpr_ & VMPR_MODE_MASK) >> 5];

    // Bit 0 of the VMPR page is always taken as zero for modes 3 and 4
    int nPage = (bNewVmpr_ & VMPR_MDE1_MASK) ? (bNewVmpr_ & VMPR_PAGE_MASK & ~1) : bNewVmpr_ & VMPR_PAGE_MASK;
    m_pbScreenData = PageReadPtr(nPage);
}

// Update a line segment of display or border
void CFrame::UpdateLine (CScreen *pScreen_, int nLine_, int nFrom_, int nTo_)
{
    // Is the line within the view port?
    if (nLine_ >= s_nViewTop && nLine_ < s_nViewBottom)
    {
        // Fetch the screen data pointer for the line
        BYTE* pbLine = pScreen_->GetLine(nLine_-s_nViewTop);

        // Screen off in mode 3 or 4?
        if (BORD_SOFF && VMPR_MODE_3_OR_4)
            BlackLine(pbLine, nFrom_, nTo_);

        // Line on the main screen?
        else if (nLine_ >= TOP_BORDER_LINES && nLine_ < (TOP_BORDER_LINES+SCREEN_LINES))
            (this->*m_pLineUpdate)(pbLine, nLine_, nFrom_, nTo_);

        // Top or bottom border
        else// if (nLine_ < TOP_BORDER_LINES || nLine_ >= (TOP_BORDER_LINES+SCREEN_LINES))
            BorderLine(pbLine, nFrom_, nTo_);
    }
}

// Fetch the internal ASIC working values used when drawing the display
void CFrame::GetAsicData (BYTE *pb0_, BYTE *pb1_, BYTE *pb2_, BYTE *pb3_)
{
    int nLine = g_dwCycleCounter / TSTATES_PER_LINE, nBlock = (g_dwCycleCounter % TSTATES_PER_LINE) >> 3;

    nLine -= TOP_BORDER_LINES;
    nBlock -= BORDER_BLOCKS+BORDER_BLOCKS;
    if (nBlock < 0) { nLine--; nBlock = SCREEN_BLOCKS-1; }
    if (nLine < 0 || nLine >= SCREEN_LINES) { nLine = SCREEN_LINES-1; nBlock = SCREEN_BLOCKS-1; }

    if (VMPR_MODE_3_OR_4)
    {
        BYTE* pb = m_pbScreenData + (nLine << 7) + (nBlock << 2);
        *pb0_ = pb[0];
        *pb1_ = pb[1];
        *pb2_ = pb[2];
        *pb3_ = pb[3];
    }
    else
    {
        BYTE* pData = m_pbScreenData + ((VMPR_MODE == MODE_1) ? g_awMode1LineToByte[nLine] + nBlock : (nLine << 5) + nBlock);
        BYTE* pAttr = (VMPR_MODE == MODE_1) ? m_pbScreenData + 6144 + ((nLine & 0xf8) << 2) + nBlock : pData + 0x2000;
        *pb0_ = *pb1_ = *pData;
        *pb2_ = *pb3_ = *pAttr;
    }
}
