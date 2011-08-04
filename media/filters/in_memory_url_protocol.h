// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
#define MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_

#include "base/basictypes.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Simple FFmpegURLProtocol that reads from a buffer.
// NOTE: This object does not copy the buffer so the
//       buffer pointer passed into the constructor
//       needs to remain valid for the entire lifetime of
//       this object.
class MEDIA_EXPORT InMemoryUrlProtocol : public FFmpegURLProtocol {
 public:
  InMemoryUrlProtocol(const uint8* buf, int64 size, bool streaming);
  virtual ~InMemoryUrlProtocol();

  // FFmpegURLProtocol methods.
  virtual int Read(int size, uint8* data);
  virtual bool GetPosition(int64* position_out);
  virtual bool SetPosition(int64 position);
  virtual bool GetSize(int64* size_out);
  virtual bool IsStreaming();

 private:
  const uint8* data_;
  int64 size_;
  int64 position_;
  bool streaming_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InMemoryUrlProtocol);
};

}  // namespace media

#endif  // MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
