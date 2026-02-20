/// @file SampleLoader.cpp
/// @brief Implementation of sample loading functions.
///        OFFLINE ONLY — never call from the audio thread.

#include "SampleLoader.h"
#include "Constants.h"
#include "dr_wav.h"

#include <algorithm>
#include <cstring>

namespace rvrse {

SampleLoadResult LoadSample(const std::string& filePath)
{
  SampleLoadResult result;

  if (filePath.empty())
  {
    result.errorMessage = "Empty file path";
    return result;
  }

  if (!IsSupportedAudioFile(filePath))
  {
    result.errorMessage = "Unsupported file format (need .wav, .aif, or .aiff)";
    return result;
  }

  // dr_wav handles both WAV and AIFF containers
  drwav wav;
  if (!drwav_init_file(&wav, filePath.c_str(), nullptr))
  {
    result.errorMessage = "Failed to open audio file: " + filePath;
    return result;
  }

  const drwav_uint64 totalFrames = wav.totalPCMFrameCount;
  const unsigned int channels = wav.channels;
  const unsigned int sampleRate = wav.sampleRate;

  // Sanity checks
  if (totalFrames == 0)
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file is empty (0 frames)";
    return result;
  }

  if (totalFrames > static_cast<drwav_uint64>(kMaxSampleFrames))
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file too long (max " + std::to_string(kMaxSampleLengthSeconds) + " seconds at " + std::to_string(sampleRate) + " Hz)";
    return result;
  }

  if (channels == 0)
  {
    drwav_uninit(&wav);
    result.errorMessage = "Audio file has 0 channels";
    return result;
  }

  // Read all frames as interleaved float32
  const size_t interleavedSize = static_cast<size_t>(totalFrames) * channels;
  std::vector<float> interleaved(interleavedSize);

  const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(&wav, totalFrames, interleaved.data());
  drwav_uninit(&wav);

  if (framesRead == 0)
  {
    result.errorMessage = "Failed to read PCM data from file";
    return result;
  }

  // Deinterleave into separate L/R channels
  const int numFrames = static_cast<int>(framesRead);
  result.data.mLeft.resize(numFrames);
  result.data.mRight.resize(numFrames);

  if (channels == 1)
  {
    // Mono: duplicate to both channels
    std::copy(interleaved.begin(), interleaved.begin() + numFrames, result.data.mLeft.begin());
    std::copy(interleaved.begin(), interleaved.begin() + numFrames, result.data.mRight.begin());
  }
  else
  {
    // Stereo (or more): deinterleave first 2 channels
    for (int i = 0; i < numFrames; ++i)
    {
      result.data.mLeft[i]  = interleaved[static_cast<size_t>(i) * channels];
      result.data.mRight[i] = interleaved[static_cast<size_t>(i) * channels + 1];
    }
  }

  result.data.mSampleRate = static_cast<double>(sampleRate);
  result.data.mNumChannels = static_cast<int>(std::min(channels, 2u));
  result.data.mFilePath = filePath;
  result.data.mFileName = ExtractFileName(filePath);

  result.success = true;
  return result;
}

std::string ExtractFileName(const std::string& filePath)
{
  // Find last path separator (works on both macOS/Linux and Windows)
  const auto posSlash = filePath.find_last_of('/');
  const auto posBackslash = filePath.find_last_of('\\');

  size_t pos = std::string::npos;
  if (posSlash != std::string::npos && posBackslash != std::string::npos)
    pos = std::max(posSlash, posBackslash);
  else if (posSlash != std::string::npos)
    pos = posSlash;
  else if (posBackslash != std::string::npos)
    pos = posBackslash;

  if (pos != std::string::npos)
    return filePath.substr(pos + 1);

  return filePath;
}

bool IsSupportedAudioFile(const std::string& filePath)
{
  if (filePath.size() < 4)
    return false;

  // Find the extension
  const auto dotPos = filePath.find_last_of('.');
  if (dotPos == std::string::npos)
    return false;

  std::string ext = filePath.substr(dotPos);

  // Convert to lowercase for comparison
  std::transform(ext.begin(), ext.end(), ext.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}

} // namespace rvrse
