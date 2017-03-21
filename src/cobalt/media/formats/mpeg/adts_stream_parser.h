// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_
#define COBALT_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_

#include <stdint.h>

#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/formats/mpeg/mpeg_audio_stream_parser_base.h"

namespace media {

class MEDIA_EXPORT ADTSStreamParser : public MPEGAudioStreamParserBase {
 public:
  ADTSStreamParser();
  ~ADTSStreamParser() OVERRIDE;

  // MPEGAudioStreamParserBase overrides.
  int ParseFrameHeader(const uint8_t* data, int size, int* frame_size,
                       int* sample_rate, ChannelLayout* channel_layout,
                       int* sample_count, bool* metadata_frame,
                       std::vector<uint8_t>* extra_data) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ADTSStreamParser);
};

}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_
