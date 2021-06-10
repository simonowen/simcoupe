#[[
FindSAASound

IMPORTED Targets:

This module defines :prop_tgt:`IMPORTED` target ``SAASound::SAASound``, if
SAASound has been found.

Result variables:

  SAASOUND_INCLUDE_DIRS   - location of find SAASound.h
  SAASOUND_LIBRARIES      - locations of SAASound library.
  SAASOUND_FOUND          - True if SAASound found.

]]#

find_path(SAASOUND_INCLUDE_DIR SAASound.h)

if(NOT SAASOUND_LIBRARY)
  find_library(SAASOUND_LIBRARY NAMES SAASound saasound)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SAASound REQUIRED_VARS SAASOUND_LIBRARY SAASOUND_INCLUDE_DIR)

if(SAASOUND_FOUND)
  set(SAASOUND_INCLUDE_DIRS ${SAASOUND_INCLUDE_DIR})

  if(NOT SAASOUND_LIBRARIES)
    set(SAASOUND_LIBRARIES ${SAASOUND_LIBRARY})
  endif()

  if(NOT TARGET SAASound::SAASound)
    add_library(SAASound::SAASound UNKNOWN IMPORTED)
    set_target_properties(SAASound::SAASound PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${SAASOUND_INCLUDE_DIR}")
  endif()

  if(EXISTS "${SAASOUND_LIBRARY}")
    set_target_properties(SAASound::SAASound PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      IMPORTED_LOCATION "${SAASOUND_LIBRARY}")
  endif()
endif()
