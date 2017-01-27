// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_
#define MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media2/base/media_log.h"
#include "media2/blink/media_blink_export.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"

namespace media {
class ChunkDemuxer;

class MEDIA_BLINK_EXPORT WebMediaSourceImpl
    : NON_EXPORTED_BASE(public blink::WebMediaSource) {
 public:
  WebMediaSourceImpl(ChunkDemuxer* demuxer,
                     const scoped_refptr<MediaLog>& media_log);
  ~WebMediaSourceImpl() OVERRIDE;

  // blink::WebMediaSource implementation.
  AddStatus addSourceBuffer(const blink::WebString& type,
                            const blink::WebString& codecs,
                            blink::WebSourceBuffer** source_buffer) OVERRIDE;
  double duration() OVERRIDE;
  void setDuration(double duration) OVERRIDE;
  void markEndOfStream(EndOfStreamStatus status) OVERRIDE;
  void unmarkEndOfStream() OVERRIDE;

 private:
  ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.
  scoped_refptr<MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaSourceImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIASOURCE_IMPL_H_
