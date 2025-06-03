/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_

#include <memory>

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_stretch_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

struct hb_buffer_t;

namespace blink {

struct CharacterRange;
class Font;
template <typename TextContainerType>
class PLATFORM_EXPORT ShapeResultSpacing;
class TextRun;
class ShapeResultView;
struct TabSize;

enum class AdjustMidCluster {
  // Adjust the middle of a grapheme cluster to the logical end boundary.
  kToEnd,
  // Adjust the middle of a grapheme cluster to the logical start boundary.
  kToStart
};

struct ShapeResultCharacterData {
  DISALLOW_NEW();

  ShapeResultCharacterData()
      : is_cluster_base(false),
        safe_to_break_before(false),
        has_auto_spacing_after(false) {}

  void SetCachedData(float new_x_position,
                     bool new_is_cluster_base,
                     bool new_safe_to_break_before) {
    x_position = new_x_position;
    is_cluster_base = new_is_cluster_base;
    safe_to_break_before = new_safe_to_break_before;
  }

  float x_position = 0;
  // Set for the logical first character of a cluster.
  unsigned is_cluster_base : 1;
  unsigned safe_to_break_before : 1;
  unsigned has_auto_spacing_after : 1;
};

// A space should be appended after `offset` with the width of `spacing`.
struct OffsetWithSpacing {
  wtf_size_t offset;
  float spacing;
};

// There are two options for how OffsetForPosition behaves:
// IncludePartialGlyphs - decides what to do when the position hits more than
// 50% of the glyph. If enabled, we count that glyph, if disable we don't.
enum IncludePartialGlyphsOption {
  kOnlyFullGlyphs,
  kIncludePartialGlyphs,
};

// BreakGlyphsOption - allows OffsetForPosition to consider graphemes
// separations inside a glyph. It allows the function to return a point inside
// a glyph when multiple graphemes share a glyph (for example, in a ligature)
using BreakGlyphsOption = base::StrongAlias<class BreakGlyphsOptionTag, bool>;

// std::function is forbidden in Chromium and base::RepeatingCallback is way too
// expensive so we resort to a good old function pointer instead.
typedef void (*GlyphCallback)(void* context,
                              unsigned character_index,
                              Glyph,
                              gfx::Vector2dF glyph_offset,
                              float total_advance,
                              bool is_horizontal,
                              CanvasRotationInVertical,
                              const SimpleFontData*);

typedef void (*GraphemeClusterCallback)(void* context,
                                        unsigned character_index,
                                        float total_advance,
                                        unsigned graphemes_in_cluster,
                                        float cluster_advance,
                                        CanvasRotationInVertical);

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
  USING_FAST_MALLOC(ShapeResult);

 public:
  static scoped_refptr<ShapeResult> Create(const Font* font,
                                           unsigned start_index,
                                           unsigned num_characters,
                                           TextDirection direction) {
    return base::AdoptRef(
        new ShapeResult(font, start_index, num_characters, direction));
  }
  static scoped_refptr<ShapeResult> CreateEmpty(const ShapeResult& other) {
    return base::AdoptRef(
        new ShapeResult(other.primary_font_, 0, 0, other.Direction()));
  }
  static scoped_refptr<ShapeResult> Create(const ShapeResult& other) {
    return base::AdoptRef(new ShapeResult(other));
  }
  static scoped_refptr<ShapeResult> CreateForTabulationCharacters(
      const Font* font,
      TextDirection direction,
      const TabSize& tab_size,
      float position,
      unsigned start_index,
      unsigned length);
  // The first glyph has |width| advance, and other glyphs have 0 advance.
  static scoped_refptr<ShapeResult> CreateForSpaces(const Font* font,
                                                    TextDirection direction,
                                                    unsigned start_index,
                                                    unsigned length,
                                                    float width);
  static scoped_refptr<ShapeResult> CreateForStretchyMathOperator(
      const Font*,
      TextDirection,
      Glyph,
      float stretch_size);
  static scoped_refptr<ShapeResult> CreateForStretchyMathOperator(
      const Font*,
      TextDirection,
      OpenTypeMathStretchData::StretchAxis,
      const OpenTypeMathStretchData::AssemblyParameters&);
  ~ShapeResult();

  // The logical width of this result.
  float Width() const { return width_; }
  LayoutUnit SnappedWidth() const { return LayoutUnit::FromFloatCeil(width_); }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return num_glyphs_; }
  const SimpleFontData* PrimaryFont() const { return primary_font_.get(); }
  bool HasFallbackFonts() const;

  // TODO(eae): Remove start_x and return value once ShapeResultBuffer has been
  // removed.
  float IndividualCharacterRanges(Vector<CharacterRange>* ranges,
                                  float start_x = 0) const;

  // The character start/end index of a range shape result.
  unsigned StartIndex() const { return start_index_; }
  unsigned EndIndex() const { return start_index_ + num_characters_; }
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool IsLtr() const { return blink::IsLtr(Direction()); }
  bool IsRtl() const { return blink::IsRtl(Direction()); }

  // True if at least one glyph in this result has vertical offsets.
  //
  // Vertical result always has vertical offsets, but horizontal result may also
  // have vertical offsets.
  bool HasVerticalOffsets() const { return has_vertical_offsets_; }

  // Note: We should not reuse |ShapeResult| if we call |ApplySpacing()|.
  bool IsAppliedSpacing() const { return is_applied_spacing_; }

  // For memory reporting.
  size_t ByteSize() const;

  // True if |StartIndex()| is safe to break.
  bool IsStartSafeToBreak() const;

  // Returns the next or previous offsets respectively at which it is safe to
  // break without reshaping.
  // The |offset| given and the return value is for the original string, between
  // |StartIndex| and |EndIndex|.
  // TODO(eae): Remove these ones the cached versions are used everywhere.
  unsigned NextSafeToBreakOffset(unsigned offset) const;
  unsigned PreviousSafeToBreakOffset(unsigned offset) const;

  // Returns the offset, relative to StartIndex, whose (origin,
  // origin+advance) contains |x|.
  unsigned OffsetForPosition(float x, BreakGlyphsOption) const;
  // Returns the offset whose glyph boundary is nearest to |x|. Depends on
  // whether |x| is on the left-half or the right-half of the glyph, it
  // determines the left-boundary or the right-boundary, then computes the
  // offset from the bidi direction.
  unsigned CaretOffsetForHitTest(float x,
                                 const StringView& text,
                                 BreakGlyphsOption) const;
  // Returns the offset that can fit to between |x| and the left or the right
  // edge. The side of the edge is determined by |line_direction|.
  unsigned OffsetToFit(float x, TextDirection line_direction) const;
  unsigned OffsetForPosition(float x,
                             const StringView& text,
                             IncludePartialGlyphsOption include_partial_glyphs,
                             BreakGlyphsOption break_glyphs) const {
    if (include_partial_glyphs == kOnlyFullGlyphs) {
      // TODO(kojii): Consider prohibiting OnlyFullGlyphs +
      // BreakGlyphsOption(true), sed only in tests.
      if (break_glyphs)
        EnsureGraphemes(text);
      return OffsetForPosition(x, break_glyphs);
    }
    return CaretOffsetForHitTest(x, text, break_glyphs);
  }

  // Returns the position for a given offset, relative to StartIndex.
  float PositionForOffset(unsigned offset,
                          AdjustMidCluster = AdjustMidCluster::kToEnd) const;
  // Similar to |PositionForOffset| with mid-glyph (mid-ligature) support.
  float CaretPositionForOffset(
      unsigned offset,
      const StringView& text,
      AdjustMidCluster = AdjustMidCluster::kToEnd) const;
  LayoutUnit SnappedStartPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatFloor(PositionForOffset(offset));
  }
  LayoutUnit SnappedEndPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatCeil(PositionForOffset(offset));
  }

  // Computes and caches a position data object as needed.
  void EnsurePositionData() const;

  const ShapeResultCharacterData& CharacterData(unsigned offset) const;
  ShapeResultCharacterData& CharacterData(unsigned offset);

  // Fast versions of OffsetForPosition and PositionForOffset that operates on
  // a cache (that needs to be pre-computed using EnsurePositionData) and that
  // does not take partial glyphs into account.
  unsigned CachedOffsetForPosition(float x) const;
  float CachedPositionForOffset(unsigned offset) const;

  // Returns the next or previous offsets respectively at which it is safe to
  // break without reshaping. Operates on a cache (that needs to be pre-computed
  // using EnsurePositionData) and does not take partial glyphs into account.
  // The |offset| given and the return value is for the original string, between
  // |StartIndex| and |EndIndex|.
  unsigned CachedNextSafeToBreakOffset(unsigned offset) const;
  unsigned CachedPreviousSafeToBreakOffset(unsigned offset) const;

  // Apply spacings (letter-spacing, word-spacing, and justification) as
  // configured to |ShapeResultSpacing|.
  // |text_start_offset| adjusts the character index in the ShapeResult before
  // giving it to |ShapeResultSpacing|. It can be negative if
  // |StartIndex()| is larger than the text in |ShapeResultSpacing|.
  void ApplySpacing(ShapeResultSpacing<String>&, int text_start_offset = 0);
  scoped_refptr<ShapeResult> ApplySpacingToCopy(ShapeResultSpacing<TextRun>&,
                                         const TextRun&) const;

  // Adds spacing between ideograph character and non-ideograph character for
  // the property of text-autospace.
  void ApplyTextAutoSpacing(
      const Vector<OffsetWithSpacing, 16>& offsets_with_spacing);

  // True if the auto-spacing is applied. See `ApplyTextAutoSpacing`.
  bool HasAutoSpacingAfter(unsigned offset) const;
  bool HasAutoSpacingBefore(unsigned offset) const;

  // Returns a line-end `ShapeResult` when breaking at `break_offset`, and the
  // glyph before `break_offset` has auto-spacing.
  scoped_refptr<ShapeResult> UnapplyAutoSpacing(unsigned start_offset,
                                                unsigned break_offset) const;

  // Adjust the offset from `OffsetForPosition` when the offset has
  // `HasAutoSpacingAfter`.
  unsigned AdjustOffsetForAutoSpacing(unsigned offset, float position) const;

  // Append a copy of a range within an existing result to another result.
  //
  // For sequential copies the vector version below is prefered as it avoid a
  // linear scan to find the first run for the range.
  void CopyRange(unsigned start, unsigned end, ShapeResult*) const;

  struct ShapeRange {
    ShapeRange(unsigned start, unsigned end, ShapeResult* target)
        : start(start), end(end), target(target) {}

    unsigned start;
    unsigned end;
    // TODO(crbug.com/1489080): When this member was given MiraclePtr
    // protection, it was found dangling.
    ShapeResult* target;
  };

  // Copy a set of sequential ranges. The ranges may not overlap and the offsets
  // must be sequential and monotically increasing.
  void CopyRanges(const ShapeRange* ranges, unsigned num_ranges) const;

  // Create a new ShapeResult instance from a range within an existing result.
  scoped_refptr<ShapeResult> SubRange(unsigned start_offset,
                                      unsigned end_offset) const;

  // Create a new ShapeResult instance with the start offset adjusted.
  scoped_refptr<ShapeResult> CopyAdjustedOffset(unsigned start_offset) const;

  // Computes the list of fonts along with the number of glyphs for each font.
  struct RunFontData {
    SimpleFontData* font_data_;
    wtf_size_t glyph_count_;
  };
  void GetRunFontData(Vector<RunFontData>* font_data) const;

  // Iterates over, and calls the specified callback function, for all the
  // glyphs. Also tracks (and returns) a seeded total advance.
  // The second version of the method only invokes the callback for glyphs in
  // the specified range and stops after the range.
  // The context parameter will be given as the first parameter for the callback
  // function.
  //
  // TODO(eae): Remove the initial_advance and index_offset parameters once
  // ShapeResultBuffer has been removed as they're only used in cases where
  // multiple ShapeResult are combined in a ShapeResultBuffer.
  float ForEachGlyph(float initial_advance, GlyphCallback, void* context) const;
  float ForEachGlyph(float initial_advance,
                     unsigned from,
                     unsigned to,
                     unsigned index_offset,
                     GlyphCallback,
                     void* context) const;

  // Iterates over, and calls the specified callback function, for all the
  // grapheme clusters. As ShapeResuls do not contain the original text content
  // a StringView with the text must be supplied and must match the text that
  // was used generate the ShapeResult.
  // Also tracks (and returns) a seeded total advance.
  // The context parameter will be given as the first parameter for the callback
  // function.
  float ForEachGraphemeClusters(const StringView& text,
                                float initial_advance,
                                unsigned from,
                                unsigned to,
                                unsigned index_offset,
                                GraphemeClusterCallback,
                                void* context) const;

  // Computes and returns the ink bounds (or visual overflow rect). This is
  // quite expensive and involves measuring each glyph accumulating the bounds.
  gfx::RectF ComputeInkBounds() const;

  // Only used by CachingWordShapeIterator
  // TODO(eae): Remove once LayoutNG lands. https://crbug.com/591099
  void SetDeprecatedInkBounds(gfx::RectF r) const {
    deprecated_ink_bounds_ = r;
  }
  gfx::RectF DeprecatedInkBounds() const { return deprecated_ink_bounds_; }

  String ToString() const;
  void ToString(StringBuilder*) const;

  using GlyphOffset = gfx::Vector2dF;
  struct RunInfo;
  RunInfo* InsertRunForTesting(unsigned start_index,
                               unsigned num_characters,
                               TextDirection,
                               Vector<uint16_t> safe_break_offsets = {});
#if DCHECK_IS_ON()
  void CheckConsistency() const;
#endif

 protected:
  ShapeResult(scoped_refptr<const SimpleFontData>,
              unsigned start_index,
              unsigned num_characters,
              TextDirection);
  ShapeResult(const Font*,
              unsigned start_index,
              unsigned num_characters,
              TextDirection);
  ShapeResult(const ShapeResult&);

  static scoped_refptr<ShapeResult> Create(const SimpleFontData* font_data,
                                           unsigned start_index,
                                           unsigned num_characters,
                                           TextDirection direction) {
    return base::AdoptRef(
        new ShapeResult(font_data, start_index, num_characters, direction));
  }

  // Ensure |grapheme_| is computed. |BreakGlyphs| is valid only when
  // |grapheme_| is computed.
  void EnsureGraphemes(const StringView& text) const;

  static unsigned CountGraphemesInCluster(base::span<const UChar>,
                                          uint16_t start_index,
                                          uint16_t end_index);

  struct GlyphIndexResult {
    STACK_ALLOCATED();

   public:
    // The total number of characters of runs_[0..run_index - 1].
    unsigned characters_on_left_runs = 0;

    // Those are the left and right character indexes of the group of glyphs
    // that were selected by OffsetForPosition.
    unsigned left_character_index = 0;
    unsigned right_character_index = 0;

    // The glyph origin of the glyph.
    float origin_x = 0;
    // The advance of the glyph.
    float advance = 0;
  };

  void OffsetForPosition(float target_x,
                         BreakGlyphsOption,
                         GlyphIndexResult*) const;

  // Helper class storing a map between offsets and x-positions.
  // Unlike the RunInfo and GlyphData structures in ShapeResult, which operates
  // in glyph order, this class stores a map between character index and the
  // total accumulated advance for each character. Allowing constant time
  // mapping from character index to x-position and O(log n) time, using binary
  // search, from x-position to character index.
  class CharacterPositionData {
    USING_FAST_MALLOC(CharacterPositionData);

   public:
    CharacterPositionData(unsigned num_characters, float width)
        : data_(num_characters), width_(width) {}

    ShapeResultCharacterData& operator[](unsigned index) {
      return data_[index];
    }
    const ShapeResultCharacterData& operator[](unsigned index) const {
      return data_[index];
    }

    // Returns the next or previous offsets respectively at which it is safe to
    // break without reshaping.
    unsigned NextSafeToBreakOffset(unsigned offset) const;
    unsigned PreviousSafeToBreakOffset(unsigned offset) const;

    // Returns the offset of the last character that fully fits before the given
    // x-position.
    unsigned OffsetForPosition(float x, bool rtl) const;

    // Returns the x-position for a given offset.
    float PositionForOffset(unsigned offset, bool rtl) const;

   private:
    // This vector is indexed by visual-offset; the character offset from the
    // left edge regardless of the TextDirection.
    Vector<ShapeResultCharacterData> data_;
    unsigned start_offset_;
    float width_;

    friend class ShapeResult;
  };

  // Append a copy of a range within an existing result to another result.
  //
  // For sequential copies the run_index argument indicates the run to start at.
  // If set to zero it will always scan from the first run which is guaranteed
  // to produce the correct results at the cost of run-time performance.
  // Returns the appropriate run_index for the next sequential invocation.
  unsigned CopyRangeInternal(unsigned run_index,
                             unsigned start,
                             unsigned end,
                             ShapeResult* target) const;

  template <bool>
  void ComputePositionData() const;
  void RecalcCharacterPositions() const;

  template <typename TextContainerType>
  void ApplySpacingImpl(ShapeResultSpacing<TextContainerType>&,
                        int text_start_offset = 0);
  template <bool is_horizontal_run>
  void ComputeGlyphPositions(ShapeResult::RunInfo*,
                             unsigned start_glyph,
                             unsigned num_glyphs,
                             hb_buffer_t*);
  // Inserts as many glyphs as possible as a RunInfo, and sets
  // |next_start_glyph| to the start index of the remaining glyphs to be
  // inserted.
  void InsertRun(scoped_refptr<ShapeResult::RunInfo>,
                 unsigned start_glyph,
                 unsigned num_glyphs,
                 unsigned* next_start_glyph,
                 hb_buffer_t*);
  void InsertRun(scoped_refptr<ShapeResult::RunInfo>);
  void ReorderRtlRuns(unsigned run_size_before);

  template <bool is_horizontal_run, bool has_non_zero_glyph_offsets>
  void ComputeRunInkBounds(const ShapeResult::RunInfo&,
                           float run_advance,
                           gfx::RectF* ink_bounds) const;

  // Common signatures with ShapeResultView, to templatize algorithms.
  const Vector<scoped_refptr<RunInfo>>& RunsOrParts() const { return runs_; }
  unsigned StartIndexOffsetForRun() const { return 0; }

  float width_;

  // Only used by CachingWordShapeIterator and stored here for memory reduction
  // reasons. See https://crbug.com/955776
  // TODO(eae): Remove once LayoutNG lands. https://crbug.com/591099
  mutable gfx::RectF deprecated_ink_bounds_;

  Vector<scoped_refptr<RunInfo>> runs_;
  scoped_refptr<const SimpleFontData> primary_font_;
  mutable std::unique_ptr<CharacterPositionData> character_position_;

  unsigned start_index_;
  unsigned num_characters_;
  unsigned num_glyphs_ : 29;

  // Overall direction for the TextRun, dictates which order each individual
  // sub run (represented by RunInfo structs in the m_runs vector) can have a
  // different text direction.
  unsigned direction_ : 1;

  // Tracks whether any runs contain glyphs with a y-offset != 0.
  unsigned has_vertical_offsets_ : 1;

  // True once called |ApplySpacing()|.
  unsigned is_applied_spacing_ : 1;

  // Note: When you add more bit flags, please consider to reduce size of
  // |num_glyphs_| or |num_characters_|.

 private:
  friend class HarfBuzzShaper;
  friend class ShapeResultBuffer;
  friend class ShapeResultBloberizer;
  friend class ShapeResultView;
  friend class ShapeResultTest;
  friend class StretchyOperatorShaper;

  template <bool has_non_zero_glyph_offsets>
  float ForEachGlyphImpl(float initial_advance,
                         GlyphCallback,
                         void* context,
                         const RunInfo& run) const;

  template <bool has_non_zero_glyph_offsets>
  float ForEachGlyphImpl(float initial_advance,
                         unsigned from,
                         unsigned to,
                         unsigned index_offset,
                         GlyphCallback,
                         void* context,
                         const RunInfo& run) const;

  // Internal implementation of `ApplyTextAutoSpacing`. The iterator can be
  // Vector::iterator or Vector::reverse_iterator, depending on the text
  // direction.
  template <TextDirection direction, class Iterator>
  void ApplyTextAutoSpacingCore(Iterator offset_begin, Iterator offset_end);
};

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const ShapeResult&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_H_
