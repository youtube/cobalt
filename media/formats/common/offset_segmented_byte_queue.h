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

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "media/base/media_export.h"
#include "media/formats/common/segmented_byte_queue.h"

namespace media {

// A SegmentedByteQueue wrapper that addresses data by monotonically increasing
// absolute stream offsets, in the same spirit as OffsetByteQueue.
//
// The API mirrors SegmentedByteQueue, with two differences. First, every
// `offset` is an absolute stream offset rather than one relative to the front
// of the queue. Offsets therefore have no default, and a read before head()
// fails rather than silently resolving to the oldest buffered byte. Second,
// every Peek method takes `max_offset`, an absolute exclusive bound that the
// read never reaches. This serves parsers that limit how far into the buffered
// data they may look, such as MP4StreamParser with its `max_parse_offset_`.
// Callers without such a limit can pass tail().
//
// Relative to OffsetByteQueue there is one semantic difference worth calling
// out. In borrow mode each append stays its own segment, so the bytes at an
// offset are not necessarily contiguous. Never gate on the length of the run
// returned by PeekContiguousData() to decide whether enough data has arrived:
// a caller doing so would stall forever on a range straddling a segment
// boundary. Decide with tail() or `max_offset` instead, then read across
// boundaries with PeekSegmentedData() or PeekLinearizedData().
//
// Spans handed out by this class are invalidated by the next Push(), Pop(),
// Trim() or Reset(). A span returned by PeekLinearizedData() is also
// invalidated by the next call to PeekLinearizedData().
//
// This class is not thread-safe.
class MEDIA_EXPORT OffsetSegmentedByteQueue {
 public:
  // Returns a queue backed by SegmentedByteQueue::Create(borrow_mode).
  static std::unique_ptr<OffsetSegmentedByteQueue> Create(bool borrow_mode);

  OffsetSegmentedByteQueue(const OffsetSegmentedByteQueue&) = delete;
  OffsetSegmentedByteQueue& operator=(const OffsetSegmentedByteQueue&) = delete;

  ~OffsetSegmentedByteQueue();

  // These work like their SegmentedByteQueue counterparts. Reset() also returns
  // the head to offset 0.
  void Reset();
  [[nodiscard]] bool Push(base::span<const uint8_t> data,
                          base::ScopedClosureRunner release);
  void Pop(size_t count);
  size_t size() const { return queue_->size(); }

  // The head and tail positions in absolute stream offsets. tail() is an
  // exclusive bound.
  int64_t head() const { return head_; }
  int64_t tail() const { return head_ + static_cast<int64_t>(queue_->size()); }

  // Pops the bytes before `max_offset`. Does nothing if `max_offset` is at or
  // before head(), and pops everything if it is beyond tail().
  void Trim(int64_t max_offset);

  // Like their SegmentedByteQueue counterparts, except that `offset` is
  // absolute and nothing at or past `max_offset` is read. PeekContiguousData()
  // cuts the run short at `max_offset`, so it is empty when `offset` is at or
  // past it. PeekSegmentedData() and PeekLinearizedData() treat a range ending
  // past `max_offset` like one that is not buffered.
  //
  // A read starting before head() fails like a read past tail():
  // PeekContiguousData() returns an empty span, and PeekSegmentedData() and
  // PeekLinearizedData() return std::nullopt.
  [[nodiscard]] base::span<const uint8_t> PeekContiguousData(
      int64_t offset,
      int64_t max_offset) const;
  [[nodiscard]] std::optional<SegmentedByteQueue::Segments>
  PeekSegmentedData(int64_t offset, size_t size, int64_t max_offset) const;
  [[nodiscard]] std::optional<base::span<const uint8_t>>
  PeekLinearizedData(int64_t offset, size_t size, int64_t max_offset);

 private:
  explicit OffsetSegmentedByteQueue(std::unique_ptr<SegmentedByteQueue> queue);

  // True if `offset` addresses readable data. The tail itself counts, so that
  // zero-length requests there resolve; anything before head() does not, since
  // those bytes have already been popped.
  bool IsReadableOffset(int64_t offset) const {
    return offset >= head_ && offset <= tail();
  }

  // True if the `size` bytes starting at `offset` end at or before
  // `max_offset`. `offset` must satisfy IsReadableOffset(), which keeps it from
  // being negative, so the subtraction cannot overflow.
  static bool EndsByMaxOffset(int64_t offset, size_t size, int64_t max_offset) {
    return offset <= max_offset &&
           size <= static_cast<uint64_t>(max_offset - offset);
  }

  // Converts an absolute offset into one relative to the front of `queue_`.
  // `offset` must satisfy IsReadableOffset().
  size_t OffsetFromHead(int64_t offset) const;

  const std::unique_ptr<SegmentedByteQueue> queue_;

  // Absolute stream offset of the first buffered byte.
  int64_t head_ = 0;
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_OFFSET_SEGMENTED_BYTE_QUEUE_H_
