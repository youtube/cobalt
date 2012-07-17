// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DECODER_H_
#define MEDIA_BASE_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/channel_layout.h"
#include "media/base/pipeline_status.h"
#include "media/base/media_export.h"

namespace media {

class Buffer;
class DemuxerStream;

class MEDIA_EXPORT AudioDecoder
    : public base::RefCountedThreadSafe<AudioDecoder> {
 public:
  // Status codes for read operations.
  enum Status {
    kOk,
    kAborted,
    kDecodeError,
  };

  // Initialize an AudioDecoder with the given DemuxerStream, executing the
  // callback upon completion.
  // statistics_cb is used to update global pipeline statistics.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) = 0;

  // Request samples to be decoded and returned via the provided callback.
  // Only one read may be in flight at any given time.
  //
  // Implementations guarantee that the callback will not be called from within
  // this method.
  //
  // Non-NULL sample buffer pointers will contain decoded audio data or may
  // indicate the end of the stream. A NULL buffer pointer indicates an aborted
  // Read(). This can happen if the DemuxerStream gets flushed and doesn't have
  // any more data to return.
  typedef base::Callback<void(Status, const scoped_refptr<Buffer>&)> ReadCB;
  virtual void Read(const ReadCB& read_cb) = 0;

  // Reset decoder state, dropping any queued encoded data.
  virtual void Reset(const base::Closure& closure) = 0;

  // Returns various information about the decoded audio format.
  virtual int bits_per_channel() = 0;
  virtual ChannelLayout channel_layout() = 0;
  virtual int samples_per_second() = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioDecoder>;
  virtual ~AudioDecoder();
  AudioDecoder();

  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DECODER_H_
