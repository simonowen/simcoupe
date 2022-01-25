// license:BSD-3-Clause
// copyright-holders:Joseph Zbiciak,Tim Lindner
/**********************************************************************

    SP0256 Narrator Speech Processor emulation

**********************************************************************
                            _____   _____
                   Vss   1 |*    \_/     | 28  OSC 2
                _RESET   2 |             | 27  OSC 1
           ROM DISABLE   3 |             | 26  ROM CLOCK
                    C1   4 |             | 25  _SBY RESET
                    C2   5 |             | 24  DIGITAL OUT
                    C3   6 |             | 23  Vdi
                   Vdd   7 |    SP0256   | 22  TEST
                   SBY   8 |             | 21  SER IN
                  _LRQ   9 |             | 20  _ALD
                    A8  10 |             | 19  SE
                    A7  11 |             | 18  A1
               SER OUT  12 |             | 17  A2
                    A6  13 |             | 16  A3
                    A5  14 |_____________| 15  A4

**********************************************************************/

/*
   GI SP0256 Narrator Speech Processor

   By Joe Zbiciak. Ported to MESS by tim lindner.

*/

#pragma once

using stream_sample_t = int16_t;

class sp0256_device
{
public:
    sp0256_device(uint32_t clock);
    void load_rom(uint16_t base_addr, const std::vector<uint8_t>& rom);

    void ald_w(uint8_t data);
    uint16_t spb640_r(uint16_t offset);
    void spb640_w(uint16_t offset, uint16_t data);

    void reset();

    void sound_stream_update(stream_sample_t *output, int samples);

private:
    struct lpc12_t
    {
        int update(int num_samp, int16_t *out, uint32_t *optr);
        void regdec();

        int       rpt, cnt;       // Repeat counter, Period down-counter.
        uint32_t  per, rng;       // Period, Amplitude, Random Number Generator
        int       amp;
        int16_t   f_coef[6];      // F0 through F5.
        int16_t   b_coef[6];      // B0 through B5.
        int16_t   z_data[6][2];   // Time-delay data for the filter stages.
        uint8_t   r[16];          // The encoded register set.
        int     interp;

    private:
        static int16_t limit(int16_t s);
    };

    uint32_t getb(int len);
    void micro();

    std::array<uint8_t, 0x10000> m_rom{}; // 64K ROM.

    bool    m_silent{};        // Flag: SP0256 is silent.

    std::vector<int16_t> m_scratch{};  // Scratch buffer for audio.
    uint32_t    m_sc_head{};   // Head pointer into scratch circular buf
    uint32_t    m_sc_tail{};   // Tail pointer into scratch circular buf

    lpc12_t m_filt{};          // 12-pole filter
    int     m_lrq;             // Load ReQuest.  == 0 if we can accept a load
    int     m_ald{};           // Address LoaD.  < 0 if no command pending.
    int     m_pc{};            // Microcontroller's PC value.
    int     m_stack{};         // Microcontroller's PC stack.
    bool    m_fifo_sel{};      // True when executing from FIFO.
    bool    m_halted{};        // True when CPU is halted.
    uint32_t    m_mode{};      // Mode register.
    uint32_t    m_page{};      // Page set by SETPAGE

    uint32_t    m_fifo_head{}; // FIFO head pointer (where new data goes).
    uint32_t    m_fifo_tail{}; // FIFO tail pointer (where data comes from).
    uint32_t    m_fifo_bitp{}; // FIFO bit-pointer (for partial decles).
    std::array<uint16_t, 64> m_fifo{}; // The 64-decle FIFO.

    std::vector<int16_t> m_window{};
    int32_t m_wind_sum{};   // current resample sum
    size_t m_wind_ptr{};    // resample window index
    int m_sample_frc{};     // resample frequency
    int m_rate{};           // output device frequency
};
