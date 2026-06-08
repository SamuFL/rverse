#pragma once

/// @file AirwindowsMatrixVerb.h
/// @brief Minimal offline adaptation of Airwindows MatrixVerb (MIT).

#include "Constants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace rvrse {

struct AirwindowsMatrixVerbParams
{
  float mFilter = 1.0f;
  float mDamping = 0.0f;
  float mSpeed = 0.0f;
  float mVibrato = 0.0f;
  float mRoomSize = 0.5f;
  float mFlavor = 0.5f;
  float mPreDelayScale = 1.0f;
  float mWet = 1.0f;
};

class AirwindowsMatrixVerb
{
public:
  AirwindowsMatrixVerb()
  {
    Reset();
  }

  void SetSampleRate(double sampleRate)
  {
    mSampleRate = sampleRate > 1.0 ? sampleRate : 44100.0;
  }

  void ProcessStereo(const float* inL, const float* inR,
                     float* outL, float* outR,
                     size_t numFrames,
                     const AirwindowsMatrixVerbParams& params)
  {
    if (numFrames == 0)
    {
      return;
    }

    const double filter = std::clamp(static_cast<double>(params.mFilter), 0.0, 1.0);
    const double damping = std::clamp(static_cast<double>(params.mDamping), 0.0, 1.0);
    const double speed = std::clamp(static_cast<double>(params.mSpeed), 0.0, 1.0);
    const double vibrato = std::clamp(static_cast<double>(params.mVibrato), 0.0, 1.0);
    const double roomSize = std::clamp(static_cast<double>(params.mRoomSize), 0.0, 1.0);
    const double flavor = std::clamp(static_cast<double>(params.mFlavor), 0.0, 1.0);
    const double preDelayScale = std::max(0.0, static_cast<double>(params.mPreDelayScale));
    const double wet = std::clamp(static_cast<double>(params.mWet), 0.0, 1.0);

    mBiquadC[0] = mBiquadB[0] = mBiquadA[0] = ((filter * 9000.0) + 1000.0) / mSampleRate;
    mBiquadA[1] = 1.6180339887498948482;
    mBiquadB[1] = 0.6180339887498948482;
    mBiquadC[1] = 0.5;

    ConfigureLowpass(mBiquadA, mBiquadA[1]);
    ConfigureLowpass(mBiquadB, mBiquadB[1]);
    ConfigureLowpass(mBiquadC, mBiquadC[1]);

    const double vibSpeed = 0.06 + speed;
    const double vibDepth = (0.027 + std::pow(vibrato, 3.0)) * 100.0;
    const double size = (std::pow(roomSize, 2.0) * 90.0) + 10.0;
    const double depthFactor = 1.0 - std::pow((1.0 - (0.82 - ((damping * 0.5) + (size * 0.002)))), 4.0);
    const double blend = 0.955 - (size * 0.007);
    double crossmod = (flavor - 0.5) * 2.0;
    crossmod = std::pow(crossmod, 3.0) * 0.5;
    const double regen = depthFactor * (0.5 - (std::fabs(crossmod) * 0.031));

    mDelayA = static_cast<int>(79.0 * size);
    mDelayB = static_cast<int>(73.0 * size);
    mDelayC = static_cast<int>(71.0 * size);
    mDelayD = static_cast<int>(67.0 * size);
    mDelayE = static_cast<int>(61.0 * size);
    mDelayF = static_cast<int>(59.0 * size);
    mDelayG = static_cast<int>(53.0 * size);
    mDelayH = static_cast<int>(47.0 * size);
    mDelayI = static_cast<int>(43.0 * size);
    mDelayJ = static_cast<int>(41.0 * size);
    mDelayK = static_cast<int>(37.0 * size);
    mDelayL = static_cast<int>(31.0 * size);
    mDelayM = std::max(0, static_cast<int>(((29.0 * size) - (56.0 * size * std::fabs(crossmod))) * preDelayScale));

    for (size_t frame = 0; frame < numFrames; ++frame)
    {
      double inputSampleL = inL[frame];
      double inputSampleR = inR[frame];
      if (std::fabs(inputSampleL) < 1.18e-23) inputSampleL = static_cast<double>(mFpdL) * 1.18e-17;
      if (std::fabs(inputSampleR) < 1.18e-23) inputSampleR = static_cast<double>(mFpdR) * 1.18e-17;
      const double drySampleL = inputSampleL;
      const double drySampleR = inputSampleR;

      if (mDelayM > 0)
      {
        mAML[mCountM] = inputSampleL;
        mAMR[mCountM] = inputSampleR;
        AdvanceCount(mCountM, mDelayM);
        inputSampleL = mAML[mCountM];
        inputSampleR = mAMR[mCountM];
      }

      inputSampleL = ProcessBiquad(mBiquadA, inputSampleL, true);
      inputSampleR = ProcessBiquad(mBiquadA, inputSampleR, false);

      inputSampleL *= wet;
      inputSampleR *= wet;

      inputSampleL = std::sin(inputSampleL);
      inputSampleR = std::sin(inputSampleR);

      double allpassIL = inputSampleL;
      double allpassJL = inputSampleL;
      double allpassKL = inputSampleL;
      double allpassLL = inputSampleL;
      double allpassIR = inputSampleR;
      double allpassJR = inputSampleR;
      double allpassKR = inputSampleR;
      double allpassLR = inputSampleR;

      ProcessFrontAllpass(mAIL, mAIR, mCountI, mDelayI, allpassIL, allpassIR);
      ProcessFrontAllpass(mAJL, mAJR, mCountJ, mDelayJ, allpassJL, allpassJR);
      ProcessFrontAllpass(mAKL, mAKR, mCountK, mDelayK, allpassKL, allpassKR);
      ProcessFrontAllpass(mALL, mALR, mCountL, mDelayL, allpassLL, allpassLR);

      mAAL[mCountA] = allpassLL + mFeedbackAL;
      mABL[mCountB] = allpassKL + mFeedbackBL;
      mACL[mCountC] = allpassJL + mFeedbackCL;
      mADL[mCountD] = allpassIL + mFeedbackDL;
      mAEL[mCountE] = allpassIL + mFeedbackEL;
      mAFL[mCountF] = allpassJL + mFeedbackFL;
      mAGL[mCountG] = allpassKL + mFeedbackGL;
      mAHL[mCountH] = allpassLL + mFeedbackHL;

      mAAR[mCountA] = allpassLR + mFeedbackAR;
      mABR[mCountB] = allpassKR + mFeedbackBR;
      mACR[mCountC] = allpassJR + mFeedbackCR;
      mADR[mCountD] = allpassIR + mFeedbackDR;
      mAER[mCountE] = allpassIR + mFeedbackER;
      mAFR[mCountF] = allpassJR + mFeedbackFR;
      mAGR[mCountG] = allpassKR + mFeedbackGR;
      mAHR[mCountH] = allpassLR + mFeedbackHR;

      AdvanceCount(mCountA, mDelayA);
      AdvanceCount(mCountB, mDelayB);
      AdvanceCount(mCountC, mDelayC);
      AdvanceCount(mCountD, mDelayD);
      AdvanceCount(mCountE, mDelayE);
      AdvanceCount(mCountF, mDelayF);
      AdvanceCount(mCountG, mDelayG);
      AdvanceCount(mCountH, mDelayH);

      mVibAL += (mDepthA * vibSpeed);
      mVibBL += (mDepthB * vibSpeed);
      mVibCL += (mDepthC * vibSpeed);
      mVibDL += (mDepthD * vibSpeed);
      mVibEL += (mDepthE * vibSpeed);
      mVibFL += (mDepthF * vibSpeed);
      mVibGL += (mDepthG * vibSpeed);
      mVibHL += (mDepthH * vibSpeed);

      mVibAR += (mDepthA * vibSpeed);
      mVibBR += (mDepthB * vibSpeed);
      mVibCR += (mDepthC * vibSpeed);
      mVibDR += (mDepthD * vibSpeed);
      mVibER += (mDepthE * vibSpeed);
      mVibFR += (mDepthF * vibSpeed);
      mVibGR += (mDepthG * vibSpeed);
      mVibHR += (mDepthH * vibSpeed);

      const double offsetAL = (std::sin(mVibAL) + 1.0) * vibDepth;
      const double offsetBL = (std::sin(mVibBL) + 1.0) * vibDepth;
      const double offsetCL = (std::sin(mVibCL) + 1.0) * vibDepth;
      const double offsetDL = (std::sin(mVibDL) + 1.0) * vibDepth;
      const double offsetEL = (std::sin(mVibEL) + 1.0) * vibDepth;
      const double offsetFL = (std::sin(mVibFL) + 1.0) * vibDepth;
      const double offsetGL = (std::sin(mVibGL) + 1.0) * vibDepth;
      const double offsetHL = (std::sin(mVibHL) + 1.0) * vibDepth;
      const double offsetAR = (std::sin(mVibAR) + 1.0) * vibDepth;
      const double offsetBR = (std::sin(mVibBR) + 1.0) * vibDepth;
      const double offsetCR = (std::sin(mVibCR) + 1.0) * vibDepth;
      const double offsetDR = (std::sin(mVibDR) + 1.0) * vibDepth;
      const double offsetER = (std::sin(mVibER) + 1.0) * vibDepth;
      const double offsetFR = (std::sin(mVibFR) + 1.0) * vibDepth;
      const double offsetGR = (std::sin(mVibGR) + 1.0) * vibDepth;
      const double offsetHR = (std::sin(mVibHR) + 1.0) * vibDepth;

      double interpolAL = InterpolateTap(mAAL, mCountA, offsetAL, mDelayA);
      double interpolBL = InterpolateTap(mABL, mCountB, offsetBL, mDelayB);
      double interpolCL = InterpolateTap(mACL, mCountC, offsetCL, mDelayC);
      double interpolDL = InterpolateTap(mADL, mCountD, offsetDL, mDelayD);
      double interpolEL = InterpolateTap(mAEL, mCountE, offsetEL, mDelayE);
      double interpolFL = InterpolateTap(mAFL, mCountF, offsetFL, mDelayF);
      double interpolGL = InterpolateTap(mAGL, mCountG, offsetGL, mDelayG);
      double interpolHL = InterpolateTap(mAHL, mCountH, offsetHL, mDelayH);
      double interpolAR = InterpolateTap(mAAR, mCountA, offsetAR, mDelayA);
      double interpolBR = InterpolateTap(mABR, mCountB, offsetBR, mDelayB);
      double interpolCR = InterpolateTap(mACR, mCountC, offsetCR, mDelayC);
      double interpolDR = InterpolateTap(mADR, mCountD, offsetDR, mDelayD);
      double interpolER = InterpolateTap(mAER, mCountE, offsetER, mDelayE);
      double interpolFR = InterpolateTap(mAFR, mCountF, offsetFR, mDelayF);
      double interpolGR = InterpolateTap(mAGR, mCountG, offsetGR, mDelayG);
      double interpolHR = InterpolateTap(mAHR, mCountH, offsetHR, mDelayH);

      interpolAL = BlendTap(interpolAL, mAAL, mCountA, mDelayA, blend);
      interpolBL = BlendTap(interpolBL, mABL, mCountB, mDelayB, blend);
      interpolCL = BlendTap(interpolCL, mACL, mCountC, mDelayC, blend);
      interpolDL = BlendTap(interpolDL, mADL, mCountD, mDelayD, blend);
      interpolEL = BlendTap(interpolEL, mAEL, mCountE, mDelayE, blend);
      interpolFL = BlendTap(interpolFL, mAFL, mCountF, mDelayF, blend);
      interpolGL = BlendTap(interpolGL, mAGL, mCountG, mDelayG, blend);
      interpolHL = BlendTap(interpolHL, mAHL, mCountH, mDelayH, blend);
      interpolAR = BlendTap(interpolAR, mAAR, mCountA, mDelayA, blend);
      interpolBR = BlendTap(interpolBR, mABR, mCountB, mDelayB, blend);
      interpolCR = BlendTap(interpolCR, mACR, mCountC, mDelayC, blend);
      interpolDR = BlendTap(interpolDR, mADR, mCountD, mDelayD, blend);
      interpolER = BlendTap(interpolER, mAER, mCountE, mDelayE, blend);
      interpolFR = BlendTap(interpolFR, mAFR, mCountF, mDelayF, blend);
      interpolGR = BlendTap(interpolGR, mAGR, mCountG, mDelayG, blend);
      interpolHR = BlendTap(interpolHR, mAHR, mCountH, mDelayH, blend);

      const double interpolALBeforeCross = interpolAL;
      const double interpolARBeforeCross = interpolAR;
      interpolAL = (interpolAL * (1.0 - std::fabs(crossmod))) + (interpolEL * crossmod);
      interpolEL = (interpolEL * (1.0 - std::fabs(crossmod))) + (interpolALBeforeCross * crossmod);
      interpolAR = (interpolAR * (1.0 - std::fabs(crossmod))) + (interpolER * crossmod);
      interpolER = (interpolER * (1.0 - std::fabs(crossmod))) + (interpolARBeforeCross * crossmod);

      mFeedbackAL = (interpolAL - (interpolBL + interpolCL + interpolDL)) * regen;
      mFeedbackBL = (interpolBL - (interpolAL + interpolCL + interpolDL)) * regen;
      mFeedbackCL = (interpolCL - (interpolAL + interpolBL + interpolDL)) * regen;
      mFeedbackDL = (interpolDL - (interpolAL + interpolBL + interpolCL)) * regen;
      mFeedbackEL = (interpolEL - (interpolFL + interpolGL + interpolHL)) * regen;
      mFeedbackFL = (interpolFL - (interpolEL + interpolGL + interpolHL)) * regen;
      mFeedbackGL = (interpolGL - (interpolEL + interpolFL + interpolHL)) * regen;
      mFeedbackHL = (interpolHL - (interpolEL + interpolFL + interpolGL)) * regen;

      mFeedbackAR = (interpolAR - (interpolBR + interpolCR + interpolDR)) * regen;
      mFeedbackBR = (interpolBR - (interpolAR + interpolCR + interpolDR)) * regen;
      mFeedbackCR = (interpolCR - (interpolAR + interpolBR + interpolDR)) * regen;
      mFeedbackDR = (interpolDR - (interpolAR + interpolBR + interpolCR)) * regen;
      mFeedbackER = (interpolER - (interpolFR + interpolGR + interpolHR)) * regen;
      mFeedbackFR = (interpolFR - (interpolER + interpolGR + interpolHR)) * regen;
      mFeedbackGR = (interpolGR - (interpolER + interpolFR + interpolHR)) * regen;
      mFeedbackHR = (interpolHR - (interpolER + interpolFR + interpolGR)) * regen;

      inputSampleL = (interpolAL + interpolBL + interpolCL + interpolDL +
                      interpolEL + interpolFL + interpolGL + interpolHL) / 8.0;
      inputSampleR = (interpolAR + interpolBR + interpolCR + interpolDR +
                      interpolER + interpolFR + interpolGR + interpolHR) / 8.0;

      inputSampleL = ProcessBiquad(mBiquadB, inputSampleL, true);
      inputSampleR = ProcessBiquad(mBiquadB, inputSampleR, false);

      inputSampleL = std::clamp(inputSampleL, -1.0, 1.0);
      inputSampleR = std::clamp(inputSampleR, -1.0, 1.0);

      inputSampleL = std::asin(inputSampleL);
      inputSampleR = std::asin(inputSampleR);

      inputSampleL = ProcessBiquad(mBiquadC, inputSampleL, true);
      inputSampleR = ProcessBiquad(mBiquadC, inputSampleR, false);

      if (wet != 1.0)
      {
        inputSampleL += drySampleL * (1.0 - wet);
        inputSampleR += drySampleR * (1.0 - wet);
      }

      int exponent = 0;
      std::frexp(static_cast<float>(inputSampleL), &exponent);
      XorShift(mFpdL);
      inputSampleL += ((static_cast<double>(mFpdL) - static_cast<double>(0x7fffffffU))
                       * 5.5e-36 * std::pow(2.0, exponent + 62));
      std::frexp(static_cast<float>(inputSampleR), &exponent);
      XorShift(mFpdR);
      inputSampleR += ((static_cast<double>(mFpdR) - static_cast<double>(0x7fffffffU))
                       * 5.5e-36 * std::pow(2.0, exponent + 62));

      outL[frame] = static_cast<float>(inputSampleL);
      outR[frame] = static_cast<float>(inputSampleR);
    }
  }

private:
  static void XorShift(std::uint32_t& value)
  {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
  }

  void Reset()
  {
    mBiquadA.fill(0.0);
    mBiquadB.fill(0.0);
    mBiquadC.fill(0.0);

    mAAL.fill(0.0); mABL.fill(0.0); mACL.fill(0.0); mADL.fill(0.0);
    mAEL.fill(0.0); mAFL.fill(0.0); mAGL.fill(0.0); mAHL.fill(0.0);
    mAIL.fill(0.0); mAJL.fill(0.0); mAKL.fill(0.0); mALL.fill(0.0); mAML.fill(0.0);
    mAAR.fill(0.0); mABR.fill(0.0); mACR.fill(0.0); mADR.fill(0.0);
    mAER.fill(0.0); mAFR.fill(0.0); mAGR.fill(0.0); mAHR.fill(0.0);
    mAIR.fill(0.0); mAJR.fill(0.0); mAKR.fill(0.0); mALR.fill(0.0); mAMR.fill(0.0);

    mCountA = mCountB = mCountC = mCountD = 1;
    mCountE = mCountF = mCountG = mCountH = 1;
    mCountI = mCountJ = mCountK = mCountL = mCountM = 1;
    mDelayA = 79; mDelayB = 73; mDelayC = 71; mDelayD = 67;
    mDelayE = 61; mDelayF = 59; mDelayG = 53; mDelayH = 47;
    mDelayI = 43; mDelayJ = 41; mDelayK = 37; mDelayL = 31; mDelayM = 29;

    mFeedbackAL = mFeedbackAR = 0.0;
    mFeedbackBL = mFeedbackBR = 0.0;
    mFeedbackCL = mFeedbackCR = 0.0;
    mFeedbackDL = mFeedbackDR = 0.0;
    mFeedbackEL = mFeedbackER = 0.0;
    mFeedbackFL = mFeedbackFR = 0.0;
    mFeedbackGL = mFeedbackGR = 0.0;
    mFeedbackHL = mFeedbackHR = 0.0;

    mDepthA = 0.003251;
    mDepthB = 0.002999;
    mDepthC = 0.002917;
    mDepthD = 0.002749;
    mDepthE = 0.002503;
    mDepthF = 0.002423;
    mDepthG = 0.002146;
    mDepthH = 0.002088;

    mVibAL = 0.0; mVibBL = 0.1; mVibCL = 0.2; mVibDL = 0.3;
    mVibEL = 0.4; mVibFL = 0.5; mVibGL = 0.6; mVibHL = 0.7;
    mVibAR = 0.8; mVibBR = 0.9; mVibCR = 1.0; mVibDR = 1.1;
    mVibER = 1.2; mVibFR = 1.3; mVibGR = 1.4; mVibHR = 1.5;

    mFpdL = 17u;
    mFpdR = 29u;
  }

  static void AdvanceCount(int& count, int delay)
  {
    ++count;
    if (count < 0 || count > delay)
    {
      count = 0;
    }
  }

  static void ConfigureLowpass(std::array<double, 11>& biquad, double resonance)
  {
    const double k = std::tan(kPi * biquad[0]);
    const double norm = 1.0 / (1.0 + k / resonance + k * k);
    biquad[2] = k * k * norm;
    biquad[3] = 2.0 * biquad[2];
    biquad[4] = biquad[2];
    biquad[5] = 2.0 * (k * k - 1.0) * norm;
    biquad[6] = (1.0 - k / resonance + k * k) * norm;
  }

  static double ProcessBiquad(std::array<double, 11>& biquad, double sample, bool left)
  {
    const int idx0 = left ? 7 : 9;
    const int idx1 = left ? 8 : 10;
    const double temp = (sample * biquad[2]) + biquad[idx0];
    biquad[idx0] = (sample * biquad[3]) - (temp * biquad[5]) + biquad[idx1];
    biquad[idx1] = (sample * biquad[4]) - (temp * biquad[6]);
    return temp;
  }

  template <size_t N>
  static void ProcessFrontAllpass(std::array<double, N>& delayL,
                                  std::array<double, N>& delayR,
                                  int& count, int delay,
                                  double& sampleL, double& sampleR)
  {
    int tap = count + 1;
    if (tap < 0 || tap > delay)
    {
      tap = 0;
    }

    sampleL -= delayL[tap] * 0.5;
    delayL[count] = sampleL;
    sampleL *= 0.5;
    sampleR -= delayR[tap] * 0.5;
    delayR[count] = sampleR;
    sampleR *= 0.5;
    AdvanceCount(count, delay);
    sampleL += delayL[count];
    sampleR += delayR[count];
  }

  template <size_t N>
  static double InterpolateTap(const std::array<double, N>& delayLine,
                               int count,
                               double offset,
                               int delay)
  {
    const int working = count + static_cast<int>(offset);
    const double frac = offset - std::floor(offset);
    const int first = WrapIndex(working, delay);
    const int second = WrapIndex(working + 1, delay);
    return (delayLine[first] * (1.0 - frac)) + (delayLine[second] * frac);
  }

  template <size_t N>
  static double BlendTap(double interpolated,
                         const std::array<double, N>& delayLine,
                         int count,
                         int delay,
                         double blend)
  {
    const int working = WrapIndex(count, delay);
    return ((1.0 - blend) * interpolated) + (delayLine[working] * blend);
  }

  static int WrapIndex(int index, int delay)
  {
    const int size = delay + 1;
    while (index > delay) index -= size;
    while (index < 0) index += size;
    return index;
  }

  double mSampleRate = 44100.0;
  std::array<double, 11> mBiquadA {};
  std::array<double, 11> mBiquadB {};
  std::array<double, 11> mBiquadC {};

  std::array<double, 8111> mAAL {};
  std::array<double, 7511> mABL {};
  std::array<double, 7311> mACL {};
  std::array<double, 6911> mADL {};
  std::array<double, 6311> mAEL {};
  std::array<double, 6111> mAFL {};
  std::array<double, 5511> mAGL {};
  std::array<double, 4911> mAHL {};
  std::array<double, 4511> mAIL {};
  std::array<double, 4311> mAJL {};
  std::array<double, 3911> mAKL {};
  std::array<double, 3311> mALL {};
  std::array<double, 3111> mAML {};

  std::array<double, 8111> mAAR {};
  std::array<double, 7511> mABR {};
  std::array<double, 7311> mACR {};
  std::array<double, 6911> mADR {};
  std::array<double, 6311> mAER {};
  std::array<double, 6111> mAFR {};
  std::array<double, 5511> mAGR {};
  std::array<double, 4911> mAHR {};
  std::array<double, 4511> mAIR {};
  std::array<double, 4311> mAJR {};
  std::array<double, 3911> mAKR {};
  std::array<double, 3311> mALR {};
  std::array<double, 3111> mAMR {};

  int mCountA = 1, mDelayA = 79;
  int mCountB = 1, mDelayB = 73;
  int mCountC = 1, mDelayC = 71;
  int mCountD = 1, mDelayD = 67;
  int mCountE = 1, mDelayE = 61;
  int mCountF = 1, mDelayF = 59;
  int mCountG = 1, mDelayG = 53;
  int mCountH = 1, mDelayH = 47;
  int mCountI = 1, mDelayI = 43;
  int mCountJ = 1, mDelayJ = 41;
  int mCountK = 1, mDelayK = 37;
  int mCountL = 1, mDelayL = 31;
  int mCountM = 1, mDelayM = 29;

  double mFeedbackAL = 0.0, mVibAL = 0.0, mDepthA = 0.003251;
  double mFeedbackBL = 0.0, mVibBL = 0.0, mDepthB = 0.002999;
  double mFeedbackCL = 0.0, mVibCL = 0.0, mDepthC = 0.002917;
  double mFeedbackDL = 0.0, mVibDL = 0.0, mDepthD = 0.002749;
  double mFeedbackEL = 0.0, mVibEL = 0.0, mDepthE = 0.002503;
  double mFeedbackFL = 0.0, mVibFL = 0.0, mDepthF = 0.002423;
  double mFeedbackGL = 0.0, mVibGL = 0.0, mDepthG = 0.002146;
  double mFeedbackHL = 0.0, mVibHL = 0.0, mDepthH = 0.002088;

  double mFeedbackAR = 0.0, mVibAR = 0.0;
  double mFeedbackBR = 0.0, mVibBR = 0.0;
  double mFeedbackCR = 0.0, mVibCR = 0.0;
  double mFeedbackDR = 0.0, mVibDR = 0.0;
  double mFeedbackER = 0.0, mVibER = 0.0;
  double mFeedbackFR = 0.0, mVibFR = 0.0;
  double mFeedbackGR = 0.0, mVibGR = 0.0;
  double mFeedbackHR = 0.0, mVibHR = 0.0;

  std::uint32_t mFpdL = 17u;
  std::uint32_t mFpdR = 29u;
};

} // namespace rvrse
