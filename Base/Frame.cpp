// Part of SimCoupe - A SAM Coupé emulator
//
// Frame.cpp: Display frame generation
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

// Notes:
//  This module generates a display-independant representation of a single
//  TV frame in a CScreen object.  The platform-specific conversion to the
//  native display format is done in Display.cpp
//
//  The actual drawing work is done by a template class in Frame.h, depending
//  on whether or not the current line is high resolution.

// ToDo:
//  - change from dirty lines to dirty rectangles, to reduce rendering further
//  - catch-up flips for partially drawn screens (for when using debugger)
//  - maybe move away from the template class, as it's not as useful anymore

#include "SimCoupe.h"
#include <sys/stat.h>
#include "Frame.h"

#include "Debug.h"
#include "CDrive.h"
#include "Display.h"
#include "Options.h"
#include "OSD.h"
#include "PNG.h"
#include "Util.h"
#include "UI.h"


// SAM palette colours to use for the floppy drive LED states
const BYTE LED_OFF_COLOUR       = 8;    // Dark grey light off colour
const BYTE FLOPPY_LED_ON_COLOUR = 70;   // Light green floppy light on colour (or 98 for amber :-)
const BYTE ATOM_LED_ON_COLOUR   = 32;   // Red hard disk light colour

const unsigned int STATUS_ACTIVE_TIME = 1500;   // Time the status text is visible for (in ms)

int s_nViewTop, s_nViewBottom;
int s_nViewLeft, s_nViewRight;
int s_nViewWidth, s_nViewHeight;


namespace Frame
{
CScreen *g_pScreen, *g_pLastScreen;
CFrame *g_pFrame, *pFrameLow, *pFrameHigh;

bool fDrawFrame;
int nFrame;

int nLastLine, nLastBlock;      // Line and block we've drawn up to so far this frame
int nDrawnFrames;               // Frame number in last second and number of those actually drawn

DWORD dwProfileTime;            // Time the profiler stats were updated
DWORD dwStatusTime;             // Time the status line was made visible

int s_nWidth, s_nHeight;

char szStatus[128], szProfile[128];
char szScreenPath[MAX_PATH];


AREA asViews[] =
{
    { BORDER_BLOCKS, BORDER_BLOCKS+SCREEN_BLOCKS, TOP_BORDER_LINES, TOP_BORDER_LINES+SCREEN_LINES },
    { BORDER_BLOCKS-1, BORDER_BLOCKS+SCREEN_BLOCKS+1, TOP_BORDER_LINES-10, TOP_BORDER_LINES+SCREEN_LINES+10 },
    { BORDER_BLOCKS-2, BORDER_BLOCKS+SCREEN_BLOCKS+2, TOP_BORDER_LINES-24, TOP_BORDER_LINES+SCREEN_LINES+24 },
    { BORDER_BLOCKS-2, BORDER_BLOCKS+SCREEN_BLOCKS+2, TOP_BORDER_LINES-36, TOP_BORDER_LINES+SCREEN_LINES+36 },
    { 0, WIDTH_BLOCKS, 0, HEIGHT_LINES },
};

void DrawOSD ();

const AREA* GetViewArea ()
{
    int nBorders = min(GetOption(borders), (int)(sizeof asViews / sizeof asViews[0]) - 1);
    return &asViews[nBorders];
}


bool Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("-> Frame::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Set the last line and block draw to the start of the display
    nLastLine = nLastBlock = 0;


    const AREA* pView = GetViewArea();

    s_nViewLeft = pView->left;
    s_nViewRight = pView->right;
    s_nViewTop = pView->top;
    s_nViewBottom = pView->bottom;

    s_nWidth = (s_nViewRight - s_nViewLeft) << 4;       // convert from blocks to hi-res pixels
    s_nHeight = s_nViewBottom - s_nViewTop;

    // Create the two screens and two render classes from the template
    if ((g_pScreen = new CScreen(s_nWidth, s_nHeight)) && (g_pLastScreen = new CScreen(s_nWidth, s_nHeight)) &&
        (pFrameLow = new CFrameXx1<false>) && (pFrameHigh = new CFrameXx1<true>))
    {
        // Make sure the mode we're drawing reflects the current mode
        g_pFrame = pFrameLow;
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

void Exit (bool fReInit_/*=false*/)
{
    Display::Exit(fReInit_);
    TRACE("-> Frame::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pFrameLow)  { delete pFrameLow; pFrameLow = NULL; }
    if (pFrameHigh) { delete pFrameHigh; pFrameHigh = NULL; }
    g_pFrame = NULL;

    if (g_pScreen)  { delete g_pScreen; g_pScreen = NULL; }
    if (g_pLastScreen){ delete g_pLastScreen; g_pLastScreen = NULL; }

    TRACE("<- Frame::Exit()\n");
}


CScreen* GetScreen ()
{
    return g_pScreen;
}


// Update the frame image to the current raster position
void Update ()
{
    // Don't do anything if the current frame is being skipped
    if (!fDrawFrame)
        return;

    ProfileStart(Gfx);

    // Work out the line and block for the current position
    int nLine = g_nLine, nBlock = g_nLineCycle >> 3;
    // Should actually be as below, but the above will have the same effect when it matters
//  int nLine = g_nLine, nBlock = (g_nLineCycle + 1 - VIDEO_DELAY) >> 3;

    // The line-cycle value has not been adjusted at this point, and may need wrapping if it's too large
    if (nBlock >= WIDTH_BLOCKS)
    {
        nBlock -= WIDTH_BLOCKS;
        nLine++;
    }

    // If we're still on the same line as last time we've only got one section to draw
    if (nLine == nLastLine)
    {
        g_pFrame->UpdateLine(nLine, nLastBlock, nBlock);
        nLastBlock = nBlock;
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


// Start of frame
void Start ()
{
    if (!fDrawFrame)
        return;

    nLastLine = nLastBlock = 0;


    bool fHiRes = (vmpr_mode == MODE_3) && (s_nViewTop >= TOP_BORDER_LINES);
    g_pScreen->SetHiRes(0, fHiRes);
    g_pFrame = fHiRes ? pFrameHigh : pFrameLow;
}

// End of frame
void End ()
{
    DWORD dwNow = OSD::GetTime();

    // Was the current frame drawn?
    if (fDrawFrame)
    {
        nDrawnFrames++;

        // Update to the end of the frame image
        Update();

        // If the status line has been visible long enough, hide it
        if (szStatus[0] && ((dwNow - dwStatusTime) > STATUS_ACTIVE_TIME))
            szStatus[0] = '\0';

        // Save the screen if we have a path
        if (szScreenPath[0])
            SaveFrame(szScreenPath);

        // Draw the on-screen indicators, flip buffer, and display the completed frame
        DrawOSD();
        Flip();
        Redraw();
    }

    // Next frame
    nFrame++;


    // Turbo mode shows only 1 frame 50
    if (GetOption(turbo))
    {
        static DWORD dwLastFrame;

        // Draw 5 frames a second in turbo mode
        if (fDrawFrame = ((dwNow - dwLastFrame) >= 200))
            dwLastFrame = dwNow;
    }
    else
    {
        // Whether the next frame gets draw depends on the turbo and frame-skip settings
        fDrawFrame = (!nDrawnFrames || (GetOption(frameskip) ? !(nFrame % GetOption(frameskip)) : OSD::FrameSync(false)));

        // Frame sync if required
        ProfileStart(Idle);
        if (GetOption(sync))
            OSD::FrameSync();
        ProfileEnd();
    }

    // How long since the last profile update?
    DWORD dwDiff = OSD::GetTime() - dwProfileTime;

    // Are we as close to 1 seconds worth as we'll be?
    if (dwDiff >= (1000 - (1000 / EMULATED_FRAMES_PER_SECOND / 2)))
    {
        // Format the profile string and reset it
        sprintf(szProfile, "%3d%%:%2dfps%s", nFrame * 2, nDrawnFrames, Profile::GetStats());
        TRACE("%s\n", szProfile);
        Profile::Reset();
        dwProfileTime = OSD::GetTime();

        // Reset frame counters to wait for the next second
        nFrame = nDrawnFrames = 0;
    }
}


// Clear and invalidate the frame buffers
void Clear ()
{
    // Clear the frame buffers
    g_pScreen->Clear();
    g_pLastScreen->Clear();

    // Mark the full display as dirty so it gets redrawn
    Display::SetDirty();
}


void Redraw ()
{
    // Draw the last complete frame
    Display::Update(g_pLastScreen);
}


// Flip buffers, so we can start working on the new frame
void Flip ()
{
    ProfileStart(Gfx);

    int nHeight = g_pScreen->GetHeight();

    DWORD* pdwA = reinterpret_cast<DWORD*>(g_pScreen->GetLine(0));
    DWORD* pdwB = reinterpret_cast<DWORD*>(g_pLastScreen->GetLine(0));
    int nPitchDW = g_pScreen->GetPitch() >> 2;


    bool *pfHiRes = g_pScreen->GetHiRes(), *pfHiResLast = g_pLastScreen->GetHiRes();


    // Time to work out what changed since the last frame
    for (int i = 0 ; i < nHeight ; i++)
    {
        // Are the lines different resolutions?
        if (pfHiRes[i] != pfHiResLast[i])
            Display::SetDirty(i);
        else
        {
            int nWidth = g_pScreen->GetWidth(i) >> 2;
            DWORD *pA = pdwA, *pB = pdwB;

            // Scan the line width
            for (int j = 0 ; j < nWidth ; j += 4)
            {
                // Check for differences 4 DWORDs at a time
                if ((pA[0] ^ pB[0]) | (pA[1] ^ pB[1]) | (pA[2] ^ pB[2]) | (pA[3] ^ pB[3]))
                {
                    Display::SetDirty(i);
                    break;
                }

                pA += 4;
                pB += 4;
            }
        }

        pdwA += nPitchDW;
        pdwB += nPitchDW;
    }

    // Swap pointers to the current screen becomes the last screen
    swap(g_pScreen, g_pLastScreen);

    ProfileEnd();
}


// Draw on-screen display indicators, such as the floppy LEDs and the status text
void DrawOSD ()
{
    ProfileStart(Gfx);

    // Drive LEDs enabled?
    if (GetOption(drivelights))
    {
        int nX = (GetOption(fullscreen) && GetOption(ratio5_4)) ? 20 : 4, nY = ((GetOption(drivelights)-1) & 1) ? -2 : 2;

        // Floppy 1 light
        if (GetOption(drive1))
            g_pScreen->FillRect(nX, nY, 14, 2, pDrive1->IsLightOn() ? FLOPPY_LED_ON_COLOUR : LED_OFF_COLOUR);

        // Floppy 2 or Atom drive light
        if (GetOption(drive2))
        {
            BYTE bColour = (GetOption(drive2) == 1) ? FLOPPY_LED_ON_COLOUR : ATOM_LED_ON_COLOUR;
            g_pScreen->FillRect(nX + 18, nY, 14, 2, pDrive2->IsLightOn() ? bColour : LED_OFF_COLOUR);
        }
    }

    // Show the profiling statistics?
    if (GetOption(profile))
    {
        g_pScreen->DrawString(-1, 2, szProfile, 0);
        g_pScreen->DrawString(-2, 1, szProfile, 127);
    }

    // Any active status line?
    if (GetOption(status) && szStatus[0])
    {
        g_pScreen->DrawString(-1, -1, szStatus, 0);
        g_pScreen->DrawString(-2, -2, szStatus, 127);
    }

    // Debugger enabled?
    if (g_fDebugging)
        Debug::Display(g_pScreen);

    ProfileEnd();
}


// Save the frame image to a file
void SaveFrame (const char* pcszPath_/*=NULL*/)
{
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
    }

    // Save the latest screen image to the specified file
    else
    {
        // Get the save path
        const char* pcszFile = OSD::GetFilePath("");

        // If the file is in the save path, we can refer to the file by it's name, to avoid a verbose path
        if (!strncmp(szScreenPath, pcszFile, strlen(pcszFile)))
            pcszFile = szScreenPath + strlen(pcszFile);


        FILE* f = fopen(szScreenPath, "wb");
        if (!f)
            SetStatus("Failed to open %s for writing!", szScreenPath);
        else
        {
            bool fSaved = SaveImage(f, g_pScreen);
            fclose(f);

            // If the save failed, delete the empty image
            if (fSaved)
                SetStatus("Saved screen to %s", pcszFile);
            else
            {
                SetStatus("Failed to save screen to %s!", szScreenPath);
                remove(szScreenPath);
            }
        }

        // We've finished with the path now, so prevent it being saved again
        szScreenPath[0] = '\0';
    }
}


// Set a status message, which will remain on screen for a few seconds
void SetStatus (const char *pcszFormat_, ...)
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
void ChangeMode (BYTE bVal_)
{
    // Is the current line in the main display area?
    if (IsScreenLine(g_nLine))
    {
        // Calculate the position of the change
        int nLine = g_nLine - s_nViewTop, nBlock = g_nLineCycle >> 3;
        // Should actually be as below, but the above will have the same effect when it matters
//      int nLine = g_nLine - s_nViewTop, nBlock = (g_nLineCycle + 1 - VIDEO_DELAY) >> 3;

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
void TouchLines (int nFrom_, int nTo_)
{
    // Is the line being modified in the area since we last update
    if (nTo_ >= nLastLine && nFrom_ <= g_nLine)
        Update();
}

}   // namespace Frame
