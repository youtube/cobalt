// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_stream.h"

#include <algorithm>
#include <map>

#include "base/logging.h"

namespace media {

// Helper class representing a range of buffered data. All buffers in a
// SourceBufferRange are ordered sequentially in presentation order with no
// gaps.
class SourceBufferRange {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;

  // Creates a source buffer range with |new_buffers|. |new_buffers| cannot be
  // empty and the front of |new_buffers| must be a keyframe.
  explicit SourceBufferRange(const BufferQueue& new_buffers);

  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes. Assumes |buffers| belongs at the end of the
  // range.
  void AppendToEnd(const BufferQueue& buffers);
  bool CanAppendToEnd(const BufferQueue& buffers) const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_index_|
  // is transfered to this SourceBufferRange.
  void AppendToEnd(const SourceBufferRange& range,
                   bool transfer_current_position);
  bool CanAppendToEnd(const SourceBufferRange& range) const;

  // Updates |next_buffer_index_| to point to the Buffer containing |timestamp|.
  // Assumes |timestamp| is valid and in this range.
  void Seek(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe after or equal to
  // |timestamp|.
  void SeekAfter(base::TimeDelta timestamp);

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
  // If |deleted_buffers| or |next_buffer| are null, they are ignored.
  // Otherwise, |deleted_buffers| contains the buffers that were deleted from
  // this range, and |next_buffer| points to the buffer in |deleted_buffers|
  // that had been at |next_buffer_index_|. If |next_buffer_index_| did not
  // point to any buffer added to |deleted_buffers|, then |next_buffer| points
  // to |deleted_buffers.end()|.
  void DeleteAfter(scoped_refptr<StreamParserBuffer> buffer,
                   BufferQueue* deleted_buffers,
                   BufferQueue::iterator* next_buffer);
  // Deletes all buffers in range.
  void DeleteAll(BufferQueue* deleted_buffers,
                 BufferQueue::iterator* next_buffer);

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  bool HasNextBuffer() const;

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(). Returns kNoTimestamp() if Seek() has never been called or
  // if this range does not have the next buffer yet.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the end timestamp of the buffered data. (Note that this is equal to
  // the last buffer's timestamp + its duration.)
  base::TimeDelta GetEndTimestamp() const;

  // Returns the Timespan of buffered time in this range.
  SourceBufferStream::Timespan GetBufferedTime() const;

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  // Returns 0 if |timestamp| is in this range, -1 if |timestamp| appears
  // before this range, or 1 if |timestamp| appears after this range.
  int BelongsToRange(base::TimeDelta timestamp) const;

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Returns true if this range's buffered timespan completely overlaps the
  // buffered timespan of |range|.
  bool CompletelyOverlaps(const SourceBufferRange& range) const;

  // Returns true if the end of this range contains buffers that overlaps with
  // the beginning of |range|.
  bool EndOverlaps(const SourceBufferRange& range) const;

 private:
  // Helper method to delete buffers in |buffers_| starting from
  // |starting_point|, an iterator in |buffers_|.
  void DeleteAfter(const BufferQueue::iterator& starting_point,
                   BufferQueue* deleted_buffers,
                   BufferQueue::iterator* next_buffer);

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  typedef std::map<base::TimeDelta, size_t> KeyframeMap;
  KeyframeMap keyframe_map_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetBufferedTime(), set to -1 before Seek().
  int next_buffer_index_;

  // True if the range needs to wait for the next keyframe to be appended before
  // returning buffers from GetNextBuffer().
  bool waiting_for_keyframe_;

  // If |waiting_for_keyframe_| is true, this range will wait for the next
  // keyframe with timestamp >= |next_keyframe_timestamp_|.
  base::TimeDelta next_keyframe_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(SourceBufferRange);
};

}  // namespace media

// Helper method that returns true if |ranges| is sorted in increasing order,
// false otherwise.
static bool IsRangeListSorted(
    const std::list<media::SourceBufferRange*>& ranges) {
  base::TimeDelta prev = media::kNoTimestamp();
  for (std::list<media::SourceBufferRange*>::const_iterator itr =
       ranges.begin(); itr != ranges.end(); itr++) {
    media::SourceBufferStream::Timespan buffered = (*itr)->GetBufferedTime();
    if (prev != media::kNoTimestamp() && prev >= buffered.first)
      return false;
    prev = buffered.second;
  }
  return true;
}

// Comparison function for two Buffers based on timestamp.
static bool BufferComparator(
    const scoped_refptr<media::StreamParserBuffer>& first,
    const scoped_refptr<media::StreamParserBuffer>& second) {
  return first->GetTimestamp() < second->GetTimestamp();
}

// Returns the upper bound for the starting timestamp for the next buffer
// in sequence after |buffer|. Assumes |buffer|'s timestamp and
// duration are valid.
static base::TimeDelta MaxNextTimestamp(
    const scoped_refptr<media::StreamParserBuffer>& buffer) {
  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 1/3 of the duration past the end of this buffer
  // is considered the next buffer in the sequence.
  return buffer->GetEndTimestamp() + buffer->GetDuration() / 3;
}

// Returns true if |timestamp| is the timestamp of the next buffer in
// sequence after |buffer|, false otherwise.
static bool IsNextInSequence(
    const scoped_refptr<media::StreamParserBuffer>& buffer,
    base::TimeDelta timestamp) {
  return timestamp >= buffer->GetEndTimestamp() &&
      timestamp <= MaxNextTimestamp(buffer);
}

namespace media {

SourceBufferStream::SourceBufferStream()
    : seek_pending_(true),
      seek_buffer_timestamp_(base::TimeDelta()),
      selected_range_(NULL),
      end_of_stream_(false) {
}

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config)
    : seek_pending_(true),
      seek_buffer_timestamp_(base::TimeDelta()),
      selected_range_(NULL),
      end_of_stream_(false) {
  audio_config_.CopyFrom(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config)
    : seek_pending_(true),
      seek_buffer_timestamp_(base::TimeDelta()),
      selected_range_(NULL),
      end_of_stream_(false) {
  video_config_.CopyFrom(video_config);
}

SourceBufferStream::~SourceBufferStream() {
  while (!ranges_.empty()) {
    delete ranges_.front();
    ranges_.pop_front();
  }
}

bool SourceBufferStream::Append(
    const SourceBufferStream::BufferQueue& buffers) {
  DCHECK(!buffers.empty());

  RangeList::iterator range_for_new_buffers = ranges_.end();
  RangeList::iterator itr = ranges_.end();
  base::TimeDelta start_timestamp = buffers.front()->GetTimestamp();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    int range_value = (*itr)->BelongsToRange(start_timestamp);
    // |start_timestamp| is before the current range in this loop. Because
    // |ranges_| is sorted, this means that we need to create a new range and
    // should be placed before |itr|.
    if (range_value < 0)
      break;

    if (range_value == 0) {
      // Found an existing range into which we can append buffers.
      range_for_new_buffers = itr;
      break;
    }
  }

  if (range_for_new_buffers == ranges_.end()) {
    // Ranges must begin with a keyframe.
    if (!buffers.front()->IsKeyframe())
      return false;
    range_for_new_buffers =
        ranges_.insert(itr, new SourceBufferRange(buffers));
  } else {
    InsertIntoExistingRange(range_for_new_buffers, buffers);
  }

  // Resolve overlaps.
  ResolveCompleteOverlaps(range_for_new_buffers);
  ResolveEndOverlap(range_for_new_buffers);
  MergeWithAdjacentRangeIfNecessary(range_for_new_buffers);

  // Finally, try to complete pending seek if one exists.
  if (seek_pending_)
    Seek(seek_buffer_timestamp_);

  DCHECK(IsRangeListSorted(ranges_));
  return true;
}

void SourceBufferStream::InsertIntoExistingRange(
    const RangeList::iterator& range_for_new_buffers_itr,
    const BufferQueue& new_buffers) {
  SourceBufferRange* range_for_new_buffers = *range_for_new_buffers_itr;
  RangeList::iterator next_range_itr = range_for_new_buffers_itr;
  next_range_itr++;

  // In case this is a middle overlap, save the buffers that come after the end
  // of |new_buffers|, and add them into a new range.
  SourceBufferRange* new_portion =
      range_for_new_buffers->SplitRange(new_buffers.back()->GetEndTimestamp());

  if (new_portion) {
    next_range_itr = ranges_.insert(next_range_itr, new_portion);
    // If |range_for_new_buffers| was selected and the next buffer was in the
    // |new_portion| half, update |selected_range_|.
    if (selected_range_ == range_for_new_buffers &&
        new_portion->GetNextTimestamp() != kNoTimestamp()) {
      selected_range_ = new_portion;
    }
  }

  BufferQueue deleted_buffers;
  BufferQueue::iterator next_buffer;
  range_for_new_buffers->DeleteAfter(
      new_buffers.front(), &deleted_buffers, &next_buffer);
  range_for_new_buffers->AppendToEnd(new_buffers);

  if (selected_range_ == range_for_new_buffers &&
      !deleted_buffers.empty() && next_buffer != deleted_buffers.end()) {
    UpdateTrackBuffer(range_for_new_buffers_itr, deleted_buffers, next_buffer);
  }
}

void SourceBufferStream::ResolveCompleteOverlaps(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  next_range_itr++;

  while (next_range_itr != ranges_.end() &&
         range_with_new_buffers->CompletelyOverlaps(**next_range_itr)) {
    if (*next_range_itr == selected_range_) {
      // Delete everything from the selected range that |new_range| overlaps,
      // and save the next buffers.
      BufferQueue deleted_buffers;
      BufferQueue::iterator next_buffer;
      (*next_range_itr)->DeleteAll(&deleted_buffers, &next_buffer);
      UpdateTrackBuffer(range_with_new_buffers_itr, deleted_buffers,
                        next_buffer);
      DCHECK_NE(selected_range_, *next_range_itr);
    }
    delete *next_range_itr;
    next_range_itr = ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::ResolveEndOverlap(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  next_range_itr++;

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
    ranges_.insert(next_range_itr, new_next_range);

  // If we didn't overlap a selected range, return.
  if (selected_range_ != overlapped_range.get())
    return;

  // If the next buffer was in the |new_next_range| half of the overlapped
  // range, then the |selected_range_| is now |new_next_range|.
  if (new_next_range &&
      new_next_range->GetNextTimestamp() != kNoTimestamp()) {
    selected_range_ = new_next_range;
    return;
  }

  // Otherwise, update track buffer with overlapped buffers.
  BufferQueue deleted_buffers;
  scoped_refptr<StreamParserBuffer> buffer;
  while (overlapped_range->GetNextBuffer(&buffer)) {
    deleted_buffers.push_back(buffer);
  }
  BufferQueue::iterator next_buffer = deleted_buffers.begin();

  // This will update |selected_range_| to no longer point to
  // |overlapped_range|.
  UpdateTrackBuffer(range_with_new_buffers_itr, deleted_buffers, next_buffer);
  DCHECK_NE(selected_range_, overlapped_range.get());
}

void SourceBufferStream::UpdateTrackBuffer(
    const RangeList::iterator& range_with_new_buffers_itr,
    const BufferQueue& deleted_buffers,
    const BufferQueue::iterator& next_buffer) {
  DCHECK(!deleted_buffers.empty() && next_buffer != deleted_buffers.end());

  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;

  // Seek to the next keyframe after (or equal to) the timestamp of the next
  // buffer being overlapped.
  range_with_new_buffers->SeekAfter((*next_buffer)->GetTimestamp());
  selected_range_ = range_with_new_buffers;

  base::TimeDelta next_keyframe_timestamp =
      range_with_new_buffers->GetNextTimestamp();

  if (track_buffer_.empty()) {
    // Add all the old buffers up until |next_keyframe_timestamp| into
    // |track_buffer_|. If there was no next keyframe, then we add all buffers
    // into |track_buffer_|.
    BufferQueue::iterator next_buffer_itr = next_buffer;
    while (next_buffer_itr != deleted_buffers.end() &&
           (next_keyframe_timestamp == kNoTimestamp() ||
            (*next_buffer_itr)->GetTimestamp() < next_keyframe_timestamp)) {
      track_buffer_.push_back(*next_buffer_itr);
      next_buffer_itr++;
    }
  }

  // See if the next range contains the keyframe after the end of the
  // |track_buffer_|, and if so, change |selected_range_|.
  if (next_keyframe_timestamp == kNoTimestamp()) {
    DCHECK(!track_buffer_.empty());
    RangeList::iterator next_range_itr = range_with_new_buffers_itr;
    next_range_itr++;
    if (next_range_itr != ranges_.end()) {
      (*next_range_itr)->SeekAfter(track_buffer_.back()->GetEndTimestamp());
      if (IsNextInSequence(track_buffer_.back(),
                           (*next_range_itr)->GetNextTimestamp())) {
        selected_range_ = *next_range_itr;
      }
    }
  }
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  next_range_itr++;

  if (next_range_itr != ranges_.end() &&
      range_with_new_buffers->CanAppendToEnd(**next_range_itr)) {
    bool transfer_current_position = selected_range_ == *next_range_itr;
    range_with_new_buffers->AppendToEnd(**next_range_itr,
                                        transfer_current_position);
    // Update |selected_range_| pointer if |range| has become selected after
    // merges.
    if (transfer_current_position)
      selected_range_ = range_with_new_buffers;

    delete *next_range_itr;
    ranges_.erase(next_range_itr);
  }
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  selected_range_ = NULL;
  track_buffer_.clear();

  seek_buffer_timestamp_ = timestamp;
  seek_pending_ = true;

  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    if ((*itr)->CanSeekTo(timestamp))
      break;
  }

  if (itr == ranges_.end())
    return;

  selected_range_ = *itr;
  selected_range_->Seek(timestamp);
  seek_pending_ = false;
  end_of_stream_ = false;
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

  if (end_of_stream_ && (!selected_range_ ||
                         !selected_range_->HasNextBuffer())) {
    *out_buffer = StreamParserBuffer::CreateEOSBuffer();
    return true;
  }

  return selected_range_ && selected_range_->GetNextBuffer(out_buffer);
}

SourceBufferStream::TimespanList SourceBufferStream::GetBufferedTime() const {
  TimespanList timespans;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); itr++) {
    timespans.push_back((*itr)->GetBufferedTime());
  }
  return timespans;
}

void SourceBufferStream::EndOfStream() {
  DCHECK(CanEndOfStream());
  end_of_stream_ = true;
}

bool SourceBufferStream::CanEndOfStream() const {
  return ranges_.empty() || selected_range_ == ranges_.back();
}

SourceBufferRange::SourceBufferRange(const BufferQueue& new_buffers)
    : next_buffer_index_(-1),
      waiting_for_keyframe_(false),
      next_keyframe_timestamp_(kNoTimestamp()) {
  DCHECK(!new_buffers.empty());
  DCHECK(new_buffers.front()->IsKeyframe());
  AppendToEnd(new_buffers);
}

void SourceBufferRange::AppendToEnd(const BufferQueue& new_buffers) {
  for (BufferQueue::const_iterator itr = new_buffers.begin();
       itr != new_buffers.end(); itr++) {
    DCHECK((*itr)->GetDuration() > base::TimeDelta());
    DCHECK((*itr)->GetTimestamp() != kNoTimestamp());
    buffers_.push_back(*itr);
    if ((*itr)->IsKeyframe()) {
      keyframe_map_.insert(
          std::make_pair((*itr)->GetTimestamp(), buffers_.size() - 1));

      if (waiting_for_keyframe_ &&
          (*itr)->GetTimestamp() >= next_keyframe_timestamp_) {
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
  if (result == keyframe_map_.end() || result->first != timestamp) {
    DCHECK(result != keyframe_map_.begin());
    result--;
  }
  next_buffer_index_ = result->second;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
}

void SourceBufferRange::SeekAfter(base::TimeDelta timestamp) {
  DCHECK(!keyframe_map_.empty());

  // lower_bound() returns the first element >= |timestamp|, so |result| is the
  // value that we want.
  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);

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

SourceBufferRange* SourceBufferRange::SplitRange(base::TimeDelta timestamp) {
  // Find the first keyframe after |timestamp|.
  KeyframeMap::iterator new_beginning_keyframe =
      keyframe_map_.lower_bound(timestamp);

  // If there is no keyframe after |timestamp|, we can't split the range.
  if (new_beginning_keyframe == keyframe_map_.end())
    return NULL;

  int keyframe_index = new_beginning_keyframe->second;
  DCHECK_LT(keyframe_index, static_cast<int>(buffers_.size()));

  BufferQueue removed_buffers;
  BufferQueue::iterator next_buffer;
  DeleteAfter(
      buffers_.begin() + keyframe_index, &removed_buffers, &next_buffer);

  SourceBufferRange* split_range = new SourceBufferRange(removed_buffers);
  if (next_buffer != removed_buffers.end()) {
    split_range->next_buffer_index_ = next_buffer - removed_buffers.begin();
  }
  return split_range;
}

void SourceBufferRange::DeleteAll(BufferQueue* removed_buffers,
                                  BufferQueue::iterator* next_buffer) {
  DeleteAfter(buffers_.begin(), removed_buffers, next_buffer);
}

void SourceBufferRange::DeleteAfter(
    scoped_refptr<StreamParserBuffer> buffer,
    BufferQueue* removed_buffers,
    BufferQueue::iterator* next_buffer) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::iterator starting_point =
      std::lower_bound(buffers_.begin(), buffers_.end(),
                       buffer,
                       BufferComparator);
  DeleteAfter(starting_point, removed_buffers, next_buffer);
}

void SourceBufferRange::DeleteAfter(
    const BufferQueue::iterator& starting_point,
    BufferQueue* removed_buffers,
    BufferQueue::iterator* next_buffer) {
  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return;

  // Find the first keyframe after |starting_point|.
  KeyframeMap::iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetTimestamp());

  // Save the buffers we're about to delete.
  if (removed_buffers) {
    BufferQueue saved(starting_point, buffers_.end());
    removed_buffers->swap(saved);
    if (next_buffer)
      *next_buffer = removed_buffers->end();
  }

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  base::TimeDelta next_buffer_timestamp = GetNextTimestamp();
  if (next_buffer_timestamp != kNoTimestamp() &&
      next_buffer_timestamp >= (*starting_point)->GetTimestamp()) {
    if (removed_buffers && next_buffer) {
      int starting_offset = starting_point - buffers_.begin();
      int next_buffer_offset = next_buffer_index_ - starting_offset;
      DCHECK_GE(next_buffer_offset, 0);
      *next_buffer = removed_buffers->begin() + next_buffer_offset;
    }
    next_buffer_index_ = -1;
  }

  // Remove keyframes from |starting_point| onward.
  keyframe_map_.erase(starting_point_keyframe, keyframe_map_.end());

  // Remove everything from |starting_point| onward.
  buffers_.erase(starting_point, buffers_.end());
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

  if (next_buffer_index_ >= static_cast<int>(buffers_.size()) ||
      next_buffer_index_ < 0 || waiting_for_keyframe_) {
    return kNoTimestamp();
  }

  return buffers_.at(next_buffer_index_)->GetTimestamp();
}

SourceBufferStream::Timespan
SourceBufferRange::GetBufferedTime() const {
  return std::make_pair(GetStartTimestamp(), GetEndTimestamp());
}

void SourceBufferRange::AppendToEnd(const SourceBufferRange& range,
                                    bool transfer_current_position) {
  DCHECK(CanAppendToEnd(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendToEnd(const SourceBufferRange& range) const {
  return CanAppendToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendToEnd(const BufferQueue& buffers) const {
  DCHECK(!buffers_.empty());
  return IsNextInSequence(buffers_.back(), buffers.front()->GetTimestamp());
}

int SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  DCHECK(!buffers_.empty());

  if (IsNextInSequence(buffers_.back(), timestamp) ||
      (GetEndTimestamp() >= timestamp && GetStartTimestamp() <= timestamp)) {
    return 0;
  }

  if (GetStartTimestamp() > timestamp)
    return -1;

  // |timestamp| must be after this range.
  return 1;
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  return !keyframe_map_.empty() && GetStartTimestamp() <= timestamp &&
      GetEndTimestamp() > timestamp;
}

bool SourceBufferRange::CompletelyOverlaps(
    const SourceBufferRange& range) const {
  return GetStartTimestamp() <= range.GetStartTimestamp() &&
      GetEndTimestamp() >= range.GetEndTimestamp();
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.GetStartTimestamp() < GetEndTimestamp() &&
      GetEndTimestamp() < range.GetEndTimestamp();
}

base::TimeDelta SourceBufferRange::GetStartTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.front()->GetTimestamp();
}

base::TimeDelta SourceBufferRange::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetEndTimestamp();
}

}  // namespace media
