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
  // |media_segment_start_time| refers to the starting timestamp for the media
  // segment to which these buffers belong.
  SourceBufferRange(const BufferQueue& new_buffers,
                    base::TimeDelta media_segment_start_time);

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
  // |deleted_buffers| contains the buffers that were deleted from this range,
  // starting at the buffer that had been at |next_buffer_index_|.
  // |deleted_buffers| is empty if the buffer at |next_buffer_index_| was not
  // deleted.
  void DeleteAfter(scoped_refptr<StreamParserBuffer> buffer,
                   BufferQueue* deleted_buffers);
  // Deletes all buffers in range.
  void DeleteAll(BufferQueue* deleted_buffers);

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

  // Returns the timestamp of the next buffer that will be returned from
  // GetNextBuffer(). This may be an approximation if the range does not have
  // next buffer buffered.
  base::TimeDelta GetNextTimestamp() const;

  // Returns the start timestamp of the range.
  base::TimeDelta GetStartTimestamp() const;

  // Returns the end timestamp of the buffered data. (Note that this is equal to
  // the last buffer's timestamp + its duration.)
  base::TimeDelta GetEndTimestamp() const;

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

 private:
  // Helper method to delete buffers in |buffers_| starting from
  // |starting_point|, an iterator in |buffers_|.
  void DeleteAfter(const BufferQueue::iterator& starting_point,
                   BufferQueue* deleted_buffers);

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  typedef std::map<base::TimeDelta, size_t> KeyframeMap;
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
    : seek_pending_(false),
      seek_buffer_timestamp_(base::TimeDelta()),
      selected_range_(NULL),
      end_of_stream_(false) {
}

SourceBufferStream::SourceBufferStream(const AudioDecoderConfig& audio_config)
    : seek_pending_(false),
      seek_buffer_timestamp_(base::TimeDelta()),
      selected_range_(NULL),
      end_of_stream_(false) {
  audio_config_.CopyFrom(audio_config);
}

SourceBufferStream::SourceBufferStream(const VideoDecoderConfig& video_config)
    : seek_pending_(false),
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
    const SourceBufferStream::BufferQueue& buffers,
    base::TimeDelta media_segment_start_time) {
  DCHECK(!buffers.empty());
  DCHECK(media_segment_start_time != kNoTimestamp());

  // Find a range into which we'll append |buffers|.
  RangeList::iterator range_for_new_buffers = FindExistingRangeFor(buffers);

  // If there's a range for |buffers|, insert |buffers| accordingly. Otherwise,
  // create a new range with |buffers|.
  if (range_for_new_buffers != ranges_.end()) {
    InsertIntoExistingRange(range_for_new_buffers, buffers);
  } else {
    // Ranges must begin with a keyframe.
    if (!buffers.front()->IsKeyframe())
      return false;
    range_for_new_buffers =
        AddToRanges(new SourceBufferRange(buffers, media_segment_start_time));
  }

  // Resolve overlaps.
  ResolveCompleteOverlaps(range_for_new_buffers);
  ResolveEndOverlap(range_for_new_buffers);
  MergeWithAdjacentRangeIfNecessary(range_for_new_buffers);

  // If these were the first buffers appended to the stream, seek to the
  // beginning of the range.
  // TODO(vrk): This should be done by ChunkDemuxer. (crbug.com/132815)
  if (!seek_pending_ && !selected_range_) {
    selected_range_ = *range_for_new_buffers;
    selected_range_->Seek(buffers.front()->GetTimestamp());
  }

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

  // If this is a simple case where we can just append to the end of the range,
  // do so and return.
  if (range_for_new_buffers->CanAppendToEnd(new_buffers)) {
    range_for_new_buffers->AppendToEnd(new_buffers);
    return;
  }

  // Otherwise, this is either a start overlap or an middle overlap.

  // In case this is a middle overlap, save the buffers that come after the end
  // of |new_buffers|, and add them into a new range.
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();
  bool had_next_buffer = range_for_new_buffers->HasNextBuffer();
  SourceBufferRange* new_next_range =
      range_for_new_buffers->SplitRange(new_buffers.back()->GetEndTimestamp());

  if (new_next_range)
    AddToRanges(new_next_range);

  // Delete the buffers that are overlapped by |new_buffers|, then append
  // |new_buffers| to the end of the range.
  BufferQueue deleted_buffers;
  range_for_new_buffers->DeleteAfter(new_buffers.front(), &deleted_buffers);
  range_for_new_buffers->AppendToEnd(new_buffers);

  // If |new_buffers| doesn't overlap the selected range, no need to do anything
  // more.
  if (selected_range_ != range_for_new_buffers || !had_next_buffer ||
      next_buffer_timestamp < new_buffers.front()->GetTimestamp()) {
    return;
  }

  // If this was a middle overlap resulting in a new range, and the next buffer
  // position has been transferred to the newly created range, update the
  // |selected_range_| accordingly.
  if (new_next_range && new_next_range->HasNextBufferPosition()) {
    DCHECK(!range_for_new_buffers->HasNextBufferPosition());
    selected_range_ = new_next_range;
    return;
  }

  selected_range_ = range_for_new_buffers;
  selected_range_->SeekAfter(next_buffer_timestamp);
  UpdateTrackBuffer(deleted_buffers);
}

void SourceBufferStream::ResolveCompleteOverlaps(
    const RangeList::iterator& range_with_new_buffers_itr) {
  SourceBufferRange* range_with_new_buffers = *range_with_new_buffers_itr;
  RangeList::iterator next_range_itr = range_with_new_buffers_itr;
  next_range_itr++;
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();

  while (next_range_itr != ranges_.end() &&
         range_with_new_buffers->CompletelyOverlaps(**next_range_itr)) {
    if (*next_range_itr == selected_range_) {
      // Transfer the next buffer position from the old selected range to the
      // range with the new buffers.
      selected_range_ = range_with_new_buffers;
      selected_range_->SeekAfter(next_buffer_timestamp);

      // Delete everything from the old selected range and save the next
      // buffers.
      BufferQueue deleted_buffers;
      (*next_range_itr)->DeleteAll(&deleted_buffers);
      UpdateTrackBuffer(deleted_buffers);
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
  base::TimeDelta next_buffer_timestamp = GetNextBufferTimestamp();

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
    selected_range_ = new_next_range;
    return;
  }

  // Transfer the next buffer position from the old range to the range with
  // the new buffers.
  selected_range_ = range_with_new_buffers;
  selected_range_->SeekAfter(next_buffer_timestamp);

  // Update track buffer with overlapped buffers.
  BufferQueue deleted_buffers;
  scoped_refptr<StreamParserBuffer> buffer;
  while (overlapped_range->GetNextBuffer(&buffer)) {
    deleted_buffers.push_back(buffer);
  }
  UpdateTrackBuffer(deleted_buffers);
}

void SourceBufferStream::UpdateTrackBuffer(const BufferQueue& deleted_buffers) {
  if (!track_buffer_.empty() || deleted_buffers.empty())
    return;

  DCHECK(selected_range_);
  DCHECK(selected_range_->HasNextBufferPosition());

  base::TimeDelta next_keyframe_timestamp = selected_range_->GetNextTimestamp();

  // If there is no gap between what was deleted and what was added, nothing
  // should be added to the track buffer.
  if (selected_range_->HasNextBuffer() &&
      next_keyframe_timestamp == deleted_buffers.front()->GetTimestamp()) {
    return;
  }

  DCHECK(next_keyframe_timestamp >= deleted_buffers.front()->GetTimestamp());

  // If the |selected_range_| is ready to return data, fill the track buffer
  // with all buffers that come before |next_keyframe_timestamp| and return.
  if (selected_range_->HasNextBuffer()) {
    for (BufferQueue::const_iterator itr = deleted_buffers.begin();
         itr != deleted_buffers.end() &&
         (*itr)->GetTimestamp() < next_keyframe_timestamp; ++itr) {
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
    (*next_range_itr)->SeekAfter(track_buffer_.back()->GetEndTimestamp());
    if ((*next_range_itr)->HasNextBuffer() &&
        IsNextInSequence(track_buffer_.back(),
                         (*next_range_itr)->GetNextTimestamp())) {
      selected_range_ = *next_range_itr;
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

base::TimeDelta SourceBufferStream::GetNextBufferTimestamp() {
  if (!selected_range_)
    return kNoTimestamp();

  DCHECK(selected_range_->HasNextBufferPosition());
  return selected_range_->GetNextTimestamp();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::FindExistingRangeFor(const BufferQueue& new_buffers) {
  DCHECK(!new_buffers.empty());
  base::TimeDelta start_timestamp = new_buffers.front()->GetTimestamp();
  for (RangeList::iterator itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    if ((*itr)->BelongsToRange(start_timestamp))
      return itr;
  }
  return ranges_.end();
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::AddToRanges(SourceBufferRange* new_range) {
  base::TimeDelta start_timestamp = new_range->GetStartTimestamp();
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    if ((*itr)->GetStartTimestamp() > start_timestamp)
      break;
  }
  return ranges_.insert(itr, new_range);
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::GetSelectedRangeItr() {
  DCHECK(selected_range_);
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    if (*itr == selected_range_)
      break;
  }
  DCHECK(itr != ranges_.end());
  return itr;
}

Ranges<base::TimeDelta> SourceBufferStream::GetBufferedTime() const {
  Ranges<base::TimeDelta> ranges;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); itr++) {
    ranges.Add((*itr)->GetStartTimestamp(), (*itr)->GetEndTimestamp());
  }
  return ranges;
}

void SourceBufferStream::EndOfStream() {
  DCHECK(CanEndOfStream());
  end_of_stream_ = true;
}

bool SourceBufferStream::CanEndOfStream() const {
  return ranges_.empty() || selected_range_ == ranges_.back();
}

SourceBufferRange::SourceBufferRange(const BufferQueue& new_buffers,
                                     base::TimeDelta media_segment_start_time)
    : next_buffer_index_(-1),
      waiting_for_keyframe_(false),
      next_keyframe_timestamp_(kNoTimestamp()),
      media_segment_start_time_(media_segment_start_time) {
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
  if (result != keyframe_map_.begin() &&
      (result == keyframe_map_.end() || result->first != timestamp)) {
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
      new SourceBufferRange(removed_buffers, kNoTimestamp());

  // If |next_buffer_index_| points to a buffer in |split_range|, update the
  // |next_buffer_index_| of this range and |split_range| accordingly.
  if (next_buffer_index_ >= static_cast<int>(buffers_.size())) {
    split_range->next_buffer_index_ = next_buffer_index_ - keyframe_index;
    next_buffer_index_ = -1;
  }
  return split_range;
}

void SourceBufferRange::DeleteAll(BufferQueue* removed_buffers) {
  DeleteAfter(buffers_.begin(), removed_buffers);
}

void SourceBufferRange::DeleteAfter(
    scoped_refptr<StreamParserBuffer> buffer, BufferQueue* removed_buffers) {
  // Find the place in |buffers_| where we will begin deleting data.
  BufferQueue::iterator starting_point =
      std::lower_bound(buffers_.begin(), buffers_.end(),
                       buffer,
                       BufferComparator);
  DeleteAfter(starting_point, removed_buffers);
}

void SourceBufferRange::DeleteAfter(
    const BufferQueue::iterator& starting_point, BufferQueue* removed_buffers) {
  DCHECK(removed_buffers);
  // Return if we're not deleting anything.
  if (starting_point == buffers_.end())
    return;

  // Reset the next buffer index if we will be deleting the buffer that's next
  // in sequence.
  if (HasNextBuffer() &&
      GetNextTimestamp() >= (*starting_point)->GetTimestamp()) {
    // Save the buffers we're about to delete if the output parameter is valid.
    int starting_offset = starting_point - buffers_.begin();
    int next_buffer_offset = next_buffer_index_ - starting_offset;
    DCHECK_GE(next_buffer_offset, 0);
    BufferQueue saved(starting_point + next_buffer_offset, buffers_.end());
    removed_buffers->swap(saved);
    next_buffer_index_ = -1;
  }

  // Remove keyframes from |starting_point| onward.
  KeyframeMap::iterator starting_point_keyframe =
      keyframe_map_.lower_bound((*starting_point)->GetTimestamp());
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
  DCHECK(HasNextBufferPosition());

  if (waiting_for_keyframe_)
    return next_keyframe_timestamp_;

  if (next_buffer_index_ >= static_cast<int>(buffers_.size()))
    return buffers_.back()->GetEndTimestamp();

  return buffers_.at(next_buffer_index_)->GetTimestamp();
}

bool SourceBufferRange::HasNextBufferPosition() const {
  return next_buffer_index_ >= 0 || waiting_for_keyframe_;
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

bool SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  DCHECK(!buffers_.empty());

  return (IsNextInSequence(buffers_.back(), timestamp) ||
          (GetStartTimestamp() <= timestamp && timestamp <= GetEndTimestamp()));
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
  base::TimeDelta start_timestamp = media_segment_start_time_;
  if (start_timestamp == kNoTimestamp())
    start_timestamp = buffers_.front()->GetTimestamp();
  return start_timestamp;
}

base::TimeDelta SourceBufferRange::GetEndTimestamp() const {
  DCHECK(!buffers_.empty());
  return buffers_.back()->GetEndTimestamp();
}

}  // namespace media
