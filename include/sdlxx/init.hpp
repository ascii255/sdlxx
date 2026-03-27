#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <SDL3/SDL_init.h>

#include <capi/unique_flgd.hpp>
#include <capi/unique_sys.hpp>

#include <sdlxx/utility.hpp>

namespace sdlxx::inline v1_0_0 {

using System = capi::unique_sys<[] { return Invoke(SDL_Init(0)); }, SDL_Quit>;

inline bool InitSubSystem(SDL_InitFlags flags) noexcept { return Invoke(SDL_InitSubSystem(flags)); }

using AudioSubSystem = capi::unique_flgd<SDL_INIT_AUDIO, InitSubSystem, SDL_QuitSubSystem, SDL_WasInit>;

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <type_traits>
#include <utility>

#include <expect/expect.hpp>

namespace sdlxx::testing {

constexpr void system_is_non_copyable_and_non_movable() {
  expect(!std::is_copy_constructible_v<System>);
  expect(!std::is_copy_assignable_v<System>);
  expect(!std::is_move_constructible_v<System>);
  expect(!std::is_move_assignable_v<System>);
}

constexpr void system_has_bool_conversion() {
  expect(std::is_same_v<decltype(static_cast<bool>(std::declval<const System&>())), bool>);
}

constexpr void audio_sub_system_is_move_only() {
  expect(std::is_move_constructible_v<AudioSubSystem>);
  expect(std::is_move_assignable_v<AudioSubSystem>);
  expect(!std::is_copy_constructible_v<AudioSubSystem>);
  expect(!std::is_copy_assignable_v<AudioSubSystem>);
}

constexpr void audio_sub_system_has_explicit_conversions() {
  expect(std::is_same_v<decltype(static_cast<bool>(std::declval<const AudioSubSystem&>())), bool>);
  expect(std::is_same_v<decltype(static_cast<SDL_InitFlags>(std::declval<const AudioSubSystem&>())), SDL_InitFlags>);
}

inline void system_default_construction_initializes_sdl() {
  System system {};
  expect(static_cast<bool>(system));
}

inline void init_sub_system_audio_sets_query_flag_when_successful() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  const bool initialized = InitSubSystem(SDL_INIT_AUDIO);
  const SDL_InitFlags active_flags = SDL_WasInit(SDL_INIT_AUDIO);

  expect(((active_flags & SDL_INIT_AUDIO) != 0U) == initialized);

  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

inline void audio_sub_system_lifecycle_restores_clean_state() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  const SDL_InitFlags before_flags = SDL_WasInit(SDL_INIT_AUDIO);
  expect((before_flags & SDL_INIT_AUDIO) == 0U);

  {
    AudioSubSystem audio {};
    const bool audio_initialized = static_cast<bool>(audio);
    expect((static_cast<SDL_InitFlags>(audio) == SDL_INIT_AUDIO) == audio_initialized);
    expect(((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0U) == audio_initialized);
  }

  const SDL_InitFlags after_flags = SDL_WasInit(SDL_INIT_AUDIO);
  expect((after_flags & SDL_INIT_AUDIO) == 0U);
}

inline void audio_sub_system_move_constructor_transfers_ownership() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  {
    AudioSubSystem source {};
    const bool source_initialized = static_cast<bool>(source);

    AudioSubSystem target { std::move(source) };
    expect(!static_cast<bool>(source));
    expect(static_cast<bool>(target) == source_initialized);
    expect(static_cast<SDL_InitFlags>(source) == 0U);
    expect((static_cast<SDL_InitFlags>(target) == SDL_INIT_AUDIO) == source_initialized);
    expect(((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0U) == source_initialized);
  }

  expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U);
}

inline void audio_sub_system_move_assignment_transfers_ownership() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  {
    AudioSubSystem source {};
    const bool source_initialized = static_cast<bool>(source);

    AudioSubSystem target {};
    const bool target_initialized = static_cast<bool>(target);
    expect(!(source_initialized && target_initialized));

    target = std::move(source);

    expect(static_cast<bool>(source) == target_initialized);
    expect(static_cast<bool>(target) == source_initialized);
    expect((static_cast<SDL_InitFlags>(source) == SDL_INIT_AUDIO) == target_initialized);
    expect((static_cast<SDL_InitFlags>(target) == SDL_INIT_AUDIO) == source_initialized);
    expect(((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0U) == (source_initialized || target_initialized));
  }

  expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U);
}

inline void audio_sub_system_self_move_assignment_preserves_state() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  {
    AudioSubSystem audio {};
    const bool initialized = static_cast<bool>(audio);

    auto& same = audio;
    audio = std::move(same);

    expect(static_cast<bool>(audio) == initialized);
    expect((static_cast<SDL_InitFlags>(audio) == SDL_INIT_AUDIO) == initialized);
    expect(((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0U) == initialized);
  }

  expect((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0U);
}

constexpr void run_init_tests() {
  system_is_non_copyable_and_non_movable();
  system_has_bool_conversion();
  audio_sub_system_is_move_only();
  audio_sub_system_has_explicit_conversions();

  if !consteval {
    system_default_construction_initializes_sdl();
    init_sub_system_audio_sets_query_flag_when_successful();
    audio_sub_system_lifecycle_restores_clean_state();
    audio_sub_system_move_constructor_transfers_ownership();
    audio_sub_system_move_assignment_transfers_ownership();
    audio_sub_system_self_move_assignment_preserves_state();
  }
}

static_assert([] {
  run_init_tests();
  return true;
}());

} // namespace sdlxx::testing

#endif // SDLXX_TESTING
