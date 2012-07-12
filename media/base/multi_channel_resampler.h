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

// MultiChannelResampler is a multi channel wrapper for SincResampler; allowing
// high quality sample rate conversion of multiple channels at once.
class MEDIA_EXPORT MultiChannelResampler {
 public:
  // Callback type for providing more data into the resampler.  Expects |frames|
  // of data for all channels to be rendered into |destination|; zero padded if
  // not enough frames are available to satisfy the request.
  typedef base::Callback<void(const std::vector<float*>& destination,
                              int frames)> ReadCB;

  // Constructs a MultiChannelResampler with the specified |read_cb|, which is
  // used to acquire audio data for resampling.  |io_sample_rate_ratio| is the
  // ratio of input / output sample rates.
  MultiChannelResampler(int channels, double io_sample_rate_ratio,
                        const ReadCB& read_cb);
  virtual ~MultiChannelResampler();

  // Resample |frames| of data from |read_cb_| into |destination|.
  void Resample(const std::vector<float*>& destination, int frames);

 private:
  // SincResampler::ReadCB implementation.  ProvideInput() will be called for
  // each channel (in channel order) as SincResampler needs more data.
  void ProvideInput(int channel, float* destination, int frames);

  // Sanity check to ensure that ProvideInput() retrieves the same number of
  // frames for every channel.
  int last_frame_count_;

  // Sanity check to ensure |resampler_audio_data_| is properly allocated.
  int first_frame_count_;

  // Source of data for resampling.
  ReadCB read_cb_;

  // Each channel has its own high quality resampler.
  ScopedVector<SincResampler> resamplers_;

  // Buffer for audio data going into SincResampler from ReadCB.  Owned by this
  // class and only temporarily passed out to ReadCB when data is required.
  std::vector<float*> resampler_audio_data_;
};

}  // namespace media

#endif  // MEDIA_BASE_MULTI_CHANNEL_RESAMPLER_H_
