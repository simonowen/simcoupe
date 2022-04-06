# SimCoupe - A SAM Coupé Emulator

By Simon Owen (simon@simonowen.com)

## Introduction

SimCoupe emulates the SAM Coupé - a British Z80-based home computer released in
1989 by Miles Gordon Technology. See the Links section at the end of this
document for more information, including history and technical specifications.

---

## Loading Software

SAM starts up ready to accept BASIC programs, with software loaded from tape or
disk.

The steps required to boot a disk on a real SAM are:

   1) Press the reset button to Return to the start-up screen
   2) Insert disk in floppy drive 1
   3) Press F9 (or enter BOOT) to boot the disk

The equivalent steps in SimCoupe are:

   1) Press F12 to reset the emulated SAM
   2) Press F1 to browse for a disk image
   3) Press Numpad-9 or type BOOT and press Enter

The default SimCoupe settings actually make the final step unnecessary, as disks
inserted into drive 1 at the SAM start-up screen are booted automatically. To
boot a disk inserted at any other time, press F12 to reset then Numpad-9 to boot
(Numlock must be On).

Commercial titles will start automatically when booted, but some SAM disks found
online may not. Here are the common failure messages:

```
55 Missing Disk, 0:1
```
There's no disk in drive 1 - did you insert into drive 2 instead?

```
19 Loading error, 0:1
```
The boot sector (track 4, sector 1) could not be loaded. The disk could be
unformatted or may be damaged.

```
53 No DOS, 0:1
```
The disk does not have a valid boot sector, and cannot be booted. The default
SimCoupe settings avoid this error by substituting an internal DOS image, so
you're more likely to see the following error instead.

```
101 no AUTO* file, 0:1
```
DOS was booted, but no filename starting with "auto" was found to load. To avoid
this error, boot using BOOT 1 instead, which loads DOS but doesn't attempt to
auto-run any file. Try DIR 1 to list the files on drive 1, then LOAD n, where n
is a file number to load.

```
0 OK, 0:1
```
DOS was loaded and an "auto" file was found, but there was no auto-run line
number to execute from. Use LIST to check for a BASIC listing, and RUN to
execute it. Otherwise use DIR 1 for a directory listing, to manually select a
file to load.

---

## Disk Images

SimCoupe can use software in the following disk image types:

 `.MGT` - Simple sector dump of +D/SAM disks: 2 sides, 80 tracks/side, 10
 sectors/track, 512 bytes/sector = 819200 bytes. Older images in this format may
 have a .dsk file extension. This is the preferred format for normal format SAM
 disks.

 `.SAD` - SAm Disk format, created by Aley Keprt. Also a sector-based dump, but
 with a 22-byte file header allowing disk-wide geometry adjustments to
 sides/disk, tracks/side, sectors/track and bytes/sector. Normal SAM disks
 stored in SAD format are 819222 bytes, but a difference in track order prevents
 removing the 22-byte header to give an equivalent MGT image. Version 2 SAD
 images are the same basic format, but compressed using gzip.

 `.DSK` - Extended DSK (EDSK) images, originally used for Amstrad CPC and
 Spectum +3 disks. A flexible format able to represent all existing SAM disks.
 Images size is proportional to the disk geometry, with a normal SAM disk format
 around 840K.

 `.SBT` - Sam BooTable files, created by Andrew Collier. These are self-booting
 files designed to be copied to an empty SAM disk, then booted. While not
 technically disk images, SimCoupe treats them as such (read-only).

TeleDisk .TD0 (and other) images can be converted to EDSK using SAMdisk.

---

## Real Disks

If you are a Windows user and your PC motherboard still includes a floppy port
you can use many original SAM disks directly in SimCoupe. Even custom-formatted
titles such as Lemmings and Prince of Persia can be booted directly.

To use this you must connect your floppy drive directly to the motherboard using
a standard 34-pin cable. Very few modern motherboards still support this. USB
floppy drives will NOT work as they can't read the 10th sector from tracks.

You must also install a free 3rd party driver to give full access to the drive.
This is a one-time install and must be run by a user with Administrator access
rights. The driver installer is available from: https://simonowen.com/fdrawcmd/

To use real disks under Windows 7 or later:

   1) Install the fdrawcmd.sys driver, if not already installed
   2) Insert your SAM disk into PC drive A:
   3) Select "Open A:" from the File menu to use as SAM drive 1
   4) Boot/use the disk as normal

---

## Hard Disks

SimCoupe emulates the Atom and Atom Lite hard disk interfaces. Atom's DOS (BDOS)
is compatible with the original SAMDOS. The hard disk is treated as an array of
floppy-sized records, giving good compatibility with existing software.

Real hard disks and compact flash (CF) cards can be used under Windows, Linux
and Mac OS X. This gives a fast and reliable way to share data between with a
real SAM machine. For safety, only USB-connected disks can be opened in
SimCoupe.

If you don't have a real device to use, you can still work with existing hard
disk images files (HDF):

   1) Press F10 to open the SimCoupe options
   2) In the Disks section, click "..." next to the Atom interface type
   3) Browse to select an existing HDF image file to use
   4) Click OK twice and you're ready to boot BDOS

To create and prepare a new HDF image:

   1) Press F10 to open the SimCoupe options
   2) In the Disks section, click "..." next to the Atom interface type
   3) Enter a new path and size in megabytes (32MB = 40 floppy records)
   4) Click OK twice to create the disk and return to the emulation
   5) Boot a BDOS version (1.6e is recommended)
   6) With "Floppy drive" selected, press Cursor-right to view the files
   7) Select "Formatter" and press Enter
   8) Press Enter again, then "Y" to begin the Atom format
   9) Your HDF image is now ready to use!

For more details on using BDOS, visit Edwin Blink's pages:
  http://www.samcoupe-pro-dos.co.uk/edwin/software/bdos/bdos.htm

### Windows 7 or later

  1) Insert your CF card in the USB reader
  2) In SimCoupe, press F10 to open the options
  3) In the Disks section, click the combo-box down-arrow for the Atom
  4) Select the device name to use.
  5) Click OK twice and you're ready to boot BDOS

### Linux

The details for discovering the IDE/CF device path will depend on the Linux
distribution you're using, but is usually one of the following:

  - For IDE hard disks, try `fdisk -l` as root to list available devices.
  - For USB devices, try `tail -f /var/log/messages` then inserting it.
  - Failing those, browse through the raw `dmesg | less` output.

Once you've found the device path:

  1) In SimCoupe, press F10 to open the options
  2) In the Disks section, enter the device path for the Atom hard disk
  3) Click OK then Close, and you're ready to boot BDOS

### macOS

  1) Insert the CF card in your USB reader
  2) When warned about an unreadable disk, click Ignore
  3) Open Disk Utility and select your CF volume (orange USB icon)
  4) Click Info on the toolbar, and find the Disk Identifier (such as disk1)
  5) Prefix the identifier with `/dev/r` for the device path (`/dev/rdisk1`)
  6) In SimCoupe, press Cmd-F10 to open the options dialog
  7) In the Disks section, enter the device path for the Atom hard disk
  8) Click OK then Close, and you're ready to boot BDOS!

In most cases you only need to determine the device path once, as it will be the
same the next time you insert it. It will only change if other disk devices have
been added/removed.

---

## Printing

SimCoupe supports print-to-file on all platforms, making it easy to export code
listings to a plain text file. Configure as follows:

   1) Press F10 to open the options
   2) Select the Parallel section
   3) Under the Port 1 drop-down, select Printer
   4) In the Printer Device section, select "File: prntNNNN.txt"
   5) Click OK to accept the new settings.

You can now print from most applications, and use LLIST to print BASIC listings.
The output will be saved to a file in your Data Files directory, with a unique
name matching the template "prntNNNN.txt".

---

## Keyboard Input

The default SAM keyboard mode allows letters, digits and symbols to be typed as
normal on your keyboard, with SimCoupe automatically converting them to the
appropriate SAM key sequence. There's also a Spectrum mapping mode to use when
running Spectrum software, and a raw mode to disable the mappings.

The SAM has a keypad of function keys from F0 to F9 located on the right-side of
the keyboard. For similar key positions in SimCoupe, the SAM keypad is mapped to
the numeric keypad on your keyboard. You'll need to have Numlock enabled for
these keys to be recognised. Don't forget that when SAM software refers to
function keys, you must use the numeric keypad instead!

F1 to F12 keys on your keyboard are used for emulator functions, with the
default mappings shown below. Under OS X, keys F9 to F12 are used by Expose and
Dashboard, so you'll need to hold the Command key in addition to the
combinations below to access them.

Holding the Windows key and pressing F1 to F10 will generate the corresponding
SAM function key F1 to F0. Recent versions of Windows process Win-F1 as help,
so Win-F11 can also be used to access the SAM F1 key.

```
             F1 = Open disk 1
       Shift-F1 = Eject disk 1
        Ctrl-F1 = Save disk 1
         Alt-F1 = New disk 1
             F2 = Open disk 2
       Shift-F2 = Eject disk 2
        Ctrl-F2 = Save disk 2
         Alt-F2 = New disk 2
             F3 = Tape browser
             F4 = Import data
       Shift-F4 = Export data
         Alt-F4 = Exit application
             F5 = Toggle TV aspect ratio
             F6 = Toggle display smoothing
             F8 = Toggle full-screen
             F9 = Debugger
       Shift-F9 = Save SAM screenshot in PNG format
            F10 = Options
            F11 = NMI Button
            F12 = Reset button
       Ctrl-F12 = Exit application
         Win-Fx = SAM function key Fx

      PrintScrn = Save SAM screenshot in PNG format
          Pause = Pause emulation
     Ctrl-Break = Reset
  Ctrl-Keypad * = Reset
  Ctrl-Keypad - = Normal emulation speed
       Keypad - = Reduce emulation speed
       Keypad + = Increase emulation speed
       Keypad * = Turbo speed
```

Turbo speed disables the frame sync and sound, and limits the display to just 5
frames per second. This usually gives a big speed boost, which is useful for
zooming through slow sections in games and demos, etc.


SAM shift modifier keys and special symbols are mapped as follows:
```
         Insert = Inv
      Left-Ctrl = Symbol
     Right-Ctrl = Cntrl
       Left-Alt = Cntrl (if enabled)
      Right-Alt = Edit  (if enabled)
       Menu Key = Edit
  ` (backtick)  = (c)
  . (on keypad) = (c)
    § (section) = #
```

The following additional combinations are also provided for convenience, since
they map common keys to the equivalent function on the SAM:

```
     Native key   SAM key
     ----------   -------
         Delete = Shift-Delete
        Numlock = Symbol-Edit  (toggles SAM BASIC keypad mode)
           Home = Cntrl-Left
            End = Cntrl-Right
        Page Up = F4
      Page Down = F1
```

---

## Debugger

The debugger provides code, text, data and graphical views of memory, with
typical debugging functions such as single-stepping and breakpoints.

The debugger starts in disassembly view, highlighting the next instruction.
Symbols are shown for ROM locations, and for custom code if built with pyz80
using the --mapfile= option. The register panel on the right shows the current
system state.

### Register panel

The first 7 lines of the panel show Z80 register values, with changed registers
display in pink text. Below it are the current interrupt mode, and the interrupt
state (EI or DI). The arrows below the SP value point to the top of the stack,
where the top 4 values are shown.

**IM** shows the current interrupt mode, and IFF1 status (DI/EI).

**Stat** shows the 5 interrupt status flags: O=midi-out, F=frame, I=midi-in,
M=mouse and L=line. These letters are visible when the corresponding interrupt
type is active (low) in the status port (F9/249).

**Scan** shows the line number (0-311) and line-cycle counter (0-383) for the
current raster position within the frame. If the position is within the visible
area, the raster is shown on the display as a pulsing white dot.

**T** shows the cycle offset within the current frame (0-119807). Below it is
the number of cycles since the debugger was last active. During single stepping
this shows the timing for the last instruction, including any I/O contention
delays. Stepping over an instruction will give the total time for the step.

**A/B/C/D** show the page present in each of the four 16K mamory banks. This
will be ROM 0-1, RAM 00-1F for internal memory, or EXT 00-FF for external
memory. Cyan text indicates the area is read-only.

**L/H/V/M** show the current LMPR/HMPR/VMPR paging register values, plus the
current display mode (1-4).

**Events** shows upcoming events, and the number of cycles before they are due:
```
            FINT = start of frame interrupt
            FEND = end of frame interrupt
            LINT = start of line interrupt
            LEND = end of line interrupt
            MIDI = MIDI OUT interrupt
            MEND = end of active MIDI OUT interrupt
            MTXF = end of TXFMST active in LMPR
            MOUS = mouse reset after strobe
            BLUE = Blue Alpha clock tick
            TAPE = next tape edge due
            ASIC = end of ASIC startup sequence
```

Keys active in all views:
```
               A = enter new view address
               B = breakpoint list
               C = code trace history
               D = disassembly view
               G = graphics view
               H = change HMPR page
               L = change LMPR page
               M = change screen mode
               N = number view
               T = text view
               V = change VMPR page
        Keypad-0 = toggle ROM0
        Keypad-1 = toggle ROM1
        Keypad-2 = toggle RAM write-protection
        Keypad-3 = toggle external RAM
          Ctrl-T = toggle debugger transparency
             Esc = exit debugger, or return to disassembly view
```

Disassembly View:
```
               S = toggle symbol display
               U = execute until condition is met
        Keypad-7 = single step 1 instruction
        Keypad-8 = step over instruction
        Keypad-9 = step out of function
        Keypad-4 = step 10 instructions (10000)
    Keypad-4/5/6 = step 10/100/1000 instructions
    Ctrl-KP4/5/6 = step 10K/100K/1M instructions
   Ctrl-Keypad-8 = step over with code timing (no ints, border contention)
      Left/Right = scroll 1 byte
         Up/Down = scroll 1 instruction
       PgUp/PgDn = scroll 1 page
 Ctrl-Left/Right = move PC by 1 byte
    Ctrl-Up/Down = move PC by 1 instruction
          Return = debugger command mode (see below)
```

Text/Number View:
```
         Up/Down = scroll by 1 line
      Left/Right = scroll by 1 byte
       PgUp/PgDn = scroll by 1 page
```

Graphics View:
```
         1/2/3/4 = select screen mode
         Up/Down = scroll by 1 line
      Left/Right = scroll by 1 byte
    Ctrl-Up/Down = zoom in/out
 Ctrl-Left/Right = adjust column width by 1 byte
       PgUp/PgDn = scroll by 1 column
  Ctrl-PgUp/PgDn = scroll by 1 page
```

Trace View:
```
             Space = toggle single/double register display
                 S = toggle address symbol display
```

Debugger Command Mode:
```
           di / ei = disable/enable interrupts
              im M = set interrupt mode M (0-2)
             reset = reset emulation
               nmi = generate non-maskable interrupt
               zap = replace current instruction by NOP
         call ADDR = simulate call of address A
            push W = push 16-bit value W onto stack
         pop [reg] = pop 16-bit value, optionally to register
             break = set paging+interrupts in an attempt to return to BASIC
               x N = execute N instructions
    [x] until COND = execute until condition is true (one-shot)
          bpu COND = breakpoint on condition (permanent)
          bpx ADDR = execute breakpoint at ADDR with optional condition
 bpm ADDR [r|w|rw] = memory breakpoint with optional access specifier
 bpmr A B [r|w|rw] = memory range breakpoint from address A for length B
   bpio P [rw|r|w] = I/O breakpoint on port P
           bpint I = breakpoint on interrupt (frame/line/midi/midiin/midiout)
 flag +|- sz5h3vnc = set and/or reset flag bits
              bc N = clear breakpoint N (* for all)
              bd N = disable breakpoint N
              bd N = enable breakpoint N
               exx = exchange BC/DE/HL with BC'/DE'/HL'
          ex de,hl = exchange DE with HL
            ld R,N = load register R with value N
             r R=N = load register R with value N
           out P,N = write value N to port P
poke A,N1[,N2,...] = poke address A with one or more values
```

Breakpoint addresses are resolved to a physical location, so they'll trigger if
the same underlying memory is access from a different paging position. They can
also be specified in page:offset format for an explicit location e.g.  bpx 1:0
will break when page 1 offset 0 (BASIC address 0x8000/32768) is executed.

Breakpoints accept an optional condition, by adding `if COND` to the end of the
command. If present, the breakpoint will only trigger when the expression is
true.

Single-stepping a HALT instruction will step into the interrupt handler,
assuming interrupts are enabled. Stepping over a HALT will completely execute
the handler, as if stepping over a call. Step-over also recognises JP/JR
instructions, and will single-step to follow the jump rather than attempting to
step over it.

To return to the current execution point after browsing other memory locations,
press A to enter a new address and enter `pc` as the expression. Alternatively,
single-step and the view will automatically return to the next instruction.

To aid to debugging, conditional instructions show whether or not the condition
is met by the current flags. If execution flow is changing, the current
highlight changes from yellow to green, and an arrow indicates whether the
change is above or below the current location.

Double-clicking on an instruction in disassembly view will set an execution
breakpoint for that address.

### Numeric expressions

 Operators:
 ```
   Unary:  + - ~ ! * =
   Binary arithmetic:  + - * / \ %
   Logical:  && || and or
   Comparison:  == != <> < > <= >=
   Bitwise arithmetic:  & | ^ band bor bxor
   Bitwise shift:  << >>
```

 ### Symbols
 ```
   Single registers: a f b c d e h l i r ixh ixl iyh iyl
   Double registers: af bc de hl af' bc' de' hl' ix iy sp pc
   Paging: lpage hpage vpage vmode lepage hepage rom0 rom1 wprot
   Registers: lepr hepr lpen hpen status lmpr hmpr vmpr midi border addr
   Interrupts: ei di iff1 iff2 im
   I/O: inval outval
   Display: dline sline
   Execution: inrom call autoexec
```

#### Functions
 ```
    PEEK <addr> = 8-bit lookup in currently paged RAM
   DPEEK <addr> = 16-bit lookup in currently paged RAM
```

The '=' unary operator has a special use in expressions. Its operand is
evaluated immediately, and the value inserted in the expression instead of the
operand itself. The first example below shows why this can be useful.

Example `UNTIL` expressions:

 - Break when the current value of HL changes: `hl != =hl`
 - Break at the next HALT instruction: `peek pc == 0x76`
 - Break when 123 is written to any port: `outval == 0n123`
 - Break when screen mode 3 is selected: `vmode == 3`
 - Break when hex 1234 is top of stack: `dpeek sp == 1234`
 - Break when the raster is drawing screen line 0: `sline == 0`
 - Break when A, B and IXl are equal: `(a == b) && (b == IXl)`

Execute Until breakpoints are only temporary, and cleared when the debugger is
next activated, regardless of whether they were triggered. This also applies to
other simple breakpoints, such as step-out and step-over.

The debugger works natively in hexadecimal, but allows values to be entered in
other bases using an appropriate prefix or suffix:
```
       Decimal:  0n12345
   Hexadecimal:  1234 or 0x1234 or 1234h or $1234 or &1234 or #1234
     Character:  "a" or 'a'
        Binary:  %10101100
```

Octal is not supported, so leading zeroes have no special meaning.

---

## Options

The section below describes all setting available in the Options (`F10`):

### System

_Internal RAM_ - the base SAM model comes with only 256kB main memory, with an
internal add-on board to boost it to 512kB. Many software titles require 512kB
to work correctly.

_External memory_ - external add-on packs are available to extend memory in 1MB
blocks. Programs needed to be written specially to use external memory, with
only a few titles doing so. They include MasterDOS, MasterBASIC and the TopGun
Demo.

_Custom 32K ROM image_ - if blank a built-in v3.0 ROM image is used.

_Use AL-BOOT ROM if Atom Lite is connected_ - if the Drive 2 is configured as an
Atom Lite device, this option applies ROM patches to automatically boot from it.
This is not available if a custom ROM image is in use.

### Display

_Bi-linear fitering (smoothing)_ - smooth the display image when stretching to
fill the SimCoupe window.

### Sound

_SID Interface_ - selects the type of SID chip connected to the Quazar SID
interface. The (default) 6581 is the traditional chip found in the original C64
machines, with a gritty sound. The 8580 is the chip found in newer C64 models,
with a cleaner and brighter sound.

_DAC on port 7C_ - select the type of DAC device present on SAM's port 7C. The
Blue Alpha sampler has a custom clock frequency for variable speed playback. The
SAMVoc and Paula (no relation to the Amiga chip!) are simple DAC output devices.

_MIDI Out_ - select a device for MIDI output, which may be a real device or
software synthesizer.

### Parallel

_Port 1/2_ - selects the device to connect to the virtual printer port. With
Printer selected you have a choice of printing to a file or a real printer
device. Mono-DAC emulates an 8-bit mono sound device, and EDdac/SAMdac an 8-bit
stereo device. The latter is highly recommended for use with Stefan's SAM MOD
Player.

_Printer device_ - if Printer is selected above, this is the file or device to
use for output. The "File:" option auto-generates a unique file to hold the
output, and saves it to your Data Files path.

_Automatically flush print jobs_ - if no data is sent to the port within 2
seconds, any remaining print data with be flushed to the output device.

### Input

_Keyboard_ - in SAM Coupe mode letters and symbols are converted to the key
sequence required to generate the same symbol on SAM. For example, pressing
Shift-0 on a typical PC keyboard generates ')'. SimCoupe converts it to Shift-9
to generate the same symbol on SAM. If you require a literal Shift-0, select
Disabled from the mode list. Automatic mode also detects the presence of a
Spectrum ROM and will use ZX Spectrum mappings.

_Use Left-Alt for Cntrl key_ - maps the Left-Alt key to the Cntrl key on the SAM
keyboard, in addition to the Right-Ctrl key. Left-Alt is located in a similar
keyboard position to SAM's Cntrl key. Note: enabling this option blocks normal
Windows menu combinations, such as Alt-F for the File menu. However, you can
still press and release Alt to activate the menu, then press F to open the file
menu and navigate as normal.

_Use Alt-Gr for Edit key_ - maps the Right-Alt key to the Edit key on the SAM
keyboard. Alt-Gr is located in a similar keyboard position to SAM's Edit key.

_Enable mouse interface_ - select to use your mouse with supported SAM software.
Clicking on the SimCoupe window will activate the mouse when a program is
reading it. For BASIC use, double-click the SimCoupe window to active it. To
release mouse control for normal desktop use, press Esc or switch to another
task using the keyboard.

### Joystick

_Player 1/2_ - selects up to 2 devices to use for control input. Each can be set
to control SAM joystick 1 (keys 6,7,8,9+0), SAM joystick 2 (keys 1,2,3,4+5), or
a Kempston joystick used by Spectrum software.

### Drive 1

_Device_ - select whether floppy drive 1 is installed in the left-hand bay. The
ROM only supports booting from drive 1, but later loading is supported from
either drive.

_Media_ - select either a disk image file or real device from the drop-down list
(if supported).

### Drive 2

_Device_ - select the type of device installed in in the right-hand bay. This
can be None, Floppy, Atom Classic or Atom Lite.

_Media_ - select either a disk image file or real device from the drop-down list
(if supported). Atom and Atom Lite devices support Master and Slave drives,
which can be either HDF images or real devices. Only pre-formated hard disks and
compact flash cards are listed, and you'll also need sufficient permission to
access the raw disk devices. Under Windows this requires the user be running as
an Administrator (elevated), or having SAMdiskHelper installed. Under Linux the
user will often need to be a member of the 'disk' group.

### Misc

_SAMBUS clock_ - the most common clock hardware interface, as used by MasterDOS
and BDOS.

_DALLAS clock_ - advanced clock hardware, supported by BDOS.

_Show disk drive activity lights_ - enables on-screen LEDs in the top left of
the display, showing when the drive is active. Floppy drives are shown in green,
Atom in orange, and Atom Lite in blue.

_Show status messages_ - enables the display of status text in the bottom right
of the display. This are used to confirm various user actions, such as ejecting
disks and changing runtime options.

_Show emulation speed_ - show the percentage of normal SAM running speed in the
upper-right of the display window.

_Ask before saving disk image changes_ - prompts for confirmation before saving
modifications back to disk images when they are ejected. This doesn't apply to
changes saved manually using Ctrl-F1/F2.

### Helpers

_Fast boot after hardware reset_ - accelerates the cold-boot process, avoiding a
few seconds delay while main memory is tested.

_Fast floppy disk access_ - accelerates the emulation speed when disks are being
accessed, to speed up loading and saving.

_Auto-load media inserted at startup screen_ - automatically load disks and
tapes inserted when SAM is showing the stripey boot screen.

_Allow booting from non-bootable disks_ - detects booting from an unbootable
disk and temporarily replaces the boot disk with either an internal DOS image or
a user-specified disk. Once DOS has booted the original disk is restored and the
boot process continued.

---

## Command-line Options

SimCoupe supports the following command-line options, which override setting in
the configuration file:
```
    -scale <int>            Windowed mode scaling: 1=50%, 2=100%, 3=150%
    -tvaspect <bool>        TV aspect ratio (default=yes)
    -mode3 <bool>           Sample odd pixels in low-res (default=no)
    -fullscreen <bool>      Start in full-screen mode (default=no)
    -depth <int>            Colour depth for full-screen (default=16)
    -visiblearea <int>      0=No Border, 1=Small Border, 2=TV Visible,
                            3=Full Active (default=2)
    -greyscale <bool>       Greyscale mode (default=no)
    -filter <bool>          Smooth emulated display (default=yes)
    -filtergui <bool>       Smooth built-in GUI display (default=no)

    -rom <path>             32K custom ROM image (blank for default v3.0)
    -romwrite <bool>        Enable memory writes to ROM (default=no)
    -albootrom <bool>       Enable Atom Lite boot ROM patches (default=no)
    -fastreset <bool>       Skip SAM power-on memory test (default=yes)
    -asicdelay <bool>       ASIC delay on first start (default=yes)
    -mainmemory <int>       Main memory size in kB: 256 or 512 (default)
    -externalmem <int>      External memory size in MB: 0 (default) to 4
    -cmosz80 <bool>         CMOS rather than NMOS Z80 (default=no)
    -speed <int>            Emulator speed percentage (default=100)

    -drive1 <int>           Drive 1: 0=none, 1=floppy
    -drive2 <int>           Drive 2: 0=none, 1=floppy, 2=Atom, 3=Atom Lite
    -turbodisk <bool>       Fast disk access sensitivity (default=yes)
    -dosboot <bool>         Automagically boot DOS (default=yes)
    -dosdisk <path>         Custom DOS boot disk (blank for SamDos 2.2)
    -stdfloppy <bool>       Assume real disks are normal format (default=yes)

    -disk1 <path>           Disk image file for drive 1
    -disk2 <path>           Disk image file for drive 2
    -atomdisk0 <path>       Atom hard disk image or device path (Master)
    -atomdisk1 <path>       Atom hard disk image or device path (Slave)
    -autoload <bool>        Auto-load media at startup screen (default=yes)

    -turbotape <bool>       Fast tape access (default=yes)
    -tapetraps <bool>       Use tape traps for instant loading (default=yes)

    -inpath <path>          Default path for input files
    -outpath <path>         Default path for output files

    -keymapping <int>       Keyboard mapping: 0=none, 1=auto, 2=SAM, 3=ZX
    -altforcntrl <bool>     Use Left-Alt for SAM Cntrl key (default=no)
    -altgrforedit <bool>    Use Alt-Gr for SAM Edit key (default=yes)
    -mouse <bool>           Mouse interface enabled (default=no)
    -mouseesc <bool>        Esc to release mouse capture (default=yes)

    -joytype1 <int>         Joystick 1: 0=none, 1=Joy1, 2=Joy2, 3=Kempston
    -joytype2 <int>         Joystick 2: 0=none, 1=Joy1, 2=Joy2, 3=Kempston
    -joydev1 <string>       Joystick 1 device (default=none)
    -joydev2 <string>       Joystick 2 device (default=none)
    -deadzone1 <int>        Joystick 1 deadzone percentage (default=20)
    -deadzone2 <int>        Joystick 2 deadzone percentage (default=20)

    -parallel1 <int>        Parallel port 1 device: 0=none (default),
                             1=printer, 2=mono DAC, 3=stereo DAC
    -parallel2 <int>        Parallel port 2 device: 0=none (default),
                             1=printer, 2=mono DAC, 3=stereo DAC
    -printerdev <string>    Printer device name or path
    -printeronline <bool>   Printer online (default=yes)
    -flushdelay <int>       Printer flush delay in seconds (default=2)

    -midi <int>             0=none (default), 1=midi synth [Win32]
    -midiindev <string>     MIDI-in device name/path (future)
    -midioutdev <string>    MIDI-out device name/path

    -sambusclock <bool>     SAMBUS clock (default=yes)
    -dallasclock <bool>     DALLAS clock (default=no)

    -sound <bool>           Sound enabled (default=yes)
    -latency <int>          Sound latency: 1=best, 5=(default), 20=worst
    -dac7c <bool>           DAC on port 7C: 0=none, 1=Blue Alpha (default),
                            2=SAMVox, 3=Paula
    -samplerfreq <int>      Blue Alpha sampler frequency (defaut=18000)
    -sid <bool>             SID chip: 0=none, 1=6581 (default), 2=8580

    -drivelights <int>      Floppy drive LEDs: 0=none, 1=top, 2=bottom
    -profile <int>          Profiling stats: 0=off, 1=simple (default),
                             2=detailed percentage, 3=detailed timings
    -status <bool>          Show status messages (default=yes)

  Key:
    <bool>    0 or 1, true or false, yes or no
    <int>     an integer value in the range shown next to the parameter
    <string>  string of characters, in "quotes" if it contains spaces
    <path>    file/dir path, in "quotes" if it contains spaces
```

To restore the defaults settings, close SimCoupe and delete the file:
  - `%APPDATA%\SimCoupe\SimCoupe.cfg`  [Windows]
  - `~/.simcouperc`  [Linux]
  - `~/Library/Preferences/SimCoupe Preferences`  [Mac OS X]

---

## Links

SimCoupe Homepage:
  https://simonowen.com/simcoupe

SimCoupe project page:
  https://github.com/simonowen/simcoupe
  
World of Sam archive:
  https://www.worldofsam.org

Wikipedia entry for the SAM Coupe (and for more links):
  https://wikipedia.org/wiki/SAM_Coupé

---

## Disclaimer

THIS PROGRAM AND DOCUMENTATION ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
KIND, NOT EVEN THE IMPLIED WARRANTY OF MERCHANTABILITY OR FITNESS FOR A
PARTICULAR PURPOSE. BY USING THE PROGRAM, YOU AGREE TO BEAR ALL RISKS AND
LIABILITIES ARISING FROM THE USE OF THE PROGRAM AND DOCUMENTATION AND THE
INFORMATION PROVIDED BY THE PROGRAM AND THE DOCUMENTATION.
