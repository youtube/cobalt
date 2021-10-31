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

#ifndef COBALT_MEDIA_PROGRESSIVE_ACCESS_UNIT_H_
#define COBALT_MEDIA_PROGRESSIVE_ACCESS_UNIT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/progressive/data_source_reader.h"

namespace cobalt {
namespace media {

class ProgressiveParser;

static const int kAnnexBStartCodeSize = 4;
static const uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};

// The basic unit of currency between ProgressiveDemuxer and ProgressiveParser,
// the AvcAccessUnit defines all needed information for a given AvcAccessUnit
// (Frame) of encoded media data.
class AvcAccessUnit : public base::RefCountedThreadSafe<AvcAccessUnit> {
 public:
  typedef base::TimeDelta TimeDelta;
  typedef DemuxerStream::Type Type;

  static scoped_refptr<AvcAccessUnit> CreateEndOfStreamAU(Type type,
                                                          TimeDelta timestamp);
  static scoped_refptr<AvcAccessUnit> CreateAudioAU(
      uint64 offset, size_t size, size_t prepend_size, bool is_keyframe,
      TimeDelta timestamp, TimeDelta duration, ProgressiveParser* parser);
  static scoped_refptr<AvcAccessUnit> CreateVideoAU(
      uint64 offset, size_t size, size_t prepend_size,
      uint8 length_of_nalu_size, bool is_keyframe, TimeDelta timestamp,
      TimeDelta duration, ProgressiveParser* parser);

  virtual bool IsEndOfStream() const = 0;
  virtual bool IsValid() const = 0;
  // Read an AU from reader to buffer and also do all the necessary operations
  // like prepending head to make it ready to decode.
  virtual bool Read(DataSourceReader* reader, DecoderBuffer* buffer) = 0;
  virtual Type GetType() const = 0;
  virtual bool IsKeyframe() const = 0;
  virtual bool AddPrepend() const = 0;
  // Get the size of this AU, it is always no larger than its max size.
  virtual size_t GetSize() const = 0;
  // Get the max required buffer of this AU
  virtual size_t GetMaxSize() const = 0;
  virtual TimeDelta GetTimestamp() const = 0;
  virtual TimeDelta GetDuration() const = 0;
  virtual void SetDuration(TimeDelta duration) = 0;
  virtual void SetTimestamp(TimeDelta timestamp) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AvcAccessUnit>;

  AvcAccessUnit();
  virtual ~AvcAccessUnit();
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_ACCESS_UNIT_H_
