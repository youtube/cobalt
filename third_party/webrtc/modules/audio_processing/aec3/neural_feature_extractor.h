/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_NEURAL_FEATURE_EXTRACTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_NEURAL_FEATURE_EXTRACTOR_H_

#include <vector>

#include "api/array_view.h"
#include "third_party/pffft/src/pffft.h"
namespace webrtc {

class FeatureExtractor {
 public:
  virtual ~FeatureExtractor() = default;
  virtual void PushFeaturesToModelInput(std::vector<float>& frame,
                                        ArrayView<float> input) = 0;
};

class TimeDomainFeatureExtractor : public FeatureExtractor {
  void PushFeaturesToModelInput(std::vector<float>& frame,
                                ArrayView<float> input) override;
};

class FrequencyDomainFeatureExtractor : public FeatureExtractor {
 public:
  explicit FrequencyDomainFeatureExtractor(int step_size);
  ~FrequencyDomainFeatureExtractor();
  void PushFeaturesToModelInput(std::vector<float>& frame,
                                ArrayView<float> input) override;

 private:
  const int step_size_;
  const int frame_size_;
  const std::vector<float> sqrt_hanning_;
  float* const data_;
  float* const spectrum_;
  PFFFT_Setup* pffft_setup_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_FEATURE_EXTRACTOR_H_
