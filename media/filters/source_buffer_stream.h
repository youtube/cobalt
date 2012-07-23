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

#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"

namespace media {

class SourceBufferRange;

// See file-level comment for complete description.
class MEDIA_EXPORT SourceBufferStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  explicit SourceBufferStream(const AudioDecoderConfig& audio_config);
  explicit SourceBufferStream(const VideoDecoderConfig& video_config);

  ~SourceBufferStream();

  // Signals that the next buffers appended are part of a new media segment
  // starting at |media_segment_start_time|.
  void OnNewMediaSegment(base::TimeDelta media_segment_start_time);

  // Sets the start time of the stream.
  void SetStartTime(base::TimeDelta stream_start_time);

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

  // Fills |out_buffer| with a new buffer. Buffers are presented in order from
  // the last call to Seek(), or starting with the first buffer appended if
  // Seek() has not been called yet.
  // |out_buffer|'s timestamp may be earlier than the |timestamp| passed to
  // the last Seek() call.
  // Returns true if |out_buffer| is filled with a valid buffer, false if
  // there is not enough data buffered to fulfill the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);

  // Returns a list of the buffered time ranges.
  Ranges<base::TimeDelta> GetBufferedTime() const;

  // Returns true if we don't have any ranges or the last range is selected.
  bool IsEndSelected() const;

  const AudioDecoderConfig& GetCurrentAudioDecoderConfig() {
    return audio_config_;
  }
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig() {
    return video_config_;
  }

  // Returns the largest distance between two adjacent buffers in this stream,
  // or an estimate if no two adjacent buffers have been appended to the stream
  // yet.
  base::TimeDelta GetMaxInterbufferDistance() const;

 private:
  typedef std::list<SourceBufferRange*> RangeList;

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

  // Checks to see if |range_with_new_buffers_itr| can be merged with the range
  // next to it, and merges them if so.
  void MergeWithAdjacentRangeIfNecessary(
      const RangeList::iterator& range_with_new_buffers_itr);

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

  // Returns true if |timestamp| occurs before the start timestamp of the first
  // range in |ranges_|, false otherwise or if |ranges_| is empty.
  bool IsBeforeFirstRange(base::TimeDelta timestamp) const;

  // Returns true if the timestamps of |buffers| are monotonically increasing
  // since the previous append to the media segment, false otherwise.
  bool IsMonotonicallyIncreasing(const BufferQueue& buffers) const;

  // Returns true if |selected_range_| is the only range in |ranges_| that
  // HasNextBufferPosition().
  bool OnlySelectedRangeIsSeeked() const;

  // Measures the distances between buffer timestamps and tracks the max.
  void UpdateMaxInterbufferDistance(const BufferQueue& buffers);

  // List of disjoint buffered ranges, ordered by start time.
  RangeList ranges_;

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  // The starting time of the stream.
  base::TimeDelta stream_start_time_;

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

  DISALLOW_COPY_AND_ASSIGN(SourceBufferStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_STREAM_H_
