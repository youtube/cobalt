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

#ifndef MEDIA_FILTERS_SHELL_FLV_PARSER_H_
#define MEDIA_FILTERS_SHELL_FLV_PARSER_H_

#include <map>
#include <list>

#include "media/base/shell_buffer_factory.h"
#include "media/filters/shell_avc_parser.h"

namespace media {

class ShellFLVParser : public ShellAVCParser {
 public:
  // Attempts to make sense of the provided bytes of the top of a file as an
  // flv, and if it does make sense returns PIPELINE_OK and |*parser| contains a
  // ShellFLVParser initialized with some basic state.  If it doesn't make sense
  // this returns an error status and |*parser| contains NULL.
  static PipelineStatus Construct(scoped_refptr<ShellDataSourceReader> reader,
                                  const uint8* construction_header,
                                  scoped_refptr<ShellParser>* parser);
  ShellFLVParser(scoped_refptr<ShellDataSourceReader> reader,
                 uint32 tag_start_offset);
  virtual ~ShellFLVParser();

  // === ShellParser Implementation
  virtual bool ParseConfig() override;
  virtual scoped_refptr<ShellAU> GetNextAU(DemuxerStream::Type type) override;
  virtual bool SeekTo(base::TimeDelta timestamp) override;

 protected:
  scoped_refptr<ShellAU> GetNextAudioAU();
  scoped_refptr<ShellAU> GetNextVideoAU();

  // Advance by one tag through the FLV. If encountering a keyframe, update the
  // time-to-byte map. If a regular video or audio data tag, update the next tag
  // structures. Otherwise it will download and parse the tag as it must contain
  // video metadata. Returns false on fatal error. Will call one of the Parse
  // helper methods defined below.
  bool ParseNextTag();
  bool ParseAudioDataTag(uint8* tag, uint32 size, uint32 timestamp);
  bool ParseVideoDataTag(uint8* tag, uint32 size, uint32 timestamp);
  bool ParseScriptDataObjectTag(uint8* tag, uint32 size, uint32 timestamp);

  // SCRIPTDATAOBJECT parsing
  bool ExtractAMF0Number(scoped_refptr<ShellScopedArray> amf0,
                         const char* name,
                         double* number_out);

  // flush internal parsing state and move tag_offset_ to the provided argument.
  void JumpParserTo(uint64 byte_offset);

  // The byte position in the stream of the tag parser. Between calls to
  // ParseNextTag() should point at the start of the next FLV tag in the file.
  uint64 tag_offset_;

  // Stores a map of video keyframe times to byte offsets in the FLV file. At
  // peak keyframe rates of 1 per second of video, and 16 bytes per entry
  // this map will consume approximately 1 MB of memory for 18 hours
  // of video worst-case. We build the data structure while traversing the
  // FLV tag-to-tag. The stream positions point at the start of the FLV tag
  // just like the entry conditions for tag_offset_ in ParseNextTag().
  typedef std::map<uint32, uint64> TimeToByteMap;
  TimeToByteMap time_to_byte_map_;

  // We maintain a record of data tags we have parsed headers for but not
  // downloaded the actual byte contents of.
  typedef std::list<scoped_refptr<ShellAU> > AUList;
  AUList next_video_aus_;
  AUList next_audio_aus_;

  // When true, all subsequent reads to ParseNextTag() will enqueue EOS AUs
  // in both audio and video AU queues.
  bool at_end_of_file_;

  base::TimeDelta audio_track_duration_;
  base::TimeDelta video_track_duration_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellFLVParser);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_FLV_PARSER_H_
