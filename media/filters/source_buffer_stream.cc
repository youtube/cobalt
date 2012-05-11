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

  SourceBufferRange();

  // Adds |new_buffers| into this range. |new_buffers| must belong to this
  // range. Garbage collection may occur after Append().
  void Append(const BufferQueue& new_buffers);

  // Updates |next_buffer_index_| to point to the Buffer containing |timestamp|.
  // Assumes |timestamp| is valid and in this range.
  void Seek(base::TimeDelta timestamp);

  // Updates |next_buffer_index_| to point to next keyframe after or equal to
  // |timestamp|. If there is no such keyframe, then this range will seek to
  // the end and return kNoTimestamp().
  // Assumes |timestamp| is valid and in this range.
  base::TimeDelta SeekAfter(base::TimeDelta timestamp);

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);
  base::TimeDelta GetNextTimestamp() const;

  // Returns the Timespan of buffered time in this range.
  SourceBufferStream::Timespan GetBufferedTime() const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_position_|
  // is transfered to this SourceBufferRange.
  void AppendToEnd(const SourceBufferRange& range,
                   bool transfer_current_position);
  bool CanAppendToEnd(const SourceBufferRange& range) const;
  bool CanAppendToEnd(const BufferQueue& buffers) const;

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

  // Functions that tell how |buffers| intersects with this range.
  // TODO(vrk): These functions should be unnecessary when overlapping the
  // selected range is implemented properly. (crbug.com/126560)
  bool IsStartOverlappedBy(const BufferQueue& buffers) const;
  bool IsEndOverlappedBy(const BufferQueue& buffers) const;
  bool IsCompletelyOverlappedBy(const BufferQueue& buffers) const;

 private:
  // Appends |buffers| to the end of the range and updates |keyframe_map_| as
  // it encounters new keyframes. Assumes |buffers| belongs at the end of the
  // range.
  void AppendToEnd(const BufferQueue& buffers);

  // Returns the start timestamp of the range, or kNoTimestamp if the range is
  // empty.
  base::TimeDelta BufferedStart() const;

  // Returns the end timestamp of the buffered data. (Note that this is equal to
  // the last buffer's timestamp + its duration.) Returns kNoTimestamp if the
  // range is empty.
  base::TimeDelta BufferedEnd() const;

  // An ordered list of buffers in this range.
  BufferQueue buffers_;

  // Maps keyframe timestamps to its index position in |buffers_|.
  typedef std::map<base::TimeDelta, size_t> KeyframeMap;
  KeyframeMap keyframe_map_;

  // Index into |buffers_| for the next buffer to be returned by
  // GetBufferedTime(), set to -1 before Seek().
  int next_buffer_index_;

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
static bool BufferComparator(scoped_refptr<media::Buffer> first,
                             scoped_refptr<media::Buffer> second) {
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
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL),
      waiting_for_keyframe_(false) {
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

  // Check to see if |buffers| will overlap the currently |selected_range_|,
  // and if so, ignore this Append() request.
  // TODO(vrk): Support overlapping selected range properly. (crbug.com/126560)
  if (selected_range_ &&
      (selected_range_->IsEndOverlappedBy(buffers) ||
       selected_range_->IsStartOverlappedBy(buffers))) {
    return false;
  }

  SourceBufferRange* range = NULL;
  RangeList::iterator itr = ranges_.end();
  base::TimeDelta start_timestamp = buffers.front()->GetTimestamp();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    int range_value = (*itr)->BelongsToRange(start_timestamp);

    // |start_timestamp| is before the current range in this loop. Because
    // |ranges_| is sorted, this means that we need to create a new range and it
    // should be placed before |itr|.
    // TODO(vrk): We also break out of the loop if |buffers| completely overlaps
    // the current range. This is to cover the case when |buffers| belongs to
    // the current range, but also completely overlaps it. This should be
    // removed when start overlap is handled properly.
    if (range_value < 0 || (*itr)->IsCompletelyOverlappedBy(buffers))
      break;

    if (range_value == 0) {
      // Found an existing range into which we can append buffers.
      range = *itr;

      if (range->CanAppendToEnd(buffers) && waiting_for_keyframe_) {
        // Currently we do not support the case where the next buffer after the
        // buffers in the track buffer is not a keyframe.
        if (!buffers.front()->IsKeyframe())
          return false;
        waiting_for_keyframe_ = false;
      }
      break;
    }
  }

  if (!range) {
    // Ranges must begin with a keyframe.
    if (!buffers.front()->IsKeyframe())
      return false;

    range = new SourceBufferRange();
    itr = ranges_.insert(itr, range);
  }

  // Append buffers to the appropriate range.
  range->Append(buffers);

  // Increment |itr| to be the range after |range|.
  itr++;

  // Resolve overlaps.
  itr = ResolveCompleteOverlaps(itr, range);
  itr = ResolveEndOverlaps(itr, range);
  MergeWithAdjacentRangeIfNecessary(itr, range);

  // Finally, try to complete pending seek if one exists.
  if (seek_pending_)
    Seek(seek_buffer_timestamp_);

  DCHECK(IsRangeListSorted(ranges_));
  return true;
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::ResolveCompleteOverlaps(
    const RangeList::iterator& range_itr, SourceBufferRange* new_range) {
  RangeList::iterator itr = range_itr;
  while (itr != ranges_.end() && new_range->CompletelyOverlaps(**itr)) {
    if (*itr == selected_range_) {
      // Get the timestamp for the next buffer in the sequence.
      base::TimeDelta next_timestamp = selected_range_->GetNextTimestamp();
      // Then seek to the next keyframe after (or equal to) |next_timestamp|.
      // This will allow us to transition from the old buffers to the new
      // buffers seamlessly.
      base::TimeDelta next_keyframe_timestamp =
          new_range->SeekAfter(next_timestamp);

      // If there's no keyframe after |next_timestamp|, then set flag to wait
      // for the next keyframe in this range to be appended.
      if (next_keyframe_timestamp == kNoTimestamp())
        waiting_for_keyframe_ = true;

      // Add all the old buffers up until |next_keyframe_timestamp| into
      // |track_buffer_|. If there was no keyframe, then we add all buffers into
      // |track_buffer_|.
      scoped_refptr<StreamParserBuffer> next_buffer;
      while (selected_range_->GetNextBuffer(&next_buffer) &&
             (waiting_for_keyframe_ ||
             next_buffer->GetTimestamp() < next_keyframe_timestamp)) {
        track_buffer_.push_back(next_buffer);
      }

      selected_range_ = new_range;
    }
    delete *itr;
    itr = ranges_.erase(itr);
  }
  return itr;
}

SourceBufferStream::RangeList::iterator
SourceBufferStream::ResolveEndOverlaps(
    const RangeList::iterator& range_itr, SourceBufferRange* new_range) {
  RangeList::iterator itr = range_itr;
  while (itr != ranges_.end() && new_range->EndOverlaps(**itr)) {
    DCHECK_NE(*itr, selected_range_);
    delete *itr;
    itr = ranges_.erase(itr);
  }
  return itr;
}

void SourceBufferStream::MergeWithAdjacentRangeIfNecessary(
    const RangeList::iterator& itr, SourceBufferRange* new_range) {
  if (itr != ranges_.end() && new_range->CanAppendToEnd(**itr)) {
    bool transfer_current_position = selected_range_ == *itr;
    new_range->AppendToEnd(**itr, transfer_current_position);
    // Update |selected_range_| pointer if |range| has become selected after
    // merges.
    if (transfer_current_position)
      selected_range_ = new_range;

    delete *itr;
    ranges_.erase(itr);
  }
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  selected_range_ = NULL;
  track_buffer_.clear();
  waiting_for_keyframe_ = false;

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

std::list<SourceBufferStream::Timespan>
SourceBufferStream::GetBufferedTime() const {
  std::list<Timespan> timespans;
  for (RangeList::const_iterator itr = ranges_.begin();
       itr != ranges_.end(); itr++) {
    timespans.push_back((*itr)->GetBufferedTime());
  }
  return timespans;
}

SourceBufferRange::SourceBufferRange()
    : next_buffer_index_(-1) {
}

void SourceBufferRange::Append(const BufferQueue& new_buffers) {
  base::TimeDelta start_timestamp = new_buffers.front()->GetTimestamp();

  if (!buffers_.empty() && start_timestamp < BufferedEnd()) {
    // We are overwriting existing data, so find the starting point where
    // things will get overwritten.
    BufferQueue::iterator starting_point =
        std::lower_bound(buffers_.begin(), buffers_.end(),
                         new_buffers.front(),
                         BufferComparator);

    // Remove everything from |starting_point| onward.
    buffers_.erase(starting_point, buffers_.end());
  }

  // Append data.
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
    }
  }
}

void SourceBufferRange::Seek(base::TimeDelta timestamp) {
  DCHECK(CanSeekTo(timestamp));
  DCHECK(!keyframe_map_.empty());

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

base::TimeDelta SourceBufferRange::SeekAfter(base::TimeDelta timestamp) {
  DCHECK_EQ(BelongsToRange(timestamp), 0);
  DCHECK(!keyframe_map_.empty());

  // lower_bound() returns the first element >= |timestamp|, so |result| is the
  // value that we want.
  KeyframeMap::iterator result = keyframe_map_.lower_bound(timestamp);

  // If there isn't a keyframe after |timestamp|, then seek to end and return
  // kNoTimestamp to signal such.
  if (result == keyframe_map_.end()) {
    next_buffer_index_ = buffers_.size();
    return kNoTimestamp();
  }

  next_buffer_index_ = result->second;
  DCHECK_LT(next_buffer_index_, static_cast<int>(buffers_.size()));
  return result->first;
}


bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK_GE(next_buffer_index_, 0);
  if (next_buffer_index_ >= static_cast<int>(buffers_.size()))
    return false;

  *out_buffer = buffers_.at(next_buffer_index_);
  next_buffer_index_++;
  return true;
}

base::TimeDelta SourceBufferRange::GetNextTimestamp() const {
  DCHECK_GE(next_buffer_index_, 0);
  DCHECK(!buffers_.empty());

  if (next_buffer_index_ >= static_cast<int>(buffers_.size()))
    return buffers_.back()->GetEndTimestamp();

  return buffers_.at(next_buffer_index_)->GetTimestamp();
}

SourceBufferStream::Timespan
SourceBufferRange::GetBufferedTime() const {
  return std::make_pair(BufferedStart(), BufferedEnd());
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
  return buffers_.empty() ||
      IsNextInSequence(buffers_.back(), buffers.front()->GetTimestamp());
}

int SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  if (buffers_.empty())
    return 1;

  if (IsNextInSequence(buffers_.back(), timestamp) ||
      (BufferedEnd() >= timestamp && BufferedStart() <= timestamp)) {
    return 0;
  }

  if (BufferedStart() > timestamp)
    return -1;

  // |timestamp| must be after this range.
  return 1;
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  return !keyframe_map_.empty() && BufferedStart() <= timestamp &&
      BufferedEnd() > timestamp;
}

bool SourceBufferRange::CompletelyOverlaps(
    const SourceBufferRange& range) const {
  return BufferedStart() <= range.BufferedStart() &&
      BufferedEnd() >= range.BufferedEnd();
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.BufferedStart() < BufferedEnd() &&
      BufferedEnd() < range.BufferedEnd();
}

bool SourceBufferRange::IsStartOverlappedBy(const BufferQueue& buffers) const {
  base::TimeDelta start_timestamp = buffers.front()->GetTimestamp();
  return BufferedStart() < start_timestamp && start_timestamp < BufferedEnd();
}

bool SourceBufferRange::IsEndOverlappedBy(const BufferQueue& buffers) const {
  base::TimeDelta end_timestamp = buffers.back()->GetEndTimestamp();
  return BufferedStart() < end_timestamp && end_timestamp < BufferedEnd();
}

bool SourceBufferRange::IsCompletelyOverlappedBy(
    const BufferQueue& buffers) const {
  base::TimeDelta start_timestamp = buffers.front()->GetTimestamp();
  base::TimeDelta end_timestamp = buffers.back()->GetEndTimestamp();
  return start_timestamp <= BufferedStart() && BufferedEnd() <= end_timestamp;
}

base::TimeDelta SourceBufferRange::BufferedStart() const {
  if (buffers_.empty())
    return kNoTimestamp();

  return buffers_.front()->GetTimestamp();
}

base::TimeDelta SourceBufferRange::BufferedEnd() const {
  if (buffers_.empty())
    return kNoTimestamp();

  return buffers_.back()->GetEndTimestamp();
}

}  // namespace media
