// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_
#define COBALT_MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/stream_parser.h"
#include "cobalt/media/formats/common/offset_byte_queue.h"
#include "cobalt/media/formats/mp4/track_run_iterator.h"

namespace media {
namespace mp4 {

struct Movie;
class BoxReader;

class MEDIA_EXPORT MP4StreamParser : public StreamParser {
 public:
  MP4StreamParser(const std::set<int>& audio_object_types, bool has_sbr);
  ~MP4StreamParser() OVERRIDE;

  void Init(const InitCB& init_cb, const NewConfigCB& config_cb,
            const NewBuffersCB& new_buffers_cb, bool ignore_text_tracks,
            const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
            const NewMediaSegmentCB& new_segment_cb,
            const EndMediaSegmentCB& end_of_segment_cb,
            const scoped_refptr<MediaLog>& media_log) OVERRIDE;
  void Flush() OVERRIDE;
  bool Parse(const uint8_t* buf, int size) OVERRIDE;

 private:
  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kWaitingForSampleData,
    kEmittingSamples,
    kError
  };

  bool ParseBox(bool* err);
  bool ParseMoov(mp4::BoxReader* reader);
  bool ParseMoof(mp4::BoxReader* reader);

  void OnEncryptedMediaInitData(
      const std::vector<ProtectionSystemSpecificHeader>& headers);

  // To retain proper framing, each 'mdat' atom must be read; to limit memory
  // usage, the atom's data needs to be discarded incrementally as frames are
  // extracted from the stream. This function discards data from the stream up
  // to |max_clear_offset|, updating the |mdat_tail_| value so that framing can
  // be retained after all 'mdat' information has been read. |max_clear_offset|
  // is the upper bound on what can be removed from |queue_|. Anything below
  // this offset is no longer needed by the parser.
  // Returns 'true' on success, 'false' if there was an error.
  bool ReadAndDiscardMDATsUntil(int64_t max_clear_offset);

  void ChangeState(State new_state);

  bool EmitConfigs();
  bool PrepareAACBuffer(const AAC& aac_config, std::vector<uint8_t>* frame_buf,
                        std::vector<SubsampleEntry>* subsamples) const;
  bool EnqueueSample(BufferQueueMap* buffers, bool* err);
  bool SendAndFlushSamples(BufferQueueMap* buffers);

  void Reset();

  // Checks to see if we have enough data in |queue_| to transition to
  // kEmittingSamples and start enqueuing samples.
  bool HaveEnoughDataToEnqueueSamples();

  // Sets |highest_end_offset_| based on the data in |moov_|
  // and |moof|. Returns true if |highest_end_offset_| was successfully
  // computed.
  bool ComputeHighestEndOffset(const MovieFragment& moof);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  scoped_refptr<MediaLog> media_log_;

  OffsetByteQueue queue_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64_t moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64_t mdat_tail_;

  // The highest end offset in the current moof. This offset is
  // relative to |moof_head_|. This value is used to make sure we have collected
  // enough bytes to parse all samples and aux_info in the current moof.
  int64_t highest_end_offset_;

  scoped_ptr<mp4::Movie> moov_;
  scoped_ptr<mp4::TrackRunIterator> runs_;

  bool has_audio_;
  bool has_video_;
  std::set<uint32_t> audio_track_ids_;
  std::set<uint32_t> video_track_ids_;
  // The object types allowed for audio tracks.
  std::set<int> audio_object_types_;
  bool has_sbr_;
  std::map<uint32_t, bool> is_track_encrypted_;

  // Tracks the number of MEDIA_LOGS for skipping empty trun samples.
  int num_empty_samples_skipped_;

  DISALLOW_COPY_AND_ASSIGN(MP4StreamParser);
};

}  // namespace mp4
}  // namespace media

#endif  // COBALT_MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_
