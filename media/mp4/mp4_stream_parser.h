// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_MP4_STREAM_PARSER_H_
#define MEDIA_MP4_MP4_STREAM_PARSER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/mp4/offset_byte_queue.h"
#include "media/mp4/track_run_iterator.h"

namespace media {
namespace mp4 {

struct Movie;
class BoxReader;

class MEDIA_EXPORT MP4StreamParser : public StreamParser {
 public:
  MP4StreamParser();
  virtual ~MP4StreamParser();

  virtual void Init(const InitCB& init_cb, const NewConfigCB& config_cb,
                    const NewBuffersCB& audio_cb,
                    const NewBuffersCB& video_cb,
                    const NeedKeyCB& need_key_cb,
                    const NewMediaSegmentCB& new_segment_cb) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool Parse(const uint8* buf, int size) OVERRIDE;

 private:
  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kEmittingSamples,
    kError
  };

  bool ParseBox(bool* err);
  bool ParseMoov(mp4::BoxReader* reader);
  bool ParseMoof(mp4::BoxReader* reader);

  bool ReadMDATsUntil(const int64 tgt_tail);

  void ChangeState(State new_state);

  bool EmitConfigs();
  bool EnqueueSample(BufferQueue* audio_buffers,
                     BufferQueue* video_buffers,
                     bool* err);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB audio_cb_;
  NewBuffersCB video_cb_;
  NeedKeyCB need_key_cb_;
  NewMediaSegmentCB new_segment_cb_;

  OffsetByteQueue queue_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64 moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64 mdat_tail_;

  mp4::TrackRunIterator runs_;

  scoped_ptr<mp4::Movie> moov_;

  bool has_audio_;
  bool has_video_;
  uint32 audio_track_id_;
  uint32 video_track_id_;
  bool parameter_sets_inserted_;

  // We keep this around to avoid having to go digging through the moov with
  // every frame.
  uint8 size_of_nalu_length_;

  DISALLOW_COPY_AND_ASSIGN(MP4StreamParser);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_MP4_STREAM_PARSER_H_
