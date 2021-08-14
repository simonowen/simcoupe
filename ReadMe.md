# SimCoupe - A SAM Coupé Emulator

By Simon Owen (simon@simonowen.com)

[![Linux/macOS build status](https://travis-ci.com/simonowen/simcoupe.svg?branch=master)](https://travis-ci.com/simonowen/simcoupe)
[![Windows build status](https://ci.appveyor.com/api/projects/status/512shikql4ctfrv9/branch/master?svg=true)](https://ci.appveyor.com/project/simonowen/simcoupe/branch/master)
[![Linux/macOS dev branch](https://travis-ci.com/simonowen/simcoupe.svg?branch=dev)](https://travis-ci.com/simonowen/simcoupe)
[![Windows dev branch](https://ci.appveyor.com/api/projects/status/512shikql4ctfrv9/branch/dev?svg=true)](https://ci.appveyor.com/project/simonowen/simcoupe/branch/dev)
[![Licence](https://img.shields.io/badge/License-GPLv2-blue.svg?style=flat)](https://www.gnu.org/licenses/gpl-2.0.html)

---

## Introduction

SimCoupe emulates the SAM Coupé - a British Z80-based home computer released in
1989 by Miles Gordon Technology. See the Links section at the end of this
document for more information, including history and technical specifications.

This version of SimCoupe was derived from Allan Skillman's SimCoupe 0.72 for DOS
and Unix. It has been almost completely rewritten to improve accuracy, features,
and portability.

---

## Building SimCoupe

The [latest code](https://github.com/simonowen/simcoupe) is available on GitHub.
It builds under Windows, Linux and macOS, and should be portable to other
systems. Building requires a C++ compiler with C++17 support, such as Visual
Studio 2017, g++ 7, or Clang 5. The latest versions of each are recommended to
give the best performance and standards compliance.

All platforms require the [CMake](https://cmake.org/) build system. Windows
users can use the _Open Folder_ option in Visual Studio to trigger the built-in
CMake generator. A number of optional libraries will be used if detected at
configuration time.

Non-Windows platforms require the SDL 2.x library. This is usually available as
a `libsdl2-dev` package in Linux. macOS users can download the SDL2 framework
and install to `/Library/Frameworks`, where it'll be picked up by CMake.

Windows developers may wish to install
[vcpkg](https://github.com/Microsoft/vcpkg) and add the zlib, bzip2 and libpng
packages, for use by SimCoupe.

Typical command-line build process:
```
git clone https://github.com/simonowen/simcoupe.git
mkdir simcoupe-build
cd simcoupe-build
cmake ../simcoupe
```

---

## Using SimCoupe

See the [User Manual](Manual.md) for instructions on how to use SimCoupe.

## Thanks

- Allan Skillman - Father of the original SimCoupe
- Dave Laundon - CPU contention and sound enhancements
- Dave Hooper - Phillips SAA 1099 chip emulator
- Ivan Kosarev - Z80 CPU core
- Dag Lem - MOS 6581/8580 chip emulator
- Dr Andy Wright - Permission to distribute the SAM ROMs
- Philip Kendall - Spectrum support library

Special thanks to Andrew Collier, Edwin Blink, Chris Pile, Frode Tennebø, Steve
Parry-Thomas and Robert Wilkinson, for their active roles during development.
Thanks also to the sam-users mailing list, and everyone who sent feedback.

The sp0256-al2.bin allophone data is copyright Microchip Technology
Incorporated. See: http://spatula-city.org/~im14u2c/sp0256-al2/

---

## License

The SimCoupe source code is released under the
[GNU GPL v2.0 license](https://www.gnu.org/licenses/gpl-2.0.html).

## Contact

Simon Owen  
[https://simonowen.com](https://simonowen.com)
