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
int s_nViewWidth, s_nViewHeight;

CScreen *pScreen, *pGuiScreen, *pLastScreen;
CFrame *pFrame, *pFrameLow, *pFrameHigh;

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

static void DrawOSD (CScreen* pScreen_);
static void Flip (CScreen*& rpScreen_);


bool Frame::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("Frame::Init(%d)\n", fFirstInit_);

    // Set the last line and block draw to the start of the display
    nLastLine = nLastBlock = 0;

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

    // Create the two screens and two render classes from the template
    if ((pScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pLastScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pGuiScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pFrameLow = new CFrameXx1<false>) && (pFrameHigh = new CFrameXx1<true>))
    {
        Start();
        ChangeMode(vmpr);
    }
    else
    {
        Message(msgFatal, "Out of memory!");
        fRet = false;
    }

    return fRet;
}

void Frame::Exit (bool fReInit_/*=false*/)
{
    TRACE("Frame::Exit(%d)\n", fReInit_);

    // Stop any recording
    GIF::Stop();
    AVI::Stop();

    if (pFrameHigh != pFrameLow)
        delete pFrameHigh;
    delete pFrameLow;
    pFrame = pFrameHigh = pFrameLow = NULL;

    delete pScreen;
    delete pGuiScreen;
    delete pLastScreen;
    pScreen = pGuiScreen = pLastScreen = NULL;
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

            // If the last line drawn was incomplete, restore the renderer used for it
            if (pLast)
                pFrame = pLast;
        }

        // Remember the current scan position so we can continue from it next time
        nLastLine = nLine;
        nLastBlock = nBlock;
    }
}

// Update the full frame image using the current video settings
void Frame::UpdateAll ()
{
    // Keep the current position and last raster settings safe
    int nSafeLastLine = nLastLine, nSafeLastBlock = nLastBlock;
    DWORD dwSafeCycleCounter = g_dwCycleCounter;

    // Set up an update region that covers the entire display
    nLastLine = nLastBlock = 0;
    g_dwCycleCounter = TSTATES_PER_FRAME;

    // Redraw using the current video/palette/border settings
    Update();

    // Restore the saved settings
    g_dwCycleCounter = dwSafeCycleCounter;
    nLastLine = nSafeLastLine;
    nLastBlock = nSafeLastBlock;
}


// Fill the display after current raster position, currently with a dark grey
static void RasterComplete ()
{
    static DWORD dwCycleCounter;
    static int nLastFrame;

    // Don't do anything if the current frame is being skipped or we've already completed the area
    if (dwCycleCounter == g_dwCycleCounter && nLastFrame == nFrame)
        return;

    dwCycleCounter = g_dwCycleCounter;
    nLastFrame = nFrame;

    // If this frame was being skipped, clear the whole buffer and start drawing from now
    if (!fDrawFrame)
    {
        nLastLine = nLastBlock = 0;
        fDrawFrame = true;
    }

    // Work out the range that is within the visible area
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
}


// Complete the displayed frame at the end of an emulated frame
void Frame::Complete ()
{
    nFrame++;

    // Was the current frame drawn?
    if (fDrawFrame)
    {
        if (!GUI::IsModal())
        {
/*
            // ToDo: check debugger option for redraw type
            int i = 0;
            if (i)
                UpdateAll();        // Update the entire display using the current video settings
            else
*/
            {
                Update();           // Draw up to the current raster point
                RasterComplete();   // Grey the undefined area after the raster
            }
        }

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
            // Screenshot required?
            if (fSaveScreen)
            {
                PNG::Save(pScreen);
                fSaveScreen = false;
            }

            // Add the frame to any recordings
            GIF::AddFrame(pScreen);
            AVI::AddFrame(pScreen);

            DrawOSD(pScreen);
            Flip(pScreen);
        }

        // Redraw what's new
        Redraw();

        fLastActive = GUI::IsActive();
    }

    // Decide whether we should draw the next frame
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

    // Slow us down when the GUI is active
    if (GUI::IsActive())
    {
        // Add a frame's worth of silence
        static BYTE abSilence[SAMPLE_FREQ*SAMPLE_BLOCK/EMULATED_FRAMES_PER_SECOND];
        Audio::AddData(abSilence, sizeof(abSilence));
    }
}


// Clear and invalidate the frame buffers
void Frame::Clear ()
{
    // Clear the frame buffers
    pScreen->Clear();
    pLastScreen->Clear();

    // Mark the full display as dirty so it gets redrawn
    Video::SetDirty();
}


void Frame::Redraw ()
{
    // Draw the last complete frame
    Video::Update(pLastScreen);
}


// Flip buffers, so we can start working on the new frame
void Flip (CScreen*& rpScreen_)
{
    int nHeight = rpScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);

    DWORD* pdwA = reinterpret_cast<DWORD*>(rpScreen_->GetLine(0));
    DWORD* pdwB = reinterpret_cast<DWORD*>(pLastScreen->GetLine(0));
    int nPitchDW = rpScreen_->GetPitch() >> 2;

    bool *pfHiRes = rpScreen_->GetHiRes(), *pfHiResLast = pLastScreen->GetHiRes();

    // Work out what has changed since the last frame
    for (int i = 0 ; i < nHeight ; i++)
    {
        // Skip lines currently marked as dirty
        if (Video::IsLineDirty(i))
            continue;

        // If they're different resolutions, or have different contents, they're dirty
        if (pfHiRes[i] != pfHiResLast[i] || memcmp(pdwA, pdwB, rpScreen_->GetWidth(i)))
            Video::SetLineDirty(i);

        pdwA += nPitchDW;
        pdwB += nPitchDW;
    }

    swap(pLastScreen, rpScreen_);
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
    if (GetOption(profile))
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

// Save the frame image to a file
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
    pFrame->GetAsicData(pb0_, pb1_, pb2_, pb3_);
}

// Handle screen mode changes, which may require converting low-res data to hi-res
// Changes on the main screen may generate an artefact by using old data in the new mode (described by Dave Laundon)
void Frame::ChangeMode (BYTE bVal_)
{
    int nLine, nBlock = GetRasterPos(&nLine) >> 3;

    // Action only needs to be taken on main screen lines
    if (IsScreenLine(nLine))
    {
        // Changes into the right border can't affect appearance, so ignore them
        if (nBlock < (BORDER_BLOCKS+SCREEN_BLOCKS))
        {
            // Are we switching to mode 3 on a lo-res line?
            if (((bVal_ & VMPR_MODE_MASK) == MODE_3) && !pScreen->IsHiRes(nLine-s_nViewTop))
            {
                // Convert the used part of the line to high resolution, and use the high resolution object
                pScreen->GetHiResLine(nLine-s_nViewTop, nBlock);
                pFrame = pFrameHigh;
            }

            // Is the mode changing between 1/2 <-> 3/4 on the main screen?
            if (((vmpr_mode ^ bVal_) & VMPR_MDE1_MASK) && nBlock >= BORDER_BLOCKS)
            {
                // Draw the artefact and advance the draw position
                pFrame->ModeChange(bVal_, nLine, nBlock);
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
    int nLine, nBlock = GetRasterPos(&nLine) >> 3;

    // Only draw if the artefact cell is visible
    if (nLine >= s_nViewTop && nLine < s_nViewBottom && nBlock >= s_nViewLeft && nBlock < s_nViewRight)
    {
        // Convert the used part of the line to high resolution
        pScreen->GetHiResLine(nLine - s_nViewTop, nBlock);
        pFrame = pFrameHigh;

        // Draw the artefact and advance the draw position
        pFrame->ScreenChange(bVal_, nLine, nBlock);
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
void CFrame::SetMode (BYTE bVal_)
{
    static FNLINEUPDATE apfnLineUpdates[] =
        { &CFrame::Mode1Line, &CFrame::Mode2Line, &CFrame::Mode3Line, &CFrame::Mode4Line };

    m_pLineUpdate = apfnLineUpdates[(bVal_ & VMPR_MODE_MASK) >> 5];

    // Bit 0 of the VMPR page is always taken as zero for modes 3 and 4
    int nPage = (bVal_ & VMPR_MDE1_MASK) ? (bVal_ & VMPR_PAGE_MASK & ~1) : bVal_ & VMPR_PAGE_MASK;
    m_pbScreenData = PageReadPtr(nPage);
}

// Update a line segment of display or border
void CFrame::UpdateLine (int nLine_, int nFrom_, int nTo_)
{
    // Is the line within the view port?
    if (nLine_ >= s_nViewTop && nLine_ < s_nViewBottom)
    {
        // Screen off in mode 3 or 4?
        if (BORD_SOFF && VMPR_MODE_3_OR_4)
            BlackLine(nLine_, nFrom_, nTo_);

        // Line on the main screen?
        else if (nLine_ >= TOP_BORDER_LINES && nLine_ < (TOP_BORDER_LINES+SCREEN_LINES))
            (this->*m_pLineUpdate)(nLine_, nFrom_, nTo_);

        // Top or bottom border
        else// if (nLine_ < TOP_BORDER_LINES || nLine_ >= (TOP_BORDER_LINES+SCREEN_LINES))
            BorderLine(nLine_, nFrom_, nTo_);
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
