/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_CONTAINERS_ROW_MAP_H_
#define SRC_TRACE_PROCESSOR_CONTAINERS_ROW_MAP_H_

<<<<<<< HEAD
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "perfetto/base/compiler.h"
#include "perfetto/base/logging.h"
#include "src/trace_processor/containers/bit_vector.h"
=======
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "perfetto/base/logging.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/containers/bit_vector_iterators.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

namespace perfetto {
namespace trace_processor {

<<<<<<< HEAD
// Stores a list of row indices in a space efficient manner. One or more
=======
// Stores a list of row indicies in a space efficient manner. One or more
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
// columns can refer to the same RowMap. The RowMap defines the access pattern
// to iterate on rows.
//
// Naming convention:
//
// As both the input and output of RowMap is a uint32_t, it can be quite
// confusing to reason about what parameters/return values of the functions
// of RowMap actually means. To help with this, we define a strict convention
// of naming.
//
// row:     input - that is, rows are what are passed into operator[]; named as
//          such because a "row" number in a table is converted to an index to
//          lookup in the backing vectors.
// index:   output - that is, indices are what are returned from operator[];
//          named as such because an "index" is what's used to lookup data
//          from the backing vectors.
//
// Implementation details:
//
<<<<<<< HEAD
// Behind the scenes, this class is implemented using one of three backing
=======
// Behind the scenes, this class is impelemented using one of three backing
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
// data-structures:
// 1. A start and end index (internally named 'range')
// 1. BitVector
// 2. std::vector<uint32_t> (internally named IndexVector).
//
// Generally the preference for data structures is range > BitVector >
// std::vector<uint32>; this ordering is based mainly on memory efficiency as we
// expect RowMaps to be large.
//
// However, BitVector and std::vector<uint32_t> allow things which are not
// possible with the data-structures preferred to them:
//  * a range (as the name suggests) can only store a compact set of indices
//  with no holes. A BitVector works around this limitation by storing a 1 at an
//  index where that row is part of the RowMap and 0 otherwise.
//  * as soon as ordering or duplicate rows come into play, we cannot use a
//   BitVector anymore as ordering/duplicate row information cannot be captured
//   by a BitVector.
//
// For small, sparse RowMaps, it is possible that a std::vector<uint32_t> is
// more efficient than a BitVector; in this case, we will make a best effort
// switch to it but the cases where this happens is not precisely defined.
class RowMap {
<<<<<<< HEAD
 public:
  using InputRow = uint32_t;
  using OutputIndex = uint32_t;
  using IndexVector = std::vector<OutputIndex>;

  struct Range {
    Range(OutputIndex start_index, OutputIndex end_index)
        : start(start_index), end(end_index) {
      PERFETTO_DCHECK(start_index <= end_index);
    }
    Range() : start(0), end(0) {}

    OutputIndex start;  // This is an inclusive index.
    OutputIndex end;    // This is an exclusive index.

    bool empty() const { return size() == 0; }
    uint32_t size() const { return end - start; }
    inline bool Contains(uint32_t val) const {
      return val >= start && val < end;
    }
  };
=======
 private:
  // We need to declare these iterator classes before RowMap::Iterator as it
  // depends on them. However, we don't want to make these public so keep them
  // under a special private section.

  // Iterator for ranged mode of RowMap.
  // This class should act as a drop-in replacement for
  // BitVector::SetBitsIterator.
  class RangeIterator {
   public:
    RangeIterator(const RowMap* rm) : rm_(rm), index_(rm->start_index_) {}

    void Next() { ++index_; }

    operator bool() const { return index_ < rm_->end_index_; }

    uint32_t index() const { return index_; }

    uint32_t ordinal() const { return index_ - rm_->start_index_; }

   private:
    const RowMap* rm_ = nullptr;
    uint32_t index_ = 0;
  };

  // Iterator for index vector mode of RowMap.
  // This class should act as a drop-in replacement for
  // BitVector::SetBitsIterator.
  class IndexVectorIterator {
   public:
    IndexVectorIterator(const RowMap* rm) : rm_(rm) {}

    void Next() { ++ordinal_; }

    operator bool() const { return ordinal_ < rm_->index_vector_.size(); }

    uint32_t index() const { return rm_->index_vector_[ordinal_]; }

    uint32_t ordinal() const { return ordinal_; }

   private:
    const RowMap* rm_ = nullptr;
    uint32_t ordinal_ = 0;
  };

 public:
  // Input type.
  using InputRow = uint32_t;
  using OutputIndex = uint32_t;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Allows efficient iteration over the rows of a RowMap.
  //
  // Note: you should usually prefer to use the methods on RowMap directly (if
  // they exist for the task being attempted) to avoid the lookup for the mode
  // of the RowMap on every method call.
  class Iterator {
   public:
<<<<<<< HEAD
    explicit Iterator(const RowMap* rm);

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
=======
    Iterator(const RowMap* rm) : rm_(rm) {
      switch (rm->mode_) {
        case Mode::kRange:
          range_it_.reset(new RangeIterator(rm));
          break;
        case Mode::kBitVector:
          set_bits_it_.reset(
              new BitVector::SetBitsIterator(rm->bit_vector_.IterateSetBits()));
          break;
        case Mode::kIndexVector:
          iv_it_.reset(new IndexVectorIterator(rm));
          break;
      }
    }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) = default;

    // Forwards the iterator to the next row of the RowMap.
<<<<<<< HEAD
    void Next() { ++ordinal_; }

    // Returns if the iterator is still valid.
    explicit operator bool() const {
      if (const auto* range = std::get_if<Range>(&rm_->data_)) {
        return ordinal_ < range->end;
      }
      if (std::get_if<BitVector>(&rm_->data_)) {
        return ordinal_ < results_.size();
      }
      if (const auto* vec = std::get_if<IndexVector>(&rm_->data_)) {
        return ordinal_ < vec->size();
      }
      PERFETTO_FATAL("Didn't match any variant type.");
=======
    void Next() {
      switch (rm_->mode_) {
        case Mode::kRange:
          range_it_->Next();
          break;
        case Mode::kBitVector:
          set_bits_it_->Next();
          break;
        case Mode::kIndexVector:
          iv_it_->Next();
          break;
      }
    }

    // Returns if the iterator is still valid.
    operator bool() const {
      switch (rm_->mode_) {
        case Mode::kRange:
          return *range_it_;
        case Mode::kBitVector:
          return *set_bits_it_;
        case Mode::kIndexVector:
          return *iv_it_;
      }
      PERFETTO_FATAL("For GCC");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    }

    // Returns the index pointed to by this iterator.
    OutputIndex index() const {
<<<<<<< HEAD
      if (std::get_if<Range>(&rm_->data_)) {
        return ordinal_;
      }
      if (std::get_if<BitVector>(&rm_->data_)) {
        return results_[ordinal_];
      }
      if (const auto* vec = std::get_if<IndexVector>(&rm_->data_)) {
        return (*vec)[ordinal_];
      }
      PERFETTO_FATAL("Didn't match any variant type.");
=======
      switch (rm_->mode_) {
        case Mode::kRange:
          return range_it_->index();
        case Mode::kBitVector:
          return set_bits_it_->index();
        case Mode::kIndexVector:
          return iv_it_->index();
      }
      PERFETTO_FATAL("For GCC");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    }

    // Returns the row of the index the iterator points to.
    InputRow row() const {
<<<<<<< HEAD
      if (const auto* range = std::get_if<Range>(&rm_->data_)) {
        return ordinal_ - range->start;
      }
      if (std::get_if<BitVector>(&rm_->data_) ||
          std::get_if<IndexVector>(&rm_->data_)) {
        return ordinal_;
      }
      PERFETTO_FATAL("Didn't match any variant type.");
    }

   private:
    // Ordinal will not be used for BitVector based RowMap.
    uint32_t ordinal_ = 0;
    // Not empty for BitVector based RowMap.
    std::vector<uint32_t> results_;
=======
      switch (rm_->mode_) {
        case Mode::kRange:
          return range_it_->ordinal();
        case Mode::kBitVector:
          return set_bits_it_->ordinal();
        case Mode::kIndexVector:
          return iv_it_->ordinal();
      }
      PERFETTO_FATAL("For GCC");
    }

   private:
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    // Only one of the below will be non-null depending on the mode of the
    // RowMap.
    std::unique_ptr<RangeIterator> range_it_;
    std::unique_ptr<BitVector::SetBitsIterator> set_bits_it_;
    std::unique_ptr<IndexVectorIterator> iv_it_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    const RowMap* rm_ = nullptr;
  };

<<<<<<< HEAD
=======
  // Enum to allow users of RowMap to decide whether they want to optimize for
  // memory usage or for speed of lookups.
  enum class OptimizeFor {
    kMemory,
    kLookupSpeed,
  };

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // Creates an empty RowMap.
  // By default this will be implemented using a range.
  RowMap();

  // Creates a RowMap containing the range of indices between |start| and |end|
  // i.e. all indices between |start| (inclusive) and |end| (exclusive).
<<<<<<< HEAD
  RowMap(OutputIndex start, OutputIndex end);

  // Creates a RowMap backed by a BitVector.
  explicit RowMap(BitVector);

  // Creates a RowMap backed by an std::vector<uint32_t>.
  explicit RowMap(IndexVector);
=======
  explicit RowMap(OutputIndex start,
                  OutputIndex end,
                  OptimizeFor optimize_for = OptimizeFor::kMemory);

  // Creates a RowMap backed by a BitVector.
  explicit RowMap(BitVector bit_vector);

  // Creates a RowMap backed by an std::vector<uint32_t>.
  explicit RowMap(std::vector<OutputIndex> vec);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  RowMap(const RowMap&) noexcept = delete;
  RowMap& operator=(const RowMap&) = delete;

  RowMap(RowMap&&) noexcept = default;
  RowMap& operator=(RowMap&&) = default;

  // Creates a RowMap containing just |index|.
  // By default this will be implemented using a range.
  static RowMap SingleRow(OutputIndex index) {
    return RowMap(index, index + 1);
  }

  // Creates a copy of the RowMap.
  // We have an explicit copy function because RowMap can hold onto large chunks
  // of memory and we want to be very explicit when making a copy to avoid
  // accidental leaks and copies.
  RowMap Copy() const;

  // Returns the size of the RowMap; that is the number of indices in the
  // RowMap.
  uint32_t size() const {
<<<<<<< HEAD
    if (const auto* range = std::get_if<Range>(&data_)) {
      return range->size();
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return bv->CountSetBits();
    }
    if (const auto* vec = std::get_if<IndexVector>(&data_)) {
      return static_cast<uint32_t>(vec->size());
    }
    NoVariantMatched();
=======
    switch (mode_) {
      case Mode::kRange:
        return end_index_ - start_index_;
      case Mode::kBitVector:
        return bit_vector_.CountSetBits();
      case Mode::kIndexVector:
        return static_cast<uint32_t>(index_vector_.size());
    }
    PERFETTO_FATAL("For GCC");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  // Returns whether this rowmap is empty.
  bool empty() const { return size() == 0; }

  // Returns the index at the given |row|.
  OutputIndex Get(InputRow row) const {
<<<<<<< HEAD
    if (const auto* range = std::get_if<Range>(&data_)) {
      return GetRange(*range, row);
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return GetBitVector(*bv, row);
    }
    if (const auto* vec = std::get_if<IndexVector>(&data_)) {
      return GetIndexVector(*vec, row);
    }
    NoVariantMatched();
  }

  // Returns the vector of all indices in the RowMap.
  std::vector<OutputIndex> GetAllIndices() const {
    if (const auto* range = std::get_if<Range>(&data_)) {
      std::vector<uint32_t> res(range->size());
      std::iota(res.begin(), res.end(), range->start);
      return res;
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return bv->GetSetBitIndices();
    }
    if (const auto* vec = std::get_if<IndexVector>(&data_)) {
      return *vec;
    }
    NoVariantMatched();
  }

  // Returns maximum size of the output. Ie range.end or size of the BV.
  OutputIndex Max() const;

  // Returns whether the RowMap contains the given index.
  bool Contains(OutputIndex index) const {
    if (const auto* range = std::get_if<Range>(&data_)) {
      return index >= range->start && index < range->end;
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return index < bv->size() && bv->IsSet(index);
    }
    if (const auto* vec = std::get_if<IndexVector>(&data_)) {
      return std::find(vec->begin(), vec->end(), index) != vec->end();
    }
    NoVariantMatched();
=======
    PERFETTO_DCHECK(row < size());
    switch (mode_) {
      case Mode::kRange:
        return GetRange(row);
      case Mode::kBitVector:
        return GetBitVector(row);
      case Mode::kIndexVector:
        return GetIndexVector(row);
    }
    PERFETTO_FATAL("For GCC");
  }

  // Returns whether the RowMap contains the given index.
  bool Contains(OutputIndex index) const {
    switch (mode_) {
      case Mode::kRange: {
        return index >= start_index_ && index < end_index_;
      }
      case Mode::kBitVector: {
        return index < bit_vector_.size() && bit_vector_.IsSet(index);
      }
      case Mode::kIndexVector: {
        auto it = std::find(index_vector_.begin(), index_vector_.end(), index);
        return it != index_vector_.end();
      }
    }
    PERFETTO_FATAL("For GCC");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  // Returns the first row of the given |index| in the RowMap.
  std::optional<InputRow> RowOf(OutputIndex index) const {
<<<<<<< HEAD
    if (const auto* range = std::get_if<Range>(&data_)) {
      if (index < range->start || index >= range->end)
        return std::nullopt;
      return index - range->start;
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return index < bv->size() && bv->IsSet(index)
                 ? std::make_optional(bv->CountSetBits(index))
                 : std::nullopt;
    }
    if (const auto* vec = std::get_if<IndexVector>(&data_)) {
      auto it = std::find(vec->begin(), vec->end(), index);
      return it != vec->end() ? std::make_optional(static_cast<InputRow>(
                                    std::distance(vec->begin(), it)))
                              : std::nullopt;
    }
    NoVariantMatched();
=======
    switch (mode_) {
      case Mode::kRange: {
        if (index < start_index_ || index >= end_index_)
          return std::nullopt;
        return index - start_index_;
      }
      case Mode::kBitVector: {
        return index < bit_vector_.size() && bit_vector_.IsSet(index)
                   ? std::make_optional(bit_vector_.CountSetBits(index))
                   : std::nullopt;
      }
      case Mode::kIndexVector: {
        auto it = std::find(index_vector_.begin(), index_vector_.end(), index);
        return it != index_vector_.end()
                   ? std::make_optional(static_cast<InputRow>(
                         std::distance(index_vector_.begin(), it)))
                   : std::nullopt;
      }
    }
    PERFETTO_FATAL("For GCC");
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  // Performs an ordered insert of the index into the current RowMap
  // (precondition: this RowMap is ordered based on the indices it contains).
  //
  // Example:
  // this = [1, 5, 10, 11, 20]
  // Insert(10)  // this = [1, 5, 10, 11, 20]
  // Insert(12)  // this = [1, 5, 10, 11, 12, 20]
  // Insert(21)  // this = [1, 5, 10, 11, 12, 20, 21]
  // Insert(2)   // this = [1, 2, 5, 10, 11, 12, 20, 21]
  //
  // Speecifically, this means that it is only valid to call Insert on a RowMap
  // which is sorted by the indices it contains; this is automatically true when
  // the RowMap is in range or BitVector mode but is a required condition for
  // IndexVector mode.
  void Insert(OutputIndex index) {
<<<<<<< HEAD
    if (auto* range = std::get_if<Range>(&data_)) {
      if (index == range->end) {
        // Fast path: if we're just appending to the end
        // of the range, we can stay in range mode and
        // just bump the end index.
        range->end++;
        return;
      }

      // Slow path: the insert is somewhere else other
      // than the end. This means we need to switch to
      // using a BitVector instead.
      BitVector bv;
      bv.Resize(range->start, false);
      bv.Resize(range->end, true);
      InsertIntoBitVector(bv, index);
      data_ = std::move(bv);
      return;
    }
    if (auto* bv = std::get_if<BitVector>(&data_)) {
      InsertIntoBitVector(*bv, index);
      return;
    }
    if (auto* vec = std::get_if<IndexVector>(&data_)) {
      PERFETTO_DCHECK(std::is_sorted(vec->begin(), vec->end()));
      auto it = std::upper_bound(vec->begin(), vec->end(), index);
      vec->insert(it, index);
      return;
    }
    NoVariantMatched();
=======
    switch (mode_) {
      case Mode::kRange:
        if (index == end_index_) {
          // Fast path: if we're just appending to the end of the range, we can
          // stay in range mode and just bump the end index.
          end_index_++;
        } else {
          // Slow path: the insert is somewhere else other than the end. This
          // means we need to switch to using a BitVector instead.
          bit_vector_.Resize(start_index_, false);
          bit_vector_.Resize(end_index_, true);
          *this = RowMap(std::move(bit_vector_));

          InsertIntoBitVector(index);
        }
        break;
      case Mode::kBitVector:
        InsertIntoBitVector(index);
        break;
      case Mode::kIndexVector: {
        PERFETTO_DCHECK(
            std::is_sorted(index_vector_.begin(), index_vector_.end()));
        auto it =
            std::upper_bound(index_vector_.begin(), index_vector_.end(), index);
        index_vector_.insert(it, index);
        break;
      }
    }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  // Updates this RowMap by 'picking' the indices given by |picker|.
  // This is easiest to explain with an example; suppose we have the following
  // RowMaps:
  // this  : [0, 1, 4, 10, 11]
  // picker: [0, 3, 4, 4, 2]
  //
  // After calling Apply(picker), we now have the following:
  // this  : [0, 10, 11, 11, 4]
  //
  // Conceptually, we are performing the following algorithm:
  // RowMap rm = Copy()
  // for (p : picker)
  //   rm[i++] = this[p]
  // return rm;
  RowMap SelectRows(const RowMap& selector) const {
    uint32_t size = selector.size();

    // If the selector is empty, just return an empty RowMap.
    if (size == 0u)
<<<<<<< HEAD
      return {};
=======
      return RowMap();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

    // If the selector is just picking a single row, just return that row
    // without any additional overhead.
    if (size == 1u)
      return RowMap::SingleRow(Get(selector.Get(0)));

    // For all other cases, go into the slow-path.
    return SelectRowsSlow(selector);
  }

<<<<<<< HEAD
  // Intersects |this| with |second| independent of underlying structure of both
  // RowMaps. Modifies |this| to only contain indices present in |second|.
  void Intersect(const RowMap& second);
=======
  // Intersects the range [start_index, end_index) with |this| writing the
  // result into |this|. By "intersect", we mean to keep only the indices
  // present in both this RowMap and in the Range [start_index, end_index). The
  // order of the preserved indices will be the same as |this|.
  //
  // Conceptually, we are performing the following algorithm:
  // for (idx : this)
  //   if (start_index <= idx && idx < end_index)
  //     continue;
  //   Remove(idx)
  void Intersect(uint32_t start_index, uint32_t end_index) {
    if (mode_ == Mode::kRange) {
      // If both RowMaps have ranges, we can just take the smallest intersection
      // of them as the new RowMap.
      // We have this as an explicit fast path as this is very common for
      // constraints on id and sorted columns to satisfy this condition.
      start_index_ = std::max(start_index_, start_index);
      end_index_ = std::max(start_index_, std::min(end_index_, end_index));
      return;
    }

    // TODO(lalitm): improve efficiency of this if we end up needing it.
    Filter([start_index, end_index](OutputIndex index) {
      return index >= start_index && index < end_index;
    });
  }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Intersects this RowMap with |index|. If this RowMap contained |index|, then
  // it will *only* contain |index|. Otherwise, it will be empty.
  void IntersectExact(OutputIndex index) {
    if (Contains(index)) {
      *this = RowMap(index, index + 1);
    } else {
      Clear();
    }
  }

  // Clears this RowMap by resetting it to a newly constructed state.
  void Clear() { *this = RowMap(); }

<<<<<<< HEAD
  // Converts this RowMap to an index vector in the most efficient way
  // possible.
  std::vector<uint32_t> TakeAsIndexVector() && {
    if (const auto* range = std::get_if<Range>(&data_)) {
      std::vector<uint32_t> rm(range->size());
      std::iota(rm.begin(), rm.end(), range->start);
      return rm;
    }
    if (const auto* bv = std::get_if<BitVector>(&data_)) {
      return bv->GetSetBitIndices();
    }
    if (auto* vec = std::get_if<IndexVector>(&data_)) {
      return std::move(*vec);
    }
    NoVariantMatched();
  }

  // Returns the data in RowMap BitVector, nullptr if RowMap is in a different
  // mode.
  const BitVector* GetIfBitVector() const {
    return std::get_if<BitVector>(&data_);
  }

  // Returns the data in RowMap IndexVector, nullptr if RowMap is in a different
  // mode.
  const std::vector<uint32_t>* GetIfIndexVector() const {
    return std::get_if<IndexVector>(&data_);
  }

  // Returns the data in RowMap Range, nullptr if RowMap is in a different
  // mode.
  const Range* GetIfIRange() const { return std::get_if<Range>(&data_); }

=======
  template <typename Comparator = bool(uint32_t, uint32_t)>
  void StableSort(std::vector<uint32_t>* out, Comparator c) const {
    switch (mode_) {
      case Mode::kRange:
        std::stable_sort(out->begin(), out->end(),
                         [this, c](uint32_t a, uint32_t b) {
                           return c(GetRange(a), GetRange(b));
                         });
        break;
      case Mode::kBitVector:
        std::stable_sort(out->begin(), out->end(),
                         [this, c](uint32_t a, uint32_t b) {
                           return c(GetBitVector(a), GetBitVector(b));
                         });
        break;
      case Mode::kIndexVector:
        std::stable_sort(out->begin(), out->end(),
                         [this, c](uint32_t a, uint32_t b) {
                           return c(GetIndexVector(a), GetIndexVector(b));
                         });
        break;
    }
  }

  // Filters the indices in |out| by keeping those which meet |p|.
  template <typename Predicate = bool(OutputIndex)>
  void Filter(Predicate p) {
    switch (mode_) {
      case Mode::kRange:
        FilterRange(p);
        break;
      case Mode::kBitVector: {
        for (auto it = bit_vector_.IterateSetBits(); it; it.Next()) {
          if (!p(it.index()))
            it.Clear();
        }
        break;
      }
      case Mode::kIndexVector: {
        auto ret = std::remove_if(index_vector_.begin(), index_vector_.end(),
                                  [p](uint32_t i) { return !p(i); });
        index_vector_.erase(ret, index_vector_.end());
        break;
      }
    }
  }

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // Returns the iterator over the rows in this RowMap.
  Iterator IterateRows() const { return Iterator(this); }

  // Returns if the RowMap is internally represented using a range.
<<<<<<< HEAD
  bool IsRange() const { return std::holds_alternative<Range>(data_); }

  // Returns if the RowMap is internally represented using a BitVector.
  bool IsBitVector() const { return std::holds_alternative<BitVector>(data_); }

  // Returns if the RowMap is internally represented using an index vector.
  bool IsIndexVector() const {
    return std::holds_alternative<IndexVector>(data_);
  }

 private:
  using Variant = std::variant<Range, BitVector, IndexVector>;

  explicit RowMap(Range);

  explicit RowMap(Variant);

  PERFETTO_ALWAYS_INLINE static OutputIndex GetRange(Range r, InputRow row) {
    return r.start + row;
  }
  PERFETTO_ALWAYS_INLINE static OutputIndex GetBitVector(const BitVector& bv,
                                                         uint32_t row) {
    return bv.IndexOfNthSet(row);
  }
  PERFETTO_ALWAYS_INLINE static OutputIndex GetIndexVector(
      const IndexVector& vec,
      uint32_t row) {
    return vec[row];
=======
  bool IsRange() const { return mode_ == Mode::kRange; }

 private:
  enum class Mode {
    kRange,
    kBitVector,
    kIndexVector,
  };
  // TODO(lalitm): remove this when the coupling between RowMap and
  // ColumnStorage Selector is broken (after filtering is moved out of here).
  friend class ColumnStorageOverlay;

  template <typename Predicate>
  void FilterRange(Predicate p) {
    uint32_t count = end_index_ - start_index_;

    // Optimization: if we are only going to scan a few indices, it's not
    // worth the haslle of working with a BitVector.
    constexpr uint32_t kSmallRangeLimit = 2048;
    bool is_small_range = count < kSmallRangeLimit;

    // Optimization: weif the cost of a BitVector is more than the highest
    // possible cost an index vector could have, use the index vector.
    uint32_t bit_vector_cost = BitVector::ApproxBytesCost(end_index_);
    uint32_t index_vector_cost_ub = sizeof(uint32_t) * count;

    // If either of the conditions hold which make it better to use an
    // index vector, use it instead. Alternatively, if we are optimizing for
    // lookup speed, we also want to use an index vector.
    if (is_small_range || index_vector_cost_ub <= bit_vector_cost ||
        optimize_for_ == OptimizeFor::kLookupSpeed) {
      // Try and strike a good balance between not making the vector too
      // big and good performance.
      std::vector<uint32_t> iv(std::min(kSmallRangeLimit, count));

      uint32_t out_i = 0;
      for (uint32_t i = 0; i < count; ++i) {
        // If we reach the capacity add another small set of indices.
        if (PERFETTO_UNLIKELY(out_i == iv.size()))
          iv.resize(iv.size() + kSmallRangeLimit);

        // We keep this branch free by always writing the index but only
        // incrementing the out index if the return value is true.
        bool value = p(i + start_index_);
        iv[out_i] = i + start_index_;
        out_i += value;
      }

      // Make the vector the correct size and as small as possible.
      iv.resize(out_i);
      iv.shrink_to_fit();

      *this = RowMap(std::move(iv));
      return;
    }

    // Otherwise, create a bitvector which spans the full range using
    // |p| as the filler for the bits between start and end.
    *this = RowMap(BitVector::Range(start_index_, end_index_, p));
  }

  void InsertIntoBitVector(uint32_t row) {
    PERFETTO_DCHECK(mode_ == Mode::kBitVector);

    // If we're adding a row to precisely the end of the BitVector, just append
    // true instead of resizing and then setting.
    if (row == bit_vector_.size()) {
      bit_vector_.AppendTrue();
      return;
    }

    if (row > bit_vector_.size()) {
      bit_vector_.Resize(row + 1, false);
    }
    bit_vector_.Set(row);
  }

  PERFETTO_ALWAYS_INLINE OutputIndex GetRange(InputRow row) const {
    PERFETTO_DCHECK(mode_ == Mode::kRange);
    return start_index_ + row;
  }
  PERFETTO_ALWAYS_INLINE OutputIndex GetBitVector(uint32_t row) const {
    PERFETTO_DCHECK(mode_ == Mode::kBitVector);
    return bit_vector_.IndexOfNthSet(row);
  }
  PERFETTO_ALWAYS_INLINE OutputIndex GetIndexVector(uint32_t row) const {
    PERFETTO_DCHECK(mode_ == Mode::kIndexVector);
    return index_vector_[row];
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  RowMap SelectRowsSlow(const RowMap& selector) const;

<<<<<<< HEAD
  static void InsertIntoBitVector(BitVector& bv, OutputIndex row) {
    if (row == bv.size()) {
      bv.AppendTrue();
      return;
    }
    if (row > bv.size())
      bv.Resize(row + 1, false);
    bv.Set(row);
  }

  PERFETTO_NORETURN static void NoVariantMatched() {
    PERFETTO_FATAL("Didn't match any variant type.");
  }

  Variant data_;
=======
  Mode mode_ = Mode::kRange;

  // Only valid when |mode_| == Mode::kRange.
  OutputIndex start_index_ = 0;  // This is an inclusive index.
  OutputIndex end_index_ = 0;    // This is an exclusive index.

  // Only valid when |mode_| == Mode::kBitVector.
  BitVector bit_vector_;

  // Only valid when |mode_| == Mode::kIndexVector.
  std::vector<OutputIndex> index_vector_;

  OptimizeFor optimize_for_ = OptimizeFor::kMemory;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_CONTAINERS_ROW_MAP_H_
