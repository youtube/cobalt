// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SourceBufferStream is a data structure that stores media Buffers in ranges.
// Buffers can be appended out of presentation order. Buffers are retrieved by
// seeking to the desired start point and calling GetNextBuffer(). Buffers are
// returned in sequential presentation order.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_

#include <deque>
#include <list>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"

namespace media {

class SourceBufferRange;

// See file-level comment for complete description.
class MEDIA_EXPORT SourceBufferStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  // Status returned by GetNextBuffer().
  // kSuccess: Indicates that the next buffer was returned.
  // kNeedBuffer: Indicates that we need more data before a buffer can be
  //              returned.
  // kConfigChange: Indicates that the next buffer requires a config change.
  enum Status {
    kSuccess,
    kNeedBuffer,
    kConfigChange,
  };

  SourceBufferStream(const AudioDecoderConfig& audio_config,
                     const LogCB& log_cb);
  SourceBufferStream(const VideoDecoderConfig& video_config,
                     const LogCB& log_cb);

  ~SourceBufferStream();

  // Signals that the next buffers appended are part of a new media segment
  // starting at |media_segment_start_time|.
  void OnNewMediaSegment(base::TimeDelta media_segment_start_time);

  // Add the |buffers| to the SourceBufferStream. Buffers within the queue are
  // expected to be in order, but multiple calls to Append() may add buffers out
  // of order or overlapping. Assumes all buffers within |buffers| are in
  // presentation order and are non-overlapping.
  // Returns true if Append() was successful, false if |buffers| are not added.
  // TODO(vrk): Implement garbage collection. (crbug.com/125070)
  bool Append(const BufferQueue& buffers);

  // Changes the SourceBufferStream's state so that it will start returning
  // buffers starting from the closest keyframe before |timestamp|.
  void Seek(base::TimeDelta timestamp);

  // Returns true if the SourceBufferStream has seeked to a time without
  // buffered data and is waiting for more data to be appended.
  bool IsSeekPending() const;

  // Notifies the SourceBufferStream that the media duration has been changed to
  // |duration| so it should drop any data past that point.
  void OnSetDuration(base::TimeDelta duration);

  // Fills |out_buffer| with a new buffer. Buffers are presented in order from
  // the last call to Seek(), or starting with the first buffer appended if
  // Seek() has not been called yet.
  // |out_buffer|'s timestamp may be earlier than the |timestamp| passed to
  // the last Seek() call.
  // Returns kSuccess if |out_buffer| is filled with a valid buffer, kNeedBuffer
  // if there is not enough data buffered to fulfill the request, and
  // kConfigChange if the next buffer requires a config change.
  Status GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);

  // Returns a list of the buffered time ranges.
  Ranges<base::TimeDelta> GetBufferedTime() const;

  // Returns true if we don't have any ranges or the last range is selected.
  bool IsEndSelected() const;

  const AudioDecoderConfig& GetCurrentAudioDecoderConfig();
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig();

  // Notifies this object that the audio config has changed and buffers in
  // future Append() calls should be associated with this new config.
  bool UpdateAudioConfig(const AudioDecoderConfig& config);

  // Notifies this object that the video config has changed and buffers in
  // future Append() calls should be associated with this new config.
  bool UpdateVideoConfig(const VideoDecoderConfig& config);

  // Returns the largest distance between two adjacent buffers in this stream,
  // or an estimate if no two adjacent buffers have been appended to the stream
  // yet.
  base::TimeDelta GetMaxInterbufferDistance() const;

 private:
  friend class SourceBufferStreamTest;
  typedef std::list<SourceBufferRange*> RangeList;

  void set_memory_limit(int memory_limit) { memory_limit_ = memory_limit; }

  // Frees up space if the SourceBufferStream is taking up too much memory.
  void GarbageCollectIfNeeded();

  // Attempts to delete approximately |total_bytes_to_free| amount of data
  // |ranges_|, starting at the front of |ranges_| and moving linearly forward
  // through the buffers. Deletes starting from the back if |reverse_direction|
  // is true. Returns the number of bytes freed.
  int FreeBuffers(int total_bytes_to_free, bool reverse_direction);

  // Appends |new_buffers| into |range_for_new_buffers_itr|, handling start and
  // end overlaps if necessary.
  // |deleted_next_buffer| is an output parameter that is true if the next
  // buffer that would have been returned from GetNextBuffer() was deleted
  // during this call.
  // |deleted_buffers| is an output parameter containing candidates for
  // |track_buffer_|.
  void InsertIntoExistingRange(
      const RangeList::iterator& range_for_new_buffers_itr,
      const BufferQueue& new_buffers,
      bool* deleted_next_buffer, BufferQueue* deleted_buffers);

  // Resolve overlapping ranges such that no ranges overlap anymore.
  // |range_with_new_buffers_itr| points to the range that has newly appended
  // buffers.
  // |deleted_next_buffer| is an output parameter that is true if the next
  // buffer that would have been returned from GetNextBuffer() was deleted
  // during this call.
  // |deleted_buffers| is an output parameter containing candidates for
  // |track_buffer_|.
  void ResolveCompleteOverlaps(
      const RangeList::iterator& range_with_new_buffers_itr,
      bool* deleted_next_buffer, BufferQueue* deleted_buffers);
  void ResolveEndOverlap(
      const RangeList::iterator& range_with_new_buffers_itr,
      bool* deleted_next_buffer, BufferQueue* deleted_buffers);

  // This method is a bit tricky to describe. When what would have been the
  // next buffer returned from |selected_range_| is overlapped by new data,
  // the |selected_range_| seeks forward to the next keyframe after (or at) the
  // next buffer timestamp and the overlapped buffers are deleted. But for
  // smooth playback between the old data to the new data's keyframe, some of
  // these |deleted_buffers| may be temporarily saved into |track_buffer_|.
  // UpdateTrackBuffer() takes these |deleted_buffers| and decides whether it
  // wants to save any buffers into |track_buffer_|.
  // TODO(vrk): This is a little crazy! Ideas for cleanup in crbug.com/129623.
  void UpdateTrackBuffer(const BufferQueue& deleted_buffers);

  // Removes buffers that come before |selected_range_|'s next buffer from the
  // |track_buffer_|.
  void PruneTrackBuffer();

  // Checks to see if |range_with_new_buffers_itr| can be merged with the range
  // next to it, and merges them if so.
  void MergeWithAdjacentRangeIfNecessary(
      const RangeList::iterator& range_with_new_buffers_itr);

  // Deletes the buffers between |start_timestamp|, |end_timestamp| from
  // |range|. Deletes between [start,end] if |is_range_exclusive| is true, or
  // (start,end) if |is_range_exclusive| is false.
  // Buffers are deleted in GOPs, so this method may delete buffers past
  // |end_timestamp| if the keyframe a buffer depends on was deleted.
  // Returns true if the |next_buffer_index_| is reset, and places the buffers
  // removed from the range starting at |next_buffer_index_| in
  // |deleted_buffers|.
  bool DeleteBetween(SourceBufferRange* range,
                     base::TimeDelta start_timestamp,
                     base::TimeDelta end_timestamp,
                     bool is_range_exclusive,
                     BufferQueue* deleted_buffers);

  // Returns true if |second_timestamp| is the timestamp of the next buffer in
  // sequence after |first_timestamp|, false otherwise.
  bool AreAdjacentInSequence(
      base::TimeDelta first_timestamp, base::TimeDelta second_timestamp) const;

  // Helper method that returns the timestamp for the next buffer that
  // |selected_range_| will return from GetNextBuffer() call, or kNoTimestamp()
  // if in between seeking (i.e. |selected_range_| is null).
  base::TimeDelta GetNextBufferTimestamp();

  // Returns the timestamp of the last buffer in the |selected_range_| or
  // kNoTimestamp() if |selected_range_| is null.
  base::TimeDelta GetEndBufferTimestamp();

  // Finds the range that should contain a media segment that begins with
  // |start_timestamp| and returns the iterator pointing to it. Returns
  // |ranges_.end()| if there's no such existing range.
  RangeList::iterator FindExistingRangeFor(base::TimeDelta start_timestamp);

  // Inserts |new_range| into |ranges_| preserving sorted order. Returns an
  // iterator in |ranges_| that points to |new_range|.
  RangeList::iterator AddToRanges(SourceBufferRange* new_range);

  // Returns an iterator that points to the place in |ranges_| where
  // |selected_range_| lives.
  RangeList::iterator GetSelectedRangeItr();

  // Sets the |selected_range_| to |range| and resets the next buffer position
  // for the previous |selected_range_|.
  void SetSelectedRange(SourceBufferRange* range);

  // Resets this stream back to an unseeked state.
  void ResetSeekState();

  // Returns true if |seek_timestamp| refers to the beginning of the first range
  // in |ranges_|, false otherwise or if |ranges_| is empty.
  bool ShouldSeekToStartOfBuffered(base::TimeDelta seek_timestamp) const;

  // Returns true if the timestamps of |buffers| are monotonically increasing
  // since the previous append to the media segment, false otherwise.
  bool IsMonotonicallyIncreasing(const BufferQueue& buffers) const;

  // Returns true if |selected_range_| is the only range in |ranges_| that
  // HasNextBufferPosition().
  bool OnlySelectedRangeIsSeeked() const;

  // Measures the distances between buffer timestamps and tracks the max.
  void UpdateMaxInterbufferDistance(const BufferQueue& buffers);

  // Sets the config ID for each buffer to |append_config_index_|.
  void SetConfigIds(const BufferQueue& buffers);

  // Called to complete a config change. Updates |current_config_index_| to
  // match the index of the next buffer. Calling this method causes
  // GetNextBuffer() to stop returning kConfigChange and start returning
  // kSuccess.
  void CompleteConfigChange();

  // Callback used to report error strings that can help the web developer
  // figure out what is wrong with the content.
  LogCB log_cb_;

  // List of disjoint buffered ranges, ordered by start time.
  RangeList ranges_;

  // Indicates which decoder config is being used by the decoder.
  // GetNextBuffer() is only allows to return buffers that have a
  // config ID that matches this index. If there is a mismatch then
  // it must signal that a config change is needed.
  int current_config_index_;

  // Indicates which decoder config to associate with new buffers
  // being appended. Each new buffer appended has its config ID set
  // to the value of this field.
  int append_config_index_;

  // Holds the audio/video configs for this stream. |current_config_index_|
  // and |append_config_index_| represent indexes into one of these vectors.
  std::vector<AudioDecoderConfig*> audio_configs_;
  std::vector<VideoDecoderConfig*> video_configs_;

  // True if more data needs to be appended before the Seek() can complete,
  // false if no Seek() has been requested or the Seek() is completed.
  bool seek_pending_;

  // Timestamp of the last request to Seek().
  base::TimeDelta seek_buffer_timestamp_;

  // Pointer to the seeked-to Range. This is the range from which
  // GetNextBuffer() calls are fulfilled after the |track_buffer_| has been
  // emptied.
  SourceBufferRange* selected_range_;

  // Queue of the next buffers to be returned from calls to GetNextBuffer(). If
  // |track_buffer_| is empty, return buffers from |selected_range_|.
  BufferQueue track_buffer_;

  // The start time of the current media segment being appended.
  base::TimeDelta media_segment_start_time_;

  // Points to the range containing the current media segment being appended.
  RangeList::iterator range_for_next_append_;

  // True when the next call to Append() begins a new media segment.
  bool new_media_segment_;

  // The timestamp of the last buffer appended to the media segment, set to
  // kNoTimestamp() if the beginning of the segment.
  base::TimeDelta last_buffer_timestamp_;

  // Stores the largest distance between two adjacent buffers in this stream.
  base::TimeDelta max_interbuffer_distance_;

  // The maximum amount of data in bytes the stream will keep in memory.
  int memory_limit_;

  // Indicates that a kConfigChanged status has been reported by GetNextBuffer()
  // and GetCurrentXXXDecoderConfig() must be called to update the current
  // config. GetNextBuffer() must not be called again until
  // GetCurrentXXXDecoderConfig() has been called.
  bool config_change_pending_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
