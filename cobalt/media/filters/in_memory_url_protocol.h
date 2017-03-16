// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
#define COBALT_MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cobalt/media/filters/ffmpeg_glue.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Simple FFmpegURLProtocol that reads from a buffer.
// NOTE: This object does not copy the buffer so the
//       buffer pointer passed into the constructor
//       needs to remain valid for the entire lifetime of
//       this object.
class MEDIA_EXPORT InMemoryUrlProtocol : public FFmpegURLProtocol {
 public:
  InMemoryUrlProtocol(const uint8_t* buf, int64_t size, bool streaming);
  virtual ~InMemoryUrlProtocol();

  // FFmpegURLProtocol methods.
  int Read(int size, uint8_t* data) OVERRIDE;
  bool GetPosition(int64_t* position_out) OVERRIDE;
  bool SetPosition(int64_t position) OVERRIDE;
  bool GetSize(int64_t* size_out) OVERRIDE;
  bool IsStreaming() OVERRIDE;

 private:
  const uint8_t* data_;
  int64_t size_;
  int64_t position_;
  bool streaming_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InMemoryUrlProtocol);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILTERS_IN_MEMORY_URL_PROTOCOL_H_
