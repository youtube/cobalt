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

#ifndef COBALT_MEDIA_PROGRESSIVE_PARSER_H_
#define COBALT_MEDIA_PROGRESSIVE_PARSER_H_
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/video_decoder_config.h"
#include "cobalt/media/progressive/avc_access_unit.h"
#include "cobalt/media/progressive/data_source_reader.h"

namespace cobalt {
namespace media {

// abstract base class to define a stream parser interface used by
// ProgressiveDemuxer.
class ProgressiveParser : public base::RefCountedThreadSafe<ProgressiveParser> {
 public:
  static const int kInitialHeaderSize;
  // Determine stream type, construct appropriate parser object, and returns
  // PIPELINE_OK on success or error code.
  static PipelineStatus Construct(scoped_refptr<DataSourceReader> reader,
                                  scoped_refptr<ProgressiveParser>* parser,
                                  const scoped_refptr<MediaLog>& media_log);
  explicit ProgressiveParser(scoped_refptr<DataSourceReader> reader);

  // Seek through the file looking for audio and video configuration info,
  // saving as much config state as is possible. Should try to be fast but this
  // may result in the downloading of MB of data. Returns false on fatal error.
  virtual bool ParseConfig() = 0;

  // Returns a populated, valid AU indicating the needed information for
  // downloading and decoding the next access unit in the stream, or NULL on
  // fatal error. On success this advances the respective audio or video cursor
  // to the next AU.
  virtual scoped_refptr<AvcAccessUnit> GetNextAU(DemuxerStream::Type type) = 0;
  // Write the appropriate prepend header for the supplied au into the supplied
  // buffer. Return false on error.
  virtual bool Prepend(scoped_refptr<AvcAccessUnit> au,
                       scoped_refptr<DecoderBuffer> buffer) = 0;
  // Advance internal state to provided timestamp. Return false on error.
  virtual bool SeekTo(base::TimeDelta timestamp) = 0;

  // ======= config state methods, values should be set by ParseConfig()
  // Returns true if all of the required variables defined below are valid.
  // BitsPerSecond() is optional.
  virtual bool IsConfigComplete();
  // time-duration of file, may return kInfiniteDuration() if unknown
  virtual base::TimeDelta Duration() { return duration_; }
  // bits per second of media, if known, otherwise 0
  virtual uint32 BitsPerSecond() { return bits_per_second_; }
  virtual const AudioDecoderConfig& AudioConfig() { return audio_config_; }
  virtual const VideoDecoderConfig& VideoConfig() { return video_config_; }

 protected:
  // only allow RefCountedThreadSafe to delete us
  friend class base::RefCountedThreadSafe<ProgressiveParser>;
  virtual ~ProgressiveParser();
  scoped_refptr<DataSourceReader> reader_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  base::TimeDelta duration_;
  uint32 bits_per_second_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_PARSER_H_
