// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_SATURATED_GAIN_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_SATURATED_GAIN_H_

#include <string>

#include "chromecast/media/base/slew_volume.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

// Applys a gain to audio, limited such that gain + system_volume <= 0dB.
class SaturatedGain : public AudioPostProcessor2 {
 public:
  SaturatedGain(const std::string& config, int channels);

  SaturatedGain(const SaturatedGain&) = delete;
  SaturatedGain& operator=(const SaturatedGain&) = delete;

  ~SaturatedGain() override;

  // AudioPostProcessor implementation:
  bool SetConfig(const Config& config) override;
  const Status& GetStatus() override;
  void ProcessFrames(float* data, int frames, Metadata* metadata) override;
  bool UpdateParameters(const std::string& message) override;

 private:
  Status status_;
  float last_volume_dbfs_;
  SlewVolume slew_volume_;
  float gain_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_POST_PROCESSORS_SATURATED_GAIN_H_
