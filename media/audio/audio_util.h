// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace base {
class SharedMemory;
}

namespace media {
class AudioBus;

// For all audio functions 3 audio formats are supported:
// 8 bits unsigned 0 to 255.
// 16 bit signed (little endian).
// 32 bit signed (little endian)

// AdjustVolume() does a software volume adjustment of a sample buffer.
// The samples are multiplied by the volume, which should range from
// 0.0 (mute) to 1.0 (full volume).
// Using software allows each audio and video to have its own volume without
// affecting the master volume.
// In the future the function may be used to adjust the sample format to
// simplify hardware requirements and to support a wider variety of input
// formats.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
MEDIA_EXPORT bool AdjustVolume(void* buf,
                               size_t buflen,
                               int channels,
                               int bytes_per_sample,
                               float volume);

// MixStreams() mixes 2 audio streams with same sample rate and number of
// samples, adjusting volume on one of them.
// Dst += Src * volume.
MEDIA_EXPORT void MixStreams(void* dst,
                             void* src,
                             size_t buflen,
                             int bytes_per_sample,
                             float volume);

// FoldChannels() does a software multichannel folding down to stereo.
// Channel order is assumed to be 5.1 Dolby standard which is
// front left, front right, center, surround left, surround right.
// The subwoofer is ignored.
// 6.1 adds a rear center speaker, and 7.1 has 2 rear speakers.  These
// channels are rare and ignored.
// After summing the channels, volume is adjusted and the samples are
// clipped to the maximum value.
// Volume should normally range from 0.0 (mute) to 1.0 (full volume), but
// since clamping is performed a value of more than 1 is allowed to increase
// volume.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
MEDIA_EXPORT bool FoldChannels(void* buf,
                               size_t buflen,
                               int channels,
                               int bytes_per_sample,
                               float volume);

// Returns the default audio output hardware sample-rate.
MEDIA_EXPORT int GetAudioHardwareSampleRate();

// Returns the audio input hardware sample-rate for the specified device.
MEDIA_EXPORT int GetAudioInputHardwareSampleRate(
    const std::string& device_id);

// Returns the optimal low-latency buffer size for the audio hardware.
// This is the smallest buffer size the system can comfortably render
// at without glitches.  The buffer size is in sample-frames.
MEDIA_EXPORT size_t GetAudioHardwareBufferSize();

// Returns the channel layout for the specified audio input device.
MEDIA_EXPORT ChannelLayout GetAudioInputHardwareChannelLayout(
    const std::string& device_id);

// Computes a buffer size based on the given |sample_rate|. Must be used in
// conjunction with AUDIO_PCM_LINEAR.
MEDIA_EXPORT size_t GetHighLatencyOutputBufferSize(int sample_rate);

#if defined(OS_WIN)

// Does Windows support WASAPI? We are checking in lot of places, and
// sometimes check was written incorrectly, so move into separate function.
MEDIA_EXPORT bool IsWASAPISupported();

// Returns number of buffers to be used by wave out.
MEDIA_EXPORT int NumberOfWaveOutBuffers();

#endif  // defined(OS_WIN)

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UTIL_H_
