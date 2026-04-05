#include "RVRSE.h"
#include "IPlug_include_in_plug_src.h"
#include "BufferUtils.h"
#include "Constants.h"
#include "SampleLoader.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <fstream>

#if IPLUG_EDITOR
#include "IControls.h"
#endif

RVRSE::RVRSE(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamMasterVol)->InitDouble("Master Volume", 100., 0., 100.0, 0.01, "%");
  GetParam(kParamStutterRate)->InitDouble("Stutter Rate",
    rvrse::kStutterRateDefaultHz, rvrse::kStutterRateMinHz, rvrse::kStutterRateMaxHz, 0.1, "Hz");
  GetParam(kParamStutterDepth)->InitDouble("Stutter Depth",
    rvrse::kStutterDepthDefault / 100.0, 0., 1.0, 0.01, "");
  GetParam(kParamLush)->InitDouble("Lush",
    rvrse::kLushDefault, 0., 100.0, 0.1, "%");
  GetParam(kParamRiserLength)->InitDouble("Riser Length",
    rvrse::kRiserLengthDefault, rvrse::kRiserLengthMin, rvrse::kRiserLengthMax, 0.25, "beats");
  GetParam(kParamFadeIn)->InitDouble("Fade In",
    rvrse::kFadeInDefault, 0., 100.0, 0.1, "%");
  GetParam(kParamRiserVolume)->InitDouble("Riser Volume",
    rvrse::kRiserVolumeDefault, rvrse::kVolumeMinDb, rvrse::kVolumeMaxDb, 0.1, "dB");
  GetParam(kParamHitVolume)->InitDouble("Hit Volume",
    rvrse::kHitVolumeDefault, rvrse::kVolumeMinDb, rvrse::kVolumeMaxDb, 0.1, "dB");
  GetParam(kParamDebugStage)->InitEnum("Debug Stage", rvrse::kDebugNormal, {
    "Normal", "Reverbed", "Reversed", "Riser Only"
  });

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS);
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    const IRECT bounds = pGraphics->GetBounds();
    const IRECT innerBounds = bounds.GetPadded(-10.f);
    const IRECT versionBounds = innerBounds.GetFromTRHC(300, 20);
    const IRECT titleBounds = innerBounds.GetCentredInside(300, 60).GetVShifted(-80.f);
    const IRECT loadBtnBounds = innerBounds.GetCentredInside(200, 40);
    const IRECT sampleNameBounds = innerBounds.GetCentredInside(600, 24).GetVShifted(40.f);

    if (pGraphics->NControls()) {
      pGraphics->GetBackgroundControl()->SetTargetAndDrawRECTs(bounds);
      pGraphics->GetControlWithTag(kCtrlTagTitle)->SetTargetAndDrawRECTs(titleBounds);
      pGraphics->GetControlWithTag(kCtrlTagVersionNumber)->SetTargetAndDrawRECTs(versionBounds);
      pGraphics->GetControlWithTag(kCtrlTagLoadButton)->SetTargetAndDrawRECTs(loadBtnBounds);
      pGraphics->GetControlWithTag(kCtrlTagSampleName)->SetTargetAndDrawRECTs(sampleNameBounds);
      return;
    }

    pGraphics->SetLayoutOnResize(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Size, true);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->AttachPanelBackground(COLOR_DARK_GRAY);

    // Title
    pGraphics->AttachControl(new ITextControl(titleBounds, "RVRSE",
      IText(40, COLOR_WHITE)), kCtrlTagTitle);

    // Version info
    WDL_String buildInfoStr;
    GetBuildInfoStr(buildInfoStr, __DATE__, __TIME__);
    pGraphics->AttachControl(new ITextControl(versionBounds, buildInfoStr.Get(),
      IText(12, COLOR_LIGHT_GRAY).WithAlign(EAlign::Far)), kCtrlTagVersionNumber);

    // Load Sample button
    const IVStyle buttonStyle = DEFAULT_STYLE
      .WithColor(kFG, IColor(255, 100, 180, 255))
      .WithColor(kBG, IColor(255, 40, 40, 40))
      .WithRoundness(0.3f);

    pGraphics->AttachControl(new IVButtonControl(loadBtnBounds, [this](IControl* pCaller) {
      // Spawn file dialog from the UI thread
      WDL_String fileName;
      WDL_String path;
      pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open,
        rvrse::kSupportedAudioExts,
        [this, pCaller](const WDL_String& fileName, const WDL_String& path) {
          // Note: on macOS, iPlug2 sets fileName to the FULL path,
          // and path to the directory with trailing slash.
          if (fileName.GetLength() > 0)
          {
            LoadSampleFromFile(fileName.Get());
          }
        });
    }, "LOAD SAMPLE", buttonStyle), kCtrlTagLoadButton);

    // Sample name display
    pGraphics->AttachControl(new ITextControl(sampleNameBounds, "No sample loaded",
      IText(14, IColor(200, 180, 180, 180))), kCtrlTagSampleName);
  };
#endif
}

void RVRSE::LoadSampleFromFile(const char* filePath)
{
  mSampleFilePath = filePath;
  mLoadState.store(rvrse::ESampleLoadState::Loading);

  // Update UI to show loading state
  if (GetUI())
  {
    if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
      pCtrl->As<ITextControl>()->SetStr("Loading...");
  }

  // Do the actual loading on a background thread to avoid blocking the UI
  std::string pathCopy(filePath);
  std::thread([this, pathCopy]() {
    auto result = rvrse::LoadSample(pathCopy);

    if (result.success)
    {
      auto newSample = std::make_shared<rvrse::SampleData>(std::move(result.data));

      // Resample the hit to the DAW's output sample rate if they differ.
      // This ensures ProcessBlock can play it sample-by-sample at the correct pitch.
      const double outputSR = GetSampleRate();
      auto playSample = newSample;

      if (outputSR > 0.0 && std::abs(newSample->mSampleRate - outputSR) > 0.5)
      {
        auto resampled = std::make_shared<rvrse::SampleData>();
        rvrse::resampleLinearStereo(newSample->mLeft, newSample->mRight,
                                    newSample->mSampleRate, outputSR,
                                    resampled->mLeft, resampled->mRight);
        resampled->mSampleRate = outputSR;
        resampled->mNumChannels = newSample->mNumChannels;
        resampled->mFilePath = newSample->mFilePath;
        resampled->mFileName = newSample->mFileName;
        playSample = resampled;
      }

      {
        std::lock_guard<std::mutex> lock(mSampleMutex);
        mHitSample = playSample;
      }

      // Signal the audio thread that a new sample is ready
      mNewSampleReady.store(true, std::memory_order_release);
      mLoadState.store(rvrse::ESampleLoadState::Ready);

      // Feed the ORIGINAL sample into the offline pipeline (it resamples internally)
      mProcessor.setSample(newSample);

      // Update UI on the main thread via SendControlMsgFromDelegate isn't ideal,
      // so we use GetUI() — this is safe because we check and the UI lambda captures
      // will run on the next UI tick
      if (GetUI())
      {
        if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
        {
          WDL_String displayStr;
          displayStr.SetFormatted(256, "%s (%d Hz, %s, %.1fs)",
            newSample->mFileName.c_str(),
            static_cast<int>(newSample->mSampleRate),
            newSample->mNumChannels == 1 ? "mono" : "stereo",
            static_cast<double>(newSample->NumFrames()) / newSample->mSampleRate);
          pCtrl->As<ITextControl>()->SetStr(displayStr.Get());
          pCtrl->SetDirty(false);
        }
      }
    }
    else
    {
      mLoadState.store(rvrse::ESampleLoadState::Error);

      if (GetUI())
      {
        if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
        {
          WDL_String errStr;
          errStr.SetFormatted(256, "Error: %s", result.errorMessage.c_str());
          pCtrl->As<ITextControl>()->SetStr(errStr.Get());
          pCtrl->SetDirty(false);
        }
      }
    }
  }).detach();
}

// --- State persistence (save/restore with DAW project) ---

static constexpr int kStateChunkVersion = 1;

bool RVRSE::SerializeState(IByteChunk& chunk) const
{
  chunk.Put(&kStateChunkVersion);
  chunk.PutStr(mSampleFilePath.c_str());
  return SerializeParams(chunk);
}

int RVRSE::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int version = 0;
  startPos = chunk.Get(&version, startPos);
  if (startPos < 0) return startPos;

  if (version >= 1)
  {
    WDL_String pathStr;
    startPos = chunk.GetStr(pathStr, startPos);
    if (startPos < 0) return startPos;

    const std::string restoredPath(pathStr.Get());

    if (!restoredPath.empty())
    {
      std::ifstream probe(restoredPath);
      if (probe.good())
      {
        LoadSampleFromFile(restoredPath.c_str());
      }
      else
      {
        mSampleFilePath = restoredPath;
        mLoadState.store(rvrse::ESampleLoadState::Error);

        if (GetUI())
        {
          if (auto* pCtrl = GetUI()->GetControlWithTag(kCtrlTagSampleName))
          {
            WDL_String errStr;
            errStr.SetFormatted(256, "Missing: %s",
              rvrse::ExtractFileName(restoredPath).c_str());
            pCtrl->As<ITextControl>()->SetStr(errStr.Get());
            pCtrl->SetDirty(false);
          }
        }
      }
    }
  }

  return UnserializeParams(chunk, startPos);
}

#if IPLUG_DSP
void RVRSE::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  const double masterVol = GetParam(kParamMasterVol)->Value() / 100.0;
  const auto stutterRateHz = static_cast<float>(GetParam(kParamStutterRate)->Value());
  const float stutterDepth = static_cast<float>(GetParam(kParamStutterDepth)->Value());
  const float lush = static_cast<float>(GetParam(kParamLush)->Value() / 100.0);
  const double riserLengthBeats = GetParam(kParamRiserLength)->Value();
  const float fadeInPct = static_cast<float>(GetParam(kParamFadeIn)->Value() / 100.0);
  const float riserVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamRiserVolume)->Value()) / 20.0f);
  const float hitVolumeGain = std::pow(10.0f, static_cast<float>(GetParam(kParamHitVolume)->Value()) / 20.0f);
  const auto debugStage = static_cast<rvrse::EDebugStage>(
    static_cast<int>(GetParam(kParamDebugStage)->Value()));
  const double sr = GetSampleRate();

  // Read host BPM and propagate to the offline pipeline when it changes.
  // GetTempo() reads from the host-provided ITimeInfo (populated each block).
  const double hostBPM = GetTempo();
  if (hostBPM > 0.0 && std::abs(hostBPM - mLastBPM) > 0.01)
  {
    mLastBPM = hostBPM;
    mProcessor.setBPM(hostBPM);
  }

  // Propagate Lush to the offline processor (triggers reverb rebuild)
  if (std::abs(lush - mLastLush) > 1e-4f)
  {
    mLastLush = lush;
    mProcessor.setLush(lush);
  }

  // Propagate Riser Length to the offline processor (triggers stretch rebuild)
  if (std::abs(riserLengthBeats - mLastRiserLength) > 1e-6)
  {
    mLastRiserLength = riserLengthBeats;
    mProcessor.setRiserLength(riserLengthBeats);
  }

  // Check if a new sample is ready from the loader thread (lock-free flag)
  if (mNewSampleReady.load(std::memory_order_acquire))
  {
    {
      std::lock_guard<std::mutex> lock(mSampleMutex);
      mPlaySample = mHitSample;
    }
    mNewSampleReady.store(false, std::memory_order_release);
    // Reset all voice state when new sample arrives
    mRiserPos = -1;
    mHitPos = -1;
    mSamplesFromNoteOn = -1;
  }

  // Check if a new riser buffer is ready from the offline pipeline (lock-free).
  // Latch: only swap when no riser is actively playing to avoid mid-note discontinuities.
  if (mProcessor.isNewRiserReady() && mRiserPos < 0)
  {
    mRiserBuffer = mProcessor.consumeRiser();
  }

  // Get local pointers — no further locking needed
  const auto& hit = mPlaySample;
  const auto& riser = mRiserBuffer;

  for (int s = 0; s < nFrames; s++)
  {
    // Process MIDI events at this sample offset
    while (!mMidiQueue.Empty() && mMidiQueue.Peek().mOffset <= s)
    {
      const IMidiMsg& msg = mMidiQueue.Peek();

      if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() > 0)
      {
        // Note-on: start the riser from the beginning
        mVelocityGain = static_cast<float>(msg.Velocity()) / 127.0f;
        mRiserFadeRemaining = 0;
        mHitFadeRemaining = 0;
        rvrse::stutterReset(mStutterState);

        if (riser && riser->IsReady())
        {
          mRiserPos = 0;
          mHitPos = -1;

          // In debug modes, play the selected buffer; only trigger hit in Normal mode
          if (debugStage == rvrse::kDebugNormal)
          {
            mSamplesFromNoteOn = 0;
            mHitOffset = riser->NumFrames();
          }
          else
          {
            mSamplesFromNoteOn = -1; // No hit in debug modes
          }
        }
        else if (hit && hit->IsLoaded())
        {
          // No riser available yet — fall back to direct hit playback
          mRiserPos = -1;
          mSamplesFromNoteOn = -1;
          mHitPos = 0;
        }
      }
      else if (msg.StatusMsg() == IMidiMsg::kNoteOff ||
               (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() == 0))
      {
        // Note-off: begin fade-out on both voices and kill the hit trigger
        if (mRiserPos >= 0)
          mRiserFadeRemaining = mFadeOutLength;
        if (mHitPos >= 0)
          mHitFadeRemaining = mFadeOutLength;

        // Stop the hit trigger counter so the hit can't re-trigger after fade-out
        mSamplesFromNoteOn = -1;
      }

      else if (msg.StatusMsg() == IMidiMsg::kControlChange)
      {
        const IMidiMsg::EControlChangeMsg cc = msg.ControlChangeIdx();

        if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_RATE))
          GetParam(kParamStutterRate)->Set(msg.ControlChange(cc) * rvrse::kStutterRateMaxHz);
        else if (cc == static_cast<IMidiMsg::EControlChangeMsg>(rvrse::CC_STUTTER_DEPTH))
          GetParam(kParamStutterDepth)->Set(msg.ControlChange(cc));
      }

      mMidiQueue.Remove();
    }

    // --- Generate riser/debug output ---
    float riserL = 0.0f;
    float riserR = 0.0f;

    if (mRiserPos >= 0 && riser && riser->IsReady())
    {
      // Select the active buffer based on debug stage
      const float* bufL = riser->mLeft.data();
      const float* bufR = riser->mRight.data();
      int bufLen = riser->NumFrames();

      if (debugStage == rvrse::kDebugReverbed && riser->ReverbedFrames() > 0)
      {
        bufL = riser->mReverbedL.data();
        bufR = riser->mReverbedR.data();
        bufLen = riser->ReverbedFrames();
      }
      else if (debugStage == rvrse::kDebugReversed && riser->ReversedFrames() > 0)
      {
        bufL = riser->mReversedL.data();
        bufR = riser->mReversedR.data();
        bufLen = riser->ReversedFrames();
      }

      // Precompute fade-in length (constant for this buffer)
      const int fadeInLen = static_cast<int>(static_cast<float>(bufLen) * fadeInPct);

      if (mRiserPos < bufLen)
      {
        riserL = bufL[mRiserPos];
        riserR = bufR[mRiserPos];

        const bool isDebugRaw = (debugStage == rvrse::kDebugReverbed ||
                                 debugStage == rvrse::kDebugReversed);

        if (!isDebugRaw)
        {
          // Apply fade-in envelope over the first fadeInPct of the buffer
          if (fadeInPct > 0.0f && fadeInLen > 1 && mRiserPos < fadeInLen)
          {
            const float fadeGain = static_cast<float>(mRiserPos) / static_cast<float>(fadeInLen - 1);
            riserL *= fadeGain;
            riserR *= fadeGain;
          }

          riserL *= mVelocityGain * riserVolumeGain;
          riserR *= mVelocityGain * riserVolumeGain;

          // Apply stutter gate (per-sample, MIDI CC responsive)
          const float stutterGain = rvrse::stutterProcess(
            mStutterState, stutterRateHz, stutterDepth, sr);
          riserL *= stutterGain;
          riserR *= stutterGain;
        }
        else
        {
          // Debug raw modes: only velocity gain (no fade-in, stutter, or riser volume)
          riserL *= mVelocityGain;
          riserR *= mVelocityGain;
        }

        if (mRiserFadeRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mRiserFadeRemaining) / static_cast<float>(mFadeOutLength);
          riserL *= fadeGain;
          riserR *= fadeGain;
          mRiserFadeRemaining--;
          if (mRiserFadeRemaining == 0) mRiserPos = -1;
        }

        if (mRiserPos >= 0) mRiserPos++;
      }
      else
      {
        // Buffer finished naturally
        mRiserPos = -1;
      }
    }

    // --- Check if it's time to fire the hit ---
    if (mSamplesFromNoteOn >= 0 && mHitPos < 0)
    {
      if (mSamplesFromNoteOn >= mHitOffset && hit && hit->IsLoaded())
      {
        mHitPos = 0; // Fire the hit!
      }
      mSamplesFromNoteOn++;
    }

    // --- Generate hit output ---
    float hitL = 0.0f;
    float hitR = 0.0f;

    if (mHitPos >= 0 && hit && hit->IsLoaded())
    {
      if (mHitPos < hit->NumFrames())
      {
        hitL = hit->mLeft[mHitPos] * mVelocityGain * hitVolumeGain;
        hitR = hit->mRight[mHitPos] * mVelocityGain * hitVolumeGain;

        if (mHitFadeRemaining > 0)
        {
          const float fadeGain = static_cast<float>(mHitFadeRemaining) / static_cast<float>(mFadeOutLength);
          hitL *= fadeGain;
          hitR *= fadeGain;
          mHitFadeRemaining--;
          if (mHitFadeRemaining == 0) mHitPos = -1;
        }

        if (mHitPos >= 0) mHitPos++;
      }
      else
      {
        // Hit finished naturally
        mHitPos = -1;
        mSamplesFromNoteOn = -1;
      }
    }

    // --- Mix and output (additive — both voices have independent volume) ---
    const float outL = (riserL + hitL) * static_cast<float>(masterVol);
    const float outR = (riserR + hitR) * static_cast<float>(masterVol);

    if (nChans >= 1) outputs[0][s] = outL;
    if (nChans >= 2) outputs[1][s] = outR;
  }

  mMidiQueue.Flush(nFrames);
}

void RVRSE::ProcessMidiMsg(const IMidiMsg& msg)
{
  TRACE
  mMidiQueue.Add(msg);
}

void RVRSE::OnReset()
{
  mMidiQueue.Resize(GetBlockSize());
  // Compute fade-out length from sample rate (e.g., 5ms at 44100 Hz = 220 samples)
  mFadeOutLength = std::max(1, static_cast<int>(GetSampleRate() * rvrse::kNoteOffFadeMs / 1000.0));

  // Update the offline pipeline with the current sample rate
  mProcessor.setSampleRate(GetSampleRate());
}
#endif
