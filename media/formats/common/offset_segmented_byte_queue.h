// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIA_FORMATS_COMMON_OFFSET_SEGMENTED_BYTE_QUEUE_H_
#define MEDIA_FORMATS_COMMON_OFFSET_SEGMENTED_BYTE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "media/base/media_export.h"
#include "media/base/segmented_byte_queue.h"

namespace media {

// A SegmentedByteQueue wrapper that addresses data by monotonically increasing
// absolute stream offsets, in the same spirit as OffsetByteQueue.
//
// The API mirrors SegmentedByteQueue, with one difference: every `offset` is an
// absolute stream offset rather than one relative to the front of the queue.
// Offsets therefore have no default, and a read before head() fails rather than
// silently resolving to the oldest buffered byte.
//
// Relative to OffsetByteQueue there is one semantic difference worth calling
// out. Because the underlying queue borrows each append instead of coalescing
// appends into one block, the bytes at an offset are not necessarily
// contiguous. There is deliberately no "how many contiguous bytes are here"
// accessor to gate on, since a caller doing so would stall forever on an
// element straddling a segment boundary. Use tail() to decide whether enough
// data has arrived, then GetSegmentedData() to read across boundaries without
// copying.
// GetContiguousData() lets a caller that needs contiguous memory skip the
// gather when the run available at an offset is already long enough, but it is
// an optimization only and is subject to the same warning: a short run must
// never be read as "need more data".
//
// Spans handed out by this class are invalidated by the next Pop(), Trim() or
// Reset(). Push() does not invalidate them, since it only ever adds segments.
//
// This class is not thread-safe.
class MEDIA_EXPORT OffsetSegmentedByteQueue {
 public:
  OffsetSegmentedByteQueue();

  OffsetSegmentedByteQueue(const OffsetSegmentedByteQueue&) = delete;
  OffsetSegmentedByteQueue& operator=(const OffsetSegmentedByteQueue&) = delete;

  ~OffsetSegmentedByteQueue();

  // These work like their SegmentedByteQueue counterparts. Reset() also returns
  // the head to offset 0.
  void Reset();
  void Push(base::span<const uint8_t> data, base::ScopedClosureRunner release);
  void Pop(size_t count);
  size_t size() const { return queue_.size(); }

  // The head and tail positions in absolute stream offsets. tail() is an
  // exclusive bound.
  int64_t head() const { return head_; }
  int64_t tail() const { return head_ + static_cast<int64_t>(queue_.size()); }

  // Marks bytes up to (but not including) `max_offset` as no longer needed.
  // Unlike OffsetByteQueue::Trim(), this genuinely releases the memory of any
  // segment that becomes fully consumed.
  //
  // Returns true if the full range was trimmed, including when `max_offset` is
  // at or before the current head. Returns false if `max_offset` is beyond
  // tail(), in which case everything currently buffered is still dropped.
  bool Trim(int64_t max_offset);

  // Like SegmentedByteQueue::GetContiguousData(), except `offset` is absolute.
  // Returns an empty span when `offset` is not readable, i.e. before head() or
  // at or past tail().
  //
  // Read that class's comment before using this: a run shorter than what the
  // caller wants means "take the slow path", never "wait for more data".
  [[nodiscard]] base::span<const uint8_t> GetContiguousData(
      int64_t offset) const;

  // Like SegmentedByteQueue::GetSegmentedData(), except `offset` is absolute.
  // Returns false without touching `*segments` when the requested range is not
  // entirely buffered, which includes any `offset` before head().
  [[nodiscard]] bool GetSegmentedData(
      std::vector<base::span<const uint8_t>>* segments,
      size_t size,
      int64_t offset) const;

  // Number of segments currently held.
  size_t GetSegmentCountForTesting() const {
    return queue_.GetSegmentCountForTesting();
  }

 private:
  // True if `offset` addresses readable data. The tail itself counts, so that
  // zero-length requests there resolve; anything before head() does not, since
  // those bytes have already been released.
  bool IsReadableOffset(int64_t offset) const {
    return offset >= head_ && offset <= tail();
  }

  // Converts an absolute offset into one relative to the front of `queue_`.
  // `offset` must satisfy IsReadableOffset().
  size_t OffsetFromHead(int64_t offset) const;

  SegmentedByteQueue queue_;

  // Absolute stream offset of the first buffered byte.
  int64_t head_ = 0;
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_OFFSET_SEGMENTED_BYTE_QUEUE_H_
