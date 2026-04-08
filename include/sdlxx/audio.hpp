#pragma once

static_assert(__cplusplus >= 202302L, "sdlxx requires C++23");

#include <cassert>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

#include <SDL3/SDL_audio.h>

#include <capi/unique_id.hpp>
#include <capi/unique_res.hpp>

#include <sdlxx/utility.hpp>

namespace sdlxx::inline v1_0_0 {

enum struct AudioFormat : Uint16 {
  Unknown = SDL_AUDIO_UNKNOWN,
  U8 = SDL_AUDIO_U8,
  S8 = SDL_AUDIO_S8,
  S16 = SDL_AUDIO_S16,
  S32 = SDL_AUDIO_S32,
  F32 = SDL_AUDIO_F32
};

struct AudioSpec {
  AudioFormat format;
  size_t channels;
  size_t freq;

  constexpr AudioSpec() noexcept = default;
  constexpr AudioSpec(const SDL_AudioSpec& sdl) noexcept
    : format { static_cast<AudioFormat>(sdl.format) }, channels { static_cast<size_t>(sdl.channels) },
      freq { static_cast<size_t>(sdl.freq) } {}

  constexpr operator SDL_AudioSpec() const noexcept {
    return { .format = static_cast<SDL_AudioFormat>(format),
             .channels = static_cast<int>(channels),
             .freq = static_cast<int>(freq) };
  }

  constexpr size_t get_bit_depth() const noexcept { return SDL_AUDIO_BITSIZE(std::to_underlying(format)); }
  constexpr size_t get_bytes_per_sample() const noexcept { return SDL_AUDIO_BYTESIZE(std::to_underlying(format)); }
  constexpr size_t get_bytes_per_frame() const noexcept { return get_bytes_per_sample() * channels; }
  constexpr size_t get_bytes_per_second() const noexcept { return get_bytes_per_frame() * freq; }
  constexpr bool is_float() const noexcept { return SDL_AUDIO_ISFLOAT(std::to_underlying(format)); }
  constexpr bool is_int() const noexcept { return SDL_AUDIO_ISINT(std::to_underlying(format)); }
  constexpr bool is_big_endian() const noexcept { return SDL_AUDIO_ISBIGENDIAN(std::to_underlying(format)); }
  constexpr bool is_little_endian() const noexcept { return SDL_AUDIO_ISLITTLEENDIAN(std::to_underlying(format)); }
  constexpr bool is_signed() const noexcept { return SDL_AUDIO_ISSIGNED(std::to_underlying(format)); }
  constexpr bool is_unsigned() const noexcept { return SDL_AUDIO_ISUNSIGNED(std::to_underlying(format)); }
};

inline SDL_AudioDeviceID OpenAudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec* spec) noexcept {
  return Invoke(SDL_OpenAudioDevice(type, spec));
}

struct AudioDevice : capi::unique_id<SDL_AudioDeviceID, OpenAudioDevice, SDL_CloseAudioDevice> {
  enum Default : SDL_AudioDeviceID {
    Playback = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
    Recording = SDL_AUDIO_DEVICE_DEFAULT_RECORDING,
  };

  explicit AudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec* spec = nullptr) noexcept : unique_id(type, spec) {}
  explicit AudioDevice(SDL_AudioDeviceID type, const SDL_AudioSpec& spec) noexcept : AudioDevice(type, &spec) {}

  [[nodiscard]] AudioSpec get_spec() const noexcept {
    SDL_AudioSpec spec;
    if (!Invoke(SDL_GetAudioDeviceFormat(raw(), &spec, nullptr))) return {};
    return AudioSpec(spec);
  }

  [[nodiscard]] AudioFormat get_format() const noexcept { return get_spec().format; }
  [[nodiscard]] bool is_paused() const noexcept { return SDL_AudioDevicePaused(raw()); }
  bool pause() const noexcept { return Invoke(SDL_PauseAudioDevice(raw())); }
  bool resume() const noexcept { return Invoke(SDL_ResumeAudioDevice(raw())); }

private:
  SDL_AudioDeviceID raw() const noexcept { return static_cast<SDL_AudioDeviceID>(*this); }
};

inline SDL_AudioStream* CreateAudioStream(const AudioSpec& src_spec, const AudioDevice& device) noexcept {
  if (!device) {
    print_error("AudioDevice", "invalid");
    return nullptr;
  }
  const auto sdl_src_spec { static_cast<SDL_AudioSpec>(src_spec) };
  SDL_AudioStream* stream { Invoke(SDL_CreateAudioStream(&sdl_src_spec, nullptr)) };
  if (!stream) return nullptr;
  if (!Invoke(SDL_BindAudioStream(static_cast<SDL_AudioDeviceID>(device), stream))) {
    SDL_DestroyAudioStream(stream);
    return nullptr;
  }
  return stream;
}

struct AudioStream : capi::unique_res<SDL_AudioStream, CreateAudioStream, SDL_DestroyAudioStream> {
  explicit AudioStream(const AudioSpec& src_spec, const AudioDevice& device) noexcept : unique_res(src_spec, device) {}
  // device must outlive the stream — deleted constructor enforces this at compile time
  explicit AudioStream(const AudioSpec& src_spec, AudioDevice&& device) = delete;

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

struct AudioBuffer : private std::span<std::byte> {
  AudioBuffer() noexcept = default;
  // Takes ownership of an SDL_malloc-allocated buffer.
  explicit AudioBuffer(std::span<std::byte> data, AudioSpec spec) noexcept : std::span<std::byte>(data), spec(spec) {}
  explicit AudioBuffer(std::size_t size, AudioSpec spec = {}) noexcept
    : std::span<std::byte>(size ? static_cast<std::byte*>(SDL_malloc(size)) : nullptr, size), spec(spec) {
    assert(!size || data() != nullptr);
  }
  ~AudioBuffer() noexcept { SDL_free(data()); }
  AudioBuffer(const AudioBuffer&) = delete;
  AudioBuffer& operator=(const AudioBuffer&) = delete;
  AudioBuffer(AudioBuffer&& other) noexcept
    : std::span<std::byte>(std::exchange(static_cast<std::span<std::byte>&>(other), {})),
      spec(std::exchange(other.spec, {})) {}
  AudioBuffer& operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
      SDL_free(data());
      static_cast<std::span<std::byte>&>(*this) = std::exchange(static_cast<std::span<std::byte>&>(other), {});
      spec = std::exchange(other.spec, {});
    }
    return *this;
  }

  [[nodiscard]] bool empty() const noexcept { return std::span<std::byte>::empty(); }
  [[nodiscard]] std::size_t size() const noexcept { return std::span<std::byte>::size(); }
  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return std::as_bytes(*this); }

  // Releases the audio buffer. `spec` is preserved.
  void reset() noexcept {
    SDL_free(data());
    static_cast<std::span<std::byte>&>(*this) = {};
  }

  void write_at(std::size_t offset, std::span<const std::byte> src) noexcept {
    assert(!src.empty());
    assert(offset <= size());
    assert(src.size() <= std::numeric_limits<std::size_t>::max() - offset);
    const std::size_t new_size = offset + src.size();
    if (new_size > size()) {
      auto* new_ptr { static_cast<std::byte*>(SDL_realloc(data(), new_size)) };
      assert(new_ptr);
      static_cast<std::span<std::byte>&>(*this) = { new_ptr, new_size };
    }
    SDL_memcpy(data() + offset, src.data(), src.size());
  }

  AudioSpec spec;
};

[[nodiscard]] inline AudioBuffer LoadWAV(const std::string filename) noexcept {
  SDL_AudioSpec spec;
  Uint8* buffer;
  Uint32 length;
  if (!Invoke(SDL_LoadWAV(filename.c_str(), &spec, &buffer, &length))) return {};
  return AudioBuffer { std::as_writable_bytes(std::span(buffer, static_cast<std::size_t>(length))), spec };
}

} // namespace sdlxx::inline v1_0_0

//
//
//

#ifdef SDLXX_TESTING

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

constexpr void audio_buffer_is_move_only() {
  expect(std::is_move_constructible_v<AudioBuffer>);
  expect(std::is_move_assignable_v<AudioBuffer>);
  expect(!std::is_copy_constructible_v<AudioBuffer>);
  expect(!std::is_copy_assignable_v<AudioBuffer>);
}

constexpr void load_wav_returns_audio_buffer() {
  expect(std::is_same_v<decltype(LoadWAV(std::declval<const std::string>())), AudioBuffer>);
}

constexpr void audio_spec_default_construction_has_unknown_format() {
  const AudioSpec spec {};
  expect(spec.format == AudioFormat::Unknown);
  expect(spec.channels == 0U);
  expect(spec.freq == 0U);
}

constexpr void audio_spec_u8_format_has_8_bits_and_1_byte_per_sample() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::U8;
    s.channels = 1U;
    s.freq = 44100U;
    return s;
  }();
  expect(spec.get_bit_depth() == 8U);
  expect(spec.get_bytes_per_sample() == 1U);
  expect(spec.get_bytes_per_frame() == 1U);
  expect(spec.get_bytes_per_second() == 44100U);
  expect(spec.is_int());
  expect(!spec.is_float());
}

constexpr void audio_spec_s16_format_has_16_bits_and_2_bytes_per_sample() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    s.channels = 2U;
    s.freq = 44100U;
    return s;
  }();
  expect(spec.get_bit_depth() == 16U);
  expect(spec.get_bytes_per_sample() == 2U);
  expect(spec.get_bytes_per_frame() == 4U);
  expect(spec.get_bytes_per_second() == 176400U);
  expect(spec.is_int());
  expect(!spec.is_float());
}

constexpr void audio_spec_s32_format_has_32_bits_and_4_bytes_per_sample() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S32;
    s.channels = 1U;
    s.freq = 48000U;
    return s;
  }();
  expect(spec.get_bit_depth() == 32U);
  expect(spec.get_bytes_per_sample() == 4U);
  expect(spec.get_bytes_per_frame() == 4U);
  expect(spec.get_bytes_per_second() == 192000U);
  expect(spec.is_int());
  expect(!spec.is_float());
}

constexpr void audio_spec_f32_format_has_32_bits_and_4_bytes_per_sample() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::F32;
    s.channels = 2U;
    s.freq = 48000U;
    return s;
  }();
  expect(spec.get_bit_depth() == 32U);
  expect(spec.get_bytes_per_sample() == 4U);
  expect(spec.get_bytes_per_frame() == 8U);
  expect(spec.get_bytes_per_second() == 384000U);
  expect(!spec.is_int());
  expect(spec.is_float());
}

constexpr void audio_spec_s8_format_has_8_bits_and_1_byte_per_sample() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S8;
    s.channels = 1U;
    s.freq = 22050U;
    return s;
  }();
  expect(spec.get_bit_depth() == 8U);
  expect(spec.get_bytes_per_sample() == 1U);
  expect(spec.get_bytes_per_frame() == 1U);
  expect(spec.get_bytes_per_second() == 22050U);
  expect(spec.is_int());
  expect(!spec.is_float());
}

constexpr void audio_spec_multichannel_calculates_bytes_per_frame_correctly() {
  constexpr auto stereo_s16 = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    s.channels = 2U;
    s.freq = 44100U;
    return s;
  }();
  constexpr auto quad_s32 = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S32;
    s.channels = 4U;
    s.freq = 48000U;
    return s;
  }();
  constexpr auto mono_f32 = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::F32;
    s.channels = 1U;
    s.freq = 44100U;
    return s;
  }();

  expect(stereo_s16.get_bytes_per_frame() == 4U);
  expect(quad_s32.get_bytes_per_frame() == 16U);
  expect(mono_f32.get_bytes_per_frame() == 4U);
}

constexpr void audio_spec_sdl_conversion_preserves_format_and_channels() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    s.channels = 2U;
    s.freq = 44100U;
    return s;
  }();
  constexpr SDL_AudioSpec sdl = static_cast<SDL_AudioSpec>(spec);
  expect(sdl.format == SDL_AUDIO_S16);
  expect(sdl.channels == 2);
  expect(sdl.freq == 44100);

  constexpr AudioSpec reconstructed { sdl };
  expect(reconstructed.format == AudioFormat::S16);
  expect(reconstructed.channels == 2U);
  expect(reconstructed.freq == 44100U);
}

constexpr void audio_spec_endianness_properties() {
  constexpr auto u8_format = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::U8;
    return s;
  }();

  constexpr bool is_big = u8_format.is_big_endian();
  constexpr bool is_little = u8_format.is_little_endian();
  expect(is_big || is_little);
}

constexpr void audio_spec_is_big_endian() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    return s;
  }();
  expect(std::is_same_v<decltype(spec.is_big_endian()), bool>);
}

constexpr void audio_spec_is_little_endian() {
  constexpr auto spec = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    return s;
  }();
  expect(std::is_same_v<decltype(spec.is_little_endian()), bool>);
}

constexpr void audio_spec_signedness_properties() {
  constexpr auto s16_format = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::S16;
    return s;
  }();
  constexpr auto u8_format = []() constexpr {
    AudioSpec s {};
    s.format = AudioFormat::U8;
    return s;
  }();

  expect(s16_format.is_signed());
  expect(!s16_format.is_unsigned());

  expect(!u8_format.is_signed());
  expect(u8_format.is_unsigned());
}

inline auto make_temp_wav_path() -> std::filesystem::path {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / ("sdlxx_test_" + std::to_string(now) + ".wav");
}

inline void append_le16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0x00FFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0x00FFU));
}

inline void append_le32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0x000000FFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0x000000FFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0x000000FFU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0x000000FFU));
}

inline auto write_test_wav_file(const std::filesystem::path& path) -> bool {
  constexpr std::uint16_t channels = 1U;
  constexpr std::uint32_t sample_rate = 44100U;
  constexpr std::uint16_t bits_per_sample = 16U;
  constexpr std::uint16_t block_align = static_cast<std::uint16_t>(channels * (bits_per_sample / 8U));
  constexpr std::uint32_t byte_rate = sample_rate * block_align;
  constexpr std::array<std::uint8_t, 4> pcm_data { 0x00U, 0x00U, 0xFFU, 0x7FU };
  constexpr std::uint32_t data_size = static_cast<std::uint32_t>(pcm_data.size());
  constexpr std::uint32_t riff_chunk_size = 36U + data_size;

  std::vector<std::uint8_t> bytes {};
  bytes.reserve(44U + pcm_data.size());

  bytes.insert(bytes.end(), { 'R', 'I', 'F', 'F' });
  append_le32(bytes, riff_chunk_size);
  bytes.insert(bytes.end(), { 'W', 'A', 'V', 'E' });
  bytes.insert(bytes.end(), { 'f', 'm', 't', ' ' });
  append_le32(bytes, 16U);
  append_le16(bytes, 1U);
  append_le16(bytes, channels);
  append_le32(bytes, sample_rate);
  append_le32(bytes, byte_rate);
  append_le16(bytes, block_align);
  append_le16(bytes, bits_per_sample);
  bytes.insert(bytes.end(), { 'd', 'a', 't', 'a' });
  append_le32(bytes, data_size);
  bytes.insert(bytes.end(), pcm_data.begin(), pcm_data.end());

  std::ofstream file { path, std::ios::binary | std::ios::trunc };
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return file.good();
}

inline auto make_audio_spec(AudioFormat format, std::size_t channels, std::size_t freq) -> AudioSpec {
  AudioSpec spec {};
  spec.format = format;
  spec.channels = channels;
  spec.freq = freq;
  return spec;
}

inline void audio_buffer_default_constructs_empty() {
  AudioBuffer buffer {};
  expect(buffer.empty());
  expect(buffer.size() == 0U);
  expect(buffer.bytes().empty());
  expect(buffer.spec.format == AudioFormat::Unknown);
  expect(buffer.spec.channels == 0U);
  expect(buffer.spec.freq == 0U);
}

inline void audio_buffer_reset_clears_owned_bytes_and_preserves_spec() {
  AudioBuffer buffer { 8U, make_audio_spec(AudioFormat::S16, 2U, 44100U) };
  expect(!buffer.empty());

  buffer.reset();

  expect(buffer.empty());
  expect(buffer.size() == 0U);
  expect(buffer.spec.format == AudioFormat::S16);
  expect(buffer.spec.channels == 2U);
  expect(buffer.spec.freq == 44100U);
}

inline void audio_buffer_write_at_grows_and_writes_bytes() {
  AudioBuffer buffer { 2U, make_audio_spec(AudioFormat::S16, 1U, 22050U) };

  const std::array<std::byte, 3> source { std::byte { 0x11 }, std::byte { 0x22 }, std::byte { 0x33 } };
  buffer.write_at(1U, source);

  expect(buffer.size() == 4U);
  const auto bytes = buffer.bytes();
  expect(bytes.size() == 4U);
  expect(std::to_integer<std::uint8_t>(bytes[1]) == 0x11U);
  expect(std::to_integer<std::uint8_t>(bytes[2]) == 0x22U);
  expect(std::to_integer<std::uint8_t>(bytes[3]) == 0x33U);
}

inline void audio_buffer_move_constructor_transfers_data_and_spec() {
  AudioBuffer source { 4U, make_audio_spec(AudioFormat::S16, 2U, 48000U) };
  expect(source.size() == 4U);

  AudioBuffer moved { std::move(source) };

  expect(source.empty());
  expect(source.spec.format == AudioFormat::Unknown);
  expect(source.spec.channels == 0U);
  expect(source.spec.freq == 0U);
  expect(moved.size() == 4U);
  expect(moved.spec.format == AudioFormat::S16);
  expect(moved.spec.channels == 2U);
  expect(moved.spec.freq == 48000U);
}

inline void audio_buffer_move_assignment_transfers_data_and_spec() {
  AudioBuffer source { 6U, make_audio_spec(AudioFormat::S8, 1U, 16000U) };
  AudioBuffer target { 2U, make_audio_spec(AudioFormat::F32, 2U, 96000U) };

  target = std::move(source);

  expect(source.empty());
  expect(source.spec.format == AudioFormat::Unknown);
  expect(source.spec.channels == 0U);
  expect(source.spec.freq == 0U);
  expect(target.size() == 6U);
  expect(target.spec.format == AudioFormat::S8);
  expect(target.spec.channels == 1U);
  expect(target.spec.freq == 16000U);
}

inline void load_wav_missing_path_returns_empty_buffer() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  const auto missing_path = make_temp_wav_path();
  std::error_code remove_error {};
  (void)std::filesystem::remove(missing_path, remove_error);

  AudioBuffer buffer = LoadWAV(missing_path.string());
  expect(buffer.empty());
  expect(buffer.size() == 0U);
}

inline void write_test_wav_file_returns_false_for_missing_parent_directory() {
  const auto missing_parent = make_temp_wav_path();
  const auto path = missing_parent / "nested" / "fixture.wav";
  std::error_code remove_error {};
  (void)std::filesystem::remove_all(missing_parent, remove_error);

  expect(!write_test_wav_file(path));
}

inline void load_wav_valid_file_returns_audio_data_and_spec() {
  const auto wav_path = make_temp_wav_path();
  expect(write_test_wav_file(wav_path));

  AudioBuffer buffer = LoadWAV(wav_path.string());

  std::error_code remove_error {};
  (void)std::filesystem::remove(wav_path, remove_error);

  expect(!buffer.empty());
  expect(buffer.size() == 4U);
  expect(buffer.spec.format == AudioFormat::S16);
  expect(buffer.spec.channels == 1U);
  expect(buffer.spec.freq == 44100U);

  const auto bytes = buffer.bytes();
  expect(bytes.size() == 4U);
  expect(std::to_integer<std::uint8_t>(bytes[0]) == 0x00U);
  expect(std::to_integer<std::uint8_t>(bytes[1]) == 0x00U);
  expect(std::to_integer<std::uint8_t>(bytes[2]) == 0xFFU);
  expect(std::to_integer<std::uint8_t>(bytes[3]) == 0x7FU);
}

inline void audio_device_invalid_id_exercises_failure_paths() {
  scoped_quiet_error_output quiet_error_output;
  expect(quiet_error_output.stream() != nullptr);

  AudioDevice invalid { 0U };
  expect(!static_cast<bool>(invalid));
  expect(static_cast<SDL_AudioDeviceID>(invalid) == 0U);

  const AudioFormat format = invalid.get_format();
  expect(format == AudioFormat::Unknown);

  expect(!invalid.pause());
  expect(!invalid.resume());
  expect(!invalid.is_paused());
}

inline void audio_spec_all_endianness_and_signedness_methods() {
  const AudioSpec s16_spec = make_audio_spec(AudioFormat::S16, 2U, 44100U);
  const AudioSpec u8_spec = make_audio_spec(AudioFormat::U8, 1U, 22050U);

  const bool s16_big = s16_spec.is_big_endian();
  const bool s16_little = s16_spec.is_little_endian();
  const bool s16_signed = s16_spec.is_signed();
  const bool s16_unsigned = s16_spec.is_unsigned();

  const bool u8_big = u8_spec.is_big_endian();
  const bool u8_little = u8_spec.is_little_endian();
  const bool u8_signed = u8_spec.is_signed();
  const bool u8_unsigned = u8_spec.is_unsigned();

  expect((s16_big || s16_little));
  expect((u8_big || u8_little));
  expect(s16_signed);
  expect(!s16_unsigned);
  expect(!u8_signed);
  expect(u8_unsigned);
}

inline void audio_device_default_open_has_consistent_state() {
  AudioSubSystem audio_sub_system {};
  AudioDevice device { SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK };
  const bool opened = static_cast<bool>(device);
  const SDL_AudioDeviceID id = static_cast<SDL_AudioDeviceID>(device);

  expect((id != 0U) == opened);
  const AudioFormat format = device.get_format();
  expect(!opened || format != AudioFormat::Unknown);

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
  audio_buffer_is_move_only();
  load_wav_returns_audio_buffer();
  audio_spec_default_construction_has_unknown_format();
  audio_spec_u8_format_has_8_bits_and_1_byte_per_sample();
  audio_spec_s16_format_has_16_bits_and_2_bytes_per_sample();
  audio_spec_s32_format_has_32_bits_and_4_bytes_per_sample();
  audio_spec_f32_format_has_32_bits_and_4_bytes_per_sample();
  audio_spec_s8_format_has_8_bits_and_1_byte_per_sample();
  audio_spec_multichannel_calculates_bytes_per_frame_correctly();
  audio_spec_sdl_conversion_preserves_format_and_channels();
  audio_spec_endianness_properties();
  audio_spec_is_big_endian();
  audio_spec_is_little_endian();
  audio_spec_signedness_properties();

  if !consteval {
    AudioDevice playback_device { AudioDevice::Playback };
    audio_device_invalid_id_exercises_failure_paths();
    audio_spec_all_endianness_and_signedness_methods();
    audio_device_default_open_has_consistent_state();
    audio_device_constructor_with_spec_matches_open_state();
    create_audio_stream_rejects_invalid_device();
    audio_stream_with_invalid_device_behaves_as_empty();
    create_audio_stream_with_open_device_has_consistent_state();
    create_audio_stream_cleans_up_when_bind_fails();
    audio_stream_with_open_device_exercises_operations();
    audio_buffer_default_constructs_empty();
    audio_buffer_reset_clears_owned_bytes_and_preserves_spec();
    audio_buffer_write_at_grows_and_writes_bytes();
    audio_buffer_move_constructor_transfers_data_and_spec();
    audio_buffer_move_assignment_transfers_data_and_spec();
    load_wav_missing_path_returns_empty_buffer();
    write_test_wav_file_returns_false_for_missing_parent_directory();
    load_wav_valid_file_returns_audio_data_and_spec();
  }
}

static_assert([] {
  run_audio_tests();
  return true;
}());

} // namespace sdlxx::testing

#endif // SDLXX_TESTING
