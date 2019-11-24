# Pre-compiled header, currently for Visual Studio projects only.

# One .cpp needs to be marked as Generate, everything else as Use.
function(SET_TARGET_PRECOMPILED_HEADER Target PrecompiledHeader PrecompiledSource)
  if (MSVC)
    set_target_properties(${Target} PROPERTIES COMPILE_FLAGS "/Yu${PrecompiledHeader}")
    set_source_files_properties(${PrecompiledSource} PROPERTIES COMPILE_FLAGS "/Yc${PrecompiledHeader}")
  endif()
endfunction()

# Allow opting out from pre-compiled for 3rd party modules.
function(IGNORE_PRECOMPILED_HEADER SourcesVar)
  if (MSVC)
    set_source_files_properties(${${SourcesVar}} PROPERTIES COMPILE_FLAGS "/Y-")
  endif()
endfunction()
