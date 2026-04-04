/// @file test_sample_loader.cpp
/// @brief Unit tests for SampleLoader — file I/O, validation, deinterleaving.
///        Uses dr_wav to generate test WAV files programmatically (no fixtures needed).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "SampleLoader.h"
#include "Constants.h"
#include "test_helpers.h"
#include "dr_wav.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

using Catch::Approx;
using namespace rvrse;

namespace {

/// Generate a unique temp filename using PID + a static counter to avoid collisions.
std::string uniqueTempName(const std::string& prefix, const std::string& ext)
{
  static std::atomic<int> counter{0};
  return prefix + "_" + std::to_string(getpid()) + "_"
       + std::to_string(counter.fetch_add(1)) + ext;
}

/// RAII helper that writes a WAV file on construction and deletes it on destruction.
class TempWav
{
public:
  TempWav(const std::string& filename, const std::vector<float>& samples,
          unsigned int sampleRate, unsigned int channels)
    : mPath(std::filesystem::temp_directory_path() / filename)
  {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_IEEE_FLOAT;
    format.channels = channels;
    format.sampleRate = sampleRate;
    format.bitsPerSample = 32;

    drwav wav;
    if (!drwav_init_file_write(&wav, mPath.string().c_str(), &format, nullptr))
      throw std::runtime_error("TempWav: failed to create " + mPath.string());

    const drwav_uint64 expectedFrames = samples.size() / channels;
    const drwav_uint64 written = drwav_write_pcm_frames(&wav, expectedFrames, samples.data());
    drwav_uninit(&wav);

    if (written != expectedFrames)
      throw std::runtime_error("TempWav: wrote " + std::to_string(written)
                               + "/" + std::to_string(expectedFrames) + " frames");
  }

  ~TempWav() noexcept
  {
    std::error_code ec;
    std::filesystem::remove(mPath, ec);
  }

  std::string path() const { return mPath.string(); }

private:
  std::filesystem::path mPath;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: empty path returns error", "[sampleloader]")
{
  auto result = LoadSample("");
  REQUIRE_FALSE(result.success);
  REQUIRE(result.errorMessage.find("Empty") != std::string::npos);
}

TEST_CASE("SampleLoader: non-existent file returns error", "[sampleloader]")
{
  auto path = std::filesystem::temp_directory_path() / "rvrse_nonexistent_9a8b7c6d.wav";
  // Ensure it truly doesn't exist
  std::error_code ec;
  std::filesystem::remove(path, ec);

  auto result = LoadSample(path.string());
  REQUIRE_FALSE(result.success);
  REQUIRE(result.errorMessage.find("Failed to open") != std::string::npos);
}

TEST_CASE("SampleLoader: unsupported extension returns error", "[sampleloader]")
{
  auto path = std::filesystem::temp_directory_path() / "rvrse_test.mp3";
  auto result = LoadSample(path.string());
  REQUIRE_FALSE(result.success);
  REQUIRE(result.errorMessage.find("Unsupported") != std::string::npos);
}

// ---------------------------------------------------------------------------
// IsSupportedAudioFile
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: IsSupportedAudioFile", "[sampleloader]")
{
  // Supported
  REQUIRE(IsSupportedAudioFile("kick.wav"));
  REQUIRE(IsSupportedAudioFile("SNARE.WAV"));
  REQUIRE(IsSupportedAudioFile("hi_hat.Wav"));
  REQUIRE(IsSupportedAudioFile("sample.aif"));
  REQUIRE(IsSupportedAudioFile("sample.AIFF"));

  // Not supported
  REQUIRE_FALSE(IsSupportedAudioFile("sample.mp3"));
  REQUIRE_FALSE(IsSupportedAudioFile("sample.flac"));
  REQUIRE_FALSE(IsSupportedAudioFile("sample.ogg"));
  REQUIRE_FALSE(IsSupportedAudioFile("sample.txt"));
  REQUIRE_FALSE(IsSupportedAudioFile("filewav")); // no dot
  REQUIRE_FALSE(IsSupportedAudioFile(""));
}

// ---------------------------------------------------------------------------
// ExtractFileName
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: ExtractFileName", "[sampleloader]")
{
  REQUIRE(ExtractFileName("/Users/foo/kick.wav") == "kick.wav");
  REQUIRE(ExtractFileName("C:\\Windows\\sample.aif") == "sample.aif");
  REQUIRE(ExtractFileName("no_path.wav") == "no_path.wav");
  REQUIRE(ExtractFileName("") == "");
}

// ---------------------------------------------------------------------------
// Load mono WAV → stereo duplication
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: load mono WAV duplicates to stereo", "[sampleloader]")
{
  // Generate a 0.1s mono 440 Hz sine at 44100 Hz
  const unsigned int sr = 44100;
  const unsigned int numFrames = sr / 10; // 4410 frames
  auto samples = rvrse::test::generateSine(numFrames, 440.0, static_cast<double>(sr));

  TempWav wav(uniqueTempName("rvrse_mono", ".wav"), samples, sr, 1);
  auto result = LoadSample(wav.path());

  REQUIRE(result.success);
  REQUIRE(result.data.mNumChannels == 1);
  REQUIRE(result.data.mSampleRate == Approx(44100.0));
  REQUIRE(result.data.NumFrames() == static_cast<int>(numFrames));

  // Mono: L and R should be identical
  REQUIRE(result.data.mLeft == result.data.mRight);

  // Spot-check a sample value
  REQUIRE(result.data.mLeft[0] == Approx(samples[0]).margin(0.001f));
}

// ---------------------------------------------------------------------------
// Load stereo WAV → correct deinterleaving
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: load stereo WAV deinterleaves correctly", "[sampleloader]")
{
  const unsigned int sr = 48000;
  const unsigned int numFrames = 100;

  // Interleaved stereo: L=[1,2,3,...], R=[10,20,30,...]
  std::vector<float> interleaved(numFrames * 2);
  for (unsigned int i = 0; i < numFrames; ++i)
  {
    interleaved[i * 2]     = static_cast<float>(i + 1);
    interleaved[i * 2 + 1] = static_cast<float>((i + 1) * 10);
  }

  TempWav wav(uniqueTempName("rvrse_stereo", ".wav"), interleaved, sr, 2);
  auto result = LoadSample(wav.path());

  REQUIRE(result.success);
  REQUIRE(result.data.mNumChannels == 2);
  REQUIRE(result.data.mSampleRate == Approx(48000.0));
  REQUIRE(result.data.NumFrames() == static_cast<int>(numFrames));

  // Verify deinterleaving
  for (int i = 0; i < static_cast<int>(numFrames); ++i)
  {
    REQUIRE(result.data.mLeft[i]  == Approx(static_cast<float>(i + 1)));
    REQUIRE(result.data.mRight[i] == Approx(static_cast<float>((i + 1) * 10)));
  }
}

// ---------------------------------------------------------------------------
// Oversized file → error
// ---------------------------------------------------------------------------

TEST_CASE("SampleLoader: oversized file rejected", "[sampleloader]")
{
  // Create a file just over the 30-second limit at 44100 Hz
  const unsigned int sr = 44100;
  const unsigned int numFrames = sr * (kMaxSampleLengthSeconds + 1); // 31 seconds
  std::vector<float> samples(numFrames, 0.0f);

  TempWav wav(uniqueTempName("rvrse_oversized", ".wav"), samples, sr, 1);
  auto result = LoadSample(wav.path());

  REQUIRE_FALSE(result.success);
  REQUIRE(result.errorMessage.find("too long") != std::string::npos);
}
