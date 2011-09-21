// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

enum AudioCodec {
  kUnknownAudioCodec,
  kCodecAAC,
  kCodecMP3,
  kCodecPCM,
  kCodecVorbis,

  // DO NOT ADD RANDOM AUDIO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.
};

class MEDIA_EXPORT AudioDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  AudioDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  AudioDecoderConfig(AudioCodec codec, int bits_per_channel,
                     ChannelLayout channel_layout, int samples_per_second,
                     const uint8* extra_data, size_t extra_data_size);

  ~AudioDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(AudioCodec codec, int bits_per_channel,
                  ChannelLayout channel_layout, int samples_per_second,
                  const uint8* extra_data, size_t extra_data_size);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  AudioCodec codec() const;
  int bits_per_channel() const;
  ChannelLayout channel_layout() const;
  int samples_per_second() const;

  // Optional byte data required to initialize audio decoders such as Vorbis
  // codebooks.
  uint8* extra_data() const;
  size_t extra_data_size() const;

 private:
  AudioCodec codec_;
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  scoped_array<uint8> extra_data_;
  size_t extra_data_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderConfig);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
