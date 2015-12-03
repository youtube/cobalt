/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_parser.h"

#include "media/filters/shell_flv_parser.h"
#include "media/filters/shell_mp4_parser.h"

namespace media {

// ==== ShellParser ============================================================

// how many bytes to download of the file to determine type?
const int ShellParser::kInitialHeaderSize = 9;

// static
scoped_refptr<ShellParser> ShellParser::Construct(
    scoped_refptr<ShellDataSourceReader> reader,
    const PipelineStatusCB& status_cb) {
  scoped_refptr<ShellParser> parser;

  // download first 16 bytes of stream to determine file type and extract basic
  // container-specific stream configuration information
  uint8 header[kInitialHeaderSize];
  int bytes_read = reader->BlockingRead(0, kInitialHeaderSize, header);
  if (bytes_read != kInitialHeaderSize) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return NULL;
  }

  // attempt to construct mp4 parser from this header
  parser = ShellMP4Parser::Construct(reader, header, status_cb);
  // ok, attempt FLV
  if (!parser) {
    parser = ShellFLVParser::Construct(reader, header, status_cb);
  }
  // no additional supported container formats, set error and return
  if (!parser) {
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return NULL;
  }

  return parser;
}

ShellParser::ShellParser(scoped_refptr<ShellDataSourceReader> reader)
    : reader_(reader), duration_(kInfiniteDuration()), bits_per_second_(0) {}

ShellParser::~ShellParser() {}

bool ShellParser::IsConfigComplete() {
  return (video_config_.IsValidConfig()) && (audio_config_.IsValidConfig()) &&
         (duration_ != kInfiniteDuration());
}

}  // namespace media
