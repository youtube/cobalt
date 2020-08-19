// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/progressive/progressive_parser.h"

#include "base/logging.h"
#include "cobalt/media/base/timestamp_constants.h"
#include "cobalt/media/progressive/mp4_parser.h"

namespace cobalt {
namespace media {

// ==== ProgressiveParser
// ============================================================

// how many bytes to download of the file to determine type?
const int ProgressiveParser::kInitialHeaderSize = 9;

// static
PipelineStatus ProgressiveParser::Construct(
    scoped_refptr<DataSourceReader> reader,
    scoped_refptr<ProgressiveParser>* parser,
    const scoped_refptr<MediaLog>& media_log) {
  DCHECK(parser);
  DCHECK(media_log);
  *parser = NULL;

  // download first 16 bytes of stream to determine file type and extract basic
  // container-specific stream configuration information
  uint8 header[kInitialHeaderSize];
  int bytes_read = reader->BlockingRead(0, kInitialHeaderSize, header);
  if (bytes_read != kInitialHeaderSize) {
    return DEMUXER_ERROR_COULD_NOT_PARSE;
  }

  // attempt to construct mp4 parser from this header
  return MP4Parser::Construct(reader, header, parser, media_log);
}

ProgressiveParser::ProgressiveParser(scoped_refptr<DataSourceReader> reader)
    : reader_(reader), duration_(kInfiniteDuration), bits_per_second_(0) {}

ProgressiveParser::~ProgressiveParser() {}

bool ProgressiveParser::IsConfigComplete() {
  return video_config_.IsValidConfig() && audio_config_.IsValidConfig() &&
         duration_ != kInfiniteDuration;
}

}  // namespace media
}  // namespace cobalt
