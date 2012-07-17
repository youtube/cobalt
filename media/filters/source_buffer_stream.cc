// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/logging.h"

namespace media {

// Helper class representing a range of buffered data. All buffers in a
// SourceBufferRange are ordered sequentially in presentation order with no
// gaps.
class SourceBufferRange {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  // Returns the maximum distance in time between any buffer seen in this
  // stream. Used to estimate the duration of a buffer if its duration is not
  // known.
  typedef base::Callback<base::TimeDelta()> InterbufferDistanceCB;

  // Creates a source buffer range with |new_buffers|. |new_buffers| cannot be
  // empty and the front of |new_buffers| must be a keyframe.
  // |media_segment_start_time| refers to the starting timestamp for the media
  // segment to which these buffers belong.
  SourceBufferRange(const BufferQueue& new_buffers,
                    base::TimeDelta media_segment_start_time,
                    const InterbufferDistanceCB& interbuffer_distance_cb);

  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes. Assumes |buffers| belongs at the end of the
  // range.
  void AppendBuffersToEnd(const BufferQueue& buffers);
  bool CanAppendBuffersToEnd(const BufferQueue& buffers) const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_index_|
  // is transfered to this SourceBufferRange.
  void AppendRangeToEnd(const SourceBufferRange& range,
                        bool transfer_current_position);
  bool CanAppendRangeToEnd(const SourceBufferRange& range) const;

  // Updates |next_buffer_index_| to point to the Buffer containing |timestamp|.
  // Assumes |timestamp| is valid and in this range.
  void Seek(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe after or equal to
  // |timestamp|.
  void SeekAheadTo(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe strictly after
  // |timestamp|.
  void SeekAheadPast(base::TimeDelta timestamp);

  // Seeks to the beginning of the range.
  void SeekToStart();

  // Finds the next keyframe from |buffers_| after |timestamp|, and creates and
  // returns a new SourceBufferRange with the buffers from that keyframe onward.
  // The buffers in the new SourceBufferRange are moved out of this range. If
  // there is no keyframe after |timestamp|, SplitRange() returns null and this
  // range is unmodified.
  SourceBufferRange* SplitRange(base::TimeDelta timestamp);

  // Deletes the buffers from this range whose timestamps are greater than or
  // equal to |buffer|'s timestamp.
  // Resets |next_buffer_index_| if the buffer at |next_buffer_index_| was
  // deleted, and deletes the |keyframe_map_| entries for the buffers that
  // were removed.
  // |deleted_buffers| contains the buffers that were deleted from this range,
  // starting at the buffer that had been at |next_buffer_index_|.
  // Returns true if the |next_buffer_index_| is reset. Note that this method
  // may return true even if it does not add any buffers to |deleted_buffers|.
  // This indicates that the range had not buffered |next_buffer_index_|, but
  // a buffer at that position would have been deleted.
  bool TruncateAt(scoped_refptr<StreamParserBuffer> buffer,
                  BufferQueue* deleted_buffers);
  // Deletes all buffers in range.
  bool DeleteAll(BufferQueue* deleted_buffers);

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  bool HasNextBuffer() const;

  // Returns true if the range knows the position of the next buffer it should
  // return, i.e. it has been Seek()ed. This does not necessarily mean that it
  // has the next buffer yet.
  bool HasNextBufferPosition() const;

  // Resets this range to an "unseeked" state.
  void ResetNextBufferPosition();

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(), or kNoTimestamp() if the timestamp is unknown.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // Returns the timestamp of the last buffer in the range.
  base::TimeDelta GetEndTimestamp() const;

  // Returns the timestamp for the end of the buffered region in this range.
  // This is an approximation if the duration for the last buffer in the range
  // is unset.
  base::TimeDelta GetBufferedEndTimestamp() const;

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  bool BelongsToRange(base::TimeDelta timestamp) const;

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Returns true if this range's buffered timespan completely overlaps the
  // buffered timespan of |range|.
  bool CompletelyOverlaps(const SourceBufferRange& range) const;

  // Returns true if the end of this range contains buffers that overlaps with
  // the beginning of |range|.
  bool EndOverlaps(const SourceBufferRange& range) const;

  // Returns true if |timestamp| is the timestamp of the next buffer in
  // sequence after |buffer|, false otherwise.
  bool IsNextInSequence(
      const scoped_refptr<media::StreamParserBuffer>& buffer,
      base::TimeDelta timestamp) const;

 private:
  typedef std::map<base::TimeDelta, size_t> KeyframeMap;

  // Seeks the range to the next keyframe after |timestamp|. If
  // |skip_given_timestamp| is true, the seek will go to a keyframe with a
  // timestamp strictly greater than |timestamp|.
  void SeekAhead(base::TimeDelta timestamp, bool skip_given_timestamp);

  // Returns an iterator in |keyframe_map_| pointing to the next keyframe after
  // |timestamp|. If |skip_given_timestamp| is true, this returns the first
  // keyframe with a timestamp strictly greater than |timestamp|.
  KeyframeMap::iterator GetFirstKeyframeAt(
      base::TimeDelta timestamp, bool skip_given_timestamp);

  // Helper method to delete buffers in |buffers_| starting at
  // |starting_point|, an iterator in |buffers_|.
  bool TruncateAt(const BufferQueue::iterator& starting_point,
                  BufferQueue* deleted_buffers);

  // Returns the distance in time estimating how far from the beginning or end
  // of this range a buffer can be to considered in the range.
  base::TimeDelta GetFudgeRoom() const;

  // Returns the approximate duration of a buffer in this range.
  base::TimeDelta GetApproximateDuration() const;

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  KeyframeMap keyframe_map_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetNextBuffer(), set to -1 before Seek().
  int next_buffer_index_;

  // True if the range needs to wait for the next keyframe to be appended before
  // returning buffers from GetNextBuffer().
  bool waiting_for_keyframe_;

  // If |waiting_for_keyframe_| is true, this range will wait for the next
  // keyframe with timestamp >= |next_keyframe_timestamp_|.
  base::TimeDelta next_keyframe_timestamp_;

  // If the first buffer in this range is the beginning of a media segment,
  // |media_segment_start_time_| is the time when the media segment begins.
  // |media_segment_start_time_| may be <= the timestamp of the first buffer in
  // |buffers_|. |media_segment_start_time_| is kNoTimestamp() if this range
  // does not start at the beginning of a media segment, which can only happen
  // garbage collection or after an end overlap that results in a split range
  // (we don't have a way of knowing the media segment timestamp for the new
  // range).
  base::TimeDelta media_segment_start_time_;

  // Called to get the largest interbuffer distance seen so far in the stream.
  InterbufferDistanceCB interbuffer_distance_cb_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferRange);
};

}  // namespace media

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
static bool IsRangeListSorted(
    const std::list<media::SourceBufferRange*>& ranges) {
  base::TimeDelta prev = media::kNoTimestamp();
  for (std::list<media::SourceBufferRange*>::const_iterator itr =
       ranges.begin(); itr != ranges.end(); ++itr) {
    if (prev != media::kNoTimestamp() && prev >= (*itr)->GetStartTimestamp())
      return false;
    prev = (*itr)->GetEndTimestamp();
  }
  return true;
}

// Comparison function for two Buffers based on timestamp.
static bool BufferComparator(
    const scoped_refptr<media::StreamParserBuffer>& first,
    const scoped_refptr<media::StreamParserBuffer>& second) {
  return first->GetDecodeTimestamp() < second->GetDecodeTimestamp();
}

// An arbitrarily-chosen number to estimate the duration of a buffer if none
// is set and there's not enough information to get a better estimate.
static int kDefaultBufferDurationInMs = 125;

namespace media {

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config)
    : stream_start_time_(kNoTimestamp()),
      seek_pending_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()) {
  audio_config_.CopyFrom(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config)
    : stream_start_time_(kNoTimestamp()),
      seek_pending_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      media_segment_start_time_(kNoTimestamp()),
      range_for_next_append_(ranges_.end()),
      new_media_segment_(false),
      last_buffer_timestamp_(kNoTimestamp()),
      max_interbuffer_distance_(kNoTimestamp()) {
  video_config_.CopyFrom(video_config);
}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

void SourceBufferStream::OnNewMediaSegment(
    base::TimeDelta media_segment_start_time) {
  media_segment_start_time_ = media_segment_start_time;

  // Find the range that will house the buffers appended through the next
  // Append() call.
  range_for_next_append_ = FindExistingRangeFor(media_segment_start_time);
  new_media_segment_ = true;
  last_buffer_timestamp_ = kNoTimestamp();
}

void SourceBufferStream::SetStartTime(base::TimeDelta stream_start_time) {
  DCHECK(stream_start_time_ == kNoTimestamp());
  DCHECK(stream_start_time != kNoTimestamp());
  stream_start_time_ = stream_start_time;

  DCHECK(ranges_.empty() ||
         ranges_.front()->GetStartTimestamp() >= stream_start_time_);
}

bool SourceBufferStream::Append(
    const SourceBufferStream::BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  DCHECK(media_segment_start_time_ != kNoTimestamp());

  // New media segments must begin with a keyframe.
  if (new_media_segment_ && !buffers.front()->IsKeyframe()) {
    DVLOG(1) << "Media segment did not begin with keyframe.";
    return false;
  }

  // Buffers within a media segment should be monotonically increasing.
  if (!IsMonotonicallyIncreasing(buffers)) {
    DVLOG(1) << "Buffers were not monotonically increasing.";
    return false;
  }

  if (stream_start_time_ != kNoTimestamp() &&
      media_segment_start_time_ < stream_start_time_) {
    DVLOG(1) << "Cannot append a media segment before the start of stream.";
    return false;
  }

  UpdateMaxInterbufferDistance(buffers);

  // Save a snapshot of stream state before range modifications are made.
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  base::TimeDelta end_buffer_timestamp = GetEndBufferTimestamp();

  bool deleted_next_buffer = false;
  BufferQueue deleted_buffers;

  RangeList::iterator range_for_new_buffers = range_for_next_append_;
  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_new_buffers != ranges_.end()) {
    InsertIntoExistingRange(range_for_new_buffers, buffers,
                            &deleted_next_buffer, &deleted_buffers);
  } else {
    DCHECK(new_media_segment_);
    range_for_new_buffers =
        AddToRanges(new SourceBufferRange(
            buffers, media_segment_start_time_,
            base::Bind(&SourceBufferStream::GetMaxInterbufferDistance,
                       base::Unretained(this))));
  }

  range_for_next_append_ = range_for_new_buffers;
  new_media_segment_ = false;
  last_buffer_timestamp_ = buffers.back()->GetDecodeTimestamp();

  // Resolve overlaps.
  ResolveCompleteOverlaps(
      range_for_new_buffers, &deleted_next_buffer, &deleted_buffers);
  ResolveEndOverlap(
      range_for_new_buffers, &deleted_next_buffer, &deleted_buffers);
  MergeWithAdjacentRangeIfNecessary(range_for_new_buffers);

  // Seek to try to fulfill a previous call to Seek().
  if (seek_pending_) {
    DCHECK(!selected_range_);
    DCHECK(!deleted_next_buffer);
    Seek(seek_buffer_timestamp_);
  }

  // Seek because the Append() has deleted the buffer that would have been
  // returned in the next call to GetNextBuffer().
  if (deleted_next_buffer) {
    DCHECK(!seek_pending_);
    SetSelectedRange(*range_for_new_buffers);
    if (!deleted_buffers.empty()) {
      // Seek the range to the keyframe at or after |next_buffer_timestamp|.
      selected_range_->SeekAheadTo(next_buffer_timestamp);
      UpdateTrackBuffer(deleted_buffers);
    } else {
      // If we've deleted the next buffer but |deleted_buffers| is empty,
      // that means we need to seek past the timestamp of the last buffer in
      // the range (i.e. the keyframe timestamp needs to be strictly greater
      // than |end_buffer_timestamp|).
      selected_range_->SeekAheadPast(end_buffer_timestamp);
    }
  }

  DCHECK(IsRangeListSorted(ranges_));
  DCHECK(OnlySelectedRangeIsSeeked());
  return true;
}

bool SourceBufferStream::IsBeforeFirstRange(base::TimeDelta timestamp) const {
  if (ranges_.empty())
    return false;
  return timestamp < ranges_.front()->GetStartTimestamp();
}

bool SourceBufferStream::IsMonotonicallyIncreasing(
    const BufferQueue& buffers) const {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_buffer_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp() && current_timestamp < prev_timestamp)
      return false;

    prev_timestamp = current_timestamp;
  }
  return true;
}

bool SourceBufferStream::OnlySelectedRangeIsSeeked() const {
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    if ((*itr)->HasNextBufferPosition() && (*itr) != selected_range_)
      return false;
  }
  return !selected_range_ || selected_range_->HasNextBufferPosition();
}

void SourceBufferStream::UpdateMaxInterbufferDistance(
    const BufferQueue& buffers) {
  DCHECK(!buffers.empty());
  base::TimeDelta prev_timestamp = last_buffer_timestamp_;
  for (BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    base::TimeDelta current_timestamp = (*itr)->GetDecodeTimestamp();
    DCHECK(current_timestamp != kNoTimestamp());

    if (prev_timestamp != kNoTimestamp()) {
      base::TimeDelta interbuffer_distance = current_timestamp - prev_timestamp;
      if (max_interbuffer_distance_ == kNoTimestamp()) {
        max_interbuffer_distance_ = interbuffer_distance;
      } else {
        max_interbuffer_distance_ =
            std::max(max_interbuffer_distance_, interbuffer_distance);
      }
    }
    prev_timestamp = current_timestamp;
  }
}

void SourceBufferStream::InsertIntoExistingRange(
    const RangeList::iterator& range_for_new_buffers_itr,
    const BufferQueue& new_buffers,
    bool* deleted_next_buffer, BufferQueue* deleted_buffers) {
  DCHECK(deleted_next_buffer);
  DCHECK(deleted_buffers);

  SourceBufferRange* range_for_new_buffers = *range_for_new_buffers_itr;

  // If this is a simple case where we can just append to the end of the range,
  // do so and return.
  if (range_for_new_buffers->CanAppendBuffersToEnd(new_buffers)) {
    range_for_new_buffers->AppendBuffersToEnd(new_buffers);
    return;
  }

  // Otherwise, this is either a start overlap or an middle overlap.

  // In case this is a middle overlap, save the buffers that come after the end
  // of |new_buffers|, and add them into a new range.
  SourceBufferRange* new_next_range =
      range_for_new_buffers->SplitRange(
          new_buffers.back()->GetDecodeTimestamp());

  if (new_next_range)
    AddToRanges(new_next_range);

  // Delete the buffers that are overlapped by |new_buffers|, then append
  // |new_buffers| to the end of the range.
  BufferQueue saved_buffers;
  bool deleted_next_buffer_from_range =
      range_for_new_buffers->TruncateAt(new_buffers.front(), &saved_buffers);
  range_for_new_buffers->AppendBuffersToEnd(new_buffers);

  if (selected_range_ != range_for_new_buffers)
    return;

  DCHECK(!*deleted_next_buffer);
  *deleted_buffers = saved_buffers;
  *deleted_next_buffer = deleted_next_buffer_from_range;

  // If this was a middle overlap resulting in a new range, and the next buffer
  // position has been transferred to the newly created range, update the
  // |selected_range_| accordingly.
  if (new_next_range && new_next_range->HasNextBufferPosition()) {
    DCHECK(!range_for_new_buffers->HasNextBufferPosition());
    DCHECK(!*deleted_next_buffer);
    SetSelectedRange(new_next_range);
  }
}

void SourceBufferStream::ResolveCompleteOverlaps(
    const RangeList::iterator& range_with_new_buffers_itr,
    bool* deleted_next_buffer, BufferQueue* deleted_buffers) {
  DCHECK(deleted_next_buffer);
  DCHECK(deleted_buffers);

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  while (next_range_itr != ranges_.end() &&
         range_with_new_buffers->CompletelyOverlaps(**next_range_itr)) {
    if (*next_range_itr == selected_range_) {
      DCHECK(!*deleted_next_buffer);
      *deleted_next_buffer = selected_range_->DeleteAll(deleted_buffers);
      DCHECK(*deleted_next_buffer);
      SetSelectedRange(NULL);
    }
    delete *next_range_itr;
    next_range_itr = ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::ResolveEndOverlap(
    const RangeList::iterator& range_with_new_buffers_itr,
    bool* deleted_next_buffer, BufferQueue* deleted_buffers) {
  DCHECK(deleted_next_buffer);
  DCHECK(deleted_buffers);

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr == ranges_.end() ||
      !range_with_new_buffers->EndOverlaps(**next_range_itr)) {
    return;
  }

  // Split the overlapped range after |range_with_new_buffers|'s last buffer
  // overlaps. Now |overlapped_range| contains only the buffers that do not
  // belong in |ranges_| anymore, and |new_next_range| contains buffers that
  // go after |range_with_new_buffers| (without overlap).
  scoped_ptr<SourceBufferRange> overlapped_range(*next_range_itr);
  next_range_itr = ranges_.erase(next_range_itr);

  SourceBufferRange* new_next_range =
      overlapped_range->SplitRange(range_with_new_buffers->GetEndTimestamp());

  // If there were non-overlapped buffers, add the new range to |ranges_|.
  if (new_next_range)
    AddToRanges(new_next_range);

  // If we didn't overlap a selected range, return.
  if (selected_range_ != overlapped_range.get())
    return;

  // If the |overlapped_range| transfers its next buffer position to
  // |new_next_range|, make |new_next_range| the |selected_range_|.
  if (new_next_range && new_next_range->HasNextBufferPosition()) {
    DCHECK(!overlapped_range->HasNextBufferPosition());
    SetSelectedRange(new_next_range);
    return;
  }

  // Save the buffers in |overlapped_range|.
  DCHECK(!*deleted_next_buffer);
  DCHECK_EQ(overlapped_range.get(), selected_range_);
  *deleted_next_buffer = overlapped_range->DeleteAll(deleted_buffers);
  DCHECK(*deleted_next_buffer);

  // |overlapped_range| will be deleted, so set |selected_range_| to NULL.
  SetSelectedRange(NULL);
}

void SourceBufferStream::UpdateTrackBuffer(const BufferQueue& deleted_buffers) {
  DCHECK(!deleted_buffers.empty());
  if (!track_buffer_.empty())
    return;

  DCHECK(selected_range_);
  DCHECK(selected_range_->HasNextBufferPosition());

  base::TimeDelta next_keyframe_timestamp = selected_range_->GetNextTimestamp();

  // If there is no gap between what was deleted and what was added, nothing
  // should be added to the track buffer.
  if (selected_range_->HasNextBuffer() &&
      (next_keyframe_timestamp ==
       deleted_buffers.front()->GetDecodeTimestamp())) {
    return;
  }

  DCHECK(next_keyframe_timestamp >=
         deleted_buffers.front()->GetDecodeTimestamp());

  // If the |selected_range_| is ready to return data, fill the track buffer
  // with all buffers that come before |next_keyframe_timestamp| and return.
  if (selected_range_->HasNextBuffer()) {
    for (BufferQueue::const_iterator itr = deleted_buffers.begin();
         itr != deleted_buffers.end() &&
         (*itr)->GetDecodeTimestamp() < next_keyframe_timestamp; ++itr) {
      track_buffer_.push_back(*itr);
    }
    return;
  }

  // Otherwise, the |selected_range_| is not ready to return data, so add all
  // the deleted buffers into the |track_buffer_|.
  track_buffer_ = deleted_buffers;

  // See if the next range contains the keyframe after the end of the
  // |track_buffer_|, and if so, change |selected_range_|.
  RangeList::iterator next_range_itr = ++(GetSelectedRangeItr());
  if (next_range_itr != ranges_.end()) {
    (*next_range_itr)->SeekAheadPast(
        track_buffer_.back()->GetDecodeTimestamp());
    if ((*next_range_itr)->HasNextBuffer() &&
        selected_range_->IsNextInSequence(
            track_buffer_.back(), (*next_range_itr)->GetNextTimestamp())) {
      SetSelectedRange(*next_range_itr);
    }
  }
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  ++next_range_itr;

  if (next_range_itr != ranges_.end() &&
      range_with_new_buffers->CanAppendRangeToEnd(**next_range_itr)) {
    bool transfer_current_position = selected_range_ == *next_range_itr;
    range_with_new_buffers->AppendRangeToEnd(**next_range_itr,
                                             transfer_current_position);
    // Update |selected_range_| pointer if |range| has become selected after
    // merges.
    if (transfer_current_position)
      SetSelectedRange(range_with_new_buffers);

    delete *next_range_itr;
    ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  DCHECK(stream_start_time_ != kNoTimestamp());
  DCHECK(timestamp >= stream_start_time_);
  SetSelectedRange(NULL);
  track_buffer_.clear();

  if (IsBeforeFirstRange(timestamp)) {
    SetSelectedRange(ranges_.front());
    ranges_.front()->SeekToStart();
    seek_pending_ = false;
    return;
  }

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->CanSeekTo(timestamp))
      break;
  }

  if (itr == ranges_.end())
    return;

  SetSelectedRange(*itr);
  selected_range_->Seek(timestamp);
  seek_pending_ = false;
}

bool SourceBufferStream::IsSeekPending() const {
  return seek_pending_;
}

bool SourceBufferStream::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (!track_buffer_.empty()) {
    *out_buffer = track_buffer_.front();
    track_buffer_.pop_front();
    return true;
  }

  return selected_range_ && selected_range_->GetNextBuffer(out_buffer);
}

base::TimeDelta SourceBufferStream::GetNextBufferTimestamp() {
  if (!selected_range_)
    return kNoTimestamp();

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

base::TimeDelta SourceBufferStream::GetEndBufferTimestamp() {
  if (!selected_range_)
    return kNoTimestamp();
  return selected_range_->GetEndTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(base::TimeDelta start_timestamp) {
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::AddToRanges(SourceBufferRange* new_range) {
  base::TimeDelta start_timestamp = new_range->GetStartTimestamp();
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, new_range);
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::GetSelectedRangeItr() {
  DCHECK(selected_range_);
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); ++itr) {
    if (*itr == selected_range_)
      break;
  }
  DCHECK(itr != ranges_.end());
  return itr;
}

void SourceBufferStream::SetSelectedRange(SourceBufferRange* range) {
  if (selected_range_)
    selected_range_->ResetNextBufferPosition();
  selected_range_ = range;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); ++itr) {
    ranges.Add((*itr)->GetStartTimestamp(), (*itr)->GetBufferedEndTimestamp());
  }
  return ranges;
}

bool SourceBufferStream::IsEndSelected() const {
  return ranges_.empty() || selected_range_ == ranges_.back();
}

base::TimeDelta SourceBufferStream::GetMaxInterbufferDistance() const {
  if (max_interbuffer_distance_ == kNoTimestamp())
    return base::TimeDelta::FromMilliseconds(kDefaultBufferDurationInMs);
  return max_interbuffer_distance_;
}

SourceBufferRange::SourceBufferRange(
    const BufferQueue& new_buffers, base::TimeDelta media_segment_start_time,
    const InterbufferDistanceCB& interbuffer_distance_cb)
    : next_buffer_index_(-1),
      waiting_for_keyframe_(false),
      next_keyframe_timestamp_(kNoTimestamp()),
      media_segment_start_time_(media_segment_start_time),
      interbuffer_distance_cb_(interbuffer_distance_cb) {
  DCHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->IsKeyframe());
  DCHECK(!interbuffer_distance_cb.is_null());
  AppendBuffersToEnd(new_buffers);
}

void SourceBufferRange::AppendBuffersToEnd(const BufferQueue& new_buffers) {
  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end(); ++itr) {
    DCHECK((*itr)->GetDecodeTimestamp() != kNoTimestamp());
    buffers_.push_back(*itr);
    if ((*itr)->IsKeyframe()) {
      keyframe_map_.insert(
          std::make_pair((*itr)->GetDecodeTimestamp(), buffers_.size() - 1));

      if (waiting_for_keyframe_ &&
          (*itr)->GetDecodeTimestamp() >= next_keyframe_timestamp_) {
        next_buffer_index_ = buffers_.size() - 1;
        next_keyframe_timestamp_ = base::TimeDelta();
        waiting_for_keyframe_ = false;
      }
    }
  }
}

void SourceBufferRange::Seek(base::TimeDelta timestamp) {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

  next_keyframe_timestamp_ = base::TimeDelta();
  waiting_for_keyframe_ = false;

  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);
  // lower_bound() returns the first element >= |timestamp|, so we want the
  // previous element if it did not return the element exactly equal to
  // |timestamp|.
  if (result != keyframe_map_.begin() &&
      (result == keyframe_map_.end() || result->first != timestamp)) {
    result--;
  }
  next_buffer_index_ = result->second;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekAheadTo(base::TimeDelta timestamp) {
  SeekAhead(timestamp, false);
}

void SourceBufferRange::SeekAheadPast(base::TimeDelta timestamp) {
  SeekAhead(timestamp, true);
}

void SourceBufferRange::SeekAhead(base::TimeDelta timestamp,
                                  bool skip_given_timestamp) {
  DCHECK(!keyframe_map_.empty());

  KeyframeMap::iterator result =
      GetFirstKeyframeAt(timestamp, skip_given_timestamp);

  // If there isn't a keyframe after |timestamp|, then seek to end and return
  // kNoTimestamp to signal such.
  if (result == keyframe_map_.end()) {
    waiting_for_keyframe_ = true;
    next_buffer_index_ = -1;
    next_keyframe_timestamp_ = timestamp;
    return;
  }
  next_buffer_index_ = result->second;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekToStart() {
  DCHECK(!buffers_.empty());
  next_buffer_index_ = 0;
}

SourceBufferRange* SourceBufferRange::SplitRange(base::TimeDelta timestamp) {
  // Find the first keyframe after |timestamp|, not including |timestamp|.
  KeyframeMap::iterator new_beginning_keyframe =
      GetFirstKeyframeAt(timestamp, true);

  // If there is no keyframe after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end())
    return NULL;

  // Remove the data beginning at |keyframe_index| from |buffers_| and save it
  // into |removed_buffers|.
  int keyframe_index = new_beginning_keyframe->second;
  DCHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));
  BufferQueue::iterator starting_point = buffers_.begin() + keyframe_index;
  BufferQueue removed_buffers(starting_point, buffers_.end());
  keyframe_map_.erase(new_beginning_keyframe, keyframe_map_.end());
  buffers_.erase(starting_point, buffers_.end());

  // Create a new range with |removed_buffers|.
  SourceBufferRange* split_range =
      new SourceBufferRange(
          removed_buffers, kNoTimestamp(), interbuffer_distance_cb_);

  // If the next buffer position is now in |split_range|, update the state of
  // this range and |split_range| accordingly.
  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    DCHECK(!waiting_for_keyframe_);
    split_range->next_buffer_index_ = next_buffer_index_ - keyframe_index;
    ResetNextBufferPosition();
  } else if (waiting_for_keyframe_) {
    split_range->waiting_for_keyframe_ = true;
    split_range->next_keyframe_timestamp_ = next_keyframe_timestamp_;
    ResetNextBufferPosition();
  }

  return split_range;
}

SourceBufferRange::KeyframeMap::iterator
SourceBufferRange::GetFirstKeyframeAt(base::TimeDelta timestamp,
                                         bool skip_given_timestamp) {
  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);
  // lower_bound() returns the first element >= |timestamp|, so if we don't want
  // to include keyframes == |timestamp|, we have to increment the iterator
  // accordingly.
  if (skip_given_timestamp &&
      result != keyframe_map_.end() && result->first == timestamp) {
    ++result;
  }
  return result;
}

bool SourceBufferRange::DeleteAll(BufferQueue* removed_buffers) {
  return TruncateAt(buffers_.begin(), removed_buffers);
}

bool SourceBufferRange::TruncateAt(
    scoped_refptr<StreamParserBuffer> buffer, BufferQueue* removed_buffers) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::iterator starting_point =
      std::lower_bound(buffers_.begin(), buffers_.end(),
                       buffer,
                       BufferComparator);
  return TruncateAt(starting_point, removed_buffers);
}

bool SourceBufferRange::TruncateAt(
    const BufferQueue::iterator& starting_point, BufferQueue* removed_buffers) {
  DCHECK(removed_buffers);
  DCHECK(removed_buffers->empty());

  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return false;

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  bool removed_next_buffer = false;
  if (HasNextBufferPosition()) {
    base::TimeDelta next_buffer_timestamp = GetNextTimestamp();
    if (next_buffer_timestamp == kNoTimestamp() ||
        next_buffer_timestamp >= (*starting_point)->GetDecodeTimestamp()) {
      if (HasNextBuffer()) {
        int starting_offset = starting_point - buffers_.begin();
        int next_buffer_offset = next_buffer_index_ - starting_offset;
        DCHECK_GE(next_buffer_offset, 0);
        BufferQueue saved(starting_point + next_buffer_offset, buffers_.end());
        removed_buffers->swap(saved);
      }
      ResetNextBufferPosition();
      removed_next_buffer = true;
    }
  }

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetDecodeTimestamp());
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  buffers_.erase(starting_point, buffers_.end());
  return removed_next_buffer;
}

bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (waiting_for_keyframe_ ||
      next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    return false;
  }

  DCHECK_GE(next_buffer_index_, 0);
  *out_buffer = buffers_.at(next_buffer_index_);
  next_buffer_index_++;
  return true;
}

bool SourceBufferRange::HasNextBuffer() const {
  return next_buffer_index_ >= 0 &&
      next_buffer_index_ < static_cast<int>(buffers_.size());
}

base::TimeDelta SourceBufferRange::GetNextTimestamp() const {
  DCHECK(!buffers_.empty());
  DCHECK(HasNextBufferPosition());

  if (waiting_for_keyframe_)
    return next_keyframe_timestamp_;

  if (next_buffer_index_ >= static_cast<int>(buffers_.size()))
    return kNoTimestamp();

  return buffers_.at(next_buffer_index_)->GetDecodeTimestamp();
}

bool SourceBufferRange::HasNextBufferPosition() const {
  return next_buffer_index_ >= 0 || waiting_for_keyframe_;
}

void SourceBufferRange::ResetNextBufferPosition() {
  next_buffer_index_ = -1;
  waiting_for_keyframe_ = false;
  next_keyframe_timestamp_ = kNoTimestamp();
}

void SourceBufferRange::AppendRangeToEnd(const SourceBufferRange& range,
                                         bool transfer_current_position) {
  DCHECK(CanAppendRangeToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendRangeToEnd(
    const SourceBufferRange& range) const {
  return CanAppendBuffersToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendBuffersToEnd(
    const BufferQueue& buffers) const {
  DCHECK(!buffers_.empty());
  return IsNextInSequence(buffers_.back(),
                          buffers.front()->GetDecodeTimestamp());
}

bool SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  DCHECK(!buffers_.empty());

  return (IsNextInSequence(buffers_.back(), timestamp) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  base::TimeDelta start_timestamp =
      std::max(base::TimeDelta(), GetStartTimestamp() - GetFudgeRoom());
  return !keyframe_map_.empty() && start_timestamp <= timestamp &&
      timestamp < GetBufferedEndTimestamp();
}

bool SourceBufferRange::CompletelyOverlaps(
    const SourceBufferRange& range) const {
  return GetStartTimestamp() <= range.GetStartTimestamp() &&
      GetEndTimestamp() >= range.GetEndTimestamp();
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.GetStartTimestamp() <= GetEndTimestamp() &&
      GetEndTimestamp() < range.GetEndTimestamp();
}

base::TimeDelta SourceBufferRange::GetStartTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta start_timestamp = media_segment_start_time_;
  if (start_timestamp == kNoTimestamp())
    start_timestamp = buffers_.front()->GetDecodeTimestamp();
  return start_timestamp;
}

base::TimeDelta SourceBufferRange::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetDecodeTimestamp();
}

base::TimeDelta SourceBufferRange::GetBufferedEndTimestamp() const {
  DCHECK(!buffers_.empty());
  base::TimeDelta duration = buffers_.back()->GetDuration();
  if (duration == kNoTimestamp() || duration == base::TimeDelta())
    duration = GetApproximateDuration();
  return GetEndTimestamp() + duration;
}

bool SourceBufferRange::IsNextInSequence(
    const scoped_refptr<media::StreamParserBuffer>& buffer,
    base::TimeDelta timestamp) const {
  return buffer->GetDecodeTimestamp() < timestamp &&
      timestamp <= buffer->GetDecodeTimestamp() + GetFudgeRoom();
}

base::TimeDelta SourceBufferRange::GetFudgeRoom() const {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 2x the approximate duration of a buffer is considered
  // within this range.
  return 2 * GetApproximateDuration();
}

base::TimeDelta SourceBufferRange::GetApproximateDuration() const {
  base::TimeDelta max_interbuffer_distance = interbuffer_distance_cb_.Run();
  DCHECK(max_interbuffer_distance != kNoTimestamp());
  return max_interbuffer_distance;
}

}  // namespace media
