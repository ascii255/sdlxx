#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <cstdint>

#include <SDL3/SDL_timer.h>

namespace sdlxx::inline v1_0_0 {

inline uint64_t GetTicks() noexcept { return SDL_GetTicks(); }

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <type_traits>

#include <expect/expect.hpp>

namespace sdlxx::testing {

constexpr void get_ticks_returns_uint64() { expect(std::is_same_v<decltype(GetTicks()), uint64_t>); }

inline void get_ticks_is_monotonic_non_decreasing() {
  const uint64_t first = GetTicks();
  SDL_Delay(2U);
  const uint64_t second = GetTicks();
  expect(second >= first);
}

constexpr void run_timer_tests() {
  get_ticks_returns_uint64();

  if !consteval {
    get_ticks_is_monotonic_non_decreasing();
  }
}

static_assert([] {
  run_timer_tests();
  return true;
}());

} // namespace sdlxx::testing

#endif // SDLXX_TESTING
