#include <cstdlib>
#include <print>
#include <string_view>

#include <expect/expect.hpp>

#include <sdlxx/audio.hpp>
#include <sdlxx/init.hpp>
#include <sdlxx/timer.hpp>
#include <sdlxx/utility.hpp>

int main(int argc, char** argv) {
  const auto run_all = [] {
    sdlxx::testing::run_audio_tests();
    sdlxx::testing::run_init_tests();
    sdlxx::testing::run_timer_tests();
    sdlxx::testing::run_utility_tests();
  };

  if (argc < 2) {
    run_all();
    return EXIT_SUCCESS;
  }

  const std::string_view selector { argv[1] };

  if (selector == "utility") {
    sdlxx::testing::run_utility_tests();
    return EXIT_SUCCESS;
  }

  if (selector == "init") {
    sdlxx::testing::run_init_tests();
    return EXIT_SUCCESS;
  }

  if (selector == "audio") {
    sdlxx::testing::run_audio_tests();
    return EXIT_SUCCESS;
  }

  if (selector == "timer") {
    sdlxx::testing::run_timer_tests();
    return EXIT_SUCCESS;
  }

  if (selector == "all") {
    run_all();
    return EXIT_SUCCESS;
  }

  std::println(stderr, "unknown selector: {}", selector);

  return EXIT_FAILURE;
}
