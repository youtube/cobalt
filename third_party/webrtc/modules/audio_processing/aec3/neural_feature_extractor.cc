/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/neural_feature_extractor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "api/array_view.h"
#include "common_audio/window_generator.h"
#include "rtc_base/checks.h"
#include "third_party/pffft/src/pffft.h"

namespace webrtc {

namespace {
// Trained moodel expects [-1,1]-scaled signals while AEC3 and APM scale
// floating point signals up by 32768 to match 16-bit fixed-point formats, so we
// convert to [-1,1] scale here.
constexpr float kScale = 1.0f / 32768;
// Exponent used to compress the power spectra.
constexpr float kSpectrumCompressionExponent = 0.15f;

std::vector<float> GetSqrtHanningWindow(int frame_size, float scale) {
  std::vector<float> window(frame_size);
  WindowGenerator::Hanning(frame_size, window.data());
  std::transform(window.begin(), window.end(), window.begin(),
                 [scale](float x) { return scale * std::sqrt(x); });
  return window;
}

}  // namespace

void TimeDomainFeatureExtractor::PushFeaturesToModelInput(
    std::vector<float>& frame,
    ArrayView<float> input) {
  // Shift down overlap from previous frames.
  std::copy(input.begin() + frame.size(), input.end(), input.begin());
  std::transform(frame.begin(), frame.end(), input.end() - frame.size(),
                 [](float x) { return x * kScale; });
  frame.clear();
}

FrequencyDomainFeatureExtractor::FrequencyDomainFeatureExtractor(int step_size)
    : step_size_(step_size),
      frame_size_(2 * step_size_),
      sqrt_hanning_(GetSqrtHanningWindow(frame_size_, kScale)),
      data_(static_cast<float*>(
          pffft_aligned_malloc(frame_size_ * sizeof(float)))),
      spectrum_(static_cast<float*>(
          pffft_aligned_malloc(frame_size_ * sizeof(float)))),
      pffft_setup_(pffft_new_setup(frame_size_, PFFFT_REAL)) {
  std::memset(data_, 0, sizeof(float) * frame_size_);
  std::memset(spectrum_, 0, sizeof(float) * frame_size_);
}

FrequencyDomainFeatureExtractor::~FrequencyDomainFeatureExtractor() {
  pffft_destroy_setup(pffft_setup_);
  pffft_aligned_free(spectrum_);
  pffft_aligned_free(data_);
}

void FrequencyDomainFeatureExtractor::PushFeaturesToModelInput(
    std::vector<float>& frame,
    ArrayView<float> input) {
  std::memcpy(data_ + step_size_, frame.data(), sizeof(float) * step_size_);
  for (int k = 0; k < frame_size_; ++k) {
    data_[k] *= sqrt_hanning_[k];
  }
  pffft_transform_ordered(pffft_setup_, data_, spectrum_, nullptr,
                          PFFFT_FORWARD);
  RTC_CHECK_EQ(input.size(), step_size_ + 1);
  input[0] = spectrum_[0] * spectrum_[0];
  input[step_size_] = spectrum_[1] * spectrum_[1];
  for (int k = 1; k < step_size_; k++) {
    input[k] = spectrum_[2 * k] * spectrum_[2 * k] +
               spectrum_[2 * k + 1] * spectrum_[2 * k + 1];
  }
  // Compress the power spectra.
  std::transform(input.begin(), input.end(), input.begin(), [](float a) {
    return std::pow(a, kSpectrumCompressionExponent);
  });
  // Saving the current frame as it is used when computing the next FFT.
  std::memcpy(data_, frame.data(), sizeof(float) * step_size_);
}

}  // namespace webrtc
