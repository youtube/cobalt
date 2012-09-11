// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/multi_channel_resampler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "media/base/audio_bus.h"

namespace media {

MultiChannelResampler::MultiChannelResampler(int channels,
                                             double io_sample_rate_ratio,
                                             const ReadCB& read_cb)
    : last_frame_count_(0),
      read_cb_(read_cb) {
  // Allocate each channel's resampler.
  resamplers_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    resamplers_.push_back(new SincResampler(io_sample_rate_ratio, base::Bind(
        &MultiChannelResampler::ProvideInput, base::Unretained(this), i)));
  }
}

MultiChannelResampler::~MultiChannelResampler() {}

void MultiChannelResampler::Resample(AudioBus* audio_bus, int frames) {
  DCHECK_EQ(static_cast<size_t>(audio_bus->channels()), resamplers_.size());

  // We need to ensure that SincResampler only calls ProvideInput once for each
  // channel.  To ensure this, we chunk the number of requested frames into
  // SincResampler::ChunkSize() sized chunks.  SincResampler guarantees it will
  // only call ProvideInput() once when we resample this way.
  int frames_done = 0;
  int chunk_size = resamplers_[0]->ChunkSize();
  while (frames_done < frames) {
    int frames_this_time = std::min(frames - frames_done, chunk_size);

    // Resample each channel.
    for (size_t i = 0; i < resamplers_.size(); ++i) {
      DCHECK_EQ(chunk_size, resamplers_[i]->ChunkSize());

      // Depending on the sample-rate scale factor, and the internal buffering
      // used in a SincResampler kernel, this call to Resample() will only
      // sometimes call ProvideInput().  However, if it calls ProvideInput() for
      // the first channel, then it will call it for the remaining channels,
      // since they all buffer in the same way and are processing the same
      // number of frames.
      resamplers_[i]->Resample(
          audio_bus->channel(i) + frames_done, frames_this_time);
    }

    frames_done += frames_this_time;
  }
}

void MultiChannelResampler::ProvideInput(int channel, float* destination,
                                         int frames) {
  // Get the data from the multi-channel provider when the first channel asks
  // for it.  For subsequent channels, we can just dish out the channel data
  // from that (stored in |resampler_audio_bus_|).
  if (channel == 0) {
    // Allocate staging arrays on the first request and if the frame size or
    // |destination| changes (should only happen once).
    if (!resampler_audio_bus_.get() ||
        resampler_audio_bus_->frames() != frames ||
        wrapped_resampler_audio_bus_->channel(0) != destination) {
      resampler_audio_bus_ = AudioBus::Create(resamplers_.size(), frames);

      // Create a channel vector based on |resampler_audio_bus_| but using
      // |destination| directly for the first channel and then wrap it in a new
      // AudioBus so we can avoid an extra memcpy later.
      resampler_audio_data_.clear();
      resampler_audio_data_.reserve(resampler_audio_bus_->channels());
      resampler_audio_data_.push_back(destination);
      for (int i = 1; i < resampler_audio_bus_->channels(); ++i)
        resampler_audio_data_.push_back(resampler_audio_bus_->channel(i));
      wrapped_resampler_audio_bus_ = AudioBus::WrapVector(
          frames, resampler_audio_data_);
    }

    last_frame_count_ = frames;
    read_cb_.Run(wrapped_resampler_audio_bus_.get());
  } else {
    // All channels must ask for the same amount.  This should always be the
    // case, but let's just make sure.
    DCHECK_EQ(frames, last_frame_count_);

    // Copy the channel data from what we received from |read_cb_|.
    memcpy(destination, resampler_audio_bus_->channel(channel),
           sizeof(*resampler_audio_bus_->channel(channel)) * frames);
  }
}

void MultiChannelResampler::Flush() {
  last_frame_count_ = 0;
  for (size_t i = 0; i < resamplers_.size(); ++i)
    resamplers_[i]->Flush();
}

}  // namespace media
