// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/base/limits.h"

namespace media {

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      extra_data_size_(0) {
}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       int bits_per_channel,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const uint8* extra_data,
                                       size_t extra_data_size) {
  Initialize(codec, bits_per_channel, channel_layout, samples_per_second,
             extra_data, extra_data_size);
}

// Helper enum used only for histogramming samples-per-second.  Put
// commonly-used rates here to get accurate reporting.  Uncommon rates are
// reported in a separate, bucketized, histogram.
enum AudioSamplesPerSecond {
  k8000Hz,
  k16000Hz,
  k32000Hz,
  k48000Hz,
  k96000Hz,
  k11025Hz,
  k22050Hz,
  k44100Hz,
  kUnexpected  // Must always be last!
};

// Helper method to convert integral values to their respective enum values
// above, or kUnexpected if no match exists.
static AudioSamplesPerSecond AsAudioSamplesPerSecond(int samples_per_second) {
  switch (samples_per_second) {
    case 8000: return k8000Hz;
    case 16000: return k16000Hz;
    case 32000: return k32000Hz;
    case 48000: return k48000Hz;
    case 11025: return k11025Hz;
    case 22050: return k22050Hz;
    case 44100: return k44100Hz;
    default: return kUnexpected;
  }
}

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    int bits_per_channel,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const uint8* extra_data,
                                    size_t extra_data_size) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  UMA_HISTOGRAM_ENUMERATION("Media.AudioCodec", codec, kAudioCodecMax + 1);
  // Fake enum histogram to get exact integral buckets.  Expect to never see
  // any values over 32 and even that is huge.
  UMA_HISTOGRAM_ENUMERATION("Media.AudioBitsPerChannel", bits_per_channel, 40);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.AudioChannelLayout", channel_layout, CHANNEL_LAYOUT_MAX);
  AudioSamplesPerSecond asps = AsAudioSamplesPerSecond(samples_per_second);
  if (asps != kUnexpected) {
    UMA_HISTOGRAM_ENUMERATION("Media.AudioSamplesPerSecond", asps, kUnexpected);
  } else {
    UMA_HISTOGRAM_COUNTS(
        "Media.AudioSamplesPerSecondUnexpected", samples_per_second);
  }

  codec_ = codec;
  bits_per_channel_ = bits_per_channel;
  channel_layout_ = channel_layout;
  samples_per_second_ = samples_per_second;
  extra_data_size_ = extra_data_size;

  if (extra_data_size_ > 0) {
    extra_data_.reset(new uint8[extra_data_size_]);
    memcpy(extra_data_.get(), extra_data, extra_data_size_);
  } else {
    extra_data_.reset();
  }
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
      channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
      bits_per_channel_ > 0 &&
      bits_per_channel_ <= Limits::kMaxBitsPerSample &&
      samples_per_second_ > 0 &&
      samples_per_second_ <= Limits::kMaxSampleRate;
}

AudioCodec AudioDecoderConfig::codec() const {
  return codec_;
}

int AudioDecoderConfig::bits_per_channel() const {
  return bits_per_channel_;
}

ChannelLayout AudioDecoderConfig::channel_layout() const {
  return channel_layout_;
}

int AudioDecoderConfig::samples_per_second() const {
  return samples_per_second_;
}

uint8* AudioDecoderConfig::extra_data() const {
  return extra_data_.get();
}

size_t AudioDecoderConfig::extra_data_size() const {
  return extra_data_size_;
}

}  // namespace media
