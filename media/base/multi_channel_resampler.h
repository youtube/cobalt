// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_
#define MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "media/base/sinc_resampler.h"

namespace media {
class AudioBus;

// MultiChannelResampler is a multi channel wrapper for SincResampler; allowing
// high quality sample rate conversion of multiple channels at once.
class MEDIA_EXPORT MultiChannelResampler {
 public:
  // Callback type for providing more data into the resampler.  Expects AudioBus
  // to be completely filled with data upon return; zero padded if not enough
  // frames are available to satisfy the request.
  typedef base::Callback<void(AudioBus* audio_bus)> ReadCB;

  // Constructs a MultiChannelResampler with the specified |read_cb|, which is
  // used to acquire audio data for resampling.  |io_sample_rate_ratio| is the
  // ratio of input / output sample rates.
  MultiChannelResampler(int channels, double io_sample_rate_ratio,
                        const ReadCB& read_cb);
  virtual ~MultiChannelResampler();

  // Resamples |frames| of data from |read_cb_| into AudioBus.
  void Resample(AudioBus* audio_bus, int frames);

  // Flush all buffered data and reset internal indices.
  void Flush();

 private:
  // SincResampler::ReadCB implementation.  ProvideInput() will be called for
  // each channel (in channel order) as SincResampler needs more data.
  void ProvideInput(int channel, float* destination, int frames);

  // Sanity check to ensure that ProvideInput() retrieves the same number of
  // frames for every channel.
  int last_frame_count_;

  // Source of data for resampling.
  ReadCB read_cb_;

  // Each channel has its own high quality resampler.
  ScopedVector<SincResampler> resamplers_;

  // Buffers for audio data going into SincResampler from ReadCB.
  scoped_ptr<AudioBus> resampler_audio_bus_;
  scoped_ptr<AudioBus> wrapped_resampler_audio_bus_;
  std::vector<float*> resampler_audio_data_;
};

}  // namespace media

#endif  // MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_
