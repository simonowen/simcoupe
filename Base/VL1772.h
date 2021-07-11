// Part of SimCoupe - A SAM Coupe emulator
//
// VL1772.h: VL 1772 floppy disk controller definitions
//
//  Copyright (c) 1999-2012  Simon Owen
//  Copyright (c) 1999-2001  Allan Skillman
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

#pragma once

constexpr auto MAX_DISK_CYLS = 82;
constexpr auto MAX_DISK_HEADS = 2;
constexpr auto MIN_SECTOR_SIZE = 128;
constexpr auto MAX_SECTOR_SIZE = 1024;
constexpr auto VL1772_SIZE_MASK = 0x03;

constexpr auto MIN_TRACK_OVERHEAD = 32;     // 32 bytes of 0x4e at the start of a track
constexpr auto MIN_SECTOR_OVERHEAD = 95;    // 22+12+3+1+6+22+8+3+1+1+16

constexpr size_t MAX_TRACK_SIZE = 6250;
constexpr auto MAX_TRACK_SECTORS = (MAX_TRACK_SIZE - MIN_TRACK_OVERHEAD) / (MIN_SECTOR_OVERHEAD + MIN_SECTOR_SIZE);
constexpr auto MAX_DISK_TRACKS = MAX_DISK_CYLS * MAX_DISK_HEADS;

constexpr auto FLOPPY_RPM = 300;

enum class VLReg : uint8_t { command = 0, status = 0, track = 1, sector = 2, data = 3};

struct VL1772Regs
{
    uint8_t command = 0;
    uint8_t status = 0;
    uint8_t cyl = 0xff;
    uint8_t sector = 1;
    uint8_t data = 0;

    bool dir_out = false;
};

// Status register status bits
constexpr uint8_t BUSY = 0x01;              // Wait BUSY=0 for a new command
constexpr uint8_t INDEX_PULSE = 0x02;       // Index pulse (after type 1 command)
constexpr uint8_t DRQ = 0x02;               // Need to send or read data from DATA register (after type 2 or 3 command)
constexpr uint8_t TRACK00 = 0x04;           // Signals head on track 00 (after type 1 command)
constexpr uint8_t LOST_DATA = 0x04;         // Error (e.g. you did not respect I/O timings) (after type 2 or 3 command)
constexpr uint8_t CRC_ERROR = 0x08;         // Data corrupt
constexpr uint8_t SEEK_ERROR = 0x10;        // Seek error (after type 1 command)
constexpr uint8_t RECORD_NOT_FOUND = 0x10;  // Non-existent track/sector or no more data to read (after type 2 or 3 command)
constexpr uint8_t SPIN_UP = 0x20;           // Motor spin-up complete (after type 1 command)
constexpr uint8_t DELETED_DATA = 0x20;      // Record type: 0=data mark, 1=deleted data mark (after type 1 command)
constexpr uint8_t WRITE_FAULT = 0x20;       // Write fault (after type 2 or 3 command)
constexpr uint8_t WRITE_PROTECT = 0x40;     // Disk is write protected
constexpr uint8_t MOTOR_ON = 0x80;          // Motor is on or drive not ready

constexpr uint8_t TYPE1_ERROR_MASK = (CRC_ERROR | SEEK_ERROR);
constexpr uint8_t TYPE23_ERROR_MASK = (LOST_DATA | CRC_ERROR | RECORD_NOT_FOUND | WRITE_FAULT);

// The VL1772 commands
//
// The lower 4 bits of the command byte have a different meaning depending on the
// command class, and need to be ORed with the command codes given below.

constexpr uint8_t FDC_COMMAND_MASK = 0xf0;

// Type 1 commands
constexpr uint8_t RESTORE = 0x00;           // Restore disk head to track 0
constexpr uint8_t SEEK = 0x10;              // Seek a track (send the track number to the DATA reg)
constexpr uint8_t STEP_NUPD = 0x20;         // Step using current dir without updating track register
constexpr uint8_t STEP_UPD = 0x30;          // Step drive using current direction flag
constexpr uint8_t STEP_IN_NUPD = 0x40;      // Step in without updating track register
constexpr uint8_t STEP_IN_UPD = 0x50;       // Step in and increment track register
constexpr uint8_t STEP_OUT_NUPD = 0x60;     // Step out without updating track register
constexpr uint8_t STEP_OUT_UPD = 0x70;      // Step out and decrement track register

// Type 1 command flags
constexpr uint8_t CMD_FLAG_STEP_RATE = 0x03;// Stepping rate bits: 00=6ms, 01=12ms, 10=2ms, 11=3ms
constexpr uint8_t CMD_FLAG_VERIFY = 0x04;   // Verify destination track
constexpr uint8_t CMD_FLAG_DIR = 0x20;      // Step direction (non-zero for stepping out towards track 0)
constexpr uint8_t CMD_FLAG_SPINUP = 0x08;   // Enable spin-up sequence
constexpr uint8_t CMD_FLAG_UPDATE = 0x10;   // Update track register
constexpr uint8_t CMD_FLAG_STEPDIR = 0x40;  // Step in a specific direction

// Type 2 commands
constexpr uint8_t READ_1SECTOR = 0x80;      // Read one sector
constexpr uint8_t READ_MSECTOR = 0x90;      // Read multiple sectors
constexpr uint8_t WRITE_1SECTOR = 0xa0;     // Write one sector
constexpr uint8_t WRITE_MSECTOR = 0xb0;     // Write multiple sectors

// Type 2 command flags
constexpr uint8_t CMD_FLAG_MULTIPLE = 0x10;

// Type 3 commands
//
//  b0-b1 = 0
//  b2 = 15 ms delay
//  b3 = 0

constexpr uint8_t READ_ADDRESS = 0xc0;      // Read address
constexpr uint8_t READ_TRACK = 0xe0;        // Read a whole track
constexpr uint8_t WRITE_TRACK = 0xf0;       // Write a whole track

// Type 4 commands
//
//  b0 = Not ready to read transition
//  b1 = Ready to not read transition
//  b2 = Index pulse
//  b3 = Immediate interrupt, requires reset
//  b0-b3 = 0000 -> Terminate with no interrupt

constexpr uint8_t FORCE_INTERRUPT = 0xd0;    // Force interrupt (also resets to type 1 mode)

////////////////////////////////////////////////////////////////

struct IDFIELD
{
    uint8_t cyl;
    uint8_t head;
    uint8_t sector;
    uint8_t size;
    uint8_t crc1;
    uint8_t crc2;
};

constexpr size_t SizeFromSizeCode(uint8_t size_code)
{
    return static_cast<size_t>(128) << (size_code & VL1772_SIZE_MASK);
}
