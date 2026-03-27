cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

project(sdlxx VERSION 1.0.0 LANGUAGES CXX)

include(SDL3/SDL3)
include(CAPI/capi)

add_library(sdlxx INTERFACE)
add_library(SDL::sdlxx ALIAS sdlxx)
target_link_libraries(sdlxx INTERFACE SDL3::SDL3 CAPI::capi)
target_sources(sdlxx INTERFACE
  FILE_SET HEADERS
  BASE_DIRS include
  FILES
    include/sdlxx/audio.hpp
    include/sdlxx/init.hpp
    include/sdlxx/utility.hpp
)
target_compile_features(sdlxx INTERFACE cxx_std_23)
target_include_directories(sdlxx SYSTEM INTERFACE include include/single)

if(PROJECT_IS_TOP_LEVEL)
  add_executable(sdlxx_testing main.cpp)
  target_compile_features(sdlxx_testing PRIVATE cxx_std_23)
  target_include_directories(sdlxx_testing PRIVATE include)
  target_link_libraries(sdlxx_testing PRIVATE SDL3::SDL3 CAPI::capi)
  target_compile_definitions(sdlxx_testing PRIVATE SDLXX_TESTING)
  target_compile_options(sdlxx_testing PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
      -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion>
  )
  set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
  include(CTest)
  add_test(NAME audio COMMAND $<TARGET_FILE:sdlxx_testing> audio)
  add_test(NAME init COMMAND $<TARGET_FILE:sdlxx_testing> init)
  add_test(NAME utility COMMAND $<TARGET_FILE:sdlxx_testing> utility)
  include(Coverage)
  add_coverage(sdlxx_testing sdlxx)
endif()
