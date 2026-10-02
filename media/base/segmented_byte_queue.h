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

#ifndef MEDIA_BASE_SEGMENTED_BYTE_QUEUE_H_
#define MEDIA_BASE_SEGMENTED_BYTE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_span.h"
#include "media/base/media_export.h"

namespace media {

// A queue of borrowed byte ranges that preserves the boundary of each append
// instead of coalescing everything into one contiguous allocation.
//
// ByteQueue copies every appended buffer into a single growable block so that
// parsers always see contiguous memory. That costs a full copy of every byte
// of media data, and the block only ever grows. SegmentedByteQueue never
// copies on append: each append becomes its own segment pointing directly at
// the caller's memory, kept alive by a retention closure. Popping releases
// whole segments, so the underlying buffers are handed back as soon as they
// have been fully consumed.
//
// The trade-off is that reads are no longer contiguous. Callers choose between
// two access patterns:
//
//   * GetContiguousData() hands back the run of bytes that is contiguous at a
//     given offset, so a caller that needs contiguous memory can avoid copying
//     altogether whenever what it wants fits inside that run.
//   * GetSegmentedData() describes a byte range as an ordered list of runs, so
//     data straddling a segment boundary is returned without any copying. It
//     is the fallback for the cases GetContiguousData() cannot serve.
//
// Offsets are relative to the front of the queue: offset 0 is the next unread
// byte.
//
// This class is not thread-safe.
class MEDIA_EXPORT SegmentedByteQueue {
 public:
  SegmentedByteQueue();

  SegmentedByteQueue(const SegmentedByteQueue&) = delete;
  SegmentedByteQueue& operator=(const SegmentedByteQueue&) = delete;

  ~SegmentedByteQueue();

  // Resets the queue to empty and releases every segment, running each
  // segment's retention closure.
  void Reset();

  // Appends `data` as a new segment *without copying it*. `release` is
  // destroyed once the segment has been entirely popped, or on Reset() or
  // destruction, and must keep `data`'s memory valid until that point.
  //
  // The caller must not mutate, resize, or free the memory behind `data` while
  // the segment remains queued. Appending an empty span is a no-op, and
  // `release` is destroyed immediately in that case.
  void Push(base::span<const uint8_t> data, base::ScopedClosureRunner release);

  // Removes `count` bytes from the front of the queue. `count` must not exceed
  // size(). Segments that become fully consumed are released here.
  void Pop(size_t count);

  // Total number of unread bytes across all segments.
  size_t size() const { return total_bytes_; }

  // Returns the run of bytes that is contiguous at `offset`: everything from
  // `offset` to the end of the segment holding it, without copying. Returns an
  // empty span when `offset` is at or past the end of the queue.
  //
  // This is purely an optimization for callers that need contiguous memory. A
  // caller wanting `n` bytes takes the returned run when it is at least `n`
  // long, and otherwise falls back to a slow path, normally gathering the
  // GetSegmentedData() runs into its own storage.
  //
  // Never gate on the length of the run to decide whether enough data has
  // arrived. A caller that treats a short run as "need more data" will stall
  // forever on a range that straddles a boundary, since no future append can
  // make it contiguous. Use GetSegmentedData() or size() for availability.
  //
  // The returned span is only valid until the next Pop() or Reset(). Push()
  // does not invalidate it.
  [[nodiscard]] base::span<const uint8_t> GetContiguousData(
      size_t offset = 0u) const;

  // Replaces the contents of `*segments` with the ordered list of contiguous
  // runs covering the `size` bytes starting at `offset`, and returns true.
  // Returns false without touching `*segments` when fewer than `size` bytes
  // are available from `offset`. None of the returned runs is empty.
  //
  // Because the range is described as several runs, content straddling a
  // segment boundary is returned in full rather than being truncated at the
  // boundary. A caller deciding whether enough data has arrived should test
  // this return value (or size()), never the length of an individual run.
  //
  // The returned segments are only valid until the next Pop() or Reset().
  // Push() does not invalidate them.
  [[nodiscard]] bool GetSegmentedData(
      std::vector<base::span<const uint8_t>>* segments,
      size_t size,
      size_t offset = 0u) const;

  // Number of segments currently held.
  size_t GetSegmentCountForTesting() const { return segments_.size(); }

 private:
  struct Segment {
    // Declaration order is load-bearing: members are destroyed in reverse
    // order, so `data` must come last to be destroyed before the `release`
    // that frees the memory it points at. Otherwise it briefly references
    // released memory, which BackupRefPtr can flag.
    base::ScopedClosureRunner release;
    base::raw_span<const uint8_t> data;
  };

  // Maps `offset`, relative to the front of the queue, to the index of the
  // segment holding it and the offset within that segment. `offset` must be
  // less than `total_bytes_`.
  std::pair<size_t, size_t> Locate(size_t offset) const;

  std::vector<Segment> segments_;

  // Total number of unread bytes across all segments.
  size_t total_bytes_ = 0u;
};

}  // namespace media

#endif  // MEDIA_BASE_SEGMENTED_BYTE_QUEUE_H_
