#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <cstdio>
#include <print>
#include <string_view>
#include <type_traits>
#include <utility>

#include <SDL3/SDL_error.h>

namespace sdlxx::inline v1_0_0 {

inline void print_error(std::string_view prefix, std::string_view message = {}) {
  std::println(stderr, "{}: {}", prefix, message.empty() ? SDL_GetError() : message);
}

namespace detail {

template <typename Fn, typename OnFail> constexpr auto invoke_impl(Fn&& fn, const char* name, OnFail&& on_fail) {
  auto result = std::forward<Fn>(fn)();
  if constexpr (std::is_pointer_v<decltype(result)> || std::is_same_v<decltype(result), bool>) {
    if (!result) {
      print_error(name);
      on_fail();
    }
  } else {
    if (result < 0) {
      print_error(name);
      on_fail();
    }
  }
  return result;
}

template <typename Fn> constexpr auto invoke_impl(Fn&& fn, const char* name) {
  return invoke_impl(std::forward<Fn>(fn), name, [] {});
}

} // namespace detail

#define Invoke(fn, ...) sdlxx::detail::invoke_impl([&] { return (fn); }, #fn __VA_OPT__(, [&] { __VA_ARGS__; }))

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <array>
#include <string>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <expect/expect.hpp>

namespace sdlxx::testing {

struct scoped_quiet_error_output {
  int previous_stderr_fd = -1;
  std::FILE* quiet_stream = nullptr;

  static auto stream_fd(std::FILE* stream) -> int {
#if defined(_WIN32)
    return ::_fileno(stream);
#else
    return ::fileno(stream);
#endif
  }

  static auto duplicate_fd(int fd) -> int {
#if defined(_WIN32)
    return ::_dup(fd);
#else
    return ::dup(fd);
#endif
  }

  static auto duplicate_to_fd(int source_fd, int target_fd) -> int {
#if defined(_WIN32)
    return ::_dup2(source_fd, target_fd);
#else
    return ::dup2(source_fd, target_fd);
#endif
  }

  static void close_fd(int fd) {
#if defined(_WIN32)
    (void)::_close(fd);
#else
    (void)::close(fd);
#endif
  }

  scoped_quiet_error_output() {
    quiet_stream = std::tmpfile();
    expect(quiet_stream != nullptr);

    std::fflush(stderr);

    const int stderr_fd = stream_fd(stderr);
    const int quiet_fd = stream_fd(quiet_stream);
    expect(stderr_fd != -1);
    expect(quiet_fd != -1);

    previous_stderr_fd = duplicate_fd(stderr_fd);
    expect(previous_stderr_fd != -1);

    expect(duplicate_to_fd(quiet_fd, stderr_fd) != -1);
  }

  ~scoped_quiet_error_output() {
    std::fflush(stderr);

    const int stderr_fd = stream_fd(stderr);
    expect(previous_stderr_fd != -1);
    expect(stderr_fd != -1);

    expect(duplicate_to_fd(previous_stderr_fd, stderr_fd) != -1);
    close_fd(previous_stderr_fd);

    if (quiet_stream) {
      std::fclose(quiet_stream);
    }
  }

  auto stream() const -> std::FILE* { return quiet_stream; }

  scoped_quiet_error_output(const scoped_quiet_error_output&) = delete;
  auto operator=(const scoped_quiet_error_output&) -> scoped_quiet_error_output& = delete;
};

inline auto read_stream_contents(std::FILE* stream) -> std::string {
  expect(stream != nullptr);

  std::fflush(stream);
  std::rewind(stream);

  std::string output {};
  std::array<char, 256> chunk {};
  while (true) {
    const auto bytes_read = std::fread(chunk.data(), sizeof(char), chunk.size(), stream);
    if (bytes_read == 0U) {
      break;
    }
    output.append(chunk.data(), bytes_read);
  }

  return output;
}

// --- compile-time tests ---

constexpr void print_error_returns_void() { expect(std::is_same_v<decltype(print_error("prefix", "message")), void>); }

constexpr void invoke_impl_pointer_success_does_not_call_handler() {
  int value = 9;
  bool failed = false;
  const auto on_fail = [&] { failed = true; };
  auto* result =
    detail::invoke_impl([&] { return &value; }, "invoke_impl_pointer_success_does_not_call_handler", on_fail);
  expect(result == &value);
  expect(!failed);
  on_fail();
  expect(failed);
}

constexpr void invoke_impl_bool_success_does_not_call_handler() {
  bool value = true;
  bool failed = false;
  auto on_fail = [&] { failed = true; };
  const bool result =
    detail::invoke_impl([&] { return value; }, "invoke_impl_bool_success_does_not_call_handler", on_fail);
  expect(result);
  expect(!failed);
  on_fail();
  expect(failed);
}

constexpr void invoke_impl_integer_success_does_not_call_handler() {
  int value = 1;
  bool failed = false;
  auto on_fail = [&] { failed = true; };
  const int result =
    detail::invoke_impl([&] { return value; }, "invoke_impl_integer_success_does_not_call_handler", on_fail);
  expect(result == value);
  expect(!failed);
  on_fail();
  expect(failed);
}

constexpr void invoke_macro_success_does_not_call_handler() {
  int value = 3;
  bool failed = false;
  const auto on_fail = [&] { failed = true; };
  auto* result = detail::invoke_impl([&] { return &value; }, "invoke_macro_success_does_not_call_handler", on_fail);
  expect(result == &value);
  expect(!failed);
  on_fail();
  expect(failed);
}

// --- runtime-only tests (print_error / failure paths) ---

inline void print_error_uses_sdl_error_when_message_is_empty() {
  constexpr auto prefix = "print_error_uses_sdl_error_when_message_is_empty";
  constexpr auto expected_error = "utility_test_expected_sdl_error";

  SDL_ClearError();
  (void)SDL_SetError("%s", expected_error);

  scoped_quiet_error_output captured_output;
  expect(captured_output.stream() != nullptr);

  print_error(prefix);

  const std::string output = read_stream_contents(captured_output.stream());

  expect(output.contains(prefix));
  expect(output.contains(expected_error));

  SDL_ClearError();
}

inline void invoke_impl_pointer_failure_calls_handler() {
  bool failed = false;
  auto* result = detail::invoke_impl([] { return static_cast<int*>(nullptr); },
                                     "invoke_impl_pointer_failure_calls_handler", [&] { failed = true; });
  expect(result == nullptr);
  expect(failed);
}

inline void invoke_impl_bool_failure_calls_handler() {
  bool failed = false;
  const bool result =
    detail::invoke_impl([] { return false; }, "invoke_impl_bool_failure_calls_handler", [&] { failed = true; });
  expect(!result);
  expect(failed);
}

inline void invoke_impl_integer_failure_calls_handler() {
  bool failed = false;
  const int result =
    detail::invoke_impl([] { return -1; }, "invoke_impl_integer_failure_calls_handler", [&] { failed = true; });
  expect(result == -1);
  expect(failed);
}

inline void invoke_impl_overload_failure_without_on_fail() {
  auto* result =
    detail::invoke_impl([] { return static_cast<int*>(nullptr); }, "invoke_impl_overload_failure_without_on_fail");
  expect(result == nullptr);
}

inline void invoke_impl_bool_failure_without_on_fail() {
  const bool result = detail::invoke_impl([] { return false; }, "invoke_impl_bool_failure_without_on_fail");
  expect(!result);
}

inline void invoke_impl_integer_failure_without_on_fail() {
  const int result = detail::invoke_impl([] { return -1; }, "invoke_impl_integer_failure_without_on_fail");
  expect(result == -1);
}

inline void invoke_macro_failure_without_handler() {
  auto* result = Invoke(static_cast<int*>(nullptr));
  expect(result == nullptr);
}

inline void invoke_macro_failure_calls_handler() {
  bool failed = false;
  auto* result = Invoke(static_cast<int*>(nullptr), failed = true);
  expect(result == nullptr);
  expect(failed);
}

constexpr void run_utility_tests() {
  // compile-time and runtime safe
  invoke_impl_pointer_success_does_not_call_handler();
  invoke_impl_bool_success_does_not_call_handler();
  invoke_impl_integer_success_does_not_call_handler();
  invoke_macro_success_does_not_call_handler();
  print_error_returns_void();

  // runtime only — failure paths call print_error which calls std::println
  if !consteval {
    scoped_quiet_error_output quiet_error_output;
    expect(quiet_error_output.stream() != nullptr);
    invoke_macro_failure_calls_handler();
    invoke_macro_failure_without_handler();
    invoke_impl_pointer_failure_calls_handler();
    invoke_impl_overload_failure_without_on_fail();
    invoke_impl_bool_failure_without_on_fail();
    invoke_impl_integer_failure_without_on_fail();
    invoke_impl_bool_failure_calls_handler();
    invoke_impl_integer_failure_calls_handler();
  }

  // manages its own redirection internally — must run outside the quiet block
  if !consteval {
    print_error_uses_sdl_error_when_message_is_empty();
  }
}

static_assert([] {
  run_utility_tests();
  return true;
}());

} // namespace sdlxx::testing

#endif // SDLXX_TESTING
