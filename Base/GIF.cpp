// Part of SimCoupe - A SAM Coupe emulator
//
// GIF.cpp: GIF animation recording
//
//  Copyright (c) 1999-2014 Simon Owen
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

// BitPacker and GifCompressor classes:
// Based on code by: Christoph Hohmann (http://members.aol.com/rf21exe/gif.htm)
// Who based his code on code by: Michael A. Mayer
// Who apparently based his code on code by: Bob Montgomery circa 1988

#include "SimCoupe.h"
#include "GIF.h"

#include "Frame.h"
#include "Options.h"

static BYTE *pbCurr, *pbFirst, *pbSub;

static char szPath[MAX_PATH], *pszFile;
static FILE *f;

static int nDelay = 0;
static long lDelayOffset;
static int wl, wt, ww, wh;	// left/top/width/height for change rect
static int nFrameSkip = 3;	// 50/2 = 25fps (FF/Chrome/Safari/Opera), 50/3 = 16.6fps (IE grrr!)

enum LoopState { kNone, kIgnoreFirstChange, kWaitLoopStart, kLoopStarted };
static LoopState nLoopState;

#define COLOUR_DEPTH	7	// 128 SAM colours

////////////////////////////////////////////////////////////////////////////////

static void WriteLogicalScreenDescriptor (CScreen *pScreen_)
{
    WORD w = pScreen_->GetPitch()/2, h = pScreen_->GetHeight()/2;

    fputc(w & 0xff, f);
    fputc(w >> 8, f);
    fputc(h & 0xff, f);
    fputc(h >> 8, f);

    fputc(0xf0 | (0x7&(COLOUR_DEPTH-1)) , f);
    fputc(0x00 , f);	// Background colour index
    fputc(GetOption(ratio5_4) ? 0x41 : 0x00 , f);	// Pixel Aspect Ratio
}

static void WriteGlobalColourTable ()
{
    const COLOUR *pcPal = IO::GetPalette();

    for (int i = 0 ; i < N_PALETTE_COLOURS ; i++, pcPal++)
    {
        fputc(pcPal->bRed , f);
        fputc(pcPal->bGreen , f);
        fputc(pcPal->bBlue , f);
    }
}

static void WriteImageDescriptor (WORD wLeft_, WORD wTop_, WORD wWidth_, WORD wHeight_)
{
    fputc(0x2c,f);				// Image separator character = ','

    fputc(wLeft_ & 0xff, f);	// left
    fputc(wLeft_ >> 8, f);
    fputc(wTop_ & 0xff, f);		// top
    fputc(wTop_ >> 8, f);
    fputc(wWidth_ & 0xff, f);	// width
    fputc(wWidth_ >> 8, f);
    fputc(wHeight_ & 0xff, f);	// height
    fputc(wHeight_ >> 8, f);

    fputc(0x00|(0x7 & (COLOUR_DEPTH-1)),f); // information on the local colour table
}

static void WriteGraphicControlExtension (WORD wDelay_, BYTE bTransIdx_)
{
    fputc(0x21, f);		// GIF extension code
    fputc(0xf9, f);		// graphic control label
    fputc(0x04, f);		// data length

    BYTE bFlags = 0;
    bFlags |= (1 << 2);
    if (bTransIdx_ != 0xff) bFlags |= 1;
    fputc(bFlags, f);	// Bits 7-5: reserved
                        // Bits 4-2: disposal method (0=none, 1=leave, 2=restore bkg, 3=restore prev)
                        // Bit 1: user input field
                        // Bit 0: transparent colour flag

    fputc(wDelay_ & 0xff, f);	// delay time (ms)
    fputc(wDelay_ >> 8, f);

    fputc((bFlags & 1) ? bTransIdx_ : 0x00, f);	// Transparent colour index, or 0 if unused
    fputc(0x00, f);		// Data sub-block terminator
}

static bool WriteGraphicControlExtensionDelay (long lOffset_, WORD wDelay_)
{
    // Save current position
    long lOldPos = ftell(f);

    // Seek to delay offset
    if (lDelayOffset < 0 || fseek(f, lDelayOffset, SEEK_SET) != 0)
        return false;

    // Write delay time (in 1/100ths second)
    fputc(wDelay_ & 0xff, f);
    fputc(wDelay_ >> 8, f);

    // Return to original position
    if (lOldPos < 0 || fseek(f, lOldPos, SEEK_SET) != 0)
        return false;

    return true;
}

static void WriteNetscapeLoopExtension ()
{
    WORD loops = 0;		// infinite

    fputc(0x21, f);		// GIF Extension code
    fputc(0xff, f);		// Application Extension Label
    fputc(0x0b, f);		// Length of Application Block (eleven bytes of data to follow)

    fwrite("NETSCAPE2.0", 11, 1, f);

    fputc(0x03, f);		//Length of Data Sub-Block (three bytes of data to follow)
    fputc(0x01, f);
    fputc(loops & 0xff, f);		// 2-byte loop iteration count
    fputc(loops >> 8, f);
    fputc(0x00, f);		// Data sub-block terminator
}

static void WriteFileTerminator ()
{
    fputc(';', f);
}


// Compare our copy of the screen with the new display contents
static bool GetChangeRect (BYTE *pb_, CScreen *pScreen_)
{
    bool fHiRes;
    int l,t,r,b, w,h;
    l = t = r = b = 0;

    WORD width = pScreen_->GetPitch()/2, height = pScreen_->GetHeight()/2;
    BYTE *pbC = pb_;

    // Search down for the top-most change
    for (h = 0 ; h < height ; h++)
    {
        BYTE *pb = pScreen_->GetLine(h, fHiRes);
        int step = fHiRes ? 2 : 1;

        // Scan the full width of the current line
        for (w = 0 ; w < width ; w++, pbC++, pb += step)
        {
            if (*pbC != *pb)
            {
                // We've found a single pixel in the change rectangle
                // The top position is the only known valid side so far
                l = r = w;
                t = b = h;
                goto found_top;
            }
        }
    }

    // No changes found?
    if (h == height)
        return false;

found_top:
    pbC = pb_ + width*height - 1;

    // Search up for the bottom-most change
    for (h = height-1 ; h >= t ; h--)
    {
        BYTE *pb = pScreen_->GetLine(h, fHiRes);
        int step = fHiRes ? 2 : 1;
        pb += (width-1)*step;

        // Scan the full width of the line, right to left
        for (w = width-1 ; w >= 0 ; w--, pbC--, pb -= step)
        {
            if (*pbC != *pb)
            {
                // We've now found the bottom of the rectangle
                b = h;

                // Update left/right extents if the change position helps us
                if (w < l) l = w;
                if (w > r) r = w;
                goto found_bottom;
            }
        }
    }

found_bottom:
    pbC = pb_ + width*t;

    // Scan within the inclusive vertical extents of the change rect
    for (h = t ; h <= b ; h++, pbC += width)
    {
        BYTE *pb = pScreen_->GetLine(h, fHiRes);
        int step = fHiRes ? 2 : 1;

        // Scan the unknown left strip
        for (w = 0 ; w < l ; w++)
        {
            if (pbC[w] != pb[w*step])
            {
                // Reduce the left edge to the change point
                if (w < l) l = w;
                break;
            }
        }

        // Scan the unknown right strip
        for (w = width-1 ; w > r ; w--)
        {
            if (pbC[w] != pb[w*step])
            {
                // Increase the right edge to the change point
                if (w > r) r = w;
                break;
            }
        }
    }

//  TRACE("RECT: l=%u t=%u r=%u b=%u\n", l, t, r, b);
    wl = l;
    wt = t;
    ww = r-l+1;
    wh = b-t+1;

    return true;
}

// Update current image and determine sub-region difference to encode
static BYTE UpdateImage (BYTE *pb_, CScreen *pScreen_)
{
    WORD width = pScreen_->GetPitch()/2;
    BYTE abUsed[1<<COLOUR_DEPTH] = {0};
    BYTE *pbSub_ = pbSub;

    // Move to top-left of sub-image
    BYTE *pb = pb_ + (wt*width) + wl;

    BYTE bColour = *pb;
    int nRun = 0, nTrans = 0;

    for (int y = wt ; y < wt+wh ; y++, pb += width)
    {
        bool fHiRes;
        BYTE *pbScr = pScreen_->GetLine(y, fHiRes);
        int step = fHiRes ? 2 : 1;
        pbScr += (wl*step);

        for (int x = 0 ; x < ww ; x++, pbScr += step)
        {
            BYTE bOld = pb[x], bNew = *pbScr;
            pb[x] = bNew;
            abUsed[bNew] = 1;
#if 0
            *pbSub_++ = bNew; // force full redraw for testing
#else
            bool fMatch = (bNew == bColour);
            bool fTrans = (bNew == bOld);

            // End of a colour run?
            if (!fMatch && nRun > nTrans)
            {
//              TRACE("colour run of %d * %02X\n", nRun, bColour);
                memset(pbSub_, bColour, nRun);
                pbSub_ += nRun;
            }
            // End of a transparency run, or colour/transparent run of equal size?
            else if (!fTrans && (nTrans > nRun || (!fMatch && nRun)))
            {
//              TRACE("trans run of %d\n", nTrans);
                memset(pbSub_, 0xff, nTrans);
                pbSub_ += nTrans;
            }
            // Continuing an existing run of either type?
            else if (fMatch || fTrans)
            {
                if (fMatch) nRun++;
                if (fTrans) nTrans++;
                continue;
            }

            // Start a new run with this pixel
            bColour = bNew;
            nRun = 1;
            nTrans = fTrans ? 1 : 0;
#endif
        }
    }

    // Complete the final colour run, if larger
    if (nRun > nTrans)
    {
//      TRACE("final run of %d x %02X\n", nRun, bColour);
        memset(pbSub_, bColour, nRun);
        pbSub_ += nRun;
    }
    // or the final transparent run
    else if (nTrans)
    {
//      TRACE("final trans of %d\n", nTrans);
        memset(pbSub_, 0xff, nTrans);
        pbSub_ += nTrans;
    }

    BYTE bTrans;
    for (bTrans = 0 ; bTrans < (1<<COLOUR_DEPTH) && abUsed[bTrans] ; bTrans++);

    // Have we found a free palette position for transparency?
    if (bTrans != (1<<COLOUR_DEPTH))
    {
        // Replace the placeholder value with it
        for (int i = 0 ; i < ww*wh ; i++)
            if (pbSub[i] == 0xff)
                pbSub[i] = bTrans;

        // Return the transparency position
        return bTrans;
    }

    // Top-left of sub-image
    pb = pb_ + (wt*width) + wl;

    // In the very unlikely event of no free palette positions, give up on transparency
    // and replace the place-holder with the original pixel colour value
    for (int i = 0 ; i < ww*wh ; i++)
        if (pbSub[i] == 0xff)
            pbSub[i] = pb[width*(i/ww) + (i%ww)];

    // No transparency
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////////

bool GIF::Start (bool fAnimLoop_)
{
    // Fail if we're already recording
    if (f)
        return false;

    // Find a unique filename to use, in the format aniNNNN.gif
    pszFile = Util::GetUniqueFile("gif", szPath, sizeof(szPath));

    // Create the file
    f = fopen(szPath, "wb+");
    if (!f)
        return false;

    // Reset the frame counters
    nDelay = 0;
    lDelayOffset = 0;

    // Recording a looped animation?
    nLoopState = fAnimLoop_ ? kIgnoreFirstChange : kNone;

    Frame::SetStatus("Recording GIF %s", fAnimLoop_ ? "loop" : "animation");
    return true;
}

void GIF::Stop ()
{
    // Ignore if we're not recording
    if (!f)
        return;

    // Add any final delay to allow for identical frames at the end
    if (lDelayOffset)
        WriteGraphicControlExtensionDelay(lDelayOffset, nDelay*2);

    WriteFileTerminator();
    fclose(f);
    f = NULL;

    Frame::SetStatus("Saved %s", pszFile);
}

void GIF::Toggle (bool fAnimLoop_)
{
    if (!f)
        Start(fAnimLoop_);
    else
        Stop();
}

bool GIF::IsRecording ()
{
    return f != NULL;
}


void GIF::AddFrame (CScreen *pScreen_)
{
    // Fail if we're not recording
    if (!f)
        return;

    // Count the frames between changes
    nDelay++;

    // Return if there's no screen to record
    if (!pScreen_)
        return;

    WORD width = pScreen_->GetPitch()/2, height = pScreen_->GetHeight()/2;

    // If this is the first frame, write the file headers
    if (ftell(f) == 0)
    {
//		width = pScreen_->GetPitch()/2;
//		height = pScreen_->GetHeight()/2;
        pbCurr = new BYTE[width*height];
        pbSub = new BYTE[width*height];
        memset(pbCurr, 0xff, width*height);

        // File header
        fwrite("GIF89a", 6, 1, f);

        // Write the image dimensions and palette
        WriteLogicalScreenDescriptor(pScreen_);
        WriteGlobalColourTable();

        // Set the animation to loop back to the start when it finishes
        WriteNetscapeLoopExtension();
    }

    // GIF isn't suited to full framerate recording, so frame-skip
    static int nFrames;
    if ((nFrames++ % nFrameSkip))
        return;

    // Return if there were no changes from the last frame
    if (!GetChangeRect(pbCurr, pScreen_))
        return;

    // If recording a loop, force this frame to be encoded in full
    if (nLoopState == kWaitLoopStart)
    {
        // Invalidate the stored image and mark the whole region
        memset(pbCurr, 0xff, width*height);
        wl = wt = 0, ww = width, wh = height;
        nDelay = 0;
    }

    // Update our copy and encode the difference in the changed region
    BYTE bTrans = UpdateImage(pbCurr, pScreen_);

    // If recording a loop, wait for the first real change
    if (nLoopState == kIgnoreFirstChange)
    {
        nLoopState = kWaitLoopStart;
        return;
    }
    // If recording a loop, make a copy of the first frame
    else if (nLoopState == kWaitLoopStart)
    {
        nLoopState = kLoopStarted;
        pbFirst = new BYTE[width*height];
        memcpy(pbFirst, pbCurr, width*height);
    }
    // If we're looking for the end of a loop, compare with the first frame
    else if (pbFirst && !memcmp(pbFirst, pbCurr, width*height))
    {
        delete[] pbFirst, pbFirst = NULL;
        Stop();
        return;
    }

    // Write any accumulated delay of identical frames to the previous header
    if (lDelayOffset)
    {
        WriteGraphicControlExtensionDelay(lDelayOffset, nDelay*2);
        nDelay = 0;
    }

    // Write the GCE, storing its offset for the following delay
    lDelayOffset = ftell(f)+4;
    WriteGraphicControlExtension(0, bTrans);

    // Write the image header and changed frame data
    WriteImageDescriptor(wl, wt, ww, wh);

    // Write the compressed image data
    GifCompressor *pgc = new GifCompressor;
    if (pgc) pgc->WriteDataBlocks(f, ww*wh, COLOUR_DEPTH);
    delete pgc;
}

////////////////////////////////////////////////////////////////////////////////

BYTE *BitPacker::AddCodeToBuffer (DWORD code,short n)
/*
 Copied from Michael A. Mayer's AddCodeToBuffer(), with the following
 changes:
 - AddCodeToBuffer() is now a class member of class BitPacker
 - former local static variable 'need' now is a class member and
   initialized with 8 in the constructor
 - former formal parameter 'buf' is now the class member 'BYTE *pos' and
   initialized with 'pos=buffer' in the constructor
 - type of 'code' has been changed to 'unsigned long'.

 'n' tells how many least significant bits of 'code' are to be packed
 into 'buffer' in this call, its possible values are 1..32 and -1 
 (see the n<0 branch).
 When the function is called, 'pos' points to a partially empty byte and
 'need' (whose possible values are 1..8) tells how many bits will still fit
 in there. Since the bytes are filled bottom up (least significant bits first),
 the 'need' vacant bits are the most significant ones. 
*/
{
    DWORD mask;

    // If called with n==-1, then if the current byte is partially filled,
    // leave it alone and target the next byte (which is empty).
    if (n < 0)
    {
        if (need < 8)
        {
            pos++;
            *pos = 0x00;
        }

        need = 8;
        return pos;
    }

    while (n >= need)
    {
        mask = (1 << need) - 1;		// 'mask'= all zeroes followed by 'need' ones
        *pos += (BYTE)((mask&code) << (8-need));	// the 'need' lowest bits of 'code' fill the current byte at its upper end
        *++pos = 0x00;				// byte is now full, initialise next byte
        code = code >> need;		// remove the written bits from code
        n -= need;					// update the length of 'code'
        need = 8;					// current byte can take 8 bits 
    }

    //	Now we have n < need
    if(n > 0)
    {
        mask = (1 << n) - 1;
        *pos += (BYTE)((mask&code)<<(8-need));		// (remainder of) code is written to the n rightmost free bits of the current byte.

        // The current byte can still take 'need' bits, and we have 'need'>0.  The bits will be filled upon future calls.
        need -= n;
    }

    return pos;
}


// Packs an incoming code of n bits to the buffer. As soon as 255 bytes are full,
// they are written to 'binfile' as a data block and cleared from 'buffer'
BYTE *BitPacker::Submit (DWORD code, WORD n)
{
    AddCodeToBuffer(code,n);

    if (pos-buffer >= 255)				// pos pointing to buffer[255] or beyond
    {
        fputc(255,binfile);				// write the "bytecount-byte"
        fwrite(buffer,255,1,binfile);	// write buffer[0..254] to file
        buffer[0] = buffer[255];		// rotate the following bytes,
        buffer[1] = buffer[256];		// which may still contain data, to the
        buffer[2] = buffer[257];		// beginning of buffer, and point
        buffer[3] = buffer[258];		// (pos,need) to the position for new
        pos -= 255;						// input ('need' can stay unchanged)
        byteswritten += 256;
    }

    return pos;
}


// Writes any data contained in 'buffer' to the file as one data block of
// 1<=length<=255. Clears 'buffer' and reinitializes for new data
void BitPacker::WriteFlush ()
{
    AddCodeToBuffer(0,-1);		// close any partially filled terminal byte

    if (pos <= buffer)				// buffer is empty
        return;

    fputc((int)(pos-buffer),binfile);
    fwrite(buffer,pos-buffer,1,binfile);
    byteswritten += (int)(pos-buffer+1);

    pos = buffer;
    *pos = 0x00;
    need = 8;
}


BitPacker::BitPacker (FILE *bf)
{
    binfile = bf;
    need = 8;
    byteswritten = 0;

    pos = buffer;
    *pos = 0x00;
}



GifCompressor::GifCompressor ()
    : bp(NULL), nofdata(0), width(0), height(0), curordinal(0), pixel(0), nbits(0),
      axon(NULL), next(NULL), pix(NULL), cc(0), eoi(0), freecode(0)
{
}

// Initialize a root node for each root code
void GifCompressor::InitRoots ()
{
    WORD nofrootcodes = 1 << COLOUR_DEPTH;

    for (WORD i = 0; i < nofrootcodes; i++)
    {
        axon[i] = 0;
        pix[i] = (BYTE)i;
        // next[] is unused for root codes
    }
}


// The stringtable is flushed by removing the outlets of all root nodes
void GifCompressor::FlushStringTable ()
{
    WORD nofrootcodes = 1 << COLOUR_DEPTH;

    for (WORD i = 0; i < nofrootcodes; i++)
        axon[i] = 0;
}


// Checks if the chain emanating from headnode's axon contains a node for 'pixel'.
// Returns that node's address (=code), or 0 if there is no such node.
// (0 cannot be the root node 0, since root nodes occur in no chain).
WORD GifCompressor::FindPixelOutlet (WORD headnode,BYTE pixel)
{
    WORD outlet;
    for (outlet = axon[headnode] ; outlet && pix[outlet] != pixel ; outlet = next[outlet]);
    return outlet;
}


// Writes the next code to the codestream and adds one entry to the stringtable.
// Does not change 'freecode'. Moves 'curordinal' forward and returns it pointing
// to the first pixel that hasn't been encoded yet. Recognizes the end of the data stream.
DWORD GifCompressor::DoNext ()
{
    WORD up = pixel,down;			// start with the root node for 'pixel'

    if (++curordinal >= nofdata)	// end of data stream ? Terminate
    {
        bp->Submit(up,nbits);
        return curordinal;
    }

    //	Follow the string table and the data stream to the end of the longest string that has a code
    pixel = pbSub[curordinal];

    while ((down=FindPixelOutlet(up,pixel))!=0) 
    {
        up = down;

        if(++curordinal >= nofdata)		// end of data stream ? Terminate
        {
            bp->Submit(up,nbits);
            return curordinal;
        }

        pixel = pbSub[curordinal];
    }

    // Submit 'up' which is the code of the longest string ...
    bp->Submit(up,nbits);

    // ... and extend the string by appending 'pixel':
    //	Create a successor node for 'pixel' whose code is 'freecode'...
    pix[freecode] = pixel;
    axon[freecode] = next[freecode]=0;

    // ...and link it to the end of the chain emanating from axon[up].
    // Don't link it to the start instead: it would slow down performance.

    down = axon[up];

    if(!down)
        axon[up] = freecode;
    else
    {
        while (next[down])
            down = next[down];

        next[down] = freecode;
    }

    return curordinal;
}


DWORD GifCompressor::WriteDataBlocks (FILE *bf, DWORD nof, WORD dd)
{
    nofdata = nof;				// number of pixels in data stream

    curordinal = 0;				// pixel #0 is next to be processed
    pixel = pbSub[curordinal];	// get pixel #0

    nbits = COLOUR_DEPTH+1;		// initial size of compression codes
    cc = (1<<(nbits-1));		// 'cc' is the lowest code requiring 'nbits' bits
    eoi = cc+1;					// 'end-of-information'-code
    freecode = (WORD)cc+2;		// code of the next entry to be added to the stringtable

    bp = new BitPacker(bf);		// object that does the packing of the codes and renders them to the binary file 'bf'
    axon = new WORD[4096];
    next = new WORD[4096];
    pix = new BYTE[4096];

    if(!pix || !next || !axon || !bp)
    {
        delete[] pix;
        delete[] next;
        delete[] axon;
        delete bp;
        return 0;
    }

    InitRoots();				// initialize the string table's root nodes
    fputc(COLOUR_DEPTH,bf);		// Write what the GIF specification calls the "code size", which is the colour depth
    bp->Submit(cc,nbits);		// Submit one 'cc' as the first code

    for (;;)
    {
        DoNext();					// generates the next code, submits it to 'bp' and updates 'curordinal'

        if(curordinal >= nofdata)	// if reached the end of data stream:
        {
            bp->Submit(eoi,nbits);	// submit 'eoi' as the last item of the code stream
            bp->WriteFlush();		// write remaining codes including this 'eoi' to the binary file
            fputc(0x00,bf);			// write an empty data block to signal the end of "raster data" section in the file

            delete[] axon;
            delete[] next;
            delete[] pix;

            DWORD byteswritten = 2 + bp->byteswritten; 
            delete bp;
            return byteswritten;
        }

        if(freecode == (1U << nbits))	// if the latest code added to the stringtable exceeds 'nbits' bits:
            nbits++;					// increase size of compression codes by 1 bit

        freecode++;

        if(freecode == 0xfff)
        {
            FlushStringTable();		// avoid stringtable overflow
            bp->Submit(cc,nbits);	// tell the decoding software to flush its stringtable
            nbits = COLOUR_DEPTH+1;
            freecode = (WORD)cc+2;
        } 
    }
}
