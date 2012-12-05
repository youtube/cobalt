// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/audio/sample_rates.h"
#include "media/base/limits.h"

namespace media {

AudioDecoderConfig::AudioDecoderConfig()
    : codec_(kUnknownAudioCodec),
      bits_per_channel_(0),
      channel_layout_(CHANNEL_LAYOUT_UNSUPPORTED),
      samples_per_second_(0),
      bytes_per_frame_(0),
      extra_data_size_(0),
      is_encrypted_(false) {
}

AudioDecoderConfig::AudioDecoderConfig(AudioCodec codec,
                                       int bits_per_channel,
                                       ChannelLayout channel_layout,
                                       int samples_per_second,
                                       const uint8* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, bits_per_channel, channel_layout, samples_per_second,
             extra_data, extra_data_size, is_encrypted, true);
}

void AudioDecoderConfig::Initialize(AudioCodec codec,
                                    int bits_per_channel,
                                    ChannelLayout channel_layout,
                                    int samples_per_second,
                                    const uint8* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  if (record_stats) {
    UMA_HISTOGRAM_ENUMERATION("Media.AudioCodec", codec, kAudioCodecMax + 1);
    // Fake enum histogram to get exact integral buckets.  Expect to never see
    // any values over 32 and even that is huge.
    UMA_HISTOGRAM_ENUMERATION("Media.AudioBitsPerChannel", bits_per_channel,
                              40);
    UMA_HISTOGRAM_ENUMERATION("Media.AudioChannelLayout", channel_layout,
                              CHANNEL_LAYOUT_MAX);
    AudioSampleRate asr = media::AsAudioSampleRate(samples_per_second);
    if (asr != kUnexpectedAudioSampleRate) {
      UMA_HISTOGRAM_ENUMERATION("Media.AudioSamplesPerSecond", asr,
                                kUnexpectedAudioSampleRate);
    } else {
      UMA_HISTOGRAM_COUNTS(
          "Media.AudioSamplesPerSecondUnexpected", samples_per_second);
    }
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

  is_encrypted_ = is_encrypted;

  int channels = ChannelLayoutToChannelCount(channel_layout_);
  bytes_per_frame_ = channels * bits_per_channel_ / 8;
}

AudioDecoderConfig::~AudioDecoderConfig() {}

bool AudioDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownAudioCodec &&
      channel_layout_ != CHANNEL_LAYOUT_UNSUPPORTED &&
      bits_per_channel_ > 0 &&
      bits_per_channel_ <= limits::kMaxBitsPerSample &&
      samples_per_second_ > 0 &&
      samples_per_second_ <= limits::kMaxSampleRate;
}

bool AudioDecoderConfig::Matches(const AudioDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (bits_per_channel() == config.bits_per_channel()) &&
          (channel_layout() == config.channel_layout()) &&
          (samples_per_second() == config.samples_per_second()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()));
}

void AudioDecoderConfig::CopyFrom(const AudioDecoderConfig& audio_config) {
  Initialize(audio_config.codec(),
             audio_config.bits_per_channel(),
             audio_config.channel_layout(),
             audio_config.samples_per_second(),
             audio_config.extra_data(),
             audio_config.extra_data_size(),
             audio_config.is_encrypted(),
             false);
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

int AudioDecoderConfig::bytes_per_frame() const {
  return bytes_per_frame_;
}

uint8* AudioDecoderConfig::extra_data() const {
  return extra_data_.get();
}

size_t AudioDecoderConfig::extra_data_size() const {
  return extra_data_size_;
}

bool AudioDecoderConfig::is_encrypted() const {
  return is_encrypted_;
}

}  // namespace media
