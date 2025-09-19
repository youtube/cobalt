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

#ifndef MEDIA_STARBOARD_PROGRESSIVE_AVC_PARSER_H_
#define MEDIA_STARBOARD_PROGRESSIVE_AVC_PARSER_H_

#include "base/containers/heap_array.h"
#include "media/base/media_log.h"
#include "media/starboard/progressive/progressive_parser.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Typical size of an annexB prepend will be around 60 bytes. We make more room
// to ensure that only a very few videos will fail to play for lack of room
// in the prepend.
static const int kAnnexBPrependMaxSize = 1024;

// while not an actual parser, provides shared functionality to both the
// mp4 and flv parsers which derive from it. Implements part of
// ProgressiveParser while leaving the rest for its children.
class AVCParser : public ProgressiveParser {
 public:
  explicit AVCParser(scoped_refptr<DataSourceReader> reader,
                     MediaLog* media_log);
  virtual ~AVCParser();

  struct SPSRecord {
    gfx::Size coded_size;
    gfx::Rect visible_rect;
    gfx::Size natural_size;
    uint32_t num_ref_frames;
  };
  static bool ParseSPS(const uint8_t* sps,
                       size_t sps_size,
                       SPSRecord* record_out);

  // GetNextAU we must pass on to FLV or MP4 children.
  scoped_refptr<AvcAccessUnit> GetNextAU(DemuxerStream::Type type) override = 0;
  // Prepends are common to all AVC/AAC containers so we can do this one here.
  bool Prepend(scoped_refptr<AvcAccessUnit> au,
               scoped_refptr<DecoderBuffer> buffer) override;

 protected:
  virtual bool DownloadAndParseAVCConfigRecord(uint64_t offset, uint32_t size);
  virtual bool ParseAVCConfigRecord(uint8_t* buffer, uint32_t size);
  // pps_size can be 0. Returns false on unable to construct.
  virtual bool BuildAnnexBPrepend(uint8_t* sps,
                                  uint32_t sps_size,
                                  uint8_t* pps,
                                  uint32_t pps_size);
  virtual void ParseAudioSpecificConfig(uint8_t b0, uint8_t b1);
  virtual size_t CalculatePrependSize(DemuxerStream::Type type,
                                      bool is_keyframe);

  MediaLog* media_log_;
  uint8_t nal_header_size_;
  // audio frames have a fixed-size small prepend that we attach to every
  // audio buffer created by DownloadBuffer()
  base::HeapArray<uint8_t> audio_prepend_;
  // video frames have a variable-size prepend that we limit to a reasonable
  // upper bound. We only need to attach it to keyframes, however, the rest
  // of the frames need only an AnnexB start code.
  uint8_t video_prepend_[kAnnexBPrependMaxSize];
  uint32_t video_prepend_size_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_PROGRESSIVE_AVC_PARSER_H_
