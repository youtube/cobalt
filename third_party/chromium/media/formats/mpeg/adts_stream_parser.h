// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_

#include <stdint.h>

#include "base/macros.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/formats/mpeg/mpeg_audio_stream_parser_base.h"

namespace media_m96 {

class MEDIA_EXPORT ADTSStreamParser : public MPEGAudioStreamParserBase {
 public:
  ADTSStreamParser();

  ADTSStreamParser(const ADTSStreamParser&) = delete;
  ADTSStreamParser& operator=(const ADTSStreamParser&) = delete;

  ~ADTSStreamParser() override;

  // MPEGAudioStreamParserBase overrides.
  int ParseFrameHeader(const uint8_t* data,
                       int size,
                       int* frame_size,
                       int* sample_rate,
                       ChannelLayout* channel_layout,
                       int* sample_count,
                       bool* metadata_frame,
                       std::vector<uint8_t>* extra_data) override;

 private:
  size_t adts_parse_error_limit_ = 0;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_
