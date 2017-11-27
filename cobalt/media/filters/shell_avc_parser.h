// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_FILTERS_SHELL_AVC_PARSER_H_
#define COBALT_MEDIA_FILTERS_SHELL_AVC_PARSER_H_

#include <vector>

#include "cobalt/media/base/media_log.h"
#include "cobalt/media/filters/shell_parser.h"

namespace cobalt {
namespace media {

// Typical size of an annexB prepend will be around 60 bytes. We make more room
// to ensure that only a very few videos will fail to play for lack of room
// in the prepend.
static const int kAnnexBPrependMaxSize = 1024;

// while not an actual parser, provides shared functionality to both the
// mp4 and flv parsers which derive from it. Implements part of ShellParser
// while leaving the rest for its children.
class ShellAVCParser : public ShellParser {
 public:
  explicit ShellAVCParser(scoped_refptr<ShellDataSourceReader> reader,
                          const scoped_refptr<MediaLog>& media_log);
  virtual ~ShellAVCParser();

  struct ShellSPSRecord {
    gfx::Size coded_size;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
    uint32 num_ref_frames;
  };
  static bool ParseSPS(const uint8* sps, size_t sps_size,
                       ShellSPSRecord* record_out);

  // GetNextAU we must pass on to FLV or MP4 children.
  virtual scoped_refptr<ShellAU> GetNextAU(DemuxerStream::Type type) = 0;
  // Prepends are common to all AVC/AAC containers so we can do this one here.
  bool Prepend(scoped_refptr<ShellAU> au,
               scoped_refptr<DecoderBuffer> buffer) override;

 protected:
  virtual bool DownloadAndParseAVCConfigRecord(uint64 offset, uint32 size);
  virtual bool ParseAVCConfigRecord(uint8* buffer, uint32 size);
  // pps_size can be 0. Returns false on unable to construct.
  virtual bool BuildAnnexBPrepend(uint8* sps, uint32 sps_size, uint8* pps,
                                  uint32 pps_size);
  virtual void ParseAudioSpecificConfig(uint8 b0, uint8 b1);
  virtual size_t CalculatePrependSize(DemuxerStream::Type type,
                                      bool is_keyframe);

  scoped_refptr<MediaLog> media_log_;
  uint8 nal_header_size_;
  // audio frames have a fixed-size small prepend that we attach to every
  // audio buffer created by DownloadBuffer()
  std::vector<uint8> audio_prepend_;
  // video frames have a variable-size prepend that we limit to a reasonable
  // upper bound. We only need to attach it to keyframes, however, the rest
  // of the frames need only an AnnexB start code.
  uint8 video_prepend_[kAnnexBPrependMaxSize];
  uint32 video_prepend_size_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILTERS_SHELL_AVC_PARSER_H_
