// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_STREAM_H_
#define MEDIA_BASE_DEMUXER_STREAM_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

struct AVStream;

namespace media {

class Buffer;

class MEDIA_EXPORT DemuxerStream
    : public base::RefCountedThreadSafe<DemuxerStream> {
 public:
  typedef base::Callback<void(Buffer*)> ReadCallback;

  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
    NUM_TYPES,  // Always keep this entry as the last one!
  };

  // Schedules a read.  When the |read_callback| is called, the downstream
  // object takes ownership of the buffer by AddRef()'ing the buffer.
  virtual void Read(const ReadCallback& read_callback) = 0;

  // Returns an |AVStream*| if supported, or NULL.
  virtual AVStream* GetAVStream();

  // Returns the type of stream.
  virtual Type type() = 0;

  virtual void EnableBitstreamConverter() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DemuxerStream>;
  virtual ~DemuxerStream();
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_STREAM_H_
