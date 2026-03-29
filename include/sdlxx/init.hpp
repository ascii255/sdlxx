#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <SDL3/SDL_init.h>

#include <capi/flag.hpp>
#include <capi/unique_flgd.hpp>
#include <capi/unique_sys.hpp>

#include <sdlxx/utility.hpp>

namespace sdlxx::inline v1_0_0 {

using System = capi::unique_sys<[] { return Invoke(SDL_Init(0)); }, SDL_Quit>;

enum struct InitFlag : SDL_InitFlags {
  Audio = SDL_INIT_AUDIO,
  Video = SDL_INIT_VIDEO,
  Joystick = SDL_INIT_JOYSTICK,
  Haptic = SDL_INIT_HAPTIC,
  Gamepad = SDL_INIT_GAMEPAD,
  Events = SDL_INIT_EVENTS,
  Sensor = SDL_INIT_SENSOR,
  Camera = SDL_INIT_CAMERA,
};
ENABLE_FLAG_ENUM(InitFlag);

inline bool InitSubSystem(SDL_InitFlags flags) noexcept { return Invoke(SDL_InitSubSystem(flags)); }

template <InitFlag Flag> using SubSystem = capi::unique_flgd<Flag, InitSubSystem, SDL_QuitSubSystem, SDL_WasInit>;

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <type_traits>
#include <utility>

#include <expect/expect.hpp>

namespace sdlxx::testing {

constexpr auto audio_sub_system_flag = InitFlag::Audio;
using AudioSubSystem = SubSystem<audio_sub_system_flag>;
constexpr auto audio_flag_mask = std::to_underlying(audio_sub_system_flag);

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
  using init_flag_value_t = std::remove_cv_t<decltype(audio_flag_mask)>;
  expect(std::is_same_v<decltype(static_cast<bool>(std::declval<const AudioSubSystem&>())), bool>);
  expect(
    std::is_same_v<decltype(static_cast<init_flag_value_t>(std::declval<const AudioSubSystem&>())), init_flag_value_t>);
}

constexpr void init_flag_audio_matches_sdl_audio_mask() { expect(audio_flag_mask == SDL_INIT_AUDIO); }

constexpr void init_flag_operator_or_combines_flags() {
  using capi::operator|;
  const InitFlag combined = InitFlag::Audio | InitFlag::Events;
  const auto combined_mask = std::to_underlying(combined);
  expect((combined_mask & SDL_INIT_AUDIO) != 0U);
  expect((combined_mask & SDL_INIT_EVENTS) != 0U);
}

inline void system_default_construction_initializes_sdl() {
  System system {};
  expect(static_cast<bool>(system));
}

inline void init_sub_system_audio_sets_query_flag_when_successful() {
  SDL_QuitSubSystem(audio_flag_mask);
  const bool initialized = InitSubSystem(audio_flag_mask);
  const auto active_flags = SDL_WasInit(audio_flag_mask);

  expect(((active_flags & audio_flag_mask) != 0U) == initialized);

  SDL_QuitSubSystem(audio_flag_mask);
}

inline void audio_sub_system_lifecycle_restores_clean_state() {
  SDL_QuitSubSystem(audio_flag_mask);
  const auto before_flags = SDL_WasInit(audio_flag_mask);
  expect((before_flags & audio_flag_mask) == 0U);

  {
    AudioSubSystem audio {};
    const bool audio_initialized = static_cast<bool>(audio);
    expect((static_cast<decltype(audio_flag_mask)>(audio) == audio_flag_mask) == audio_initialized);
    expect(((SDL_WasInit(audio_flag_mask) & audio_flag_mask) != 0U) == audio_initialized);
  }

  const auto after_flags = SDL_WasInit(audio_flag_mask);
  expect((after_flags & audio_flag_mask) == 0U);
}

inline void audio_sub_system_move_constructor_transfers_ownership() {
  SDL_QuitSubSystem(audio_flag_mask);

  {
    AudioSubSystem source {};
    const bool source_initialized = static_cast<bool>(source);

    AudioSubSystem target { std::move(source) };
    expect(!static_cast<bool>(source));
    expect(static_cast<bool>(target) == source_initialized);
    expect(static_cast<decltype(audio_flag_mask)>(source) == 0U);
    expect((static_cast<decltype(audio_flag_mask)>(target) == audio_flag_mask) == source_initialized);
    expect(((SDL_WasInit(audio_flag_mask) & audio_flag_mask) != 0U) == source_initialized);
  }

  expect((SDL_WasInit(audio_flag_mask) & audio_flag_mask) == 0U);
}

inline void audio_sub_system_move_assignment_transfers_ownership() {
  SDL_QuitSubSystem(audio_flag_mask);

  {
    AudioSubSystem source {};
    const bool source_initialized = static_cast<bool>(source);

    AudioSubSystem target {};
    const bool target_initialized = static_cast<bool>(target);
    expect(!(source_initialized && target_initialized));

    target = std::move(source);

    expect(static_cast<bool>(source) == target_initialized);
    expect(static_cast<bool>(target) == source_initialized);
    expect((static_cast<decltype(audio_flag_mask)>(source) == audio_flag_mask) == target_initialized);
    expect((static_cast<decltype(audio_flag_mask)>(target) == audio_flag_mask) == source_initialized);
    expect(((SDL_WasInit(audio_flag_mask) & audio_flag_mask) != 0U) == (source_initialized || target_initialized));
  }

  expect((SDL_WasInit(audio_flag_mask) & audio_flag_mask) == 0U);
}

inline void audio_sub_system_self_move_assignment_preserves_state() {
  SDL_QuitSubSystem(audio_flag_mask);

  {
    AudioSubSystem audio {};
    const bool initialized = static_cast<bool>(audio);

    auto& same = audio;
    audio = std::move(same);

    expect(static_cast<bool>(audio) == initialized);
    expect((static_cast<decltype(audio_flag_mask)>(audio) == audio_flag_mask) == initialized);
    expect(((SDL_WasInit(audio_flag_mask) & audio_flag_mask) != 0U) == initialized);
  }

  expect((SDL_WasInit(audio_flag_mask) & audio_flag_mask) == 0U);
}

constexpr void run_init_tests() {
  system_is_non_copyable_and_non_movable();
  system_has_bool_conversion();
  audio_sub_system_is_move_only();
  audio_sub_system_has_explicit_conversions();
  init_flag_audio_matches_sdl_audio_mask();
  init_flag_operator_or_combines_flags();

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
