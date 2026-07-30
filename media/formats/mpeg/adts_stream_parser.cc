// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_stream_parser.h"

#include <stddef.h>

#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/aac.h"
#include "media/formats/mpeg/adts_constants.h"
#include "media/formats/mpeg/lib.rs.h"

namespace media {

// static
std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseHeader(
    base::span<const uint8_t> data) {
  auto rust_data = rust::Slice<const uint8_t>(data);
  auto ffi_res = media::formats::mpeg::parse_adts_header(rust_data);
  if (ffi_res.frame_size == 0) {
    return std::nullopt;
  }
  Header header;
  header.frame_size = ffi_res.frame_size;
  header.sample_rate = ffi_res.sample_rate;
  ChannelLayout layout;
  switch (ffi_res.channels) {
    case 1:
      layout = CHANNEL_LAYOUT_MONO;
      break;
    case 2:
      layout = CHANNEL_LAYOUT_STEREO;
      break;
    case 3:
      layout = CHANNEL_LAYOUT_SURROUND;
      break;
    case 4:
      layout = CHANNEL_LAYOUT_4_0;
      break;
    case 5:
      layout = CHANNEL_LAYOUT_5_0_BACK;
      break;
    case 6:
      layout = CHANNEL_LAYOUT_5_1_BACK;
      break;
    case 8:
      layout = CHANNEL_LAYOUT_7_1;
      break;
    default:
      layout = CHANNEL_LAYOUT_NONE;
      break;
  }
  header.channel_layout = layout;
  header.sample_count = ffi_res.sample_count;
  header.extra_data.push_back(ffi_res.esds >> 8);
  header.extra_data.push_back(ffi_res.esds & 0xFF);
  return header;
}

constexpr uint32_t kADTSStartCodeMask = 0xfff00000;

ADTSStreamParser::ADTSStreamParser()
    : MPEGAudioStreamParserBase(kADTSStartCodeMask, AudioCodec::kAAC, 0) {}

ADTSStreamParser::~ADTSStreamParser() = default;

size_t ADTSStreamParser::GetMinHeaderSize() const {
  return kADTSHeaderMinSize;
}

std::optional<ADTSStreamParser::Header> ADTSStreamParser::ParseFrameHeader(
    base::span<const uint8_t> data) {
  auto header = ParseHeader(data);
  if (!header) {
    LIMITED_MEDIA_LOG(DEBUG, media_log(), adts_parse_error_limit_, 5)
        << "Invalid ADTS header.";
  }
  return header;
}

}  // namespace media
