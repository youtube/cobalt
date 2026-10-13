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

#include "media/formats/common/offset_segmented_byte_queue.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"

namespace media {

// static
std::unique_ptr<OffsetSegmentedByteQueue> OffsetSegmentedByteQueue::Create(
    bool borrow_mode) {
  return base::WrapUnique(
      new OffsetSegmentedByteQueue(SegmentedByteQueue::Create(borrow_mode)));
}

OffsetSegmentedByteQueue::OffsetSegmentedByteQueue(
    std::unique_ptr<SegmentedByteQueue> queue)
    : queue_(std::move(queue)) {
  DCHECK(queue_);
}

OffsetSegmentedByteQueue::~OffsetSegmentedByteQueue() = default;

void OffsetSegmentedByteQueue::Reset() {
  queue_->Reset();
  head_ = 0;
}

bool OffsetSegmentedByteQueue::Push(base::span<const uint8_t> data,
                                    base::ScopedClosureRunner release) {
  return queue_->Push(data, std::move(release));
}

void OffsetSegmentedByteQueue::Pop(size_t count) {
  queue_->Pop(count);
  head_ += base::checked_cast<int64_t>(count);
}

void OffsetSegmentedByteQueue::Trim(int64_t max_offset) {
  if (max_offset < head_) {
    return;
  }
  if (max_offset > tail()) {
    Pop(queue_->size());
    return;
  }
  Pop(OffsetFromHead(max_offset));
}

base::span<const uint8_t> OffsetSegmentedByteQueue::PeekContiguousData(
    int64_t offset,
    int64_t max_offset) const {
  if (!IsReadableOffset(offset) || offset >= max_offset) {
    return {};
  }

  base::span<const uint8_t> run =
      queue_->PeekContiguousData(OffsetFromHead(offset));

  // A readable `offset` is not negative, so the subtraction cannot overflow.
  const uint64_t max_size = static_cast<uint64_t>(max_offset - offset);
  if (run.size() > max_size) {
    run = run.first(static_cast<size_t>(max_size));
  }
  return run;
}

std::optional<SegmentedByteQueue::Segments>
OffsetSegmentedByteQueue::PeekSegmentedData(int64_t offset,
                                            size_t size,
                                            int64_t max_offset) const {
  if (!IsReadableOffset(offset) || !EndsByMaxOffset(offset, size, max_offset)) {
    return std::nullopt;
  }
  return queue_->PeekSegmentedData(OffsetFromHead(offset), size);
}

std::optional<base::span<const uint8_t>>
OffsetSegmentedByteQueue::PeekLinearizedData(int64_t offset,
                                             size_t size,
                                             int64_t max_offset) {
  if (!IsReadableOffset(offset) || !EndsByMaxOffset(offset, size, max_offset)) {
    return std::nullopt;
  }
  return queue_->PeekLinearizedData(OffsetFromHead(offset), size);
}

size_t OffsetSegmentedByteQueue::OffsetFromHead(int64_t offset) const {
  DCHECK(IsReadableOffset(offset));
  return base::checked_cast<size_t>(offset - head_);
}

}  // namespace media
