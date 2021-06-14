cmake_minimum_required(VERSION 3.14)
project(simcoupe)
set(PROJECT_TITLE SimCoupe)

set(MAJOR_VERSION 1)
set(MINOR_VERSION 1)
set(PATCH_VERSION 1)

if (CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
  include(${CMAKE_TOOLCHAIN_FILE})
endif()

set(BUILD_BACKEND "auto" CACHE STRING "Back-end framework for video/sound/input")
set_property(CACHE BUILD_BACKEND PROPERTY STRINGS auto win32 sdl)

if (BUILD_BACKEND STREQUAL "win32" OR (BUILD_BACKEND STREQUAL "auto" AND WIN32))
  set(BUILD_WIN32 1)
elseif (BUILD_BACKEND STREQUAL "sdl" OR BUILD_BACKEND STREQUAL "auto")
  set(BUILD_SDL 1)
else()
  message(FATAL_ERROR "Unknown backend type: ${BUILD_BACKEND}")
endif()
message(STATUS "Build back-end: ${BUILD_BACKEND}")

if (CMAKE_VERBOSE_MAKEFILE STREQUAL "")
  set(CMAKE_VERBOSE_MAKEFILE OFF)
endif()

if (APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

########

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(GNUInstallDirs)
include(PreCompiledHeaders)
include(FetchContent)

set(BASE_CPP_FILES
    Base/Actions.cpp Base/ATA.cpp Base/AtaAdapter.cpp Base/Atom.cpp
    Base/AtomLite.cpp Base/AVI.cpp Base/BlipBuffer.cpp Base/BlueAlpha.cpp
    Base/Breakpoint.cpp Base/Clock.cpp Base/CPU.cpp Base/Debug.cpp
    Base/Disassem.cpp Base/Disk.cpp Base/Drive.cpp Base/Expr.cpp Base/Events.cpp
    Base/Font.cpp Base/Frame.cpp Base/FrameBuffer.cpp Base/GIF.cpp Base/GUI.cpp
    Base/GUIDlg.cpp Base/GUIIcons.cpp Base/HardDisk.cpp Base/Joystick.cpp
    Base/Keyboard.cpp Base/Keyin.cpp Base/Main.cpp Base/Memory.cpp
    Base/Mouse.cpp Base/Options.cpp Base/Parallel.cpp Base/Paula.cpp
    Base/SavePNG.cpp Base/SAMIO.cpp Base/SAMVox.cpp Base/SDIDE.cpp
    Base/SID.cpp Base/Sound.cpp Base/SSX.cpp Base/Stream.cpp Base/Symbol.cpp
    Base/Tape.cpp Base/Util.cpp Base/Video.cpp Base/WAV.cpp Base/VoiceBox.cpp
    Base/sp0256.cpp)

set(BASE_C_FILES Base/ioapi.c Base/unzip.c Extern/gl3w/src/gl3w.c)

set(BASE_H_FILES
    Base/Actions.h Base/ATA.h Base/AtaAdapter.h Base/Atom.h Base/AtomLite.h
    Base/AVI.h Base/BlipBuffer.h Base/BlueAlpha.h Base/Breakpoint.h
    Base/Clock.h Base/CPU.h Base/Debug.h Base/Disassem.h
    Base/Disk.h Base/Drive.h Base/Events.h Base/Expr.h Base/Font.h Base/Frame.h
    Base/GIF.h Base/GUI.h Base/GUIDlg.h Base/GUIIcons.h Base/HardDisk.h
    Base/ioapi.h Base/Joystick.h Base/Keyboard.h Base/Keyin.h Base/Main.h
    Base/Memory.h Base/Mouse.h Base/Options.h Base/Parallel.h Base/Paula.h
    Base/SavePNG.h Base/SAM.h Base/SAMDOS.h Base/SAMIO.h
    Base/SAMVox.h Base/SDIDE.h Base/SID.h Base/SimCoupe.h Base/Sound.h
    Base/SSX.h Base/Stream.h Base/Symbol.h Base/Tape.h Base/unzip.h
    Base/Util.h Base/Video.h Base/VL1772.h Base/WAV.h
    Base/VoiceBox.h Base/sp0256.h Extern/gl3w/include/GL/gl3w.h)

set(BASE_RUNTIME_FILES
    Resource/atom.rom Resource/atomlite.rom Resource/samcoupe.rom Resource/sp0256-al2.bin
    Resource/samports.map Resource/samrom.map Resource/SimCoupe.bmp)

set(SOURCE_FILES ${BASE_CPP_FILES} ${BASE_C_FILES})
set(HEADER_FILES ${BASE_H_FILES})
set(RESOURCE_FILES ${BASE_RUNTIME_FILES})

if (BUILD_WIN32)
  find_package(DirectX)
  include(HlslCompile)

  set(WIN_CPP_FILES
    Win32/Audio.cpp Win32/D3D11.cpp Win32/Floppy.cpp Win32/IDEDisk.cpp
    Win32/Input.cpp Win32/MIDI.cpp Win32/ODMenu.cpp Win32/OSD.cpp
    Win32/UI.cpp)

  set(WIN_H_FILES
    Win32/afxres.h Win32/Audio.h Win32/D3D11.h Extern/fdrawcmd/fdrawcmd.h
    Win32/Floppy.h Win32/IDEDisk.h Win32/Input.h Win32/MIDI.h Win32/ODMenu.h
    Win32/OSD.h Win32/resource.h Win32/UI.h)

  set(WIN_RC_FILES Win32/${PROJECT_NAME}.rc)
  set(WIN_VS_FILES Win32/Shaders/D3D11_Aspect_VS.hlsl Win32/Shaders/D3D11_Copy_VS.hlsl)
  set(WIN_PS_FILES Win32/Shaders/D3D11_Blend_PS.hlsl Win32/Shaders/D3D11_Palette_PS.hlsl Win32/Shaders/D3D11_Sample_PS.hlsl)
  set(WIN_MANIFEST_FILES Win32/Custom.manifest)

  set(SOURCE_FILES ${SOURCE_FILES} ${WIN_CPP_FILES})
  set(HEADER_FILES ${HEADER_FILES} ${WIN_H_FILES})
  set(RESOURCE_FILES ${RESOURCE_FILES} ${WIN_RC_FILES} ${WIN_VS_FILES} ${WIN_PS_FILES} ${WIN_MANIFEST_FILES})

  foreach(f ${WIN_VS_FILES})
    SET_SHADER_COMPILER(${f} vs_4_0)
  endforeach()

  foreach(f ${WIN_PS_FILES})
    SET_SHADER_COMPILER(${f} ps_4_0)
  endforeach()
elseif (BUILD_SDL)
  set(SDL_CPP_FILES
    SDL/Audio.cpp SDL/Floppy.cpp SDL/IDEDisk.cpp SDL/Input.cpp SDL/MIDI.cpp
    SDL/OSD.cpp SDL/SDL20.cpp SDL/SDL20_GL3.cpp SDL/UI.cpp)
  set(SDL_H_FILES
    SDL/Audio.h SDL/Floppy.h SDL/IDEDisk.h SDL/Input.h SDL/MIDI.h SDL/OSD.h
    SDL/SDL20.h SDL/SDL20_GL3.h SDL/UI.h)

  set(SOURCE_FILES ${SOURCE_FILES} ${SDL_CPP_FILES})
  set(HEADER_FILES ${HEADER_FILES} ${SDL_H_FILES})

  if (APPLE)
    set(SOURCE_FILES ${SOURCE_FILES} SDL/OSX/SDLMain.m)
    set(HEADER_FILES ${HEADER_FILES} SDL/OSX/SDLMain.h)

    set(MAC_ICON_FILES
      SDL/OSX/file_blank.icns SDL/OSX/file_dsk.icns SDL/OSX/file_mgt.icns
      SDL/OSX/file_sad.icns SDL/OSX/file_sbt.icns SDL/OSX/SimCoupe.icns
      SDL/OSX/Volume.icns)

    set(RESOURCE_FILES ${RESOURCE_FILES} ${MAC_ICON_FILES}
      ReadMe.md
      ChangeLog.txt
      SDL/OSX/Credits.html
      SDL/OSX/Info-SimCoupe.plist)
    set_source_files_properties(${RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()
endif()

if (WIN32)
  if (NOT WIN_RC_FILES)
    set(WIN_RC_FILES Win32/IconOnly.rc)
  endif()
  set(RESOURCE_FILES ${RESOURCE_FILES} ${WIN_RC_FILES})
  set_source_files_properties(${WIN_RC_FILES} PROPERTIES COMPILE_FLAGS /nologo)
elseif (APPLE)
  set_source_files_properties(${BASE_RUNTIME_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES})

########

add_executable(${PROJECT_NAME}
  WIN32 MACOSX_BUNDLE
  ${SOURCE_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

########

include(CheckCXXCompilerFlag)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)

# Check for some later C++17 headers known to be problematic.
check_include_file_cxx("optional" HAVE_STD_OPTIONAL)
check_include_file_cxx("variant" HAVE_STD_VARIANT)
if (NOT HAVE_STD_OPTIONAL OR NOT HAVE_STD_VARIANT)
  message(FATAL_ERROR "missing some required C++17 headers")
endif()

# Build runtime files should go with other built files.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Use a 3rd party implementation of std::filesystem to avoid needing macOS 10.15.
target_include_directories(${PROJECT_NAME} PRIVATE Extern/ghc)

# OpenGL loader
target_include_directories(${PROJECT_NAME} PRIVATE Extern/gl3w/include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})

# Z80 core
message(STATUS "Fetching github.com/kosarev/z80")
FetchContent_Declare(
  z80
  GIT_REPOSITORY https://github.com/kosarev/z80.git
  GIT_TAG 94e777443733d2946a67579a5942b22e7b71839d )
FetchContent_Populate(z80)
target_include_directories(${PROJECT_NAME} PRIVATE ${z80_SOURCE_DIR})

# Use bundled version of fdrawcmd.h for fdrawcmd.sys definitions.
if (WIN32)
  target_include_directories(${PROJECT_NAME} PRIVATE Extern/fdrawcmd)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD pthread)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

find_package(ZLIB)
if (ZLIB_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE ZLIB::ZLIB)
  set(HAVE_LIBZ 1)
endif()

find_package(BZip2)
if (BZIP2_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE BZip2::BZip2)
  set(HAVE_LIBBZ2 1)
endif()

find_package(PNG)
if (PNG_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG)
  set(HAVE_LIBPNG 1)
endif()

find_package(SAASound)
if(SAASOUND_FOUND)
  target_link_libraries(${PROJECT_NAME} PRIVATE SAASound::SAASound)
else()
  message(STATUS "Fetching github.com/stripwax/SAASound")
  FetchContent_Declare(
    SAASound
    GIT_REPOSITORY https://github.com/stripwax/SAASound.git
    GIT_TAG 11c53c7a57dc2f621ee5eabf99dd0e5b243b492d )
  FetchContent_MakeAvailable(SAASound)
  target_link_libraries(${PROJECT_NAME} PRIVATE SAASound)
  # TODO: remove once upstream is compatible
  target_include_directories(${PROJECT_NAME} PRIVATE ${saasound_SOURCE_DIR}/include)
endif()

find_library(SPECTRUM_LIBRARY NAMES spectrum)
find_path(SPECTRUM_INCLUDE_DIR libspectrum.h)
if (SPECTRUM_LIBRARY AND SPECTRUM_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SPECTRUM_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${SPECTRUM_LIBRARY})
  message(STATUS "Found libspectrum: ${SPECTRUM_LIBRARY}")
  set(HAVE_LIBSPECTRUM 1)
else()
  message(STATUS "libspectrum not found (no tape/snapshot support)")
endif()

find_library(RESID_LIBRARY NAMES resid)
find_path(RESID_INCLUDE_DIR resid/sid.h)
if (RESID_LIBRARY AND RESID_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${RESID_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${RESID_LIBRARY})
  message(STATUS "Found reSID: ${RESID_LIBRARY}")
else()
  message(STATUS "Using Extern/resid")
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Extern/resid)
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Extern)
  target_link_libraries(${PROJECT_NAME} PRIVATE resid)
endif()

find_library(XAUDIO2REDIST_LIBRARY NAMES xaudio2_9redist)
find_path(XAUDIO2REDIST_INCLUDE_DIR xaudio2redist.h)
if (XAUDIO2REDIST_LIBRARY AND XAUDIO2REDIST_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${XAUDIO2REDIST_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${XAUDIO2REDIST_LIBRARY})
  message(STATUS "Found xaudio2_9redist: ${XAUDIO2REDIST_LIBRARY}")
  set(HAVE_XAUDIO2REDIST 1)

  get_filename_component(XAUDIO2REDIST_DIR ${XAUDIO2REDIST_LIBRARY} DIRECTORY)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${XAUDIO2REDIST_DIR}/../bin/xaudio2_9redist.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
else()
  message(STATUS "xaudio2_9redist not found")
endif()

if (NOT MSVC)
  CHECK_CXX_COMPILER_FLAG("-stdlib=libc++" COMPILER_SUPPORTS_STDLIBCXX)
  if (COMPILER_SUPPORTS_STDLIBCXX AND NOT APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>)
  endif()
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Extern/fmt)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt)

########

if (BUILD_WIN32)
  find_package(DirectX)
  if (DINPUT_LIBRARY AND DINPUT_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${DINPUT_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${DINPUT_LIBRARY} dxguid.lib)
    message(STATUS "Found dinput: ${DINPUT_LIBRARY}")
    set(HAVE_DINPUT 1)
  else()
    message(FATAL_ERROR "DirectInput library (dinput8) not found")
  endif()
elseif (BUILD_SDL)
  find_package(SDL2 REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARY})
  set(HAVE_LIBSDL2 1)

  find_package(OpenGL)
  if (OPENGL_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENGL_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARY})
    set(HAVE_OPENGL 1)
  endif()
endif()

########

target_include_directories(${PROJECT_NAME} PRIVATE Base)

if (BUILD_WIN32)
  target_include_directories(${PROJECT_NAME} PRIVATE Win32)
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})  # Generated shader headers

  target_link_libraries(${PROJECT_NAME} PRIVATE winmm comctl32 shlwapi)
elseif (BUILD_SDL)
  target_include_directories(${PROJECT_NAME} PRIVATE SDL)
endif()

if (WIN32)
  if (MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set_target_properties(${PROJECT_NAME} PROPERTIES
      VS_GLOBAL_EnableCppCoreCheck true
      VS_GLOBAL_CodeAnalysisRuleSet CppCoreCheckRules.ruleset
      VS_GLOBAL_RunCodeAnalysis true)
  endif()

  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_TITLE})
elseif (APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/SDL/OSX/Info-${PROJECT_TITLE}.plist"
    MACOSX_BUNDLE_INFO_STRING "${PROJECT_TITLE} v${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${MAJOR_VERSION}.${MINOR_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}")

  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_TITLE})
endif()

set(RESOURCE_DIR ${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME})

install(DIRECTORY Resource/ DESTINATION ${RESOURCE_DIR})
install(FILES ReadMe.md DESTINATION ${RESOURCE_DIR})
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
