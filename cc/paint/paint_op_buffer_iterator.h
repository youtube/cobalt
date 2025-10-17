// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_ITERATOR_H_
#define CC_PAINT_PAINT_OP_BUFFER_ITERATOR_H_

#include <iterator>
#include <utility>
#include <variant>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

class PaintOpBufferIteratorBase {
 public:
  using value_type = PaintOp;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::forward_iterator_tag;
};

class CC_PAINT_EXPORT PaintOpBuffer::Iterator
    : public PaintOpBufferIteratorBase {
 public:
  constexpr Iterator() = default;

  explicit Iterator(const PaintOpBuffer& buffer)
      : Iterator(buffer, buffer.data_.get(), 0u) {}

  const PaintOp* get() const { return reinterpret_cast<const PaintOp*>(ptr_); }
  const PaintOp* operator->() const { return get(); }
  const PaintOp& operator*() const { return *get(); }
  Iterator begin() const { return Iterator(*buffer_); }
  Iterator end() const {
    return Iterator(*buffer_,
                    UNSAFE_TODO(buffer_->data_.get() + buffer_->used_),
                    buffer_->used_);
  }
  bool operator==(const Iterator& other) const {
    // Not valid to compare iterators on different buffers.
    DCHECK_EQ(other.buffer_, buffer_);
    return other.op_offset_ == op_offset_;
  }
  bool operator!=(const Iterator& other) const { return !(*this == other); }
  Iterator& operator++() {
    DCHECK(*this);
    const PaintOp& op = **this;
    UNSAFE_TODO(ptr_ += op.AlignedSize());
    op_offset_ += op.AlignedSize();

    CHECK_LE(op_offset_, buffer_->used_);
    return *this;
  }
  Iterator operator++(int) {
    Iterator original = *this;
    operator++();
    return original;
  }
  explicit operator bool() const { return op_offset_ < buffer_->used_; }

 private:
  Iterator(const PaintOpBuffer& buffer, const char* ptr, size_t op_offset)
      : buffer_(&buffer), ptr_(ptr), op_offset_(op_offset) {}

  // `buffer_` and `ptr_` are not a raw_ptr<...> for performance reasons
  // (based on analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION const PaintOpBuffer* buffer_ = nullptr;
  RAW_PTR_EXCLUSION const char* ptr_ = nullptr;

  size_t op_offset_ = 0;
};

class CC_PAINT_EXPORT PaintOpBuffer::OffsetIterator
    : public PaintOpBufferIteratorBase {
 public:
  // Offsets and paint op buffer must come from the same DisplayItemList.
  OffsetIterator(const PaintOpBuffer& buffer,
                 const std::vector<size_t>& offsets)
      : buffer_(&buffer), ptr_(buffer_->data_.get()), offsets_(&offsets) {
    if (offsets.empty()) {
      *this = end();
      return;
    }
    op_offset_ = offsets[0];
    UNSAFE_TODO(ptr_ += op_offset_);
  }

  const PaintOp* get() const { return reinterpret_cast<const PaintOp*>(ptr_); }
  const PaintOp* operator->() const { return get(); }
  const PaintOp& operator*() const { return *get(); }
  OffsetIterator begin() const { return OffsetIterator(*buffer_, *offsets_); }
  OffsetIterator end() const {
    return OffsetIterator(*buffer_,
                          UNSAFE_TODO(buffer_->data_.get() + buffer_->used_),
                          buffer_->used_, *offsets_);
  }
  bool operator==(const OffsetIterator& other) const {
    // Not valid to compare iterators on different buffers.
    DCHECK_EQ(other.buffer_, buffer_);
    return other.op_offset_ == op_offset_;
  }
  bool operator!=(const OffsetIterator& other) const {
    return !(*this == other);
  }
  OffsetIterator& operator++() {
    if (++offsets_index_ >= offsets_->size()) {
      *this = end();
      return *this;
    }

    size_t target_offset = (*offsets_)[offsets_index_];
    // Sanity checks.
    CHECK_GE(target_offset, op_offset_);
    // Debugging crbug.com/738182.
    base::debug::Alias(&target_offset);
    CHECK_LT(target_offset, buffer_->used_);

    // Advance the iterator to the target offset.
    UNSAFE_TODO(ptr_ += (target_offset - op_offset_));
    op_offset_ = target_offset;

    DCHECK(!*this || (*this)->type <=
                         static_cast<uint32_t>(PaintOpType::kLastPaintOpType));
    return *this;
  }
  OffsetIterator operator++(int) {
    OffsetIterator original = *this;
    operator++();
    return original;
  }

  explicit operator bool() const { return op_offset_ < buffer_->used_; }

 private:
  OffsetIterator(const PaintOpBuffer& buffer,
                 const char* ptr,
                 size_t op_offset,
                 const std::vector<size_t>& offsets)
      : buffer_(&buffer),
        ptr_(ptr),
        offsets_(&offsets),
        op_offset_(op_offset) {}

  // `buffer_`, `ptr_`, and `offsets_` are not a raw_ptr<...> for performance
  // reasons (based on analysis of sampling profiler data and
  // tab_search:top100:2020).
  RAW_PTR_EXCLUSION const PaintOpBuffer* buffer_ = nullptr;
  RAW_PTR_EXCLUSION const char* ptr_ = nullptr;
  RAW_PTR_EXCLUSION const std::vector<size_t>* offsets_;

  size_t op_offset_ = 0;
  size_t offsets_index_ = 0;
};

class CC_PAINT_EXPORT PaintOpBuffer::CompositeIterator
    : public PaintOpBufferIteratorBase {
 public:
  // Offsets and paint op buffer must come from the same DisplayItemList.
  CompositeIterator(const PaintOpBuffer& buffer,
                    const std::vector<size_t>* offsets);
  CompositeIterator(const CompositeIterator& other);
  CompositeIterator(CompositeIterator&& other);

  const PaintOp* get() const {
    return std::visit([](const auto& iter) { return iter.get(); }, iter_);
  }
  const PaintOp* operator->() const { return get(); }
  const PaintOp& operator*() const { return *get(); }
  CompositeIterator begin() const {
    return std::holds_alternative<Iterator>(iter_)
               ? CompositeIterator(std::get<Iterator>(iter_).begin())
               : CompositeIterator(std::get<OffsetIterator>(iter_).begin());
  }
  CompositeIterator end() const {
    return std::holds_alternative<Iterator>(iter_)
               ? CompositeIterator(std::get<Iterator>(iter_).end())
               : CompositeIterator(std::get<OffsetIterator>(iter_).end());
  }
  bool operator==(const CompositeIterator& other) const {
    return iter_ == other.iter_;
  }
  bool operator!=(const CompositeIterator& other) const {
    return !(*this == other);
  }
  CompositeIterator& operator++() {
    std::visit([](auto& iter) { ++iter; }, iter_);
    return *this;
  }
  CompositeIterator operator++(int) {
    CompositeIterator original = *this;
    operator++();
    return original;
  }
  explicit operator bool() const {
    return std::visit([](const auto& iter) { return !!iter; }, iter_);
  }

 private:
  explicit CompositeIterator(OffsetIterator iter) : iter_(std::move(iter)) {}
  explicit CompositeIterator(Iterator iter) : iter_(std::move(iter)) {}

  std::variant<Iterator, OffsetIterator> iter_;
};

class CC_PAINT_EXPORT PaintOpBuffer::PlaybackFoldingIterator
    : public PaintOpBufferIteratorBase {
 public:
  PlaybackFoldingIterator(const PaintOpBuffer& buffer,
                          const std::vector<size_t>* offsets);
  ~PlaybackFoldingIterator();

  const PaintOp* get() const { return current_op_; }
  const PaintOp* operator->() const { return current_op_; }
  const PaintOp& operator*() const { return *current_op_; }

  PlaybackFoldingIterator& operator++() {
    FindNextOp();
    return *this;
  }

  explicit operator bool() const { return !!current_op_; }

  // Guaranteed to be 1.0f for all ops without flags.
  float alpha() const { return current_alpha_; }

 private:
  void FindNextOp();
  const PaintOp* NextUnfoldedOp();

  PaintOpBuffer::CompositeIterator iter_;

  // FIFO queue of paint ops that have been peeked at.
  absl::InlinedVector<const PaintOp*, 3> stack_;
  DrawColorOp folded_draw_color_;

  // `current_op_` is not a raw_ptr<...> for performance reasons (based on
  // analysis of sampling profiler data and tab_search:top100:2020).
  RAW_PTR_EXCLUSION const PaintOp* current_op_ = nullptr;

  float current_alpha_ = 1.0f;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_ITERATOR_H_
