// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_bus.h"

#include <limits>

#include "base/logging.h"
#include "media/audio/audio_parameters.h"
#include "media/base/limits.h"

namespace media {

// Ensure each channel is 16-byte aligned for easy SSE optimizations.
static const int kChannelAlignment = 16;

static bool IsAligned(void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) & (kChannelAlignment - 1)) == 0U;
}

AudioBus::AudioBus(int channels, int frames)
    : frames_(frames) {
  CHECK_GT(frames, 0);
  CHECK_LE(frames, limits::kMaxSamplesPerPacket);
  CHECK_GT(channels, 0);
  CHECK_LE(channels, limits::kMaxChannels);
  DCHECK_LT(limits::kMaxSamplesPerPacket * limits::kMaxChannels,
            std::numeric_limits<int>::max());

  // Choose a size such that each channel is aligned by kChannelAlignment.
  int aligned_frames =
      (frames_ + kChannelAlignment - 1) & ~(kChannelAlignment - 1);
  data_size_ = sizeof(float) * channels * aligned_frames;

  data_.reset(static_cast<float*>(base::AlignedAlloc(
      data_size_, kChannelAlignment)));

  // Separate audio data out into channels for easy lookup later.
  channel_data_.reserve(channels);
  for (int i = 0; i < channels; ++i)
    channel_data_.push_back(data_.get() + i * aligned_frames);
}

AudioBus::AudioBus(int frames, const std::vector<float*>& channel_data)
    : data_size_(0),
      channel_data_(channel_data),
      frames_(frames) {
  // Sanity check wrapped vector for alignment and channel count.
  for (size_t i = 0; i < channel_data_.size(); ++i)
    DCHECK(IsAligned(channel_data_[i]));
}

AudioBus::~AudioBus() {}

scoped_ptr<AudioBus> AudioBus::Create(int channels, int frames) {
  return scoped_ptr<AudioBus>(new AudioBus(channels, frames));
}

scoped_ptr<AudioBus> AudioBus::Create(const AudioParameters& params) {
  return scoped_ptr<AudioBus>(new AudioBus(
      params.channels(), params.frames_per_buffer()));
}

scoped_ptr<AudioBus> AudioBus::WrapVector(
    int frames, const std::vector<float*>& channel_data) {
  return scoped_ptr<AudioBus>(new AudioBus(frames, channel_data));
}

void* AudioBus::data() {
  DCHECK(data_.get());
  return data_.get();
}

int AudioBus::data_size() const {
  DCHECK(data_.get());
  return data_size_;
}

void AudioBus::ZeroFrames(int frames) {
  DCHECK_LE(frames, frames_);
  for (size_t i = 0; i < channel_data_.size(); ++i)
    memset(channel_data_[i], 0, frames * sizeof(*channel_data_[i]));
}

void AudioBus::Zero() {
  ZeroFrames(frames_);
}

}  // namespace media
