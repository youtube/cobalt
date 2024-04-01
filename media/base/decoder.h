// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_H_
#define MEDIA_BASE_DECODER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media {

// List of known AudioDecoder implementations; recorded to UKM, always add new
// values to the end and do not reorder or delete values from this list.
enum class AudioDecoderType : int {
  kUnknown = 0,     // Decoder name string is not recognized or n/a.
  kFFmpeg = 1,      // FFmpegAudioDecoder
  kMojo = 2,        // MojoAudioDecoder
  kDecrypting = 3,  // DecryptingAudioDecoder
  kMediaCodec = 4,  // MediaCodecAudioDecoder (Android)
  kBroker = 5,      // AudioDecoderBroker
  kTesting = 6,     // Never send this to UKM, for tests only.

  // Keep this at the end and equal to the last entry.
  kMaxValue = kTesting,
};

// List of known VideoDecoder implementations; recorded to UKM, always add new
// values to the end and do not reorder or delete values from this list.
enum class VideoDecoderType : int {
  kUnknown = 0,  // Decoder name string is not recognized or n/a.
  // kGpu = 1,      // GpuVideoDecoder (DEPRECATED)
  kFFmpeg = 2,      // FFmpegVideoDecoder
  kVpx = 3,         // VpxVideoDecoder
  kAom = 4,         // AomVideoDecoder
  kMojo = 5,        // MojoVideoDecoder
  kDecrypting = 6,  // DecryptingVideoDecoder
  kDav1d = 7,       // Dav1dVideoDecoder
  kFuchsia = 8,     // FuchsiaVideoDecoder
  kMediaCodec = 9,  // MediaCodecVideoDecoder (Android)
  kGav1 = 10,       // Gav1VideoDecoder
  kD3D11 = 11,      // D3D11VideoDecoder
  kVaapi = 12,      // VaapiVideoDecoder
  kBroker = 13,     // VideoDecoderBroker (Webcodecs)
  kVda = 14,        // VDAVideoDecoder
  // kChromeOs = 15,  // DEPRECATED, should be kVaapi or kV4L2 instead.
  kV4L2 = 16,       // V4L2VideoDecoder

  kTesting = 17,  // Never send this to UKM, for tests only.

  // Keep this at the end and equal to the last entry.
  kMaxValue = kTesting
};

MEDIA_EXPORT std::string GetDecoderName(AudioDecoderType type);
MEDIA_EXPORT std::string GetDecoderName(VideoDecoderType type);
MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, AudioDecoderType type);
MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, VideoDecoderType type);

class MEDIA_EXPORT Decoder {
 public:
  virtual ~Decoder();

  // Returns true if the implementation is expected to be implemented by the
  // platform. The value should be available immediately after construction and
  // should not change within the lifetime of a decoder instance.
  virtual bool IsPlatformDecoder() const;

  // Returns true if the implementation supports decoding configs with
  // encryption.
  // TODO(crbug.com/1099488): Sometimes it's not possible to give a definitive
  // yes or no answer unless more context is given. While this doesn't pose any
  // problems, it does allow incompatible decoders to pass the filtering step in
  // |DecoderSelector| potentially slowing down the selection process.
  virtual bool SupportsDecryption() const;

 protected:
  Decoder();
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_H_
