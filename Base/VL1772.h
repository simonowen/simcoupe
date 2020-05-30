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

#define MAX_DISK_SIDES          2       // Maximum number of sides for a disk image

#define MAX_DISK_TRACKS         82      // Maxmimum number of tracks per side (docs say 0 to 244?)
#define MAX_TRACK_SIZE          6250    // Maximum raw track data size (bytes)
#define MAX_TRACK_SECTORS       (MAX_TRACK_SIZE - MIN_TRACK_OVERHEAD) / (MIN_SECTOR_OVERHEAD + MIN_SECTOR_SIZE)

#define MIN_SECTOR_SIZE         128     // Minimum sector size in bytes
#define MAX_SECTOR_SIZE         1024    // Maximum sector size in bytes

#define MIN_TRACK_OVERHEAD      32      // 32 bytes of 0x4e at the start of a track
#define MIN_SECTOR_OVERHEAD     95      // 22+12+3+1+6+22+8+3+1+1+16 - see CDrive::WriteTrack() for details

#define FLOPPY_RPM              300     // Floppy spins at 300rpm

// Register values in bottom 2 bits of I/O port values
enum { regCommand = 0, regStatus = regCommand, regTrack, regSector, regData };


// VL1772 registers
struct VL1772Regs
{
    uint8_t bCommand = 0;      // Command register (write-only)
    uint8_t bStatus = 0;       // Status register (read-only)

    uint8_t bTrack = 0xff;     // Track number register value
    uint8_t bSector = 1;       // Sector number register
    uint8_t bData = 0;         // Data read from and to write to the controller

    bool fDir = false;      // Direction flag: true to step out, false to step in (for non-specific STEP_ commands)
};

// Status register status bits
#define BUSY                0x01    // Wait BUSY=0 for a new command
#define INDEX_PULSE         0x02    // Index pulse (after type 1 command)
#define DRQ                 0x02    // Need to send or read data from DATA register (after type 2 or 3 command)
#define TRACK00             0x04    // Signals head on track 00 (after type 1 command)
#define LOST_DATA           0x04    // Error (e.g. you did not respect I/O timings) (after type 2 or 3 command)
#define CRC_ERROR           0x08    // Data corrupt
#define SEEK_ERROR          0x10    // Seek error (after type 1 command)
#define RECORD_NOT_FOUND    0x10    // Non-existent track/sector or no more data to read (after type 2 or 3 command)
#define SPIN_UP             0x20    // Motor spin-up complete (after type 1 command)
#define DELETED_DATA        0x20    // Record type: 0=data mark, 1=deleted data mark (after type 1 command)
#define WRITE_FAULT         0x20    // Write fault (after type 2 or 3 command)
#define WRITE_PROTECT       0x40    // Disk is write protected
#define MOTOR_ON            0x80    // Motor is on or drive not ready

#define TYPE1_ERROR_MASK        (CRC_ERROR|SEEK_ERROR)
#define TYPE23_ERROR_MASK       (LOST_DATA|CRC_ERROR|RECORD_NOT_FOUND|WRITE_FAULT)


// The VL1772 commands
//
// The lower 4 bits of the command byte have a different meaning depending on the
// command class, and need to be ORed with the command codes given below.

#define FDC_COMMAND_MASK    0xf0

// Type 1 commands
#define RESTORE             0x00    // Restore disk head to track 0
#define SEEK                0x10    // Seek a track (send the track number to the DATA reg)
#define STEP_NUPD           0x20    // Step using current dir without updating track register
#define STEP_UPD            0x30    // Step drive using current direction flag
#define STEP_IN_NUPD        0x40    // Step in without updating track register
#define STEP_IN_UPD         0x50    // Step in and increment track register
#define STEP_OUT_NUPD       0x60    // Step out without updating track register
#define STEP_OUT_UPD        0x70    // Step out and decrement track register

// Type 1 command flag bits
#define CMD_FLAG_STEP_RATE  0x03    // Stepping rate bits: 00=6ms, 01=12ms, 10=2ms, 11=3ms
#define CMD_FLAG_VERIFY     0x04    // Verify destination track
#define CMD_FLAG_DIR        0x20    // Step direction (non-zero for stepping out towards track 0)
#define CMD_FLAG_SPINUP     0x08    // Enable spin-up sequence
#define CMD_FLAG_UPDATE     0x10    // Update track register
#define CMD_FLAG_STEPDIR    0x40    // Step in a specific direction


// Type 2 commands
#define READ_1SECTOR        0x80    // Read one sector
#define READ_MSECTOR        0x90    // Read multiple sectors
#define WRITE_1SECTOR       0xa0    // Write one sector
#define WRITE_MSECTOR       0xb0    // Write multiple sectors


// Type 3 commands
//
//  b0-b1 = 0
//  b2 = 15 ms delay
//  b3 = 0

#define READ_ADDRESS        0xc0    // Read address
#define READ_TRACK          0xe0    // Read a whole track
#define WRITE_TRACK         0xf0    // Write a whole track


// Type 4 commands
//
//  b0 = Not ready to read transition
//  b1 = Ready to not read transition
//  b2 = Index pulse
//  b3 = Immediate interrupt, requires reset
//  b0-b3 = 0000 -> Terminate with no interrupt

#define FORCE_INTERRUPT     0xd0    // Force interrupt (also resets to type 1 mode)


////////////////////////////////////////////////////////////////

// Structure of the ID field that precedes each sector in a raw track
struct IDFIELD
{
    uint8_t bTrack;         // Track number
    uint8_t bSide;          // Side number (0 or 1)
    uint8_t bSector;        // Sector number
    uint8_t bSize;          // 128 << bSize = sector size in bytes: 0=128K, 1=256K, 2=512K, 3=1024K

    uint8_t bCRC1;          // ID field CRC MSB
    uint8_t bCRC2;          // ID field CRC LSB
};
