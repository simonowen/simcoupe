if (WIN32)
  # Compile HLSL shaders for Windows.
  function(SET_SHADER_COMPILER File Profile)
    if (NOT FXC)
      find_program(FXC fxc DOC "DirectX Shader Compiler")
      if (NOT FXC)
        message(FATAL_ERROR "Cannot find DirectX Shader Compiler (fxc.exe)")
      endif()
    endif()

    get_filename_component(BASE_NAME ${File} NAME_WE)
    set(OUTPUT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/${BASE_NAME}.h)

    add_custom_command(
      OUTPUT ${OUTPUT_HEADER}
      COMMAND ${FXC} /nologo /Emain /Vng_${BASE_NAME} /T${Profile} /Fh${OUTPUT_HEADER} ${File}
      MAIN_DEPENDENCY ${File}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Compile-shader: ${File}"
      VERBATIM)
  endfunction()
endif()
