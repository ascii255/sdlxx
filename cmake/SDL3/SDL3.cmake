include_guard(GLOBAL)

find_package(SDL3 3.4.2 QUIET)

if(NOT SDL3_FOUND)
  find_package(Git REQUIRED)
  include(FetchContent)
  FetchContent_Declare(sdl
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.2/SDL3-3.4.2.tar.gz
    URL_HASH MD5=b488ea1ede947c06855588314effe905
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
  )
  
  set(SDL_SHARED OFF CACHE INTERNAL "Build a shared version of the library" FORCE)
  set(SDL_STATIC ON CACHE INTERNAL "Build a static version of the library" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE INTERNAL "Build the test library" FORCE)
  set(SDL_TESTS OFF CACHE INTERNAL "Build tests" FORCE)
  set(SDL_EXAMPLES OFF CACHE INTERNAL "Build examples" FORCE)

  # Keep only subsystems used by this application.
  set(SDL_AUDIO ON CACHE INTERNAL "Build audio support" FORCE)
  set(SDL_VIDEO ON CACHE INTERNAL "Build video support" FORCE)
  set(SDL_RENDER ON CACHE INTERNAL "Build rendering support" FORCE)
  set(SDL_DIALOG ON CACHE INTERNAL "Build dialog support" FORCE)
  set(SDL_CAMERA OFF CACHE INTERNAL "Build camera support" FORCE)
  set(SDL_JOYSTICK ON CACHE INTERNAL "Build joystick support" FORCE)
  set(SDL_VIRTUAL_JOYSTICK ON CACHE INTERNAL "Build virtual joystick support" FORCE)
  set(SDL_HAPTIC OFF CACHE INTERNAL "Build haptic support" FORCE)
  set(SDL_HIDAPI ON CACHE INTERNAL "Build HIDAPI support" FORCE)
  set(SDL_POWER OFF CACHE INTERNAL "Build power support" FORCE)
  set(SDL_SENSOR OFF CACHE INTERNAL "Build sensor support" FORCE)
  set(SDL_GPU OFF CACHE INTERNAL "Build GPU support" FORCE)

  # Trim rendering/API backends not required by the SDL renderer path we use.
  set(SDL_OPENGL OFF CACHE INTERNAL "Build OpenGL support" FORCE)
  set(SDL_OPENGLES OFF CACHE INTERNAL "Build OpenGL ES support" FORCE)
  set(SDL_VULKAN OFF CACHE INTERNAL "Build Vulkan support" FORCE)
  set(SDL_RENDER_VULKAN OFF CACHE INTERNAL "Build Vulkan rendering support" FORCE)
  set(SDL_RENDER_GPU OFF CACHE INTERNAL "Build GPU rendering support" FORCE)

  FetchContent_MakeAvailable(sdl)

  unset(SDL_SHARED CACHE)
  unset(SDL_STATIC CACHE)
  unset(SDL_TEST_LIBRARY CACHE)
  unset(SDL_TESTS CACHE)
  unset(SDL_EXAMPLES CACHE)
  unset(SDL_AUDIO CACHE)
  unset(SDL_VIDEO CACHE)
  unset(SDL_RENDER CACHE)
  unset(SDL_DIALOG CACHE)
  unset(SDL_CAMERA CACHE)
  unset(SDL_JOYSTICK CACHE)
  unset(SDL_VIRTUAL_JOYSTICK CACHE)
  unset(SDL_HAPTIC CACHE)
  unset(SDL_HIDAPI CACHE)
  unset(SDL_POWER CACHE)
  unset(SDL_SENSOR CACHE)
  unset(SDL_GPU CACHE)
  unset(SDL_OPENGL CACHE)
  unset(SDL_OPENGLES CACHE)
  unset(SDL_VULKAN CACHE)
  unset(SDL_RENDER_VULKAN CACHE)
  unset(SDL_RENDER_GPU CACHE)
endif()
