#pragma once

static_assert(__cplusplus >= 202302L, "expect requires C++23");

namespace expect_detail {

// Calling this in a constexpr context causes a compile-time error,
// which IS the assertion failure — look up the call stack for the cause
void failed();

[[noreturn]] inline void breakpoint() noexcept {
#if defined(_MSC_VER)
  // __debugbreak() can return when a debugger resumes execution.
  // Loop to satisfy [[noreturn]] — control never escapes to the caller.
  while (true)
    __debugbreak();
#else
  __builtin_trap();
#endif
}

} // namespace expect_detail

// Always terminates on failure, even in release builds. In a constexpr context, it causes a compile-time error.
constexpr void expect(const auto& cond)
  requires requires { bool(cond); }
{
  if consteval {
    if (!cond) expect_detail::failed();
  } else {
    const bool result = bool(cond);
    if (!result) [[unlikely]]
      expect_detail::breakpoint();
  }
}
