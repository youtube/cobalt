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

#ifndef MEDIA_FILTERS_SHELL_PARSER_H_
#define MEDIA_FILTERS_SHELL_PARSER_H_

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/shell_buffer_factory.h"
#include "media/base/shell_data_source_reader.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/shell_au.h"

namespace media {

// abstract base class to define a stream parser interface used by ShellDemuxer.
class ShellParser : public base::RefCountedThreadSafe<ShellParser> {
 public:
  static const int kInitialHeaderSize;
  // Determine stream type, construct appropriate parser object, and return.
  static scoped_refptr<ShellParser> Construct(
      scoped_refptr<ShellDataSourceReader> reader,
      const PipelineStatusCB& status_cb);
  explicit ShellParser(scoped_refptr<ShellDataSourceReader> reader);

  // Seek through the file looking for audio and video configuration info,
  // saving as much config state as is possible. Should try to be fast but this
  // may result in the downloading of MB of data. Returns false on fatal error.
  virtual bool ParseConfig() = 0;

  // Returns a populated, valid AU indicating the needed information for
  // downloding and decoding the next access unit in the stream, or NULL on
  // fatal error. On success this advances the respective audio or video cursor
  // to the next AU.
  virtual scoped_refptr<ShellAU> GetNextAU(DemuxerStream::Type type) = 0;
  // Write the appropriate prepend header for the supplied au into the supplied
  // buffer. Return false on error.
  virtual bool Prepend(scoped_refptr<ShellAU> au,
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
  friend class base::RefCountedThreadSafe<ShellParser>;
  virtual ~ShellParser();
  scoped_refptr<ShellDataSourceReader> reader_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  base::TimeDelta duration_;
  uint32 bits_per_second_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_PARSER_H_
