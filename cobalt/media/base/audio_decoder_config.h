// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
#define COBALT_MEDIA_BASE_AUDIO_DECODER_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "cobalt/media/base/audio_codecs.h"
#include "cobalt/media/base/channel_layout.h"
#include "cobalt/media/base/encryption_scheme.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/sample_format.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// TODO(dalecurtis): FFmpeg API uses |bytes_per_channel| instead of
// |bits_per_channel|, we should switch over since bits are generally confusing
// to work with.
class MEDIA_EXPORT AudioDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  AudioDecoderConfig();

  // Constructs an initialized object.
  AudioDecoderConfig(AudioCodec codec, SampleFormat sample_format,
                     ChannelLayout channel_layout, int samples_per_second,
                     const std::vector<uint8_t>& extra_data,
                     const EncryptionScheme& encryption_scheme);

  ~AudioDecoderConfig();

  // Resets the internal state of this object. |codec_delay| is in frames.
  void Initialize(AudioCodec codec, SampleFormat sample_format,
                  ChannelLayout channel_layout, int samples_per_second,
                  const std::vector<uint8_t>& extra_data,
                  const EncryptionScheme& encryption_scheme,
                  base::TimeDelta seek_preroll, int codec_delay);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const AudioDecoderConfig& config) const;

  // Returns a human-readable string describing |*this|.  For debugging & test
  // output only.
  std::string AsHumanReadableString() const;

  AudioCodec codec() const { return codec_; }
  int bits_per_channel() const { return bytes_per_channel_ * 8; }
  int bytes_per_channel() const { return bytes_per_channel_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int samples_per_second() const { return samples_per_second_; }
  SampleFormat sample_format() const { return sample_format_; }
  int bytes_per_frame() const { return bytes_per_frame_; }
  base::TimeDelta seek_preroll() const { return seek_preroll_; }
  int codec_delay() const { return codec_delay_; }

  // Optional byte data required to initialize audio decoders such as Vorbis
  // codebooks.
  const std::vector<uint8_t>& extra_data() const { return extra_data_; }

  // Whether the audio stream is potentially encrypted.
  // Note that in a potentially encrypted audio stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const { return encryption_scheme_.is_encrypted(); }

  // Encryption scheme used for encrypted buffers.
  const EncryptionScheme& encryption_scheme() const {
    return encryption_scheme_;
  }

  void set_mime(const std::string& mime) { mime_ = mime; }
  const std::string& mime() const { return mime_; }

 private:
  AudioCodec codec_;
  SampleFormat sample_format_;
  int bytes_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;
  int bytes_per_frame_;
  std::vector<uint8_t> extra_data_;
  EncryptionScheme encryption_scheme_;

  // |seek_preroll_| is the duration of the data that the decoder must decode
  // before the decoded data is valid.
  base::TimeDelta seek_preroll_;

  // |codec_delay_| is the number of frames the decoder should discard before
  // returning decoded data.  This value can include both decoder delay as well
  // as padding added during encoding.
  int codec_delay_;

  // |mime_| contains the mime type string specified when creating the stream.
  // For example, when the stream is created via MediaSource, it is the mime
  // string passed to addSourceBuffer().  It can be an empty string when the
  // appropriate mime string is unknown.  It is provided as an extra information
  // and can be inconsistent with the other member variables.
  std::string mime_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_AUDIO_DECODER_CONFIG_H_
