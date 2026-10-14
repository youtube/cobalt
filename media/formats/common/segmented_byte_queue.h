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

#ifndef MEDIA_FORMATS_COMMON_SEGMENTED_BYTE_QUEUE_H_
#define MEDIA_FORMATS_COMMON_SEGMENTED_BYTE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_span.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"

namespace media {

// A queue of bytes that are not necessarily contiguous in memory. Bytes are
// appended with Push(), removed from the front with Pop(), and read without
// being removed through three access patterns:
//
//   * PeekContiguousData() hands back the run of bytes that is contiguous at a
//     given offset, so a caller that needs contiguous memory can avoid copying
//     altogether whenever what it wants fits inside that run.
//   * PeekSegmentedData() describes a byte range as an ordered list of runs, so
//     data straddling a segment boundary is returned without any copying.
//   * PeekLinearizedData() hands back a byte range as contiguous memory. It
//     reads in place when the range is already contiguous, and otherwise
//     gathers it into scratch storage owned by the queue.
//
// Offsets are relative to the front of the queue: offset 0 is the next unread
// byte. Each Peek method also has an overload without `offset` that reads from
// the front.
//
// Spans returned by the Peek methods are only valid until the next Push(),
// Pop() or Reset(). A span returned by PeekLinearizedData() is also
// invalidated by the next call to PeekLinearizedData().
//
// There are two implementations, and Create() picks one based on
// `borrow_mode`:
//
//   * BorrowedSegmentedByteQueue keeps each append as its own segment pointing
//     directly at the caller's memory, so appending never copies.
//   * OwnedSegmentedByteQueue copies every append into a single buffer, so the
//     whole queue is one segment and every read is contiguous.
//
// Implementations are not thread-safe.
class MEDIA_EXPORT SegmentedByteQueue {
 public:
  // A byte range described as an ordered list of contiguous runs, as returned
  // by PeekSegmentedData().
  using Segments = std::vector<base::span<const uint8_t>>;

  // Returns a BorrowedSegmentedByteQueue when `borrow_mode` is true, and an
  // OwnedSegmentedByteQueue otherwise.
  static std::unique_ptr<SegmentedByteQueue> Create(bool borrow_mode);

  virtual ~SegmentedByteQueue();

  // Empties the queue.
  virtual void Reset() = 0;

  // Appends `data` to the back of the queue.
  //
  // The caller must keep the memory behind `data` valid and unmodified until
  // `release` is destroyed. The queue destroys `release` once it no longer
  // needs that memory, which depends on the implementation:
  //
  //   * BorrowedSegmentedByteQueue reads `data` in place. It keeps `release`
  //     until all of `data` has been popped, or until Reset() or destruction.
  //   * OwnedSegmentedByteQueue copies `data`. It destroys `release` before
  //     returning, so the caller may reuse the memory right away.
  //
  // Returns false, without appending anything, when OwnedSegmentedByteQueue
  // could not allocate memory for the copy. `release` is still destroyed
  // before returning. BorrowedSegmentedByteQueue never fails.
  //
  // Appending an empty span appends nothing, and destroys `release` before
  // returning.
  [[nodiscard]] virtual bool Push(base::span<const uint8_t> data,
                                  base::ScopedClosureRunner release) = 0;

  // Removes `count` bytes from the front of the queue. `count` must not exceed
  // size().
  virtual void Pop(size_t count) = 0;

  // Total number of unread bytes.
  virtual size_t size() const = 0;

  // Returns the run of bytes that is contiguous at `offset`: everything from
  // `offset` to the end of the segment holding it, without copying. Returns an
  // empty span when `offset` is at or past the end of the queue.
  //
  // This is purely an optimization for callers that need contiguous memory. A
  // caller wanting `n` bytes takes the returned run when it is at least `n`
  // long, and otherwise falls back to a slow path such as PeekLinearizedData().
  //
  // Never gate on the length of the run to decide whether enough data has
  // arrived. A caller that treats a short run as "need more data" will stall
  // forever on a range that straddles a boundary, since no future append can
  // make it contiguous. Use PeekSegmentedData() or size() for availability.
  [[nodiscard]] virtual base::span<const uint8_t> PeekContiguousData(
      size_t offset) const = 0;
  [[nodiscard]] base::span<const uint8_t> PeekContiguousData() const {
    return PeekContiguousData(0u);
  }

  // Returns the ordered list of contiguous runs covering the `size` bytes
  // starting at `offset`, or std::nullopt when fewer than `size` bytes are
  // available from `offset`. None of the returned runs is empty, so the list
  // is empty when `size` is zero.
  //
  // Because the range is described as several runs, content straddling a
  // segment boundary is returned in full rather than being truncated at the
  // boundary. A caller deciding whether enough data has arrived should test
  // whether a list is returned (or size()), never the length of an individual
  // run.
  [[nodiscard]] virtual std::optional<Segments> PeekSegmentedData(
      size_t offset,
      size_t size) const = 0;
  [[nodiscard]] std::optional<Segments> PeekSegmentedData(size_t size) const {
    return PeekSegmentedData(0u, size);
  }

  // Returns the `size` bytes starting at `offset` as one contiguous span. When
  // they are already contiguous, the span points into the queue and nothing is
  // copied. Otherwise the bytes are gathered into scratch storage owned by the
  // queue.
  //
  // Returns std::nullopt when fewer than `size` bytes are available from
  // `offset`, or when the scratch storage could not grow. The span is empty
  // when `size` is zero. A caller deciding whether enough data has arrived
  // should test size() first, so that std::nullopt only ever means an
  // allocation failure.
  [[nodiscard]] virtual std::optional<base::span<const uint8_t>>
  PeekLinearizedData(size_t offset, size_t size) = 0;
  [[nodiscard]] std::optional<base::span<const uint8_t>> PeekLinearizedData(
      size_t size) {
    return PeekLinearizedData(0u, size);
  }
};

// A SegmentedByteQueue that borrows appended byte ranges instead of copying
// them, preserving the boundary of each append.
//
// ByteQueue copies every appended buffer into a single growable block so that
// parsers always see contiguous memory. That costs a full copy of every byte
// of media data, and the block only ever grows. BorrowedSegmentedByteQueue
// never copies on append: each append becomes its own segment pointing
// directly at the caller's memory, kept alive by its `release` closure until
// the segment has been entirely popped, or until Reset() or destruction.
// Popping releases whole segments, so the underlying buffers are handed back
// as soon as they have been fully consumed.
//
// The trade-off is that reads are only contiguous within a segment.
// PeekLinearizedData() gathers a range straddling a boundary into scratch
// storage, which is reused across calls: it grows to fit the largest range
// gathered so far, and is only freed by Reset().
//
// Push() always succeeds, and does not invalidate spans returned earlier.
class MEDIA_EXPORT BorrowedSegmentedByteQueue final
    : public SegmentedByteQueue {
 public:
  BorrowedSegmentedByteQueue();

  BorrowedSegmentedByteQueue(const BorrowedSegmentedByteQueue&) = delete;
  BorrowedSegmentedByteQueue& operator=(const BorrowedSegmentedByteQueue&) =
      delete;

  ~BorrowedSegmentedByteQueue() override;

  // SegmentedByteQueue implementation.
  void Reset() override;
  [[nodiscard]] bool Push(base::span<const uint8_t> data,
                          base::ScopedClosureRunner release) override;
  void Pop(size_t count) override;
  size_t size() const override;
  [[nodiscard]] base::span<const uint8_t> PeekContiguousData(
      size_t offset) const override;
  [[nodiscard]] std::optional<Segments> PeekSegmentedData(
      size_t offset,
      size_t size) const override;
  [[nodiscard]] std::optional<base::span<const uint8_t>> PeekLinearizedData(
      size_t offset,
      size_t size) override;

  // Bring back the overloads without `offset`, hidden by the overrides above.
  using SegmentedByteQueue::PeekContiguousData;
  using SegmentedByteQueue::PeekLinearizedData;
  using SegmentedByteQueue::PeekSegmentedData;

  // Number of segments currently held.
  size_t GetSegmentCountForTesting() const { return segments_.size(); }

  // Whether PeekLinearizedData() currently holds scratch storage.
  bool HasScratchForTesting() const { return scratch_.has_value(); }

 private:
  struct BorrowedSegment {
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

  std::vector<BorrowedSegment> segments_;

  // Total number of unread bytes across all segments.
  size_t total_bytes_ = 0u;

  // Scratch storage that PeekLinearizedData() gathers ranges straddling a
  // segment boundary into. Created on first use. ByteQueue::Reset() keeps its
  // storage, so Reset() destroys the ByteQueue to free it.
  std::optional<ByteQueue> scratch_;
};

// A SegmentedByteQueue that copies every append into a single ByteQueue. The
// whole queue is one segment, so PeekContiguousData() returns everything from
// `offset` to the end, PeekSegmentedData() returns at most one run, and
// PeekLinearizedData() never copies.
//
// Push() copies `data` and destroys `release` before returning. It may move
// the queued bytes, so it invalidates every span returned earlier.
class MEDIA_EXPORT OwnedSegmentedByteQueue final : public SegmentedByteQueue {
 public:
  OwnedSegmentedByteQueue();

  OwnedSegmentedByteQueue(const OwnedSegmentedByteQueue&) = delete;
  OwnedSegmentedByteQueue& operator=(const OwnedSegmentedByteQueue&) = delete;

  ~OwnedSegmentedByteQueue() override;

  // SegmentedByteQueue implementation.
  void Reset() override;
  [[nodiscard]] bool Push(base::span<const uint8_t> data,
                          base::ScopedClosureRunner release) override;
  void Pop(size_t count) override;
  size_t size() const override;
  [[nodiscard]] base::span<const uint8_t> PeekContiguousData(
      size_t offset) const override;
  [[nodiscard]] std::optional<Segments> PeekSegmentedData(
      size_t offset,
      size_t size) const override;
  [[nodiscard]] std::optional<base::span<const uint8_t>> PeekLinearizedData(
      size_t offset,
      size_t size) override;

  // Bring back the overloads without `offset`, hidden by the overrides above.
  using SegmentedByteQueue::PeekContiguousData;
  using SegmentedByteQueue::PeekLinearizedData;
  using SegmentedByteQueue::PeekSegmentedData;

 private:
  ByteQueue queue_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_COMMON_SEGMENTED_BYTE_QUEUE_H_
