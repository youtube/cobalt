// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_FILTERS_FRAME_PROCESSOR_H_
#define COBALT_MEDIA_FILTERS_FRAME_PROCESSOR_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/stream_parser.h"
#include "cobalt/media/filters/chunk_demuxer.h"

namespace media {

class MseTrackBuffer;

// Helper class that implements Media Source Extension's coded frame processing
// algorithm.
class MEDIA_EXPORT FrameProcessor {
 public:
  typedef base::Callback<void(base::TimeDelta)> UpdateDurationCB;

  FrameProcessor(const UpdateDurationCB& update_duration_cb,
                 const scoped_refptr<MediaLog>& media_log);
  ~FrameProcessor();

  // Get/set the current append mode, which if true means "sequence" and if
  // false means "segments".
  // See http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode.
  bool sequence_mode() { return sequence_mode_; }
  void SetSequenceMode(bool sequence_mode);

  // Processes buffers in |buffer_queue_map|.
  // Returns true on success or false on failure which indicates decode error.
  // |append_window_start| and |append_window_end| correspond to the MSE spec's
  // similarly named source buffer attributes that are used in coded frame
  // processing.
  // Uses |*timestamp_offset| according to the coded frame processing algorithm,
  // including updating it as required in 'sequence' mode frame processing.
  bool ProcessFrames(const StreamParser::BufferQueueMap& buffer_queue_map,
                     base::TimeDelta append_window_start,
                     base::TimeDelta append_window_end,
                     base::TimeDelta* timestamp_offset);

  // Signals the frame processor to update its group start timestamp to be
  // |timestamp_offset| if it is in sequence append mode.
  void SetGroupStartTimestampIfInSequenceMode(base::TimeDelta timestamp_offset);

  // Adds a new track with unique track ID |id|.
  // If |id| has previously been added, returns false to indicate error.
  // Otherwise, returns true, indicating future ProcessFrames() will emit
  // frames for the track |id| to |stream|.
  bool AddTrack(StreamParser::TrackId id, ChunkDemuxerStream* stream);

  // A map that describes how track ids changed between init segment. Maps the
  // old track id for a new track id for the same track.
  typedef std::map<StreamParser::TrackId, StreamParser::TrackId> TrackIdChanges;

  // Updates the internal mapping of TrackIds to track buffers. The input
  // parameter |track_id_changes| maps old track ids to new ones. The track ids
  // not present in the map must be assumed unchanged. Returns false if
  // remapping failed.
  bool UpdateTrackIds(const TrackIdChanges& track_id_changes);

  // Sets the need random access point flag on all track buffers to true.
  void SetAllTrackBuffersNeedRandomAccessPoint();

  // Resets state for the coded frame processing algorithm as described in steps
  // 2-5 of the MSE Reset Parser State algorithm described at
  // http://www.w3.org/TR/media-source/#sourcebuffer-reset-parser-state
  void Reset();

  // Must be called when the audio config is updated.  Used to manage when
  // the preroll buffer is cleared and the allowed "fudge" factor between
  // preroll buffers.
  void OnPossibleAudioConfigUpdate(const AudioDecoderConfig& config);

 private:
  friend class FrameProcessorTest;

  // If |track_buffers_| contains |id|, returns a pointer to the associated
  // MseTrackBuffer. Otherwise, returns NULL.
  MseTrackBuffer* FindTrack(StreamParser::TrackId id);

  // Signals all track buffers' streams that a coded frame group is starting
  // with decode timestamp |start_timestamp|.
  void NotifyStartOfCodedFrameGroup(DecodeTimestamp start_timestamp);

  // Helper that signals each track buffer to append any processed, but not yet
  // appended, frames to its stream. Returns true on success, or false if one or
  // more of the appends failed.
  bool FlushProcessedFrames();

  // Handles partial append window trimming of |buffer|.  Returns true if the
  // given |buffer| can be partially trimmed or have preroll added; otherwise,
  // returns false.
  //
  // If |buffer| overlaps |append_window_start|, the portion of |buffer| before
  // |append_window_start| will be marked for post-decode discard.  Further, if
  // |audio_preroll_buffer_| exists and abuts |buffer|, it will be set as
  // preroll on |buffer| and |audio_preroll_buffer_| will be cleared.  If the
  // preroll buffer does not abut |buffer|, it will be discarded unused.
  //
  // Likewise, if |buffer| overlaps |append_window_end|, the portion of |buffer|
  // after |append_window_end| will be marked for post-decode discard.
  //
  // If |buffer| lies entirely before |append_window_start|, and thus would
  // normally be discarded, |audio_preroll_buffer_| will be set to |buffer| and
  // the method will return false.
  bool HandlePartialAppendWindowTrimming(
      base::TimeDelta append_window_start, base::TimeDelta append_window_end,
      const scoped_refptr<StreamParserBuffer>& buffer);

  // Helper that processes one frame with the coded frame processing algorithm.
  // Returns false on error or true on success.
  bool ProcessFrame(const scoped_refptr<StreamParserBuffer>& frame,
                    base::TimeDelta append_window_start,
                    base::TimeDelta append_window_end,
                    base::TimeDelta* timestamp_offset);

  // TrackId-indexed map of each track's stream.
  typedef std::map<StreamParser::TrackId, scoped_refptr<MseTrackBuffer> >
      TrackBuffersMap;
  TrackBuffersMap track_buffers_;

  // The last audio buffer seen by the frame processor that was removed because
  // it was entirely before the start of the append window.
  scoped_refptr<StreamParserBuffer> audio_preroll_buffer_;

  // The AudioDecoderConfig associated with buffers handed to ProcessFrames().
  AudioDecoderConfig current_audio_config_;
  base::TimeDelta sample_duration_;

  // The AppendMode of the associated SourceBuffer.
  // See SetSequenceMode() for interpretation of |sequence_mode_|.
  // Per http://www.w3.org/TR/media-source/#widl-SourceBuffer-mode:
  // Controls how a sequence of media segments are handled. This is initially
  // set to false ("segments").
  bool sequence_mode_;

  // Tracks whether or not the next processed frame is a continuation of a coded
  // frame group (see https://w3c.github.io/media-source/#coded-frame-group).
  // Resets to kNoDecodeTimestamp() upon detection of 'segments' mode
  // discontinuity, parser reset during 'segments' mode, or switching from
  // 'sequence' to 'segments' mode.
  // Once a processed coded frame is emitted for the current coded frame group,
  // tracks the decode timestamp of the last frame emitted.
  // Explicit setting of timestampOffset will trigger subsequent notification of
  // a new coded frame start to the tracks' streams, even in 'sequence' mode, if
  // the resulting frame has a DTS less than this.
  DecodeTimestamp coded_frame_group_last_dts_;

  // Tracks the MSE coded frame processing variable of same name.
  // Initially kNoTimestamp, meaning "unset".
  base::TimeDelta group_start_timestamp_;

  // Tracks the MSE coded frame processing variable of same name. It stores the
  // highest coded frame end timestamp across all coded frames in the current
  // coded frame group. It is set to 0 when the SourceBuffer object is created
  // and gets updated by ProcessFrames().
  base::TimeDelta group_end_timestamp_;

  UpdateDurationCB update_duration_cb_;

  // MediaLog for reporting messages and properties to debug content and engine.
  scoped_refptr<MediaLog> media_log_;

  // Counters that limit spam to |media_log_| for frame processor warnings.
  int num_dropped_preroll_warnings_;
  int num_dts_beyond_pts_warnings_;

  DISALLOW_COPY_AND_ASSIGN(FrameProcessor);
};

}  // namespace media

#endif  // COBALT_MEDIA_FILTERS_FRAME_PROCESSOR_H_
