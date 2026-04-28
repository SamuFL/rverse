/// @file test_drag_and_drop.cpp
/// @brief Drag-and-drop specific tests for IsSupportedAudioFile.
///        Covers scenarios not already in test_sample_loader.cpp:
///        paths with spaces, unicode, mixed-case extensions, edge cases.
///
/// Property tests use Catch2 GENERATE to sweep supported and unsupported
/// extensions across representative path prefixes.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "SampleLoader.h"

using namespace rvrse;

// ---------------------------------------------------------------------------
// Example-based tests — drag-and-drop specific scenarios
// ---------------------------------------------------------------------------

TEST_CASE("IsSupportedAudioFile: paths with spaces", "[dragdrop]")
{
  // Unix path with spaces in directory and filename
  REQUIRE(IsSupportedAudioFile("/Users/foo/my samples/kick drum.wav"));
  REQUIRE(IsSupportedAudioFile("/Users/foo/my samples/snare hit.aif"));
  REQUIRE(IsSupportedAudioFile("/Users/foo/my samples/hi hat.aiff"));
}

TEST_CASE("IsSupportedAudioFile: paths with unicode characters", "[dragdrop]")
{
  // Unicode directory and filename
  REQUIRE(IsSupportedAudioFile("/Users/foo/\xC3\x9Cn\xC3\xAF\x63\xC3\xB6\x64\xC3\xA9/snare.aif"));
  REQUIRE(IsSupportedAudioFile("/Users/foo/\xC3\x9Cn\xC3\xAF\x63\xC3\xB6\x64\xC3\xA9/kick.wav"));
}

TEST_CASE("IsSupportedAudioFile: mixed-case extensions", "[dragdrop]")
{
  // .Wav (title case)
  REQUIRE(IsSupportedAudioFile("kick.Wav"));
  // .AIF (all caps)
  REQUIRE(IsSupportedAudioFile("snare.AIF"));
  // .Aiff (title case)
  REQUIRE(IsSupportedAudioFile("hihat.Aiff"));
}

TEST_CASE("IsSupportedAudioFile: no extension", "[dragdrop]")
{
  // No dot at all — should not be supported
  REQUIRE_FALSE(IsSupportedAudioFile("nodotfile"));
}

TEST_CASE("IsSupportedAudioFile: extension only (dot-file)", "[dragdrop]")
{
  // Filename is just the extension — still has a valid extension
  REQUIRE(IsSupportedAudioFile(".wav"));
  REQUIRE(IsSupportedAudioFile(".aif"));
  REQUIRE(IsSupportedAudioFile(".aiff"));
}

TEST_CASE("IsSupportedAudioFile: Windows-style path with spaces", "[dragdrop]")
{
  // Windows backslash path with spaces in directory and filename
  REQUIRE(IsSupportedAudioFile("C:\\My Samples\\kick.wav"));
  REQUIRE(IsSupportedAudioFile("C:\\My Samples\\snare drum.aif"));
}

// ---------------------------------------------------------------------------
// Property 1: Supported format routes to load pipeline
// Feature: drag-and-drop, Property 1: supported format routes to load pipeline
// Validates: Requirements 1.2, 2.1, 2.3
// ---------------------------------------------------------------------------

TEST_CASE("IsSupportedAudioFile: property — all supported extensions return true",
          "[dragdrop][property]")
{
  // Feature: drag-and-drop, Property 1: supported format routes to load pipeline

  auto ext = GENERATE(
    std::string(".wav"),
    std::string(".WAV"),
    std::string(".aif"),
    std::string(".AIF"),
    std::string(".aiff"),
    std::string(".AIFF")
  );

  auto prefix = GENERATE(
    std::string("sample"),                          // plain filename (no path)
    std::string("/Users/foo/samples/sample"),       // Unix path
    std::string("/Users/foo/my samples/my file"),   // Unix path with spaces
    std::string("C:\\My Samples\\sample")           // Windows path with spaces
  );

  std::string path = prefix + ext;
  INFO("Testing path: " << path);
  REQUIRE(IsSupportedAudioFile(path));
}

// ---------------------------------------------------------------------------
// Property 2: Unsupported format preserves existing sample state
// Feature: drag-and-drop, Property 2: unsupported format preserves sample state
// Validates: Requirements 2.2, 2.4
// ---------------------------------------------------------------------------

TEST_CASE("IsSupportedAudioFile: property — unsupported extensions return false",
          "[dragdrop][property]")
{
  // Feature: drag-and-drop, Property 2: unsupported format preserves sample state

  auto ext = GENERATE(
    std::string(".mp3"),
    std::string(".flac"),
    std::string(".ogg"),
    std::string(".aac"),
    std::string(".opus"),
    std::string(".m4a"),
    std::string(".txt"),
    std::string(".exe")
  );

  auto prefix = GENERATE(
    std::string("sample"),
    std::string("/Users/foo/samples/sample"),
    std::string("C:\\My Samples\\sample")
  );

  std::string path = prefix + ext;
  INFO("Testing path: " << path);
  REQUIRE_FALSE(IsSupportedAudioFile(path));
}
