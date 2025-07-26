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

#include "src/trace_processor/containers/row_map.h"

#include "src/trace_processor/containers/row_map_algorithms.h"

namespace perfetto {
namespace trace_processor {

namespace {

RowMap SelectRangeWithRange(uint32_t start,
                            uint32_t end,
                            uint32_t selector_start,
                            uint32_t selector_end) {
  PERFETTO_DCHECK(start <= end);
  PERFETTO_DCHECK(selector_start <= selector_end);
  PERFETTO_DCHECK(selector_end <= end - start);

  return RowMap(start + selector_start, start + selector_end);
}

RowMap SelectRangeWithBv(uint32_t start,
                         uint32_t end,
                         const BitVector& selector) {
  PERFETTO_DCHECK(start <= end);
  PERFETTO_DCHECK(selector.size() <= end - start);

  // If |start| == 0 and |selector.size()| <= |end - start| (which is a
  // precondition for this function), the BitVector we generate is going to be
  // exactly |selector|.
  //
  // This is a fast path for the common situation where, post-filtering,
  // SelectRows is called on all the table RowMaps with a BitVector. The self
  // RowMap will always be a range so we expect this case to be hit at least
  // once every filter operation.
  if (start == 0u)
    return RowMap(selector.Copy());

  // We only need to resize to |start| + |selector.size()| as we know any rows
  // not covered by |selector| are going to be removed below.
  BitVector bv(start, false);
  bv.Resize(start + selector.size(), true);

  bv.UpdateSetBits(selector);
  return RowMap(std::move(bv));
}

RowMap SelectRangeWithIv(uint32_t start,
                         uint32_t end,
                         const std::vector<uint32_t>& selector) {
  PERFETTO_DCHECK(start <= end);

  std::vector<uint32_t> iv(selector.size());
  for (uint32_t i = 0; i < selector.size(); ++i) {
    PERFETTO_DCHECK(selector[i] < end - start);
    iv[i] = selector[i] + start;
  }
  return RowMap(std::move(iv));
}

RowMap SelectBvWithRange(const BitVector& bv,
                         uint32_t selector_start,
                         uint32_t selector_end) {
  PERFETTO_DCHECK(selector_start <= selector_end);
  PERFETTO_DCHECK(selector_end <= bv.CountSetBits());

  // If we're simply selecting every element in the bitvector, just
  // return a copy of the BitVector without iterating.
  BitVector ret = bv.Copy();
  if (selector_start == 0 && selector_end == bv.CountSetBits()) {
    return RowMap(std::move(ret));
  }

  for (auto it = ret.IterateSetBits(); it; it.Next()) {
    auto set_idx = it.ordinal();
    if (set_idx < selector_start || set_idx >= selector_end)
      it.Clear();
  }
  return RowMap(std::move(ret));
}

RowMap SelectBvWithBv(const BitVector& bv, const BitVector& selector) {
  BitVector ret = bv.Copy();
  ret.UpdateSetBits(selector);
  return RowMap(std::move(ret));
}

RowMap SelectBvWithIv(const BitVector& bv,
                      const std::vector<uint32_t>& selector) {
  // The value of this constant was found by considering the benchmarks
  // |BM_SelectBvWithIvByConvertToIv| and |BM_SelectBvWithIvByIndexOfNthSet|.
  //
  // We use this to find the ratio between |bv.CountSetBits()| and
  // |selector.size()| where |SelectBvWithIvByIndexOfNthSet| was found to be
  // faster than |SelectBvWithIvByConvertToIv|.
  //
  // Note: as of writing this, the benchmarks do not take into account the fill
  // ratio of the BitVector; they assume 50% rate which almost never happens in
  // practice. In the future, we could also take this into account (by
  // considering the ratio between bv.size() and bv.CountSetBits()) but this
  // causes an explosion in the state space for the benchmark so we're not
  // considering this today.
  //
  // The current value of the constant was picked by running these benchmarks on
  // a E5-2690 v4 and finding the crossover point using a spreadsheet.
  constexpr uint32_t kIndexOfSetBitToSelectorRatio = 4;

  // If the selector is larger than a threshold, it's more efficient to convert
  // the entire BitVector to an index vector and use SelectIvWithIv instead.
  if (bv.CountSetBits() / kIndexOfSetBitToSelectorRatio < selector.size()) {
    return RowMap(
        row_map_algorithms::SelectBvWithIvByConvertToIv(bv, selector));
  }
  return RowMap(
      row_map_algorithms::SelectBvWithIvByIndexOfNthSet(bv, selector));
}

RowMap SelectIvWithRange(const std::vector<uint32_t>& iv,
                         uint32_t selector_start,
                         uint32_t selector_end) {
  PERFETTO_DCHECK(selector_start <= selector_end);
  PERFETTO_DCHECK(selector_end <= iv.size());

  std::vector<uint32_t> ret(selector_end - selector_start);
  for (uint32_t i = selector_start; i < selector_end; ++i) {
    ret[i - selector_start] = iv[i];
  }
  return RowMap(std::move(ret));
}

RowMap SelectIvWithBv(const std::vector<uint32_t>& iv,
                      const BitVector& selector) {
  PERFETTO_DCHECK(selector.size() <= iv.size());

  std::vector<uint32_t> copy = iv;
  copy.resize(selector.size());

  uint32_t idx = 0;
  auto it = std::remove_if(
      copy.begin(), copy.end(),
      [&idx, &selector](uint32_t) { return !selector.IsSet(idx++); });
  copy.erase(it, copy.end());
  return RowMap(std::move(copy));
}

RowMap SelectIvWithIv(const std::vector<uint32_t>& iv,
                      const std::vector<uint32_t>& selector) {
  return RowMap(row_map_algorithms::SelectIvWithIv(iv, selector));
}

}  // namespace

RowMap::RowMap() : RowMap(0, 0) {}

RowMap::RowMap(uint32_t start, uint32_t end, OptimizeFor optimize_for)
    : mode_(Mode::kRange),
      start_index_(start),
      end_index_(end),
      optimize_for_(optimize_for) {}

RowMap::RowMap(BitVector bit_vector)
    : mode_(Mode::kBitVector), bit_vector_(std::move(bit_vector)) {}

RowMap::RowMap(std::vector<uint32_t> vec)
    : mode_(Mode::kIndexVector), index_vector_(std::move(vec)) {}

RowMap RowMap::Copy() const {
  switch (mode_) {
    case Mode::kRange:
      return RowMap(start_index_, end_index_);
    case Mode::kBitVector:
      return RowMap(bit_vector_.Copy());
    case Mode::kIndexVector:
      return RowMap(index_vector_);
  }
  PERFETTO_FATAL("For GCC");
}

RowMap RowMap::SelectRowsSlow(const RowMap& selector) const {
  // Pick the strategy based on the selector as there is more common code
  // between selectors of the same mode than between the RowMaps being
  // selected of the same mode.
  switch (selector.mode_) {
    case Mode::kRange:
      switch (mode_) {
        case Mode::kRange:
          return SelectRangeWithRange(start_index_, end_index_,
                                      selector.start_index_,
                                      selector.end_index_);
        case Mode::kBitVector:
          return SelectBvWithRange(bit_vector_, selector.start_index_,
                                   selector.end_index_);
        case Mode::kIndexVector:
          return SelectIvWithRange(index_vector_, selector.start_index_,
                                   selector.end_index_);
      }
      break;
    case Mode::kBitVector:
      switch (mode_) {
        case Mode::kRange:
          return SelectRangeWithBv(start_index_, end_index_,
                                   selector.bit_vector_);
        case Mode::kBitVector:
          return SelectBvWithBv(bit_vector_, selector.bit_vector_);
        case Mode::kIndexVector:
          return SelectIvWithBv(index_vector_, selector.bit_vector_);
      }
      break;
    case Mode::kIndexVector:
      switch (mode_) {
        case Mode::kRange:
          return SelectRangeWithIv(start_index_, end_index_,
                                   selector.index_vector_);
        case Mode::kBitVector:
          return SelectBvWithIv(bit_vector_, selector.index_vector_);
        case Mode::kIndexVector:
          return SelectIvWithIv(index_vector_, selector.index_vector_);
      }
      break;
  }
  PERFETTO_FATAL("For GCC");
}

}  // namespace trace_processor
}  // namespace perfetto
