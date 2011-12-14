// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_STREAM_H_
#define MEDIA_BASE_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class Buffer;
class VideoDecoderConfig;

class MEDIA_EXPORT DemuxerStream
    : public base::RefCountedThreadSafe<DemuxerStream> {
 public:
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
    NUM_TYPES,  // Always keep this entry as the last one!
  };

  // Request a buffer to returned via the provided callback.
  //
  // Buffers will be non-NULL yet may be end of stream buffers.
  typedef base::Callback<void(scoped_refptr<Buffer>)> ReadCallback;
  virtual void Read(const ReadCallback& read_callback) = 0;

  // Returns the audio decoder configuration. It is an error to call this method
  // if type() != AUDIO.
  virtual const AudioDecoderConfig& audio_decoder_config() = 0;

  // Returns the video decoder configuration. It is an error to call this method
  // if type() != VIDEO.
  virtual const VideoDecoderConfig& video_decoder_config() = 0;

  // Returns the type of stream.
  virtual Type type() = 0;

  virtual void EnableBitstreamConverter() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DemuxerStream>;
  virtual ~DemuxerStream();
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_STREAM_H_
