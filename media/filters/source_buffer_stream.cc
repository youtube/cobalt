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

  // Updates |out_buffer| with the next buffer in presentation order. Seek()
  // must be called before calls to GetNextBuffer(), and buffers are returned
  // in order from the last call to Seek(). Returns true if |out_buffer| is
  // filled with a valid buffer, false if there is not enough data to fulfill
  // the request.
  bool GetNextBuffer(scoped_refptr<StreamParserBuffer>* out_buffer);

  // Returns the Timespan of buffered time in this range.
  SourceBufferStream::Timespan GetBufferedTime() const;

  // Appends the buffers from |range| into this range.
  // The first buffer in |range| must come directly after the last buffer
  // in this range.
  // If |transfer_current_position| is true, |range|'s |next_buffer_position_|
  // is transfered to this SourceBufferRange.
  void AppendRange(const SourceBufferRange& range,
                   bool transfer_current_position);
  bool CanAppendRange(const SourceBufferRange& range) const;

  // Returns whether a buffer with a starting timestamp of |timestamp| would
  // belong in this range. This includes a buffer that would be appended to
  // the end of the range.
  // Returns 0 if |timestamp| is in this range, -1 if |timestamp| appears
  // before this range, or 1 if |timestamp| appears after this range.
  int BelongsToRange(base::TimeDelta timestamp) const;

  // Returns true if the range has enough data to seek to the specified
  // |timestamp|, false otherwise.
  bool CanSeekTo(base::TimeDelta timestamp) const;

  // Returns true if the end of this range contains buffers that overlaps with
  // the beginning of |range|.
  bool EndOverlaps(const SourceBufferRange& range) const;

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

  // Returns the upper bound for the starting timestamp for the next buffer to
  // be appended at the end of the range. Should only be called if |buffers_| is
  // nonempty.
  base::TimeDelta MaxNextTimestamp() const;

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

namespace media {

SourceBufferStream::SourceBufferStream()
    : seek_pending_(false),
      seek_buffer_timestamp_(kNoTimestamp()),
      selected_range_(NULL) {
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
  base::TimeDelta start_timestamp = buffers.front()->GetTimestamp();
  base::TimeDelta end_timestamp = buffers.back()->GetTimestamp();

  // Check to see if |buffers| will overlap the currently |selected_range_|,
  // and if so, ignore this Append() request.
  // TODO(vrk): Support end overlap properly. (crbug.com/125072)
  if (selected_range_) {
    Timespan selected_range_span = selected_range_->GetBufferedTime();
    if (selected_range_span.second > start_timestamp &&
        selected_range_span.first <= end_timestamp) {
      return false;
    }
  }

  SourceBufferRange* range = NULL;
  RangeList::iterator itr = ranges_.end();
  for (itr = ranges_.begin(); itr != ranges_.end(); itr++) {
    int range_value = (*itr)->BelongsToRange(start_timestamp);

    // |start_timestamp| is before the current range in this loop. Because
    // |ranges_| is sorted, this means that we need to create a new range and it
    // should be placed before |itr|.
    if (range_value < 0)
      break;

    if (range_value == 0) {
      // Found an existing range into which we can append buffers.
      range = *itr;
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

  // Handle overlaps if they were created.
  while (itr != ranges_.end() && range->EndOverlaps(**itr)) {
    DCHECK_NE(*itr, selected_range_);
    delete *itr;
    itr = ranges_.erase(itr);
  }

  // Merge with neighbor if necessary.
  if (itr != ranges_.end() && range->CanAppendRange(**itr)) {
    bool transfer_current_position = selected_range_ == *itr;
    range->AppendRange(**itr, transfer_current_position);
    // Update |selected_range_| pointer if |range| has become selected after
    // merges.
    if (transfer_current_position)
      selected_range_ = range;

    delete *itr;
    ranges_.erase(itr);
  }

  // Finally, try to complete pending seek if one exists.
  if (seek_pending_)
    Seek(seek_buffer_timestamp_);

  DCHECK(IsRangeListSorted(ranges_));
  return true;
}

void SourceBufferStream::Seek(base::TimeDelta timestamp) {
  if (selected_range_)
    selected_range_ = NULL;

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

bool SourceBufferRange::GetNextBuffer(
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  DCHECK_GE(next_buffer_index_, 0);
  if (next_buffer_index_ >= static_cast<int>(buffers_.size()))
    return false;

  *out_buffer = buffers_.at(next_buffer_index_);
  next_buffer_index_++;
  return true;
}

SourceBufferStream::Timespan
SourceBufferRange::GetBufferedTime() const {
  return std::make_pair(BufferedStart(), BufferedEnd());
}

void SourceBufferRange::AppendRange(const SourceBufferRange& range,
                                    bool transfer_current_position) {
  DCHECK(CanAppendRange(range));
  DCHECK(!buffers_.empty());

  if (transfer_current_position)
    next_buffer_index_ = range.next_buffer_index_ + buffers_.size();

  AppendToEnd(range.buffers_);
}

bool SourceBufferRange::CanAppendRange(const SourceBufferRange& range) const {
  return range.BufferedStart() >= BufferedEnd() &&
      range.BufferedStart() <= MaxNextTimestamp();
}

int SourceBufferRange::BelongsToRange(base::TimeDelta timestamp) const {
  if (buffers_.empty() || MaxNextTimestamp() < timestamp)
    return 1;
  else if (BufferedStart() > timestamp)
    return -1;
  else
    return 0;
}

bool SourceBufferRange::CanSeekTo(base::TimeDelta timestamp) const {
  return !keyframe_map_.empty() && BufferedStart() <= timestamp &&
      BufferedEnd() > timestamp;
}

bool SourceBufferRange::EndOverlaps(const SourceBufferRange& range) const {
  return range.BufferedStart() < BufferedEnd();
}

base::TimeDelta SourceBufferRange::BufferedStart() const {
  if (buffers_.empty())
    return kNoTimestamp();

  return buffers_.front()->GetTimestamp();
}

base::TimeDelta SourceBufferRange::BufferedEnd() const {
  if (buffers_.empty())
    return kNoTimestamp();

  return buffers_.back()->GetTimestamp() + buffers_.back()->GetDuration();
}

base::TimeDelta SourceBufferRange::MaxNextTimestamp() const {
  DCHECK(!buffers_.empty());

  // Because we do not know exactly when is the next timestamp, any buffer
  // that starts within 1/3 of the duration past the end of the last buffer
  // in the queue is considered the next buffer in this range.
  return BufferedEnd() + buffers_.back()->GetDuration() / 3;
}

}  // namespace media
