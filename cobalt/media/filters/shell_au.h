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

#ifndef MEDIA_FILTERS_SHELL_AU_H_
#define MEDIA_FILTERS_SHELL_AU_H_

#include "base/memory/ref_counted.h"
#include "media/base/demuxer_stream.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_data_source_reader.h"

namespace media {

class ShellParser;

// AnnexB start code is 4 bytes 0x00000001 big-endian
static const int kAnnexBStartCodeSize = 4;
static const uint32 kAnnexBStartCode = 0x00000001;

// The basic unit of currency between ShellDemuxer and ShellParser, the ShellAU
// defines all needed information for a given AccessUnit (Frame) of encoded
// media data.
class ShellAU : public base::RefCountedThreadSafe<ShellAU> {
 public:
  typedef base::TimeDelta TimeDelta;
  typedef DemuxerStream::Type Type;

  static scoped_refptr<ShellAU> CreateEndOfStreamAU(Type type,
                                                    TimeDelta timestamp);
  static scoped_refptr<ShellAU> CreateAudioAU(uint64 offset,
                                              size_t size,
                                              size_t prepend_size,
                                              bool is_keyframe,
                                              TimeDelta timestamp,
                                              TimeDelta duration,
                                              ShellParser* parser);
  static scoped_refptr<ShellAU> CreateVideoAU(uint64 offset,
                                              size_t size,
                                              size_t prepend_size,
                                              uint8 length_of_nalu_size,
                                              bool is_keyframe,
                                              TimeDelta timestamp,
                                              TimeDelta duration,
                                              ShellParser* parser);

  virtual bool IsEndOfStream() const = 0;
  virtual bool IsValid() const = 0;
  // Read an AU from reader to buffer and also do all the necessary operations
  // like prepending head to make it ready to decode.
  virtual bool Read(ShellDataSourceReader* reader, DecoderBuffer* buffer) = 0;
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
  friend class base::RefCountedThreadSafe<ShellAU>;

  ShellAU();
  virtual ~ShellAU();
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AU_H_
