// Part of SimCoupe - A SAM Coupe emulator
//
// AVI.cpp: AVI movie recording
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

#include "SimCoupe.h"
#include "AVI.h"

#include "Frame.h"
#include "Options.h"
#include "Sound.h"

namespace AVI
{

static std::vector<uint8_t> frame_buffer;
static std::vector<uint8_t> resample_buffer;

static std::string avi_path;
static unique_FILE file;

static uint16_t width, height;
static bool half_size = false;

static long riff_pos, movi_pos;
static long max_video_size, max_audio_size;
static uint32_t num_video_frames, num_audio_frames, num_audio_samples;
static bool want_video;

static bool WriteLittleEndianWORD(uint16_t w)
{
    fputc(w & 0xff, file);
    return fputc(w >> 8, file) != EOF;
}

static bool WriteLittleEndianDWORD(uint32_t dw)
{
    fputc(dw & 0xff, file);
    fputc((dw >> 8) & 0xff, file);
    fputc((dw >> 16) & 0xff, file);
    return fputc((dw >> 24) & 0xff, file) != EOF;
}

static bool WriteLittleEndianLong(long val)
{
    return WriteLittleEndianDWORD(static_cast<uint32_t>(val));
}

static uint32_t ReadLittleEndianDWORD()
{
    std::array<uint8_t, 4> buf{};
    if (fread(buf.data(), 1, buf.size(), file) != buf.size())
        return 0;

    return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

static long WriteChunkStart(FILE* file, const std::string& chunk_name, const std::string& sub_type="")
{
    if (!chunk_name.empty() && fwrite(chunk_name.c_str(), 1, 4, file) != 4)
        return 0;

    long offset_pos = ftell(file);
    if (fseek(file, sizeof(uint32_t), SEEK_CUR) != 0)
        return 0;

    if (!sub_type.empty() && fwrite(sub_type.c_str(), 1, 4, file) != 4)
        return 0;

    return offset_pos;
}

static long WriteChunkEnd(FILE* file, long pos)
{
    auto current_pos = ftell(file);
    long chunk_size = current_pos - pos - sizeof(uint32_t);

    if (pos < 0 || fseek(file, pos, SEEK_SET) != 0)
        return 0;

    WriteLittleEndianLong(chunk_size);

    if (current_pos < 0 || fseek(file, current_pos, SEEK_SET) != 0)
        return 0;

    if (current_pos & 1)
    {
        // Pad to even boundary
        fputc(0x00, file);
        chunk_size++;
    }

    return chunk_size;
}

static bool WriteAVIHeader(FILE* file)
{
    auto pos = WriteChunkStart(file, "avih");
    WriteLittleEndianDWORD(19968);          // microseconds per frame: 1000000*CPU_CYCLES_PER_FRAME/CPU_CLOCK_HZ
    WriteLittleEndianLong((max_video_size * EMULATED_FRAMES_PER_SECOND) + (max_audio_size * EMULATED_FRAMES_PER_SECOND)); // approximate max data rate
    WriteLittleEndianDWORD(0);              // reserved
    WriteLittleEndianDWORD((1 << 8) | (1 << 4)); // flags: bit 4 = has index(idx1), bit 5 = use index for AVI structure, bit 8 = interleaved file, bit 16 = optimized for live video capture, bit 17 = copyrighted data
    WriteLittleEndianDWORD(num_video_frames); // total number of video frames
    WriteLittleEndianDWORD(0);              // initial frame number for interleaved files
    WriteLittleEndianDWORD(2);              // number of streams in the file (video+audio)
    WriteLittleEndianDWORD(0);              // suggested buffer size for reading the file
    WriteLittleEndianDWORD(width);          // pixel width
    WriteLittleEndianDWORD(height);         // pixel height
    WriteLittleEndianDWORD(0);              // 4 reserved DWORDs (must be zero)
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);
    return WriteChunkEnd(file, pos) != 0;
}

static bool WriteVideoHeader(FILE* file)
{
    auto pos = WriteChunkStart(file, "strh", "vids");
    fwrite("mrle", 4, 1, file);             // 'mrle' = Microsoft Run Length Encoding Video Codec
    WriteLittleEndianDWORD(0);              // flags, unused
    WriteLittleEndianDWORD(0);              // priority and language, unused
    WriteLittleEndianDWORD(0);              // initial frames
    WriteLittleEndianDWORD(CPU_CYCLES_PER_FRAME); // scale
    WriteLittleEndianDWORD(CPU_CLOCK_HZ);   // rate
    WriteLittleEndianDWORD(0);              // start time
    WriteLittleEndianDWORD(num_video_frames); // total frames in stream
    WriteLittleEndianLong(max_video_size);  // suggested buffer size
    WriteLittleEndianDWORD(10000);          // quality
    WriteLittleEndianDWORD(0);              // sample size
    WriteLittleEndianWORD(0);               // left
    WriteLittleEndianWORD(0);               // top
    WriteLittleEndianWORD(width);           // right
    WriteLittleEndianWORD(height);          // bottom
    WriteChunkEnd(file, pos);

    pos = WriteChunkStart(file, "strf");
    WriteLittleEndianDWORD(40);             // sizeof(BITMAPINFOHEADER)
    WriteLittleEndianDWORD(width);          // biWidth;
    WriteLittleEndianDWORD(height);         // biHeight;
    WriteLittleEndianWORD(1);               // biPlanes;
    WriteLittleEndianWORD(8);               // biBitCount (8 = 256 colours)
    WriteLittleEndianDWORD(1);              // biCompression (1 = BI_RLE8)
    WriteLittleEndianDWORD(width * height); // biSizeImage;
    WriteLittleEndianDWORD(0);              // biXPelsPerMeter;
    WriteLittleEndianDWORD(0);              // biYPelsPerMeter;
    WriteLittleEndianDWORD(256);            // biClrUsed;
    WriteLittleEndianDWORD(0);              // biClrImportant;

    auto palette = IO::Palette();
    for (auto& colour : palette)
    {
        // Note: BGR, plus zero reserved field from RGBQUAD
        fputc(colour.blue, file);
        fputc(colour.green, file);
        fputc(colour.red, file);
        fputc(0, file);
    }

    // The second half of the palette is all black
    std::vector<uint8_t> filler(256 - palette.size(), 0);
    fwrite(filler.data(), 1, filler.size(), file);

    return WriteChunkEnd(file, pos) != 0;
}

static bool WriteAudioHeader(FILE* file)
{
    uint16_t wFreq = SAMPLE_FREQ;
    uint16_t wBits = SAMPLE_BITS;
    uint16_t wBlock = BYTES_PER_SAMPLE;
    uint16_t wChannels = SAMPLE_CHANNELS;

    auto pos = WriteChunkStart(file, "strh", "auds");
    fwrite("\0\0\0\0", 4, 1, file);         // FOURCC not specified (PCM below)
    WriteLittleEndianDWORD(0);              // flags, unused
    WriteLittleEndianDWORD(0);              // priority and language, unused
    WriteLittleEndianDWORD(1);              // initial frames
    WriteLittleEndianDWORD(wBlock);         // scale
    WriteLittleEndianDWORD(wFreq * wBlock); // rate
    WriteLittleEndianDWORD(0);              // start time
    WriteLittleEndianDWORD(num_audio_samples); // total samples in stream
    WriteLittleEndianLong(max_audio_size);  // suggested buffer size
    WriteLittleEndianDWORD(0xffffffff);     // quality
    WriteLittleEndianDWORD(wBlock);         // sample size
    WriteLittleEndianDWORD(0);              // two unused rect coords
    WriteLittleEndianDWORD(0);              // two more unused rect coords
    WriteChunkEnd(file, pos);

    pos = WriteChunkStart(file, "strf");
    WriteLittleEndianWORD(1);               // format tag (1 = WAVE_FORMAT_PCM)
    WriteLittleEndianWORD(wChannels);       // channels
    WriteLittleEndianDWORD(wFreq);          // samples per second
    WriteLittleEndianDWORD(wFreq * wBlock); // average bytes per second
    WriteLittleEndianWORD(wBlock);          // block align
    WriteLittleEndianWORD(wBits);           // bits per sample
    WriteLittleEndianWORD(0);               // extra structure size
    return WriteChunkEnd(file, pos) != 0;
}

static bool WriteIndex(FILE* file)
{
    auto idx1_pos = WriteChunkStart(file, "idx1");
    if (fseek(file, -4, SEEK_CUR) != 0)
        return false;
    WriteLittleEndianDWORD(0);

    // Locate the start of the movi data (after the chunk header)
    movi_pos += 2 * sizeof(uint32_t);

    auto num_index_entries = num_video_frames + num_audio_frames;
    uint32_t dwVideoFrame = 0;

    for (unsigned int u = 0; u < num_index_entries; u++)
    {
        std::array<uint8_t, 4> type{};

        if (fseek(file, movi_pos, SEEK_SET) != 0)
            return false;

        if (fread(type.data(), 1, type.size(), file) != type.size())
            return false;

        auto frame_size = ReadLittleEndianDWORD();
        if (fseek(file, 0, SEEK_END) != 0)
            return false;

        // Every 50th frame is a key frame
        auto is_key_frame = (type[1] == '0') && !(dwVideoFrame++ % EMULATED_FRAMES_PER_SECOND);

        if (fwrite(type.data(), 1, type.size(), file) != type.size())
            return false;

        WriteLittleEndianDWORD(is_key_frame ? 0x10 : 0x00);
        WriteLittleEndianLong(movi_pos);
        WriteLittleEndianDWORD(frame_size);

        // Calculate next position, aligned to even boundary
        movi_pos += 2 * sizeof(uint32_t) + frame_size + (frame_size & 1);
    }

    return WriteChunkEnd(file, idx1_pos) != 0;
}

static bool WriteFileHeaders(FILE* file)
{
    if (fseek(file, 0, SEEK_SET) != 0)
        return false;

    riff_pos = WriteChunkStart(file, "RIFF", "AVI ");
    auto hdrl_pos = WriteChunkStart(file, "LIST", "hdrl");

    WriteAVIHeader(file);

    auto pos = WriteChunkStart(file, "LIST", "strl");
    WriteVideoHeader(file);
    WriteChunkEnd(file, pos);

    pos = WriteChunkStart(file, "LIST", "strl");
    WriteAudioHeader(file);
    WriteChunkEnd(file, pos);

    pos = WriteChunkStart(file, "JUNK");

    // Align movi data to 2048-byte boundary
    if (fseek(file, (-ftell(file) - 3 * sizeof(uint32_t)) & 0x3ff, SEEK_CUR) != 0)
        return false;

    WriteChunkEnd(file, pos);
    WriteChunkEnd(file, hdrl_pos);

    movi_pos = WriteChunkStart(file, "LIST", "movi");
    return movi_pos != 0;
}

static int FindRunFragment(const uint8_t* pb, uint8_t* pbP_, int width, int& jump_len)
{
    int x, nRun = 0;

    for (x = 0; x < width; x++, pb++, pbP_++)
    {
        // Include matching pixels in the run
        if (*pb == *pbP_)
            nRun++;
        // Ignore runs below the jump overhead
        else if (nRun < 4)
            nRun = 0;
        // Accept the run fragment
        else
            break;
    }

    // Is the run length below the jump overhead?
    if (nRun < 4)
    {
        // Encode the full block
        jump_len = 0;
        return width;
    }

    // Return the jump length over matching pixels and fragment length before it
    jump_len = nRun;
    return x - nRun;
}

static void EncodeAbsolute(const uint8_t* pb_, int nLength_)
{
    // Short lengths conflict with RLE codes, and must be encoded as colour runs instead
    if (nLength_ < 3)
    {
        while (nLength_--)
        {
            fputc(0x01, file);     // length=1
            fputc(*pb_++, file);   // colour
        }
    }
    else
    {
        fputc(0x00, file);                 // escape
        fputc(nLength_, file);             // length
        fwrite(pb_, nLength_, 1, file);    // data

        // Absolute blocks must maintain 16-bit alignment, so output a dummy byte if necessary
        if (nLength_ & 1)
            fputc(0x00, file);
    }
}

static void EncodeBlock(const uint8_t* pb_, int nLength_)
{
    while (nLength_)
    {
        // Maximum run length is 255 or the width of the fragment
        int nMaxRun = std::min(255, nLength_);

        // Start with a run of 1 pixel
        int nRun = 1;
        auto bColour = *pb_++;

        // Find the extent of the solid run
        for (; *pb_ == bColour && nRun < nMaxRun; nRun++, pb_++);

        // If the run is more than one byte, outputting as a colour run is the best we can do
        // Also do this if the fragment is too short to use an absolute run
        if (nRun > 1 || nLength_ < 3)
        {
            fputc(nRun, file);
            fputc(bColour, file);
            nLength_ -= nRun;

            // Try for another colour run
            continue;
        }

        // Single pixel runs become the start of an absolute block
        pb_--;

        for (nRun = 0; nRun < nMaxRun; nRun++)
        {
            // If there's not enough left for a colour run, include it in the absolute block
            if ((nMaxRun - nRun) < 4)
                nRun = nMaxRun - 1;
            // Break if we find 4 identical pixels ahead of us, which can be encoded as a run
            else if (pb_[nRun + 0] == pb_[nRun + 1] && pb_[nRun + 1] == pb_[nRun + 2] && pb_[nRun + 2] == pb_[nRun + 3])
                break;
        }

        EncodeAbsolute(pb_, nRun);
        pb_ += nRun;
        nLength_ -= nRun;
    }
}

bool Start(int flags)
{
    if (file)
        return false;

    avi_path = Util::UniqueOutputPath("avi");
    file = fopen(avi_path.c_str(), "wb+");
    if (!file)
    {
        Frame::SetStatus("Save failed: {}", avi_path);
        return false;
    }

    num_video_frames = num_audio_frames = num_audio_samples = 0;
    max_video_size = max_audio_size = 0;

    half_size = (flags & HALFSIZE) != 0;
    want_video = true;

    Frame::SetStatus("Recording AVI");
    return true;
}

void Stop()
{
    if (!file)
        return;

    WriteChunkEnd(file, movi_pos);
    WriteIndex(file);
    WriteChunkEnd(file, riff_pos);
    WriteFileHeaders(file);

    if (fseek(file, 0, SEEK_END) != 0)
        TRACE("!!! AVI::Stop(): Failed to seek to end of recording\n");

    file.reset();
    Frame::SetStatus("Saved {}", avi_path);
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
    if (!file || !want_video)
        return;

    // Old-style AVI has a 2GB size limit, so restart if we're within 1MB of that
    if (ftell(file) >= 0x7ff00000)
    {
        Stop();

        if (!Start(half_size ? HALFSIZE : FULLSIZE))
            return;
    }

    if (ftell(file) == 0)
    {
        // Store the dimensions, and allocate+invalidate the frame copy
        width = fb.Width() / (half_size ? 2 : 1);
        height = fb.Height() * (half_size ? 1 : 2);
        frame_buffer.resize(width * height);
        std::fill(frame_buffer.begin(), frame_buffer.end(), 0xff);

        // Write the placeholder file headers
        WriteFileHeaders(file);
    }

    // Set a key frame once per second, which encodes the full frame
    auto is_key_frame = !(num_video_frames % EMULATED_FRAMES_PER_SECOND);

    auto pos = WriteChunkStart(file, "00dc");

    int x, nFrag, jump_len = 0, jump_x = 0, nJumpY = 0;

    for (int y = height - 1; y > 0; y--)
    {
        auto line_ptr = fb.GetLine(y >> (half_size ? 0 : 1));
        static std::array<uint8_t, GFX_PIXELS_PER_LINE> line_buffer;

        if (half_size)
        {
            // Sample the odd pixel for mode 3 lines
            line_ptr++;

            // Use only half the pixels for a low-res line
            for (int i = 0; i < width; i++)
                line_buffer[i] = line_ptr[i * 2];

            line_ptr = line_buffer.data();
        }

        auto pb = line_ptr;
        auto pbP = frame_buffer.data() + (width * y);

        for (x = 0; x < width; )
        {
            // Use the full width as a different fragment if this is a key frame
            // otherwise find the next section different from the previous frame
            nFrag = is_key_frame ? width : FindRunFragment(pb, pbP, width - x, jump_len);

            // If we've nothing to encode, advance by the jump block
            if (!nFrag)
            {
                jump_x += jump_len;

                x += jump_len;
                pb += jump_len;
                pbP += jump_len;

                continue;
            }

            // Convert negative jumps to positive jumps on the following line
            if (jump_x < 0)
            {
                fputc(0x00, file); // escape
                fputc(0x00, file); // eol

                jump_x = x;
                nJumpY--;
            }

            // Completely process the jump, positioning us ready for the data
            while (jump_x | nJumpY)
            {
                int ndX = std::min(jump_x, 255), ndY = std::min(nJumpY, 255);

                fputc(0x00, file); // escape
                fputc(0x02, file); // jump
                fputc(ndX, file);  // dx
                fputc(ndY, file);  // dy

                jump_x -= ndX;
                nJumpY -= ndY;
            }

            // Encode the fragment
            EncodeBlock(pb, nFrag);

            // Advance by the size of the fragment
            x += nFrag;
            pb += nFrag;
            pbP += nFrag;
        }

        // Update our copy of the frame line
        memcpy(frame_buffer.data() + (width * y), line_ptr, width);

        // Jump to the next line
        nJumpY++;
        jump_x -= x;
    }

    fputc(0x00, file); // escape
    fputc(0x01, file); // eoi

    auto video_size = WriteChunkEnd(file, pos);
    max_video_size = std::max(video_size, max_video_size);
    num_video_frames++;

    want_video = false;
}

void AddFrame(const uint8_t* buffer, unsigned int len)
{
    if (!file || want_video)
        return;

    auto pos = WriteChunkStart(file, "01wb");
    fwrite(buffer, len, 1, file);

    auto num_samples = len / BYTES_PER_SAMPLE;
    num_audio_samples += num_samples;
    num_audio_frames++;

    auto audio_size = WriteChunkEnd(file, pos);
    max_audio_size = std::max(audio_size, max_audio_size);

    want_video = true;
}

} // namespace AVI
