cmake_minimum_required(VERSION 3.0)
project(SAASound)

set(MAJOR_VERSION 3)
set(MINOR_VERSION 4)
set(PATCH_VERSION 0)

option(SAASOUND_BUILD_SHARED "Build a dynamic library instead of static library" OFF)

if (APPLE)
  option(SAASOUND_BUILD_FRAMEWORK "Build the library as a macOS framework" OFF)
  option(SAASOUND_BUILD_UNIVERSAL "Build the shared/framework library as a universal binary" ON)
  if (SAASOUND_BUILD_UNIVERSAL)
    set(CMAKE_OSX_ARCHITECTURES "i386;x86_64")
  endif()
endif ()

file(GLOB SOURCES src/*.cpp)
file(GLOB HEADERS src/*.h)
file(GLOB API_HEADERS include/*.h)

if (MSVC)
  configure_file(resources/${PROJECT_NAME}.rc.in ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.rc @ONLY)
  set(RESOURCES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.rc resources/${PROJECT_NAME}.def)
endif ()

if (SAASOUND_BUILD_SHARED OR SAASOUND_BUILD_FRAMEWORK)
  add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS} ${API_HEADERS} ${RESOURCES})
else()
  add_library(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${API_HEADERS} ${RESOURCES})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${MAJOR_VERSION}.${MINOR_VERSION}
  SOVERSION ${MAJOR_VERSION}
  CXX_STANDARD 11)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_BINARY_DIR}
  include)

if (APPLE AND SAASOUND_BUILD_FRAMEWORK)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME SAASound
    FRAMEWORK TRUE
    FRAMEWORK_VERSION C
    MACOSX_FRAMEWORK_IDENTIFIER org.rebuzz)
  set_source_files_properties(${API_HEADERS} PROPERTIES
    MACOSX_PACKAGE_LOCATION Headers)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
endif ()

configure_file(src/config.h.in ${CMAKE_BINARY_DIR}/config.h)
target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H=1)

install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  FRAMEWORK DESTINATION "/Library/Frameworks"
  PUBLIC_HEADER DESTINATION include)

if (NOT APPLE OR NOT SAASOUND_BUILD_FRAMEWORK)
  install(FILES ${API_HEADERS} DESTINATION include)
endif ()
