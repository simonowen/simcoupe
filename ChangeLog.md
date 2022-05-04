# SimCoupe ChangeLog

## Version 1.2.11 (2022-05-04)
- fixed main screen contention offset.
- improved OpenGL 3 compatibility on macOS.
- added LPEN b0 support in screen modes 1-3.
- added WARP driver fallback for running in Windows VMs.
- added basic motion blur support to SDL backend.

## Version 1.2.10 (2022-04-07)
- fixed clipboard paste not working on boot screen.
- fixed broken Windows shell path auto-complete.
- fixed various issues affecting initial SDL window size
- improved portability of window position/size saving.
- improved validation of ZX82 ROM containers.
- moved samdos2 dosboot image to external resource file.
- added support for older SAM ROM images (#77)
- added manual play/pause to Win32 tape browser (#77)
- added missing tape auto-load triggers.
- added fast boot frame limit for broken or custom ROMs.

## Version 1.2.9 (2022-03-12)
- fixed read port MSB in block IN instructions (#75)
- fixed utf-8 path issues under Windows (#74)
- added additional Win-F11 shortcut for SAM F1 (#73)
- Win32 help now opens Manual.md instead of ReadMe.md

## Version 1.2.8 (2021-11-06)
- improved HALT implementation to better match real Z80
- improved behaviour of multiple breakpoints at same location (#70)
- added clipboard support for built-in GUI text inputs (#71)
- added basic session command history to debugger

## Version 1.2.7 (2021-10-20)
- fixed ATA IDENTIFY command returning too much data (#65)
- fixed command-line disk images not auto-booting (#69)
- fixed Win32 installer to add file associations (#66)
- fixed command-line disks not getting added to MRU list.
- changed Win32 installer to install x64 version if appropriate.
- corrected documentation, which suggested default base was decimal (#67)

## Version 1.2.6 (2021-09-10)
- fixed read-only disk error with Pro-Dos v2 (#64)
- fixed failure to recognise inserted tape images.
- fixed missing icon end marker that risked crash under Win32.
- changed Comet symbol export to always scrape from memory (#63)
- changed symbol export to save use local line endings (#63)
- changed to use MSVC runtime DLLs instead of static linking.

## Version 1.2.5 (2021-08-26)
- fixed CRLF issue reading old Linux/macOS configuration files (#62)
- fixed first reset after DI;HALT failing due to incomplete CPU reset.
- fixed file selector showing /home instead of full $HOME
- fixed RETI to restore iff1 from iff2 (undocumented).
- restored support for CMOS Z80 behaviour [out (c),0/255].
- restored register init for hard/soft reset.
- added support for in-memory Comet symbols. (#63)
- added support for exporting debug symbols to .map (#63)
- improved auto-type to ensure turbo mode is turned off.

## Version 1.2.4 (2021-08-22)
- fixed build optimisation on non-Windows versions.
- fixed truncated trace address labels (#63).
- added support for extracting Comet symbols (#63).
- improved trace formatting, adding disassembly symbols.
- restored memory contention tables for speed increase on some platforms.

## Version 1.2.3 (2021-08-15)
- fixed opening disk images inside zip archives (#61)
- fixed pasting of large BASIC listings
- added support for full size GIF recording (#59)
- added 50/25/16.7/12.5 framerates for GIF recording
- moved end-user instructions into Manual.md

## Version 1.2.2 (2021-08-02)
- fixed missing video update before display memory changes.
- fixed CPU emulation of undocumented BIT instructions (#56)
- fixed media warnings for unattached devices (#24)
- fixed unwanted ROM symbols in user code (#26)
- fixed ATA device revision string not showing date correctly.
- fixed menu issues, usually seen as duplicate entries [Win32].
- fixed disassembly of some DDCB undocumented instructions.
- restored full double register display in instruction trace (#55).
- added samdos2 symbols to debugger (#44)
- added address symbols to trace view (#43)
- added proper version number to About (#45)
- added hidden option to simulate disk read errors, for developers.
- added SAMdiskHelper version check to recommend upgrade [Win32].
- improved dosboot to only trigger in ROM1.
- increased menu MRU from 6 to 9 items [Win32].

## 20210707 dev (2021-07-07)
- fixed main screen memory contention alignment (#54).
- fixed debugger single-stepping just index prefixes (#53).
- fixed instruction trace showing double registers when only half changed.
- fixed step-out feature in debugger.
- fixed debugger benchmark mode still processing interrupts.
- fixed cancelled line interrupt end event showing in debugger.
- added 'tryvrr' option (default=true) to try Variable Refresh Rate video.
- added Ctrl modifier to run more instructions using the KP4/5/6.

## 20210624 dev (2021-06-24)
- improved frequency of generated sound output (#36)
- improved bus values from unhandled ports (fixes KEDisk)
- improved aspect ratio (59:54) to match signal details
- changed visible areas to better reflect what can be seen on a TV
- renamed borders option to visiblearea
- added alternate debugger display mode to give full refresh (#46)
- restored -autoboot command-line option (#49)
- removed prompt to save changes to disks
- fixed border artefact colour if using upper 8 colours
- fixed incorrect zoom handling when fullscreen/minimised/maximised
- fixed missing allophone data file in non-Windows installations
- fixed %s shown in disk insert message [SDL]
- fixed current debugger trace entry colour
- changed to more accurate Z80 CPU core
- changed to pull SAASound from official project at GitHub

## 20210501 dev (2021-05-01)
- fixed fullscreen and window position saving issues (#31)
- fixed crash opening empty tape image (#34)
- fixed frozen screen after keyboard speedup (#35)
- fixed Blue Alpha VoiceBox being detected when disabled (#40)
- fixed half-height GIF animations (#38)
- fixed cursor positioning in hex edit mode (#41)
- fixed some UTF-8 path encoding issues (#32)
- fixed issue selecting non-existent recent file
- renamed Atom Legacy to Atom Classic
- hide some OS key combinations from emulation
- allow debugger to be toggled

## 20200827 dev (2020-08-27)
- added support for Blue Alpha VoiceBox.
- changed default external memory to 1MB (new installs).
- changed motion blur to lerp instead of max.
- fixed error on minimising window [Win32]
- fixed wrong menu shortcut for motion blur (Shift-F6, not F7) [Win32]
- fixed AVI recording being half height.
- fixed New Disk only working if disk already existed.
- fixed function key options being truncated by old SimCoupe.
- fixed dropped tape files not adding to MRU menu.

## 20200819 dev (2020-08-19)
- updated D3D9 backend to D3D11 for G-Sync/FreeSync support (fullscreen).
- updated DirectSound backend to XAudio2 2.9, for better dynamic device support.
- added integer scaling to reduce scaling artefacts, especially in debugger.
- added sRGB support to rendering pipeline for gamma-correct blending.
- added motion blur option to reduce Gigascreen animation flicker.
- added alt- window scaling up to Alt-9 (500%).
- added multi-sector read support to ATA devices (used by PlayAnimHD).
- changed frame sync to be time based for smoother frame delivery.
- changed function key mappings to use strings in config file.
- fixed reset when LINE port set to zero giving black boot screen.
- fixed creating new MGT/CPM images.
- removed video scanline support, which is less useful on modern displays.
- removed window size snapping and forced window size changes.
- removed real Windows printer support.

## 20200711 dev (2020-07-11)
- Fixed data corruption writing to Atom devices (doesn't affect Atom Lite).
- added more ROM0 symbols for jump table entries and block instruction.
- Added shortcut for UNTIL breakpoints, allowing just an address to be entered.
- Updated SAASound to v3.4.0 to fix #23

## 20200528 dev (2020-05-28)
- Added support for LPEN b0 over main screen.
- Added SOFF support for LPEN and HPEN.
- Fixed TXFMST active time after MIDI OUT.

## 20200524 dev (2020-05-24)
- added experimental SSX screen image saving.

## 20200512 dev (2020-05-12)
- fixed CPI/CPD/CPIR/CPDR instruction timings

## 20200324 dev (2020-03-24)
- fixed failure to open resources in non-ASCII paths
- fixed interrupt timing issue causing sample player demo flicker.
- fixed passing bare paths to portable EXE [Win32]

## 20200117 dev (2020-01-17)
- fixed issue if PC changed when CPU is halted.
- fixed missing silence in WAV recordings.
- fixed bad WAV recordings under Linux x64.
- fixed refreshing debugger on reset or NMI.
- fixed PNG saving writing invalid images.
- fixed Atom media detection.
- fixed use of Atom Lite ROM when enabled in drive 1 slot.
- added keypad support for debugger number input.
- restored support for external SAASound library.
- moved ROM images to external files.

## Version 1.1 alpha 1 (2015-04-09)
- ?

## Version 1.0 (2006-07-21)
- added SDL port (OpenGL and regular) for Linux, BeOS, QNX, etc.
- added Pocket PC port for ARM, MIPS and SH3 devices
- added Allegro port for an updated DOS version
- added built-in GUI for all platforms without native GUI support
- added full async real disk support for 2000/XP and Linux
- added read-write EDSK support for copy-protected disks
- added formatting support within limits of disk image capabilities
- added read-only support for TD0 (TeleDisk) and SBT disk images
- added support for 9-sector Pro-DOS disk images
- added correct WD1772 CRCs to read address and read track data
- added support for deleted data address marks in track reads/formats
- added auto-boot option for disks inserted at the start-up screen
- added delay to disk image accesses, to avoid Pro-DOS bug
- added turbo-load support, for accelerated speed during disk accesses
- added preliminary debugger support, with lots of future potential
- added partial scanline support for a more natural looking display
- added support for border pixel artefacts, as used by Fred65 menu
- added support for incomplete frame drawing, needed by the debugger
- added support for using real HDD devices under Win32, Linux and OS X
- added optional automagic DOS booting of non-bootable disks
- added internal SAMDOS 2.2 image, used by automagical DOS booting
- added internal SAM ROM v3.0 image, used as the default
- added support for ZX82 and gzipped external ROM images
- added built-in support for Edwin Blink's ATOM ROM booting patches
- added support for unresponsive ASIC during first ~49ms after power-on
- added support for SDIDE and YAMOD.ATBUS hard disk interfaces
- added print-to-file support for SAM printers
- added support for swapping mouse buttons 2 and 3
- added Alt as modifier option to function key bindings
- added missing SAM pipe symbol to keyboard map
- added option to swap function keys and numeric keypad operation
- added full HPEN and LPEN support, used by Defender and BSD demo
- added mouse-wheel support to generate cursor up/down
- added basic Vista beta 2 support by working around OS bugs
- added manifest for XP themed common controls [Win32]
- fixed ADC HL,rr incorrectly setting N flag
- fixed ADD IX/IY,rr failing to set carry for overflow, or H flag
- fixed disassembly of IN C,(C) and display of DDCB/FDCB index offsets
- fixed DAA to support all undocumented flags
- fixed ROM memory accesses to be uncontended [Dave Laundon]
- fixed contention across page boundaries [Dave Laundon]
- fixed Atom support to work correctly with latest BDOS versions
- fixed SAD support for sector sizes other than 512 bytes
- fixed index pulse to be based on disk speed rather than status reads
- fixed WD1772 sector searching so the head value is no longer compared
- fixed formatting to preserve embedded data, for Pro-DOS [thanks Steve P-T]
- fixed re-insert of same disk image losing old image changes
- fixed formatting crash when no disk present [thanks Josef Prokes]
- fixed auto-frameskip to avoid wasting time (old 25fps problem)
- fixed key-bounce issues by deferring input until mid-frame
- fixed digit input on Czech keyboards, which require a shift modifier
- fixed AltGr being seen as Ctrl-Alt on some Win9x setups
- fixed Ctrl-<digit> access to symbols [thanks Edwin Blink]
- fixed incorrect SAM palette spread, which made dark colours too bright
- fixed the PNG screenshot palette being too dark
- fixed pixel format colour issues by calculating them from bit masks
- fixed broken import/export which could crash with sizes over 16K
- fixed MIC writes not reflected back to EAR [thanks Edwin Blink]
- fixed reads from ports 0 to 15, which were always returning zero
- fixed mouse read timeout (now 50us) and timer reset on each byte read
- fixed accumulation of small mouse movements, for Legend of Eshan
- fixed external memory ports to be write-only [thanks Jiri Veleba]
- fixed autoboot to hold rather than tap F9, to avoid a 2nd boot attempt
- fixed building for 64-bit platforms [thanks Stuart Brady and Terry Froy]
- fixed disk image saving on Windows shutdown/restart [Win32]
- fixed display of mode 3 pixels in 24-bit mode [Win32]
- improved efficiency of contention table lookups [Dave Laundon]
- improved fast-boot to no longer require temporary ROM patching
- improved menu layout, adding icons and a MRU file list
- improved some overly complicated dialogs and options screens
- improved sensitivity and scaling of SAM mouse movements
- improved RAM, paging, CLUT and CPU register power-on states
- improved DAC interpolation for less harsh playback in MOD Player
- improved altforcntrl use by disabling Windows key when active [Win32]
- improved PNG screenshots to include 5:4 aspect, scanlines and greyscale
- changed to use a combined 32K ROM image instead of separate 16K files
- changed unconnected ports to return 0xff rather than 0x00
- changed to dynamically bind to DirectX, for better error reporting
- removed dynamic Atom HDD generated from floppy images

## Version 0.81a (2001-07-22)
- fixed race-condition in New Disk dialogue that could cause stack overflow.
- fixed binary output in disassembler having bits reversed.
- changed Win32 open file filter to include compressed archives by default.

## Version 0.81 (2001-02-27)
- CPU loop simplified and optimised using a new event model [Dave Laundon].
- MIDI OUT interrupt timing now perfect (fixes a demo) [Dave Laundon].
- line interrupt now activated/deactivated on line port writes [Dave Laundon].
- improved pause feature to halt everything, instead of executing NOPs.
- support for background running, with optional pause when inactive.
- selectable screen area (no borders, TV view, full picture scan, ...).
- automatically accurate mode 3 on just the lines that need it.
- improved screen change detection for speed-up in most situations.
- now shows ASIC artefacts for mode 1/2 <=> 3/4 switches.
- screenshot save in PNG format.
- optional stretch-to-fit for better use of full-screen space.
- smarter window resizing to keep aspect, with snap sizes when shift held.
- improved internal screen handling, including on-screen text.
- improved mode switching, particularly Alt-TABing from full-screen.
- support for single file disk images, for easy use of SBT files.
- multi-sector reads now correctly leave BUSY flag set [fixes SAM Mines].
- implemented WRITE_TRACK floppy command for disk formatting.
- floppy 2 LED now replaced with red hard disk LED when ATOM active.
- mono and stereo parallel DAC sound support.
- removed DAC optimisation option - now detected automatically.
- parallel printer support to a windows printer (with manual flush option).
- MIDI-Out to choice of Windows device.
- Spectrum beeper implemented as DAC to sound better on slow systems.
- fixed quirky SAA sample playback (fixes Space demo on Fred 59) [Dave Hooper].
- clocks can now advance relative to emulated time as well as real time.
- system time changes now handled at run-time.
- automatic support for most keyboard mappings.
- buffered input for better key responses on slower systems.
- redefineable function keys to assign the choice of emulator actions.
- turbo key (5fps, no sound), for zooming through slow running programs.
- temporary key release to avoid ")0"-type problems with key combinations.
- simpler option presentation with Win32 property sheet.
- main window now requires left-click before grabbing mouse control.
- hardware cursor over main window to avoid flicker and slow-down.
- Windows cursor auto-hidden after 3 seconds.
- windowed mode now maintains the correct aspect ratio.
- emulator files accessed relative to the main EXE (allows shell associations).

## Version 0.80 alpha test 4 (2000-11-04)
- two-step video mode changing, with page change 1 cell after the mode.
- preliminary mono and stereo DAC support for parallel port 1.
- new DAC optimisation option for better sample plackback on slow machines.
- support for reading real SAM disks under Windows NT/2000 with SAMDISK.SYS.
- clock advancing can now be linked to emulated time.
- improved leap-year detection, instead of just checking multiples of 4.
- AltGr now only optionally for Edit key, as some keyboards use it for symbols.
- incomplete MIDI messages are discarded, for better resync on new playback.

## Version 0.80 alpha test 3 (2000-06-22)
- much improved contention timing for memory and I/O accesses [Dave Laundon]
- MIDI OUT interrupt now supported [Dave Laundon]
- interrupt active times now perfect (fixes Defender).
- hardware overlay support for UYVY, YUY2 and RGB formats.
- added on-screen floppy activity indicators and status/profile text.
- SAA address now masks unimportant bits before comparing (fixes MOD Player).
- SAA calls no longer buffered to allow fix consecutive writes to data port 511.
- MIDI OUT support to the default Windows device.
- disk save support enabled, as the general disk appears to be reliable.
- floppy motor now switches off after 10 revolutions of idle.
- input now updated half way down screen to avoid key bounce side effects.
- run-time support for Windows keyboard mapping changes.
- preliminary serial port support for modem use.
- Spectrum mode initialisation now sets up paging/palette as appropriate.
- disassembler re-written for compactness and complete instruction coverage.

## Version 0.80 alpha test (1999-11-11)
- crude instruction rounding used to implement approximate memory contention.
- additional delay and rounding performed on I/O on ASIC-handled ports.
- improved active duration for interrupts.
- fixed INI/IND not setting Z flag when B reaches zero (fixes BDOS).
- true 256K base memory configuration now available in addition to 512K.
- HPEN reads now return appropriate line number.
- ATTR reads allow border area detection (on-screen value always zero for now).
- cell-accurate display updating for much improved display of video effects.
- screen-off support to allow display disabling through border port.
- 50Hz frame sync using multimedia timer to trigger an event.
- added full-screen mode and run-time switching to/from windowed mode.
- added automatic and manual frame skipping.
- auto fallback of video support to best available mode.
- corrected flash attribute frequency to every 16 frames (was 25).
- full SAA/beeper sound support using Dave Hooper's SAASound library.
- implemented READ_TRACK and READ_ADDRESS commands (fixes SAM DICE).
- enhanced index pulse, write protect and spin-up floppy controller flags.
- all new disk imlementation for DSK/SAD files (save support disabled for now).
- new SDF image format created to handle copy protected disks.
- support for gzipped and (read-only) zipped disk images.
- drag and drop file support to mount disk in drive 1.
- keyboard and mouse changed to use DirectInput, and joystick support added.
- preliminary ATOM hard disk emulation to give basic BDOS 1.4e support.
- fast boot option temporarily modifies the SAM ROMs for an instant boot.
- auto-boot option to automatically press F9 on bootup.
- SAMBUS and DALLAS clocks supported.

## Version 0.80 pre-pre-alpha (1999-03-08)
- quick and dirty port, changing as little as possible to get code to run.
- added DirectX windowed display support for 8, 16 and 32-bit modes.
- added basic Win32 keyboard and mouse implementation.

## Origin (1999-02-27)
- Initial import of v0.72 DOS source
