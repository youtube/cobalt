// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder.h"

namespace media {

Decoder::Decoder() = default;

Decoder::~Decoder() = default;

bool Decoder::IsPlatformDecoder() const {
  return false;
}

bool Decoder::SupportsDecryption() const {
  return false;
}

std::string GetDecoderName(VideoDecoderType type) {
  switch (type) {
    case VideoDecoderType::kUnknown:
      return "Unknown Video Decoder";
    case VideoDecoderType::kFFmpeg:
      return "FFmpegVideoDecoder";
    case VideoDecoderType::kVpx:
      return "VpxVideoDecoder";
    case VideoDecoderType::kAom:
      return "AomVideoDecoder";
    case VideoDecoderType::kMojo:
      return "MojoVideoDecoder";
    case VideoDecoderType::kDecrypting:
      return "DecryptingVideoDecoder";
    case VideoDecoderType::kDav1d:
      return "Dav1dVideoDecoder";
    case VideoDecoderType::kFuchsia:
      return "FuchsiaVideoDecoder";
    case VideoDecoderType::kMediaCodec:
      return "MediaCodecVideoDecoder";
    case VideoDecoderType::kGav1:
      return "Gav1VideoDecoder";
    case VideoDecoderType::kD3D11:
      return "D3D11VideoDecoder";
    case VideoDecoderType::kVaapi:
      return "VaapiVideoDecoder";
    case VideoDecoderType::kBroker:
      return "VideoDecoderBroker";
    case VideoDecoderType::kVda:
      return "VDAVideoDecoder";
    case VideoDecoderType::kV4L2:
      return "V4L2VideoDecoder";
    case VideoDecoderType::kTesting:
      return "Testing or Mock Video decoder";
    default:
      NOTREACHED();
      return "VideoDecoderType created through invalid static_cast";
  }
}

std::string GetDecoderName(AudioDecoderType type) {
  switch (type) {
    case AudioDecoderType::kUnknown:
      return "Unknown Audio Decoder";
    case AudioDecoderType::kFFmpeg:
      return "FFmpegAudioDecoder";
    case AudioDecoderType::kMojo:
      return "MojoAudioDecoder";
    case AudioDecoderType::kDecrypting:
      return "DecryptingAudioDecoder";
    case AudioDecoderType::kMediaCodec:
      return "MediaCodecAudioDecoder";
    case AudioDecoderType::kBroker:
      return "AudioDecoderBroker";
    case AudioDecoderType::kTesting:
      return "Testing or Mock Audio decoder";
    default:
      NOTREACHED();
      return "VideoDecoderType created through invalid static_cast";
  }
}

std::ostream& operator<<(std::ostream& out, AudioDecoderType type) {
  return out << GetDecoderName(type);
}

std::ostream& operator<<(std::ostream& out, VideoDecoderType type) {
  return out << GetDecoderName(type);
}

}  // namespace media
