cmake_minimum_required(VERSION 3.3)
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
set_property(CACHE BUILD_BACKEND PROPERTY STRINGS auto win32 sdl allegro)

if (BUILD_BACKEND STREQUAL "win32" OR (BUILD_BACKEND STREQUAL "auto" AND WIN32))
  set(BUILD_WIN32 1)
elseif (BUILD_BACKEND STREQUAL "sdl" OR BUILD_BACKEND STREQUAL "auto")
  set(BUILD_SDL 1)
elseif (BUILD_BACKEND STREQUAL "allegro")
  set(BUILD_ALLEGRO 1)
  message(FATAL_ERROR "Allegro is not currently supported")
else()
  message(FATAL_ERROR "Unknown backend type: ${BUILD_BACKEND}")
endif()
message(STATUS "Build back-end: ${BUILD_BACKEND}")

if (CMAKE_VERBOSE_MAKEFILE STREQUAL "")
  set(CMAKE_VERBOSE_MAKEFILE OFF)
endif()

if (APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

########

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(PreCompiledHeaders)

file(GLOB BASE_CPP_FILES Base/*.cpp)
file(GLOB BASE_C_FILES Base/*.c)
file(GLOB BASE_H_FILES Base/*.h)
file(GLOB BASE_RUNTIME_FILES Resource/*)

set(SOURCE_FILES ${BASE_CPP_FILES} ${BASE_C_FILES})
set(HEADER_FILES ${BASE_H_FILES})
set(RESOURCE_FILES ${BASE_RUNTIME_FILES})

if (BUILD_WIN32)
  find_package(DirectX)
  include(HlslCompile)

  file(GLOB WIN_CPP_FILES Win32/*.cpp)
  file(GLOB WIN_H_FILES Win32/*.h)
  file(GLOB WIN_RC_FILES Win32/${PROJECT_NAME}.rc)
  file(GLOB WIN_VS_FILES Win32/Shaders/*VS.hlsl)
  file(GLOB WIN_PS_FILES Win32/Shaders/*PS.hlsl)
  file(GLOB WIN_MANIFEST_FILES Win32/*.manifest)

  set(SOURCE_FILES ${SOURCE_FILES} ${WIN_CPP_FILES})
  set(HEADER_FILES ${HEADER_FILES} ${WIN_H_FILES})
  set(RESOURCE_FILES ${RESOURCE_FILES} ${WIN_RC_FILES} ${WIN_VS_FILES} ${WIN_PS_FILES} ${WIN_MANIFEST_FILES})

  foreach(f ${WIN_VS_FILES})
    SET_SHADER_COMPILER(${f} vs_3_0)
  endforeach()

  foreach(f ${WIN_PS_FILES})
    SET_SHADER_COMPILER(${f} ps_3_0)
  endforeach()
elseif (BUILD_SDL)
  file(GLOB SDL_CPP_FILES SDL/*.cpp)
  file(GLOB SDL_H_FILES SDL/*.h)

  set(SOURCE_FILES ${SOURCE_FILES} ${SDL_CPP_FILES})
  set(HEADER_FILES ${HEADER_FILES} ${SDL_H_FILES})

  if (APPLE)
    set(SOURCE_FILES ${SOURCE_FILES} SDL/OSX/SDLMain.m)
    set(HEADER_FILES ${HEADER_FILES} SDL/OSX/SDLMain.h)

    file(GLOB MAC_ICON_FILES SDL/OSX/*.icns)
    set(RESOURCE_FILES ${RESOURCE_FILES} ${MAC_ICON_FILES}
      ReadMe.md
      ChangeLog.txt
      SDL/OSX/Credits.html
      SDL/OSX/Info-SimCoupe.plist)
    set_source_files_properties(${RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()
elseif (BUILD_ALLEGRO)
  file(GLOB ALLEGRO_CPP_FILES Allegro/*.cpp)
  file(GLOB ALLEGRO_H_FILES Allegro/*.h)

  set(SOURCE_FILES ${SOURCE_FILES} ${ALLEGRO_CPP_FILES})
  set(HEADER_FILES ${HEADER_FILES} ${ALLEGRO_H_FILES})
endif()

if (WIN32)
  if (NOT WIN_RC_FILES)
    file(GLOB WIN_RC_FILES Win32/IconOnly.rc)
  endif()
  set(RESOURCE_FILES ${RESOURCE_FILES} ${WIN_RC_FILES})
  set_source_files_properties(${WIN_RC_FILES} PROPERTIES COMPILE_FLAGS /nologo)

  set(HAVE_FDRAWCMD_H 1)
elseif (APPLE)
  set_source_files_properties(${BASE_RUNTIME_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES})

########

add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${SOURCE_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

########

include(CheckCXXCompilerFlag)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)

# Only newer Xcode versions and OS targets support std::filesystem
check_include_file_cxx("filesystem" HAVE_STD_FILESYSTEM)

set(CMAKE_THREAD_PREFER_PTHREAD pthread)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

find_package(ZLIB)
if (ZLIB_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARY})
  set(HAVE_LIBZ 1)
endif()

find_package(BZip2)
if (BZIP2_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${BZIP2_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${BZIP2_LIBRARIES})
  set(HAVE_LIBBZ2 1)
endif()

find_package(PNG)
if (PNG_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${PNG_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${PNG_LIBRARIES})
  set(HAVE_LIBPNG 1)
endif()

find_library(SAASOUND_LIBRARY NAMES saasound)
find_path(SAASOUND_INCLUDE_DIR SAASound.h)
if (SAASOUND_LIBRARY AND SAASOUND_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SAASOUND_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${SAASOUND_LIBRARY})
  message(STATUS "Found SAASound: ${SAASOUND_LIBRARY}")
  set(HAVE_LIBSAASOUND 1)
else()
  message(STATUS "SAASound library not found (using internal implementation)")
endif()

find_library(SPECTRUM_LIBRARY NAMES spectrum)
find_path(SPECTRUM_INCLUDE_DIR libspectrum.h)
if (SPECTRUM_LIBRARY AND SPECTRUM_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SPECTRUM_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${SPECTRUM_LIBRARY})
  message(STATUS "Found libspectrum: ${SPECTRUM_LIBRARY}")
  set(HAVE_LIBSPECTRUM 1)
else()
  message(STATUS "libspectrum not found (no tape/snapshot support)")
endif()

find_library(RESID_LIBRARY NAMES resid)
find_path(RESID_INCLUDE_DIR resid/sid.h)
if (RESID_LIBRARY AND RESID_INCLUDE_DIR)
  target_include_directories(${PROJECT_NAME} PRIVATE ${RESID_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${RESID_LIBRARY})
  message(STATUS "Found reSID: ${RESID_LIBRARY}")
  set(HAVE_LIBRESID 1)
else()
  message(STATUS "reSID library not found (no SID chip support)")
endif()

if (NOT MSVC)
  CHECK_CXX_COMPILER_FLAG("-stdlib=libc++" COMPILER_SUPPORTS_STDLIBCXX)
  if (COMPILER_SUPPORTS_STDLIBCXX AND NOT APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>)
  endif()
endif()

########

if (BUILD_WIN32)
  find_package(DirectX)
  if (D3D9_LIBRARY AND D3D9_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${D3D9_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${D3D9_LIBRARY} dxguid.lib)
    message(STATUS "Found d3d9: ${D3D9_LIBRARY}")
    set(HAVE_D3D9 1)
  else()
    message(FATAL_ERROR "Direct3D9 library (d3d9) not found")
  endif()
  if (DINPUT_LIBRARY AND DINPUT_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${DINPUT_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${DINPUT_LIBRARY} dxguid.lib)
    message(STATUS "Found dinput: ${DINPUT_LIBRARY}")
    set(HAVE_DINPUT 1)
  else()
    message(FATAL_ERROR "DirectInput library (dinput8) not found")
  endif()
  if (DDRAW_LIBRARY AND DDRAW_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${DDRAW_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${DDRAW_LIBRARY} dxguid.lib)
    message(STATUS "Found ddraw: ${DDRAW_LIBRARY}")
    set(HAVE_DDRAW 1)
  else()
    message(FATAL_ERROR "DirectDraw library (ddraw) not found")
  endif()
elseif (BUILD_SDL)
  find_package(SDL2)
  if (SDL2_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})
    set(HAVE_LIBSDL2 1)
  else()
    find_package(SDL)
    if (SDL_FOUND)
      target_include_directories(${PROJECT_NAME} PRIVATE ${SDL_INCLUDE_DIR})
      target_link_libraries(${PROJECT_NAME} ${SDL_LIBRARY})
      set(HAVE_LIBSDL 1)
    else()
      message(FATAL_ERROR "This program requires SDL 2.x (recommended) or SDL 1.2")
    endif()
  endif()
elseif (BUILD_ALLEGRO)
  find_library(ALLEGRO_LIBRARY NAMES allegro)
  find_path(ALLEGRO_INCLUDE_DIR allegro5/allegro5.h)
  if (ALLEGRO_LIBRARY AND ALLEGRO_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ALLEGRO_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${ALLEGRO_LIBRARY})
    message(STATUS "Found allegro5: ${ALLEGRO_LIBRARY}")
    set(HAVE_LIBALLEGRO5 1)
  else()
    message(FATAL_ERROR "Allegro5 library not found.")
  endif()
endif()

########

target_include_directories(${PROJECT_NAME} PRIVATE Base)

if (BUILD_WIN32)
  target_include_directories(${PROJECT_NAME} PRIVATE Win32)
  target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})  # Generated shader headers

  target_link_libraries(${PROJECT_NAME} winmm comctl32 shlwapi)
elseif (BUILD_SDL)
  target_include_directories(${PROJECT_NAME} PRIVATE SDL)
elseif (BUILD_ALLEGRO)
  target_include_directories(${PROJECT_NAME} PRIVATE Allegro)
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

if (WIN32)
  set(RESOURCE_DIR ${CMAKE_INSTALL_PREFIX})
  install(DIRECTORY Resource/ DESTINATION ${CMAKE_INSTALL_PREFIX})
  install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
else()
  set(RESOURCE_DIR ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME})
  install(DIRECTORY Resource/ DESTINATION ${RESOURCE_DIR})
  install(TARGETS ${PROJECT_NAME} DESTINATION bin)
endif()

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
