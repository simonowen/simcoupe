message(STATUS "Fetching whereami from GitHub")
FetchContent_Declare(
  whereami
  GIT_REPOSITORY https://github.com/gpakosz/whereami.git
  GIT_TAG ba364cd54fd431c76c045393b6522b4bff547f50 )
FetchContent_Populate(whereami)

add_library(whereami ${whereami_SOURCE_DIR}/src/whereami.c)
target_include_directories(whereami PUBLIC ${whereami_SOURCE_DIR}/src)
