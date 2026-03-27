#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <cstddef>
#include <limits>
#include <span>

#include <SDL3/SDL_audio.h>

#include <capi/unique_id.hpp>
#include <capi/unique_res.hpp>

#include <sdlxx/utility.hpp>

namespace sdlxx::inline v1_0_0 {

inline SDL_AudioDeviceID OpenAudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec* spec) noexcept {
  return Invoke(SDL_OpenAudioDevice(type, spec));
}

struct AudioDevice : capi::unique_id<SDL_AudioDeviceID, OpenAudioDevice, SDL_CloseAudioDevice> {
  explicit AudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec* spec = nullptr) noexcept : unique_id(type, spec) {}
  explicit AudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec& spec) noexcept : AudioDevice(type, &spec) {}

  [[nodiscard]] SDL_AudioSpec get_format() const noexcept {
    SDL_AudioSpec spec;
    if (!Invoke(SDL_GetAudioDeviceFormat(raw(), &spec, nullptr))) return {};
    return spec;
  }

  [[nodiscard]] bool is_paused() const noexcept { return SDL_AudioDevicePaused(raw()); }

  bool pause() const noexcept { return Invoke(SDL_PauseAudioDevice(raw())); }
  bool resume() const noexcept { return Invoke(SDL_ResumeAudioDevice(raw())); }

private:
  SDL_AudioDeviceID raw() const noexcept { return static_cast<SDL_AudioDeviceID>(*this); }
};

inline SDL_AudioStream* CreateAudioStream(const SDL_AudioSpec& src_spec, const AudioDevice& device) noexcept {
  if (!device) {
    print_error("AudioDevice", "invalid");
    return nullptr;
  }
  SDL_AudioStream* stream { Invoke(SDL_CreateAudioStream(&src_spec, nullptr)) };
  if (!stream) return nullptr;
  if (!Invoke(SDL_BindAudioStream(static_cast<SDL_AudioDeviceID>(device), stream))) {
    SDL_DestroyAudioStream(stream);
    return nullptr;
  }
  return stream;
}

struct AudioStream : capi::unique_res<SDL_AudioStream, CreateAudioStream, SDL_DestroyAudioStream> {
  explicit AudioStream(const SDL_AudioSpec& src_spec, const AudioDevice& device) noexcept
    : unique_res(src_spec, device) {}
  // device must outlive the stream — deleted constructor enforces this at compile time
  explicit AudioStream(const SDL_AudioSpec& src_spec, AudioDevice&& device) = delete;

  bool put(std::span<const std::byte> bytes) const noexcept {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    return Invoke(SDL_PutAudioStreamData(raw(), bytes.data(), static_cast<int>(bytes.size())));
  }

  [[nodiscard]] std::size_t get(std::span<std::byte> bytes) const noexcept {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return 0;
    int result { Invoke(SDL_GetAudioStreamData(raw(), bytes.data(), static_cast<int>(bytes.size()))) };
    if (result <= 0) return 0;
    return static_cast<std::size_t>(result);
  }

  [[nodiscard]] std::size_t get_available() const noexcept {
    int result { Invoke(SDL_GetAudioStreamAvailable(raw())) };
    if (result <= 0) return 0;
    return static_cast<std::size_t>(result);
  }

  bool clear() const noexcept { return Invoke(SDL_ClearAudioStream(raw())); }

private:
  SDL_AudioStream* raw() const noexcept { return static_cast<SDL_AudioStream*>(*this); }
};

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <array>
#include <type_traits>
#include <utility>

#include <expect/expect.hpp>

#include <sdlxx/init.hpp>

namespace sdlxx::testing {

constexpr void open_audio_device_returns_audio_device_id() {
  expect(std::is_same_v<decltype(OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr)), SDL_AudioDeviceID>);
}

constexpr void audio_device_is_move_only() {
  expect(std::is_move_constructible_v<AudioDevice>);
  expect(std::is_move_assignable_v<AudioDevice>);
  expect(!std::is_copy_constructible_v<AudioDevice>);
  expect(!std::is_copy_assignable_v<AudioDevice>);
}

constexpr void audio_device_has_explicit_conversions() {
  expect(std::is_same_v<decltype(static_cast<bool>(std::declval<const AudioDevice&>())), bool>);
  expect(
    std::is_same_v<decltype(static_cast<SDL_AudioDeviceID>(std::declval<const AudioDevice&>())), SDL_AudioDeviceID>);
}

constexpr void create_audio_stream_returns_stream_pointer() {
  expect(std::is_same_v<decltype(CreateAudioStream(std::declval<const SDL_AudioSpec&>(),
                                                   std::declval<const AudioDevice&>())),
                        SDL_AudioStream*>);
}

constexpr void audio_stream_is_move_only() {
  expect(std::is_move_constructible_v<AudioStream>);
  expect(std::is_move_assignable_v<AudioStream>);
  expect(!std::is_copy_constructible_v<AudioStream>);
  expect(!std::is_copy_assignable_v<AudioStream>);
}

constexpr void audio_stream_has_expected_constructors_and_conversions() {
  expect(std::is_constructible_v<AudioStream, const SDL_AudioSpec&, const AudioDevice&>);
  expect(!std::is_constructible_v<AudioStream, const SDL_AudioSpec&, AudioDevice&&>);
  expect(std::is_same_v<decltype(static_cast<bool>(std::declval<const AudioStream&>())), bool>);
  expect(std::is_same_v<decltype(static_cast<SDL_AudioStream*>(std::declval<const AudioStream&>())), SDL_AudioStream*>);
}

inline void audio_device_invalid_id_exercises_failure_paths() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  AudioDevice invalid { 0U };
  expect(!static_cast<bool>(invalid));
  expect(static_cast<SDL_AudioDeviceID>(invalid) == 0U);

  const SDL_AudioSpec format = invalid.get_format();
  expect(format.freq == 0);
  expect(format.channels == 0);
  expect(format.format == SDL_AUDIO_UNKNOWN);

  expect(!invalid.pause());
  expect(!invalid.resume());
  expect(!invalid.is_paused());
}

inline void audio_device_default_open_has_consistent_state() {
  AudioSubSystem audio_sub_system {};
  AudioDevice device { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };
  const bool opened = static_cast<bool>(device);
  const SDL_AudioDeviceID id = static_cast<SDL_AudioDeviceID>(device);

  expect((id != 0U) == opened);
  const SDL_AudioSpec format = device.get_format();
  expect(!opened || format.freq > 0);
  expect(!opened || format.channels > 0);
  expect(!opened || format.format != SDL_AUDIO_UNKNOWN);

  const bool pause_result = device.pause();
  expect(!opened || pause_result);
  expect(!opened || device.is_paused());

  const bool resume_result = device.resume();
  expect(!opened || resume_result);
  expect(!opened || !device.is_paused());
}

inline void audio_device_constructor_with_spec_matches_open_state() {
  AudioSubSystem audio_sub_system {};

  SDL_AudioSpec requested {};
  requested.freq = 44100;
  requested.format = SDL_AUDIO_S16;
  requested.channels = 2;

  AudioDevice from_pointer { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested };
  const bool pointer_opened = static_cast<bool>(from_pointer);
  expect((static_cast<SDL_AudioDeviceID>(from_pointer) != 0U) == pointer_opened);

  AudioDevice from_reference { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, requested };
  const bool reference_opened = static_cast<bool>(from_reference);
  expect((static_cast<SDL_AudioDeviceID>(from_reference) != 0U) == reference_opened);
}

inline auto default_stream_spec() -> SDL_AudioSpec {
  SDL_AudioSpec spec {};
  spec.freq = 44100;
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;
  return spec;
}

inline void create_audio_stream_rejects_invalid_device() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  AudioDevice invalid_device { 0U };
  const SDL_AudioSpec spec = default_stream_spec();
  SDL_AudioStream* stream = CreateAudioStream(spec, invalid_device);
  expect(stream == nullptr);
}

inline void audio_stream_with_invalid_device_behaves_as_empty() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  AudioDevice invalid_device { 0U };
  const SDL_AudioSpec spec = default_stream_spec();
  AudioStream stream { spec, invalid_device };

  expect(!static_cast<bool>(stream));
  expect(static_cast<SDL_AudioStream*>(stream) == nullptr);

  const std::array<std::byte, 8> input {};
  expect(!stream.put(input));

  std::array<std::byte, 8> output {};
  expect(stream.get(output) == 0U);
  expect(stream.get_available() == 0U);
  expect(!stream.clear());
}

inline void create_audio_stream_with_open_device_has_consistent_state() {
  AudioSubSystem audio_sub_system {};
  AudioDevice device { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };
  const bool opened = static_cast<bool>(device);

  const SDL_AudioSpec requested = default_stream_spec();
  SDL_AudioStream* stream = CreateAudioStream(requested, device);
  const bool created = stream != nullptr;

  expect(!created || opened);

  if (stream) {
    SDL_DestroyAudioStream(stream);
  }
}

inline void create_audio_stream_cleans_up_when_bind_fails() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  AudioSubSystem audio_sub_system {};
  AudioDevice device { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };

  if (static_cast<bool>(device)) {
    // Force a stale logical device id so binding fails after stream creation succeeds.
    SDL_CloseAudioDevice(static_cast<SDL_AudioDeviceID>(device));

    const SDL_AudioSpec spec = default_stream_spec();
    SDL_AudioStream* stream = CreateAudioStream(spec, device);
    expect(stream == nullptr);
  }
}

inline void audio_stream_with_open_device_exercises_operations() {
  AudioSubSystem audio_sub_system {};
  AudioDevice device { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };
  const SDL_AudioSpec requested = default_stream_spec();
  AudioStream stream { requested, device };

  const bool created = static_cast<bool>(stream);
  expect((static_cast<SDL_AudioStream*>(stream) != nullptr) == created);

  const std::array<std::byte, 32> input {};
  const bool put_result = stream.put(input);
  expect(!created || put_result);

  const std::size_t available = stream.get_available();
  expect(!created || available <= input.size());

  std::array<std::byte, 32> output {};
  const std::size_t copied = stream.get(output);
  expect(copied <= output.size());

  const bool clear_result = stream.clear();
  expect(!created || clear_result);
}

constexpr void run_audio_tests() {
  open_audio_device_returns_audio_device_id();
  audio_device_is_move_only();
  audio_device_has_explicit_conversions();
  create_audio_stream_returns_stream_pointer();
  audio_stream_is_move_only();
  audio_stream_has_expected_constructors_and_conversions();

  if !consteval {
    audio_device_invalid_id_exercises_failure_paths();
    audio_device_default_open_has_consistent_state();
    audio_device_constructor_with_spec_matches_open_state();
    create_audio_stream_rejects_invalid_device();
    audio_stream_with_invalid_device_behaves_as_empty();
    create_audio_stream_with_open_device_has_consistent_state();
    create_audio_stream_cleans_up_when_bind_fails();
    audio_stream_with_open_device_exercises_operations();
  }
}

static_assert([] {
  run_audio_tests();
  return true;
}());

} // namespace sdlxx::testing

#endif // SDLXX_TESTING
