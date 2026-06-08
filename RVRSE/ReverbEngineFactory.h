#pragma once

/// @file ReverbEngineFactory.h
/// @brief Factory for the retained offline reverb engine.

#include "AirwindowsReverbEngine.h"

#include <memory>

namespace rvrse {

inline std::unique_ptr<IReverbEngine> MakeActiveReverbEngine()
{
  return std::make_unique<AirwindowsReverbEngine>();
}

} // namespace rvrse
