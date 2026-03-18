include_guard(GLOBAL)

function(get_target_sources target out_var)
  set(sources)
  foreach(prop IN ITEMS SOURCES INTERFACE_SOURCES)
    get_target_property(value ${target} ${prop})
    if(value)
      list(APPEND sources ${value})
    endif()
  endforeach()
  get_target_property(header_sets ${target} INTERFACE_HEADER_SETS)
  if(header_sets)
    foreach(set IN LISTS header_sets)
      get_target_property(value ${target} "HEADER_SET_${set}")
      if(value)
        list(APPEND sources ${value})
      endif()
    endforeach()
  endif()
  list(REMOVE_DUPLICATES sources)
  list(TRANSFORM sources PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ REGEX ^[^/])
  set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

function(add_coverage target source_target)
  get_target_sources(${source_target} SOURCES)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    find_program(LLVM_PROFDATA NAMES llvm-profdata)
    find_program(LLVM_COV NAMES llvm-cov)
    if(APPLE)
      foreach(tool IN ITEMS LLVM_PROFDATA:llvm-profdata LLVM_COV:llvm-cov)
        string(REPLACE ":" ";" tool ${tool})
        list(GET tool 0 var)
        list(GET tool 1 bin)
        if(NOT ${var})
          execute_process(
            COMMAND xcrun --find ${bin} OUTPUT_VARIABLE ${var} OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
          )
        endif()
      endforeach()
    endif()
    if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
      message(WARNING "Coverage export skipped for ${target}: llvm-profdata or llvm-cov not found")
      return()
    endif()
    set(RAW_PROFILE $<TARGET_FILE_DIR:${target}>/default.profraw)
    target_compile_options(${target} PRIVATE -fprofile-instr-generate=${RAW_PROFILE} -fcoverage-mapping)
    target_link_options(${target} PRIVATE -fprofile-instr-generate=${RAW_PROFILE})
    add_custom_target(coverage ALL
      WORKING_DIRECTORY $<TARGET_FILE_DIR:${target}>
      COMMAND $<TARGET_FILE:${target}>
      COMMAND ${LLVM_PROFDATA} merge default.profraw --sparse --output default.profdata
      COMMAND ${LLVM_COV} export $<TARGET_FILE:${target}> --instr-profile=default.profdata --sources ${SOURCES}
        --format=lcov > ${CMAKE_BINARY_DIR}/lcov.info
      COMMAND ${LLVM_COV} report $<TARGET_FILE:${target}> --instr-profile=default.profdata --sources ${SOURCES}
        --show-region-summary=false --show-branch-summary=false
      COMMENT "Exporting coverage..."
      COMMAND_EXPAND_LISTS
      VERBATIM
    )
    add_dependencies(coverage ${target})
  endif()
endfunction()
