/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_

#include "cc/paint/node_id.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_iterator.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

// To avoid conflicts with the DrawText macro from the Windows SDK...
#undef DrawText

namespace gfx {
class PointF;
class RectF;
}

namespace cc {
class PaintCanvas;
class PaintFlags;
}  // namespace cc

namespace blink {

class NGShapeCache;
class FontSelector;
class ShapeCache;
class TextRun;
struct TextRunPaintInfo;
struct NGTextFragmentPaintInfo;

class PLATFORM_EXPORT Font {
  DISALLOW_NEW();

 public:
  Font();
  explicit Font(const FontDescription&);
  Font(const FontDescription&, FontSelector*);
  ~Font();

  Font(const Font&);
  Font& operator=(const Font&);

  bool operator==(const Font& other) const;
  bool operator!=(const Font& other) const { return !(*this == other); }

  const FontDescription& GetFontDescription() const {
    return font_description_;
  }

  enum class DrawType { kGlyphsOnly, kGlyphsAndClusters };

  enum CustomFontNotReadyAction {
    kDoNotPaintIfFontNotReady,
    kUseFallbackIfFontNotReady
  };

  void DrawText(cc::PaintCanvas*,
                const TextRunPaintInfo&,
                const gfx::PointF&,
                const cc::PaintFlags&,
                DrawType = DrawType::kGlyphsOnly) const;
  void DrawText(cc::PaintCanvas*,
                const TextRunPaintInfo&,
                const gfx::PointF&,
                cc::NodeId node_id,
                const cc::PaintFlags&,
                DrawType = DrawType::kGlyphsOnly) const;
  void DrawText(cc::PaintCanvas*,
                const NGTextFragmentPaintInfo&,
                const gfx::PointF&,
                cc::NodeId node_id,
                const cc::PaintFlags&,
                DrawType = DrawType::kGlyphsOnly) const;
  bool DrawBidiText(cc::PaintCanvas*,
                    const TextRunPaintInfo&,
                    const gfx::PointF&,
                    CustomFontNotReadyAction,
                    const cc::PaintFlags&,
                    DrawType = DrawType::kGlyphsOnly) const;
  void DrawEmphasisMarks(cc::PaintCanvas*,
                         const TextRunPaintInfo&,
                         const AtomicString& mark,
                         const gfx::PointF&,
                         const cc::PaintFlags&) const;
  void DrawEmphasisMarks(cc::PaintCanvas*,
                         const NGTextFragmentPaintInfo&,
                         const AtomicString& mark,
                         const gfx::PointF&,
                         const cc::PaintFlags&) const;

  gfx::RectF TextInkBounds(const NGTextFragmentPaintInfo&) const;

  struct TextIntercept {
    float begin_, end_;
  };

  // Compute the text intercepts along the axis of the advance and write them
  // into the specified Vector of TextIntercepts. The number of those is zero or
  // a multiple of two, and is at most the number of glyphs * 2 in the TextRun
  // part of TextRunPaintInfo. Specify bounds for the upper and lower extend of
  // a line crossing through the text, parallel to the baseline.
  // TODO(drott): crbug.com/655154 Fix this for upright in vertical.
  void GetTextIntercepts(const TextRunPaintInfo&,
                         const cc::PaintFlags&,
                         const std::tuple<float, float>& bounds,
                         Vector<TextIntercept>&) const;
  void GetTextIntercepts(const NGTextFragmentPaintInfo&,
                         const cc::PaintFlags&,
                         const std::tuple<float, float>& bounds,
                         Vector<TextIntercept>&) const;

  // Glyph bounds will be the minimum rect containing all glyph strokes, in
  // coordinates using (<text run x position>, <baseline position>) as the
  // origin.
  float Width(const TextRun&, gfx::RectF* glyph_bounds = nullptr) const;

  int OffsetForPosition(const TextRun&,
                        float position,
                        IncludePartialGlyphsOption,
                        BreakGlyphsOption) const;
  gfx::RectF SelectionRectForText(const TextRun&,
                                  const gfx::PointF&,
                                  float height,
                                  int from = 0,
                                  int to = -1) const;

  // Returns a vector of same size as TextRun.length() with advances measured
  // in pixels from the left bounding box of the full TextRun to the left bound
  // of the glyph rendered by each character. Values should always be positive.
  Vector<double> IndividualCharacterAdvances(const TextRun&) const;

  void ExpandRangeToIncludePartialGlyphs(const TextRun&,
                                         int* from,
                                         int* to) const;

  // Metrics that we query the FontFallbackList for.
  float SpaceWidth() const {
    DCHECK(PrimaryFont());
    return (PrimaryFont() ? PrimaryFont()->SpaceWidth() : 0) +
           GetFontDescription().LetterSpacing();
  }

  // Compute the base tab width; the width when its position is zero.
  float TabWidth(const SimpleFontData*, const TabSize&) const;
  // Compute the tab width for the specified |position|.
  float TabWidth(const SimpleFontData*, const TabSize&, float position) const;
  float TabWidth(const TabSize& tab_size, float position) const {
    return TabWidth(PrimaryFont(), tab_size, position);
  }
  LayoutUnit TabWidth(const TabSize&, LayoutUnit position) const;

  int EmphasisMarkAscent(const AtomicString&) const;
  int EmphasisMarkDescent(const AtomicString&) const;
  int EmphasisMarkHeight(const AtomicString&) const;

  // This may fail and return a nullptr in case the last resort font cannot be
  // loaded. This *should* not happen but in reality it does ever now and then
  // when, for whatever reason, the last resort font cannot be loaded.
  const SimpleFontData* PrimaryFont() const;

  // Access the NG shape cache associated with this particular font object.
  // Should *not* be retained across layout calls as it may become invalid.
  NGShapeCache& GetNGShapeCache() const;

  // Access the shape cache associated with this particular font object.
  // Should *not* be retained across layout calls as it may become invalid.
  ShapeCache* GetShapeCache() const;

  // Whether the font supports shaping word by word instead of shaping the
  // full run in one go. Allows better caching for fonts where space cannot
  // participate in kerning and/or ligatures.
  bool CanShapeWordByWord() const;

  void SetCanShapeWordByWordForTesting(bool b) {
    EnsureFontFallbackList()->SetCanShapeWordByWordForTesting(b);
  }

  // Causes PrimaryFont to return nullptr, which is useful for simulating
  // a situation where the "last resort font" did not load.
  void NullifyPrimaryFontForTesting() {
    EnsureFontFallbackList()->NullifyPrimarySimpleFontDataForTesting();
  }

  void ReportNotDefGlyph() const;

  void ReportEmojiSegmentGlyphCoverage(unsigned num_clusters,
                                       unsigned num_broken_clusters) const;

 private:
  enum ForTextEmphasisOrNot { kNotForTextEmphasis, kForTextEmphasis };

  GlyphData GetEmphasisMarkGlyphData(const AtomicString&) const;

 public:
  FontSelector* GetFontSelector() const;
  FontFallbackIterator CreateFontFallbackIterator(
      FontFallbackPriority fallback_priority) const {
    EnsureFontFallbackList();
    return FontFallbackIterator(font_description_, font_fallback_list_,
                                fallback_priority);
  }

  void WillUseFontData(const String& text) const;

  bool IsFallbackValid() const;

  bool ShouldSkipDrawing() const {
    if (!font_fallback_list_)
      return false;
    return EnsureFontFallbackList()->ShouldSkipDrawing();
  }

  bool HasCustomFont() const {
    if (!font_fallback_list_)
      return false;
    return EnsureFontFallbackList()->HasCustomFont();
  }

 private:
  // TODO(xiaochengh): The function not only initializes null FontFallbackList,
  // but also syncs invalid FontFallbackList. Rename it for better readability.
  FontFallbackList* EnsureFontFallbackList() const;
  void RevalidateFontFallbackList() const;
  void ReleaseFontFallbackListRef() const;

  FontDescription font_description_;
  mutable scoped_refptr<FontFallbackList> font_fallback_list_;
};

inline const SimpleFontData* Font::PrimaryFont() const {
  return EnsureFontFallbackList()->PrimarySimpleFontData(font_description_);
}

inline FontSelector* Font::GetFontSelector() const {
  return font_fallback_list_ ? font_fallback_list_->GetFontSelector() : nullptr;
}

inline float Font::TabWidth(const SimpleFontData* font_data,
                            const TabSize& tab_size) const {
  if (!font_data)
    return GetFontDescription().LetterSpacing();
  float base_tab_width = tab_size.GetPixelSize(font_data->SpaceWidth());
  return base_tab_width ? base_tab_width : GetFontDescription().LetterSpacing();
}

}  // namespace blink

namespace WTF {

template <>
struct CrossThreadCopier<blink::Font>
    : public CrossThreadCopierPassThrough<blink::Font> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_H_
