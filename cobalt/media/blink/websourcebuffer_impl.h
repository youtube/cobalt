// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_
#define COBALT_MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "third_party/WebKit/public/platform/WebSourceBuffer.h"

namespace cobalt {
namespace media {
class ChunkDemuxer;
class MediaTracks;

class WebSourceBufferImpl : public blink::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id, ChunkDemuxer* demuxer);
  ~WebSourceBufferImpl() OVERRIDE;

  // blink::WebSourceBuffer implementation.
  void setClient(blink::WebSourceBufferClient* client) OVERRIDE;
  bool setMode(AppendMode mode) OVERRIDE;
  blink::WebTimeRanges buffered() OVERRIDE;
  double highestPresentationTimestamp() OVERRIDE;
  bool evictCodedFrames(double currentPlaybackTime,
                        size_t newDataSize) OVERRIDE;
  bool append(const unsigned char* data, unsigned length,
              double* timestamp_offset) OVERRIDE;
  void resetParserState() OVERRIDE;
  void remove(double start, double end) OVERRIDE;
  bool setTimestampOffset(double offset) OVERRIDE;
  void setAppendWindowStart(double start) OVERRIDE;
  void setAppendWindowEnd(double end) OVERRIDE;
  void removedFromMediaSource() OVERRIDE;

 private:
  // Demuxer callback handler to process an initialization segment received
  // during an append() call.
  void InitSegmentReceived(std::unique_ptr<MediaTracks> tracks);

  std::string id_;
  ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  blink::WebSourceBufferClient* client_;

  // Controls the offset applied to timestamps when processing appended media
  // segments. It is initially 0, which indicates that no offset is being
  // applied. Both setTimestampOffset() and append() may update this value.
  base::TimeDelta timestamp_offset_;

  base::TimeDelta append_window_start_;
  base::TimeDelta append_window_end_;

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_WEBSOURCEBUFFER_IMPL_H_
