// Part of SimCoupe - A SAM Coupe emulator
//
// GIF.cpp: GIF animation recording
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// BitPacker and GifCompressor classes:
// Based on code by: Christoph Hohmann (http://web.archive.org/web/http://members.aol.com/rf21exe/gif.htm)
// Who based his code on code by: Michael A. Mayer
// Who apparently based his code on code by: Bob Montgomery circa 1988

#include "SimCoupe.h"
#include "GIF.h"

#include "Frame.h"
#include "Options.h"

namespace GIF
{

static std::vector<uint8_t> current_frame;
static std::vector<uint8_t> first_frame;
static std::vector<uint8_t> diff_frame;

static std::string gif_path;
static unique_FILE file;

static int delay_frames = 0;
static long delay_file_offset;
static int wl, wt, ww, wh;  // left/top/width/height for change rect
static int frame_skip = 0;  // 50/2 = 25fps (FF/Chrome/Safari/Opera), 50/3 = 16.6fps (IE grrr!)
static auto size_divisor = 1;

enum class LoopState { None, IgnoreChange, WaitStart, Started };
static LoopState loop_state;

constexpr auto COLOUR_DEPTH = 7;   // 128 SAM colours

////////////////////////////////////////////////////////////////////////////////

static void WriteLogicalScreenDescriptor(const FrameBuffer& fb)
{
    auto w = fb.Width() / size_divisor;
    auto h = fb.Height() * 2 / size_divisor;

    fputc(w & 0xff, file);
    fputc(w >> 8, file);
    fputc(h & 0xff, file);
    fputc(h >> 8, file);

    fputc(0xf0 | (0x7 & (COLOUR_DEPTH - 1)), file);
    fputc(0x00, file); // Background colour index

    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    fputc(static_cast<int>(std::round(aspect_ratio * 64)) - 15, file);
}

static void WriteGlobalColourTable()
{
    for (auto& colour : IO::Palette())
    {
        fputc(colour.red, file);
        fputc(colour.green, file);
        fputc(colour.blue, file);
    }
}

static void WriteImageDescriptor(int left, int top, int width, int height)
{
    fputc(',', file);   // image separator

    fputc(left & 0xff, file);
    fputc(left >> 8, file);
    fputc(top & 0xff, file);
    fputc(top >> 8, file);
    fputc(width & 0xff, file);
    fputc(width >> 8, file);
    fputc(height & 0xff, file);
    fputc(height >> 8, file);

    fputc(0x00 | (0x7 & (COLOUR_DEPTH - 1)), file); // information on the local colour table
}

static long WriteGraphicControlExtension(int delay_ms, uint8_t trans_idx)
{
    fputc(0x21, file);     // GIF extension code
    fputc(0xf9, file);     // graphic control label
    fputc(0x04, file);     // data length

    uint8_t flags = (1 << 2);
    if (trans_idx != 0xff) flags |= (1 << 0);
    fputc(flags, file); // Bits 7-5: reserved
                        // Bits 4-2: disposal method (0=none, 1=leave, 2=restore bkg, 3=restore prev)
                        // Bit 1: user input field
                        // Bit 0: transparent colour flag

    auto delay_pos = ftell(file);

    fputc(delay_ms & 0xff, file);
    fputc(delay_ms >> 8, file);

    fputc((flags & 1) ? trans_idx : 0x00, file);
    fputc(0x00, file);  // Data sub-block terminator

    return delay_pos;
}

static bool WriteGraphicControlExtensionDelay(long offset, int delay_100ths)
{
    auto prev_pos = ftell(file);

    if (fseek(file, offset, SEEK_SET) == 0)
    {
        fputc(delay_100ths & 0xff, file);
        fputc(delay_100ths >> 8, file);
    }

    return fseek(file, prev_pos, SEEK_SET) == 0;
}

static void WriteNetscapeLoopExtension()
{
    uint16_t loops = 0;     // infinite

    fputc(0x21, file);     // GIF Extension code
    fputc(0xff, file);     // Application Extension Label
    fputc(0x0b, file);     // Length of Application Block

    fwrite("NETSCAPE2.0", 11, 1, file);

    fputc(0x03, file);          // Length of Data Sub-Block
    fputc(0x01, file);
    fputc(loops & 0xff, file);  // 2-byte loop iteration count
    fputc(loops >> 8, file);
    fputc(0x00, file);          // Data sub-block terminator
}

static void WriteFileTerminator()
{
    fputc(';', file);
}


// Compare our copy of the screen with the new display contents
static bool GetChangeRect(const std::vector<uint8_t>& gif_frame, const FrameBuffer& fb)
{
    int l, t, r, b, x, y;
    l = t = r = b = 0;

    auto width = fb.Width() / size_divisor;
    auto height = fb.Height() * 2 / size_divisor;

    auto change_offset = 0;
    for (y = 0; y < height; y++)
    {
        auto pb = fb.GetLine(y / (2 / size_divisor));

        for (x = 0; x < width; x++, change_offset++, pb += size_divisor)
        {
            if (gif_frame [change_offset] != *pb)
            {
                // We've found a single pixel in the change rectangle
                // The top position is the only known valid side so far
                l = r = x;
                t = b = y;
                goto found_top;
            }
        }
    }

    if (y == height)
        return false;

found_top:
    change_offset = width * height - 1;

    for (y = height - 1; y >= t; y--)
    {
        auto pb = fb.GetLine(y / (2 / size_divisor));
        pb += (width - 1) * size_divisor;

        // Scan the full width of the line, right to left
        for (x = width - 1; x >= 0; x--, change_offset--, pb -= size_divisor)
        {
            if (gif_frame[change_offset] != *pb)
            {
                // We've now found the bottom of the rectangle
                b = y;

                // Update left/right extents if the change position helps us
                if (x < l) l = x;
                if (x > r) r = x;
                goto found_bottom;
            }
        }
    }

found_bottom:
    change_offset = width * t;

    for (y = t; y <= b; y++, change_offset += width)
    {
        auto pb = fb.GetLine(y / (2 / size_divisor));

        for (x = 0; x < l; x++)
        {
            if (gif_frame[change_offset + x] != pb[x * size_divisor])
            {
                // Reduce the left edge to the change point
                if (x < l) l = x;
                break;
            }
        }

        // Scan the unknown right strip
        for (x = width - 1; x > r; x--)
        {
            if (gif_frame[change_offset + x] != pb[x * size_divisor])
            {
                // Increase the right edge to the change point
                if (x > r) r = x;
                break;
            }
        }
    }

    wl = l;
    wt = t;
    ww = r - l + 1;
    wh = b - t + 1;

    return true;
}

// Update current image and determine sub-region difference to encode
static uint8_t UpdateImage(std::vector<uint8_t>& gif_frame, const FrameBuffer& fb)
{
    uint16_t width = fb.Width() / size_divisor;
    uint8_t abUsed[1 << COLOUR_DEPTH] = { 0 };
    auto pbSub_ = diff_frame.data();

    // Move to top-left of sub-image
    auto pb = gif_frame.data() + (wt * width) + wl;

    auto bColour = *pb;
    int nRun = 0, nTrans = 0;

    for (int y = wt; y < wt + wh; y++, pb += width)
    {
        auto pbScr = fb.GetLine(y / (2 / size_divisor));
        pbScr += (wl * size_divisor);

        for (int x = 0; x < ww; x++, pbScr += size_divisor)
        {
            uint8_t bOld = pb[x], bNew = *pbScr;
            pb[x] = bNew;
            abUsed[bNew] = 1;
#if 0
            *pbSub_++ = bNew; // force full redraw for testing
#else
            bool is_match = (bNew == bColour);
            bool is_transparent = (bNew == bOld);

            // End of a colour run?
            if (!is_match && nRun > nTrans)
            {
                memset(pbSub_, bColour, nRun);
                pbSub_ += nRun;
            }
            // End of a transparency run, or colour/transparent run of equal size?
            else if (!is_transparent && (nTrans > nRun || (!is_match && nRun)))
            {
                memset(pbSub_, 0xff, nTrans);
                pbSub_ += nTrans;
            }
            // Continuing an existing run of either type?
            else if (is_match || is_transparent)
            {
                if (is_match) nRun++;
                if (is_transparent) nTrans++;
                continue;
            }

            // Start a new run with this pixel
            bColour = bNew;
            nRun = 1;
            nTrans = is_transparent ? 1 : 0;
#endif
        }
    }

    // Complete the final colour run, if larger
    if (nRun > nTrans)
    {
        memset(pbSub_, bColour, nRun);
        pbSub_ += nRun;
    }
    // or the final transparent run
    else if (nTrans)
    {
        memset(pbSub_, 0xff, nTrans);
        pbSub_ += nTrans;
    }

    uint8_t bTrans;
    for (bTrans = 0; bTrans < (1 << COLOUR_DEPTH) && abUsed[bTrans]; bTrans++);

    // Have we found a free palette position for transparency?
    if (bTrans != (1 << COLOUR_DEPTH))
    {
        // Replace the placeholder value with it
        for (int i = 0; i < ww * wh; i++)
        {
            if (diff_frame[i] == 0xff)
                diff_frame[i] = bTrans;
        }

        // Return the transparency position
        return bTrans;
    }

    // Top-left of sub-image
    pb = gif_frame.data() + (wt * width) + wl;

    // In the very unlikely event of no free palette positions, give up on transparency
    // and replace the place-holder with the original pixel colour value
    for (int i = 0; i < ww * wh; i++)
    {
        if (diff_frame[i] == 0xff)
            diff_frame[i] = pb[width * (i / ww) + (i % ww)];
    }

    // No transparency
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////////

bool Start(int flags)
{
    if (file)
        return false;

    gif_path = Util::UniqueOutputPath("gif");
    file = fopen(gif_path.c_str(), "wb+");
    if (!file)
    {
        Frame::SetStatus("Save failed: {}", gif_path);
        return false;
    }

    size_divisor = (flags & HALFSIZE) ? 2 : 1;
    loop_state = (flags & LOOP) ? LoopState::IgnoreChange : LoopState::None;
    frame_skip = std::min(std::max(0, GetOption(gifframeskip)), 3);

    delay_frames = 0;
    delay_file_offset = 0;

    Frame::SetStatus("Recording GIF {}", (flags & LOOP) ? "loop" : "animation");
    return true;
}

void Stop()
{
    if (!file)
        return;

    if (delay_file_offset)
    {
        WriteGraphicControlExtensionDelay(delay_file_offset, delay_frames * 2);
        delay_file_offset = 0;
    }

    WriteFileTerminator();
    file.reset();
    Frame::SetStatus("Saved {}", gif_path);
}

void Toggle(int flags)
{
    if (!file)
        Start(flags);
    else
        Stop();
}

bool IsRecording()
{
    return file != nullptr;
}


void AddFrame(const FrameBuffer& fb)
{
    if (!file)
        return;

    delay_frames++;

    auto width = fb.Width() / size_divisor;
    auto height = fb.Height() * 2 / size_divisor;
    auto size = width * height;

    if (ftell(file) == 0)
    {
        current_frame.resize(size);
        diff_frame.resize(size);
        std::fill(current_frame.begin(), current_frame.end(), 0xff);

        fwrite("GIF89a", 6, 1, file);
        WriteLogicalScreenDescriptor(fb);
        WriteGlobalColourTable();
        WriteNetscapeLoopExtension();
    }

    static int frame_count;
    if ((frame_count++ % (frame_skip + 1)))
        return;

    if (!GetChangeRect(current_frame, fb))
        return;

    if (loop_state == LoopState::WaitStart)
    {
        // Invalidate the stored image and mark the whole region
        std::fill(current_frame.begin(), current_frame.end(), 0xff);
        wl = wt = 0;
        ww = width;
        wh = height;
        delay_frames = 0;
    }

    auto trans_idx = UpdateImage(current_frame, fb);

    if (loop_state == LoopState::IgnoreChange)
    {
        loop_state = LoopState::WaitStart;
        return;
    }

    if (loop_state == LoopState::WaitStart)
    {
        loop_state = LoopState::Started;
        first_frame = current_frame;
    }
    else if (current_frame == first_frame)
    {
        Stop();
        return;
    }

    if (delay_file_offset)
    {
        WriteGraphicControlExtensionDelay(delay_file_offset, delay_frames * 2);
        delay_frames = 0;
    }

    delay_file_offset = WriteGraphicControlExtension(0, trans_idx);

    WriteImageDescriptor(wl, wt, ww, wh);

    auto pgc = std::make_unique<GifCompressor>();
    pgc->WriteDataBlocks(file, ww * wh, COLOUR_DEPTH);
}

} // namespace GIF

////////////////////////////////////////////////////////////////////////////////

BitPacker::BitPacker(FILE* bf)
    : binfile(bf), pos(buffer)
{
    *pos = 0x00;
}

uint8_t* BitPacker::AddCodeToBuffer(uint32_t code, short n)
/*
 Copied from Michael A. Mayer's AddCodeToBuffer(), with the following
 changes:
 - AddCodeToBuffer() is now a class member of class BitPacker
 - former local static variable 'need' now is a class member and
   initialized with 8 in the constructor
 - former formal parameter 'buf' is now the class member 'uint8_t *pos' and
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
    uint32_t mask;

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
        mask = (1 << need) - 1;     // 'mask'= all zeroes followed by 'need' ones
        *pos += (uint8_t)((mask & code) << (8 - need));    // the 'need' lowest bits of 'code' fill the current byte at its upper end
        *++pos = 0x00;              // byte is now full, initialise next byte
        code = code >> need;        // remove the written bits from code
        n -= need;                  // update the length of 'code'
        need = 8;                   // current byte can take 8 bits 
    }

    // Now we have n < need
    if (n > 0)
    {
        mask = (1 << n) - 1;
        *pos += (uint8_t)((mask & code) << (8 - need));        // (remainder of) code is written to the n rightmost free bits of the current byte.

        // The current byte can still take 'need' bits, and we have 'need'>0.  The bits will be filled upon future calls.
        need -= n;
    }

    return pos;
}


// Packs an incoming code of n bits to the buffer. As soon as 255 bytes are full,
// they are written to 'binfile' as a data block and cleared from 'buffer'
uint8_t* BitPacker::Submit(uint32_t code, uint16_t n)
{
    AddCodeToBuffer(code, n);

    if (pos - buffer >= 255)            // pos pointing to buffer[255] or beyond
    {
        fputc(255, binfile);            // write the "bytecount-byte"
        fwrite(buffer, 255, 1, binfile);// write buffer[0..254] to file
        buffer[0] = buffer[255];        // rotate the following bytes,
        buffer[1] = buffer[256];        // which may still contain data, to the
        buffer[2] = buffer[257];        // beginning of buffer, and point
        buffer[3] = buffer[258];        // (pos,need) to the position for new
        pos -= 255;                     // input ('need' can stay unchanged)
        byteswritten += 256;
    }

    return pos;
}


// Writes any data contained in 'buffer' to the file as one data block of
// 1<=length<=255. Clears 'buffer' and reinitializes for new data
void BitPacker::WriteFlush()
{
    AddCodeToBuffer(0, -1);     // close any partially filled terminal byte

    if (pos <= buffer)          // buffer is empty
        return;

    fputc((int)(pos - buffer), binfile);
    fwrite(buffer, pos - buffer, 1, binfile);
    byteswritten += (int)(pos - buffer + 1);

    pos = buffer;
    *pos = 0x00;
    need = 8;
}

////////////////////////////////////////////////////////////////////////////////

// Initialize a root node for each root code
void GifCompressor::InitRoots()
{
    uint16_t nofrootcodes = 1 << GIF::COLOUR_DEPTH;

    for (uint16_t i = 0; i < nofrootcodes; i++)
    {
        axon[i] = 0;
        pix[i] = (uint8_t)i;
        // next[] is unused for root codes
    }
}


// The stringtable is flushed by removing the outlets of all root nodes
void GifCompressor::FlushStringTable()
{
    uint16_t nofrootcodes = 1 << GIF::COLOUR_DEPTH;

    for (uint16_t i = 0; i < nofrootcodes; i++)
        axon[i] = 0;
}


// Checks if the chain emanating from headnode's axon contains a node for 'pixel'.
// Returns that node's address (=code), or 0 if there is no such node.
// (0 cannot be the root node 0, since root nodes occur in no chain).
uint16_t GifCompressor::FindPixelOutlet(uint16_t headnode, uint8_t pixel_)
{
    uint16_t outlet;
    for (outlet = axon[headnode]; outlet && pix[outlet] != pixel_; outlet = next[outlet]);
    return outlet;
}


// Writes the next code to the codestream and adds one entry to the stringtable.
// Does not change 'freecode'. Moves 'curordinal' forward and returns it pointing
// to the first pixel that hasn't been encoded yet. Recognizes the end of the data stream.
uint32_t GifCompressor::DoNext()
{
    uint16_t up = pixel, down;          // start with the root node for 'pixel'

    if (++curordinal >= nofdata)    // end of data stream ? Terminate
    {
        bp->Submit(up, nbits);
        return curordinal;
    }

    // Follow the string table and the data stream to the end of the longest string that has a code
    pixel = GIF::diff_frame[curordinal];

    while ((down = FindPixelOutlet(up, pixel)) != 0)
    {
        up = down;

        if (++curordinal >= nofdata)        // end of data stream ? Terminate
        {
            bp->Submit(up, nbits);
            return curordinal;
        }

        pixel = GIF::diff_frame[curordinal];
    }

    // Submit 'up' which is the code of the longest string ...
    bp->Submit(up, nbits);

    // ... and extend the string by appending 'pixel':
    //  Create a successor node for 'pixel' whose code is 'freecode'...
    pix[freecode] = pixel;
    axon[freecode] = next[freecode] = 0;

    // ...and link it to the end of the chain emanating from axon[up].
    // Don't link it to the start instead: it would slow down performance.

    down = axon[up];

    if (!down)
        axon[up] = freecode;
    else
    {
        while (next[down])
            down = next[down];

        next[down] = freecode;
    }

    return curordinal;
}


uint32_t GifCompressor::WriteDataBlocks(FILE* bf, uint32_t nof, uint16_t dd)
{
    nofdata = nof;              // number of pixels in data stream

    curordinal = 0;             // pixel #0 is next to be processed
    pixel = GIF::diff_frame[curordinal]; // get pixel #0

    nbits = GIF::COLOUR_DEPTH + 1;  // initial size of compression codes
    cc = (1 << (nbits - 1));        // 'cc' is the lowest code requiring 'nbits' bits
    eoi = cc + 1;                   // 'end-of-information'-code
    freecode = static_cast<uint16_t>(cc + 2); // code of the next entry to be added to the stringtable

    bp = std::make_unique<BitPacker>(bf);     // object that does the packing of the codes and renders them to the binary file 'bf'

    InitRoots();                    // initialize the string table's root nodes
    fputc(GIF::COLOUR_DEPTH, bf);   // Write what the GIF specification calls the "code size", which is the colour depth
    bp->Submit(cc, nbits);          // Submit one 'cc' as the first code

    for (;;)
    {
        DoNext();                   // generates the next code, submits it to 'bp' and updates 'curordinal'

        if (curordinal >= nofdata)  // if reached the end of data stream:
        {
            bp->Submit(eoi, nbits); // submit 'eoi' as the last item of the code stream
            bp->WriteFlush();       // write remaining codes including this 'eoi' to the binary file
            fputc(0x00, bf);        // write an empty data block to signal the end of "raster data" section in the file

            return bp->byteswritten + 2;
        }

        if (freecode == (1U << nbits))  // if the latest code added to the stringtable exceeds 'nbits' bits:
            nbits++;                    // increase size of compression codes by 1 bit

        if (++freecode == 0xfff)
        {
            FlushStringTable();     // avoid stringtable overflow
            bp->Submit(cc, nbits);  // tell the decoding software to flush its stringtable
            nbits = dd + 1;
            freecode = static_cast<uint16_t>(cc + 2);
        }
    }
}
