#pragma once

/// @file ReverbEngineFactory.h
/// @brief Factory for the developer-selected offline reverb engine.

#include "AirwindowsReverbEngine.h"
#include "CurrentSchroederReverbEngine.h"
#include "ReverbEngineDevConfig.h"
#include "WDLReverbEngine.h"

#include <memory>

namespace rvrse {

inline std::unique_ptr<IReverbEngine> MakeActiveReverbEngine()
{
  if constexpr (kActiveReverbEngine == EReverbEngineKind::CurrentTuned)
  {
    return std::make_unique<CurrentSchroederReverbEngine>();
  }
  else if constexpr (kActiveReverbEngine == EReverbEngineKind::AirwindowsMatrixVerb)
  {
    return std::make_unique<AirwindowsReverbEngine>();
  }
  else if constexpr (kActiveReverbEngine == EReverbEngineKind::WDLVerbEngine)
  {
    return std::make_unique<WDLReverbEngineAdapter>();
  }

  return nullptr;
}

} // namespace rvrse
