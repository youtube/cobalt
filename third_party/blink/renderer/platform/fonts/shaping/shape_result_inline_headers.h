/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_

#include <hb.h>

#include <algorithm>
#include <memory>
#include <type_traits>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class SimpleFontData;

// This struct should be TriviallyCopyable so that std::copy() is equivalent to
// memcpy.
// Because glyph offsets are often zero, particularly for Latin runs, we hold it
// in |ShapeResult::GlyphData::_offsets_| for reducing memory usage.
struct HarfBuzzRunGlyphData {
  DISALLOW_NEW();

  // The max number of characters in a |RunInfo| is limited by
  // |character_index|.
  static constexpr unsigned kCharacterIndexBits = 15;
  static constexpr unsigned kMaxCharacters = 1 << kCharacterIndexBits;
  static constexpr unsigned kMaxCharacterIndex = kMaxCharacters - 1;
  // The max number of glyphs in a |RunInfo|. This make the number
  // of glyphs predictable and minimizes the buffer reallocations.
  static constexpr unsigned kMaxGlyphs = kMaxCharacters;

  unsigned glyph : 16;
  // The index of the character this glyph is for. To use as an index of
  // |String|, it is the index of UTF16 code unit, and it is always at the
  // HarfBuzz cluster boundary.
  unsigned character_index : kCharacterIndexBits;
  unsigned safe_to_break_before : 1;

  float advance;
};

struct ShapeResult::RunInfo final : public RefCounted<ShapeResult::RunInfo> {
  USING_FAST_MALLOC(RunInfo);

 public:
  static scoped_refptr<RunInfo> Create(const SimpleFontData* font,
                                       hb_direction_t dir,
                                       CanvasRotationInVertical canvas_rotation,
                                       hb_script_t script,
                                       unsigned start_index,
                                       unsigned num_glyphs,
                                       unsigned num_characters) {
    return base::AdoptRef(new RunInfo(font, dir, canvas_rotation, script,
                                      start_index, num_glyphs, num_characters));
  }

  static scoped_refptr<RunInfo> Create(const RunInfo& other) {
    return base::AdoptRef(new RunInfo(other));
  }

  RunInfo(const SimpleFontData* font,
          hb_direction_t dir,
          CanvasRotationInVertical canvas_rotation,
          hb_script_t script,
          unsigned start_index,
          unsigned num_glyphs,
          unsigned num_characters)
      : glyph_data_(
            std::min(num_glyphs, HarfBuzzRunGlyphData::kMaxCharacterIndex + 1)),
        font_data_(const_cast<SimpleFontData*>(font)),
        start_index_(start_index),
        num_characters_(num_characters),
        width_(0.0f),
        script_(script),
        direction_(dir),
        canvas_rotation_(canvas_rotation) {}

  RunInfo(const RunInfo& other)
      : glyph_data_(other.glyph_data_),
        font_data_(other.font_data_),
        graphemes_(other.graphemes_),
        start_index_(other.start_index_),
        num_characters_(other.num_characters_),
        width_(other.width_),
        script_(other.script_),
        direction_(other.direction_),
        canvas_rotation_(other.canvas_rotation_) {}

  unsigned NumGlyphs() const { return glyph_data_.size(); }
  bool IsLtr() const { return HB_DIRECTION_IS_FORWARD(direction_); }
  bool IsRtl() const { return HB_DIRECTION_IS_BACKWARD(direction_); }
  bool IsHorizontal() const { return HB_DIRECTION_IS_HORIZONTAL(direction_); }
  CanvasRotationInVertical CanvasRotation() const { return canvas_rotation_; }
  unsigned NextSafeToBreakOffset(unsigned) const;
  unsigned PreviousSafeToBreakOffset(unsigned) const;
  float XPositionForVisualOffset(unsigned, AdjustMidCluster) const;
  float XPositionForOffset(unsigned, AdjustMidCluster) const;
  void CharacterIndexForXPosition(float,
                                  BreakGlyphsOption,
                                  GlyphIndexResult*) const;
  void LimitNumGlyphs(unsigned start_glyph,
                      unsigned* num_glyphs_in_out,
                      unsigned* num_glyphs_removed_out,
                      const bool is_ltr,
                      const hb_glyph_info_t* glyph_infos);

  unsigned StartIndex() const { return start_index_; }
  unsigned GlyphToCharacterIndex(unsigned i) const {
    return start_index_ + glyph_data_[i].character_index;
  }

  unsigned NumGraphemes(unsigned start, unsigned end) const;

  // For memory reporting.
  size_t ByteSize() const { return sizeof(*this) + glyph_data_.ByteSize(); }

  // Represents a range of HarfBuzzRunGlyphData. |begin| and |end| follow the
  // iterator pattern; i.e., |begin| is lower or equal to |end| in the address
  // space regardless of LTR/RTL. |begin| is inclusive, |end| is exclusive.
  struct GlyphDataRange {
    GlyphDataRange FindGlyphDataRange(bool is_rtl,
                                      unsigned start_character_index,
                                      unsigned end_character_index) const;
    unsigned size() const { return static_cast<unsigned>(end - begin); }

    const HarfBuzzRunGlyphData* begin = nullptr;
    const HarfBuzzRunGlyphData* end = nullptr;
    const GlyphOffset* offsets = nullptr;
  };

  // Find the range of HarfBuzzRunGlyphData for the specified character index
  // range. This function uses binary search twice, hence O(2 log n).
  GlyphDataRange FindGlyphDataRange(unsigned start_character_index,
                                    unsigned end_character_index) const {
    GlyphDataRange range = GetGlyphDataRange().FindGlyphDataRange(
        IsRtl(), start_character_index, end_character_index);
    return range;
  }

  // Creates a new RunInfo instance representing a subset of the current run.
  // Returns |nullptr| if there are no glyphs in the specified range.
  scoped_refptr<RunInfo> CreateSubRun(unsigned start, unsigned end) {
    DCHECK(end > start);
    unsigned number_of_characters = std::min(end - start, num_characters_);
    auto glyphs = FindGlyphDataRange(start, end);
    unsigned number_of_glyphs =
        static_cast<unsigned>(std::distance(glyphs.begin, glyphs.end));
    if (UNLIKELY(!number_of_glyphs))
      return nullptr;

    auto run =
        Create(font_data_.get(), direction_, canvas_rotation_, script_,
               start_index_ + start, number_of_glyphs, number_of_characters);

    run->glyph_data_.CopyFromRange(glyphs);

    float total_advance = 0;
    for (HarfBuzzRunGlyphData& glyph_data : run->glyph_data_) {
      glyph_data.character_index -= start;
      total_advance += glyph_data.advance;
    }

    run->width_ = total_advance;
    run->num_characters_ = number_of_characters;

    return run;
  }

  // Returns new |RunInfo| if |this| and |other| are merged. Otherwise returns
  // null.
  scoped_refptr<RunInfo> MergeIfPossible(const RunInfo& other) const {
    if (!CanMerge(other))
      return nullptr;
    DCHECK_LT(start_index_, other.start_index_);
    auto run =
        Create(font_data_.get(), direction_, canvas_rotation_, script_,
               start_index_, glyph_data_.size() + other.glyph_data_.size(),
               num_characters_ + other.num_characters_);
    // Note: We populate |graphemes_| on demand, e.g. hit testing.
    const int index_adjust = other.start_index_ - start_index_;
    if (UNLIKELY(IsRtl())) {
      run->glyph_data_.CopyFrom(other.glyph_data_, glyph_data_);
      auto* const end = run->glyph_data_.begin() + other.glyph_data_.size();
      for (auto* it = run->glyph_data_.begin(); it < end; ++it)
        it->character_index += index_adjust;
    } else {
      run->glyph_data_.CopyFrom(glyph_data_, other.glyph_data_);
      auto* const end = run->glyph_data_.end();
      for (auto* it = run->glyph_data_.begin() + glyph_data_.size(); it < end;
           ++it)
        it->character_index += index_adjust;
    }
    run->width_ = width_ + other.width_;
    return run;
  }

  // Returns true if |other| can be merged at end of |this|.
  bool CanMerge(const RunInfo& other) const {
    return start_index_ + num_characters_ == other.start_index_ &&
           canvas_rotation_ == other.canvas_rotation_ &&
           font_data_ == other.font_data_ && direction_ == other.direction_ &&
           script_ == other.script_ &&
           glyph_data_.size() + other.glyph_data_.size() <
               HarfBuzzRunGlyphData::kMaxCharacterIndex + 1;
  }

  void ExpandRangeToIncludePartialGlyphs(int offset, int* from, int* to) const {
    int end = offset + num_characters_;
    int start;

    if (IsLtr()) {
      start = offset + num_characters_;
      for (unsigned i = 0; i < glyph_data_.size(); ++i) {
        int index = offset + glyph_data_[i].character_index;
        if (start == index)
          continue;
        end = index;
        if (end > *from && start < *to) {
          *from = std::min(*from, start);
          *to = std::max(*to, end);
        }
        end = offset + num_characters_;
        start = index;
      }
    } else {
      start = offset + num_characters_;
      for (unsigned i = 0; i < glyph_data_.size(); ++i) {
        int index = offset + glyph_data_[i].character_index;
        if (start == index)
          continue;
        if (end > *from && start < *to) {
          *from = std::min(*from, start);
          *to = std::max(*to, end);
        }
        end = start;
        start = index;
      }
    }

    if (end > *from && start < *to) {
      *from = std::min(*from, start);
      *to = std::max(*to, end);
    }
  }

  // Common signatures with RunInfoPart, to templatize algorithms.
  const RunInfo* GetRunInfo() const { return this; }
  const GlyphDataRange GetGlyphDataRange() const {
    return {glyph_data_.begin(), glyph_data_.end(),
            glyph_data_.GetMayBeOffsets()};
  }
  unsigned OffsetToRunStartIndex() const { return 0; }

  class GlyphDataCollection;

  // A array of glyph offsets. If all offsets are zero, we don't allocate
  // storage for reducing memory usage.
  class GlyphOffsetArray final {
   public:
    explicit GlyphOffsetArray(unsigned size) : size_(size) {}

    GlyphOffsetArray(const GlyphOffsetArray& other) : size_(other.size_) {
      if (!other.storage_)
        return;
      std::copy(other.storage_.get(), other.storage_.get() + other.size(),
                AllocateStorage());
    }

    // A return value of |GetOffsets()| to represent optional |GlyphOffset|
    // array.
    template <bool has_non_zero_glyph_offsets>
    struct iterator final {};

    template <bool has_non_zero_glyph_offsets>
    iterator<has_non_zero_glyph_offsets> GetIterator() const {
      return iterator<has_non_zero_glyph_offsets>(*this);
    }

    template <bool has_non_zero_glyph_offsets>
    iterator<has_non_zero_glyph_offsets> GetIteratorForRange(
        const GlyphDataRange& range) const {
      return iterator<has_non_zero_glyph_offsets>(range);
    }

    unsigned size() const { return size_; }
    bool IsEmpty() const { return size() == 0; }

    size_t ByteSize() const {
      return storage_ ? size() * sizeof(GlyphOffset) : 0;
    }

    void CopyFrom(const GlyphOffsetArray& other1,
                  const GlyphOffsetArray& other2) {
      SECURITY_CHECK(size() == other1.size() + other2.size());
      DCHECK(!other1.IsEmpty());
      DCHECK(!other2.IsEmpty());
      if (other1.storage_) {
        if (!storage_)
          AllocateStorage();
        std::copy(other1.storage_.get(), other1.storage_.get() + other1.size(),
                  storage_.get());
      }
      if (other2.storage_) {
        if (!storage_)
          AllocateStorage();
        std::copy(other2.storage_.get(), other2.storage_.get() + other2.size(),
                  storage_.get() + other1.size());
      }
    }

    void CopyFromRange(const GlyphDataRange& range) {
      CHECK_EQ(range.size(), size());
      if (!range.offsets || range.size() == 0) {
        storage_.reset();
        return;
      }
      std::copy(range.offsets, range.offsets + range.size(), AllocateStorage());
    }

    GlyphOffset* GetStorage() const { return storage_.get(); }
    bool HasStorage() const { return storage_.get(); }

    void Reverse() {
      if (!storage_)
        return;
      std::reverse(storage_.get(), storage_.get() + size());
    }

    void Shrink(unsigned new_size) {
      DCHECK_GE(new_size, 1u);
      // Note: To follow Vector<T>::Shrink(), we accept |new_size == size()|
      if (new_size == size())
        return;
      CHECK_LT(new_size, size());
      size_ = new_size;
      if (!storage_)
        return;
      GlyphOffset* new_offsets = new GlyphOffset[new_size];
      std::copy(storage_.get(), storage_.get() + new_size, new_offsets);
      storage_.reset(new_offsets);
    }

    // Functions to change one element.
    void AddHeightAt(unsigned index, float delta) {
      CHECK_LT(index, size());
      DCHECK_NE(delta, 0.0f);
      if (!storage_)
        AllocateStorage();
      storage_[index].set_y(storage_[index].y() + delta);
    }

    void AddWidthAt(unsigned index, float delta) {
      CHECK_LT(index, size());
      DCHECK_NE(delta, 0.0f);
      if (!storage_)
        AllocateStorage();
      storage_[index].set_x(storage_[index].x() + delta);
    }

    void SetAt(unsigned index, GlyphOffset offset) {
      CHECK_LT(index, size());
      if (!storage_) {
        if (offset.IsZero())
          return;
        AllocateStorage();
      }
      storage_[index] = offset;
    }

   private:
    // Note: HarfBuzzShaperTest.ShapeVerticalUpright uses non-zero glyph offset.
    GlyphOffset* AllocateStorage() {
      DCHECK_GE(size(), 1u);
      DCHECK(!storage_);
      storage_.reset(new GlyphOffset[size()]);
      return storage_.get();
    }

    std::unique_ptr<GlyphOffset[]> storage_;
    unsigned size_;
  };

  // Collection of |HarfBuzzRunGlyphData| with optional glyph offset
  class GlyphDataCollection final {
   public:
    explicit GlyphDataCollection(unsigned num_glyphs)
        : data_(new HarfBuzzRunGlyphData[num_glyphs]), offsets_(num_glyphs) {}

    GlyphDataCollection(const GlyphDataCollection& other)
        : data_(new HarfBuzzRunGlyphData[other.size()]),
          offsets_(other.offsets_) {
      static_assert(std::is_trivially_copyable_v<HarfBuzzRunGlyphData>);
      std::copy(other.data_.get(), other.data_.get() + other.size(),
                data_.get());
    }

    HarfBuzzRunGlyphData& operator[](unsigned index) {
      CHECK_LT(index, size());
      return data_[index];
    }

    const HarfBuzzRunGlyphData& operator[](unsigned index) const {
      CHECK_LT(index, size());
      return data_[index];
    }

    bool HasNonZeroOffsets() const { return offsets_.HasStorage(); }

    size_t ByteSize() const {
      return sizeof(*this) + size() * sizeof(HarfBuzzRunGlyphData) +
             offsets_.ByteSize();
    }

    template <bool has_non_zero_glyph_offsets>
    GlyphOffsetArray::iterator<has_non_zero_glyph_offsets> GetOffsets() const {
      return offsets_.GetIterator<has_non_zero_glyph_offsets>();
    }

    GlyphOffset* GetMayBeOffsets() const { return offsets_.GetStorage(); }

    // Note: Caller should be adjust |HarfBuzzRunGlyphData.character_index|.
    void CopyFrom(const GlyphDataCollection& other1,
                  const GlyphDataCollection& other2) {
      SECURITY_CHECK(size() == other1.size() + other2.size());
      DCHECK(!other1.IsEmpty());
      DCHECK(!other2.IsEmpty());
      std::copy(other1.data_.get(), other1.data_.get() + other1.size(),
                data_.get());
      std::copy(other2.data_.get(), other2.data_.get() + other2.size(),
                data_.get() + other1.size());
      offsets_.CopyFrom(other1.offsets_, other2.offsets_);
    }

    // Note: Caller should be adjust |HarfBuzzRunGlyphData.character_index|.
    void CopyFromRange(const GlyphDataRange& range) {
      CHECK_EQ(static_cast<size_t>(range.end - range.begin), size());
      static_assert(std::is_trivially_copyable_v<HarfBuzzRunGlyphData>);
      std::copy(range.begin, range.end, data_.get());
      offsets_.CopyFromRange(range);
    }

    void AddOffsetHeightAt(unsigned index, float delta) {
      offsets_.AddHeightAt(index, delta);
    }

    void AddOffsetWidthAt(unsigned index, float delta) {
      offsets_.AddWidthAt(index, delta);
    }

    void SetOffsetAt(unsigned index, GlyphOffset offset) {
      offsets_.SetAt(index, offset);
    }

    // Vector<HarfBuzzRunGlyphData> like functions
    using iterator = HarfBuzzRunGlyphData*;
    using const_iterator = const HarfBuzzRunGlyphData*;
    iterator begin() { return data_.get(); }
    iterator end() { return data_.get() + size(); }
    const_iterator begin() const { return data_.get(); }
    const_iterator end() const { return data_.get() + size(); }

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
    reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const {
      return std::make_reverse_iterator(end());
    }
    const_reverse_iterator rend() const {
      return std::make_reverse_iterator(begin());
    }

    unsigned size() const { return offsets_.size(); }
    bool IsEmpty() const { return size() == 0; }

    const HarfBuzzRunGlyphData& front() const {
      CHECK(!IsEmpty());
      return (*this)[0];
    }
    const HarfBuzzRunGlyphData& back() const {
      CHECK(!IsEmpty());
      return (*this)[size() - 1];
    }

    void Reverse() {
      std::reverse(begin(), end());
      offsets_.Reverse();
    }

    void Shrink(unsigned new_size) {
      DCHECK_GE(new_size, 1u);
      // Note: To follow Vector<T>::Shrink(), we accept |new_size == size()|
      if (new_size == size())
        return;
      DCHECK_LT(new_size, size());
      HarfBuzzRunGlyphData* new_data = new HarfBuzzRunGlyphData[new_size];
      std::copy(data_.get(), data_.get() + new_size, new_data);
      data_.reset(new_data);
      offsets_.Shrink(new_size);
    }

   private:
    // Note: |offsets_| holds number of elements instead o here to reduce
    // memory usage.
    std::unique_ptr<HarfBuzzRunGlyphData[]> data_;
    // |offsets_| holds collection of offset for |data_[i]|.
    // When all offsets are zero, we don't allocate for reducing memory usage.
    GlyphOffsetArray offsets_;
  };

  void CheckConsistency() const {
#if DCHECK_IS_ON()
    for (const HarfBuzzRunGlyphData& glyph : glyph_data_)
      DCHECK_LT(glyph.character_index, num_characters_);
#endif
  }

  GlyphDataCollection glyph_data_;
  scoped_refptr<SimpleFontData> font_data_;

  // graphemes_[i] is the number of graphemes up to (and including) the ith
  // character in the run.
  Vector<unsigned> graphemes_;

  unsigned start_index_;
  unsigned num_characters_;
  float width_;

  hb_script_t script_;
  hb_direction_t direction_;

  // For upright-in-vertical we need to tell the ShapeResultBloberizer to rotate
  // the canvas back 90deg for this RunInfo.
  CanvasRotationInVertical canvas_rotation_;
};

// For non-zero glyph offset array
template <>
struct ShapeResult::RunInfo::GlyphOffsetArray::iterator<true> final {
  // The constructor for ShapeResult
  explicit iterator(const GlyphOffsetArray& array)
      : pointer(array.storage_.get()) {
    DCHECK(pointer);
  }

  // The constructor for ShapeResultView
  explicit iterator(const GlyphDataRange& range) : pointer(range.offsets) {
    DCHECK(pointer);
  }

  GlyphOffset operator*() const { return *pointer; }
  void operator++() { ++pointer; }

  const GlyphOffset* pointer;
};

// For zero glyph offset array
template <>
struct ShapeResult::RunInfo::GlyphOffsetArray::iterator<false> final {
  explicit iterator(const GlyphOffsetArray& array) {
    DCHECK(!array.HasStorage());
  }
  explicit iterator(const GlyphDataRange& range) { DCHECK(!range.offsets); }
  GlyphOffset operator*() const { return GlyphOffset(); }
  void operator++() {}
};

// Find the range of HarfBuzzRunGlyphData for the specified character index
// range. This function uses binary search twice, hence O(2 log n).
inline ShapeResult::RunInfo::GlyphDataRange
ShapeResult::RunInfo::GlyphDataRange::FindGlyphDataRange(
    bool is_rtl,
    unsigned start_character_index,
    unsigned end_character_index) const {
  const auto comparer = [](const HarfBuzzRunGlyphData& glyph_data,
                           unsigned index) {
    return glyph_data.character_index < index;
  };
  if (!is_rtl) {
    const HarfBuzzRunGlyphData* start_glyph =
        std::lower_bound(begin, end, start_character_index, comparer);
    if (UNLIKELY(start_glyph == end))
      return GlyphDataRange();
    const HarfBuzzRunGlyphData* end_glyph =
        std::lower_bound(start_glyph, end, end_character_index, comparer);
    if (offsets)
      return {start_glyph, end_glyph, offsets + (start_glyph - begin)};
    return {start_glyph, end_glyph, nullptr};
  }

  // RTL needs to use reverse iterators because there maybe multiple glyphs
  // for a character, and we want to find the first one in the logical order.
  const auto rbegin = std::reverse_iterator<const HarfBuzzRunGlyphData*>(end);
  const auto rend = std::reverse_iterator<const HarfBuzzRunGlyphData*>(begin);
  const auto start_glyph_it =
      std::lower_bound(rbegin, rend, start_character_index, comparer);
  if (UNLIKELY(start_glyph_it == rend))
    return GlyphDataRange();
  const auto end_glyph_it =
      std::lower_bound(start_glyph_it, rend, end_character_index, comparer);
  // Convert reverse iterators to pointers. Then increment to make |begin|
  // inclusive and |end| exclusive.
  const HarfBuzzRunGlyphData* start_glyph = &*end_glyph_it + 1;
  const HarfBuzzRunGlyphData* end_glyph = &*start_glyph_it + 1;
  if (offsets)
    return {start_glyph, end_glyph, offsets + (start_glyph - begin)};
  return {start_glyph, end_glyph, nullptr};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
