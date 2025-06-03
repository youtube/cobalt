// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_text_combine_painter.h"

#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/ng_text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"

namespace blink {

NGTextCombinePainter::NGTextCombinePainter(
    GraphicsContext& context,
    const ComputedStyle& style,
    const LineRelativeRect& text_frame_rect)
    : TextPainterBase(context,
                      style.GetFont(),
                      text_frame_rect.offset,
                      text_frame_rect,
                      /* inline_context */ nullptr,
                      /* horizontal */ false),
      style_(style) {}

NGTextCombinePainter::~NGTextCombinePainter() = default;

void NGTextCombinePainter::Paint(const PaintInfo& paint_info,
                                 const PhysicalOffset& paint_offset,
                                 const LayoutTextCombine& text_combine) {
  if (paint_info.phase == PaintPhase::kBlockBackground ||
      paint_info.phase == PaintPhase::kForcedColorsModeBackplate ||
      paint_info.phase == PaintPhase::kFloat ||
      paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly ||
      paint_info.phase == PaintPhase::kDescendantBlockBackgroundsOnly ||
      paint_info.phase == PaintPhase::kSelfOutlineOnly) {
    // Note: We should not paint text decoration and emphasis markr in above
    // paint phases. Otherwise, text decoration and emphasis mark are painted
    // multiple time and anti-aliasing is broken.
    // See virtual/text-antialias/emphasis-combined-text.html
    return;
  }

  // Here |paint_info.phases| is one of following:
  //    PaintPhase::kSelectionDragImage
  //    PaintPhase::kTextClip
  //    PaintPhase::kForeground
  //    PaintPhase::kOutline
  // These values come from |NGBoxFragmentPainter::PaintAllPhasesAtomically()|.

  const ComputedStyle& style = text_combine.Parent()->StyleRef();
  const bool has_text_decoration = style.HasAppliedTextDecorations();
  const bool has_emphasis_mark =
      style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
  DCHECK(has_text_decoration | has_emphasis_mark);

  const LineRelativeRect& text_frame_rect =
      text_combine.ComputeTextFrameRect(paint_offset);

  // To match the logical direction
  GraphicsContextStateSaver state_saver(paint_info.context);
  paint_info.context.ConcatCTM(
      text_frame_rect.ComputeRelativeToPhysicalTransform(
          style.GetWritingMode()));

  NGTextCombinePainter text_painter(paint_info.context, style, text_frame_rect);
  const TextPaintStyle text_style = TextPainterBase::TextPaintingStyle(
      text_combine.GetDocument(), style, paint_info);

  if (has_emphasis_mark) {
    text_painter.PaintEmphasisMark(text_style, style.GetFont());
  }

  if (has_text_decoration)
    text_painter.PaintDecorations(paint_info, text_style);
}

// static
bool NGTextCombinePainter::ShouldPaint(const LayoutTextCombine& text_combine) {
  const auto& style = text_combine.Parent()->StyleRef();
  return style.HasAppliedTextDecorations() ||
         style.GetTextEmphasisMark() != TextEmphasisMark::kNone;
}

void NGTextCombinePainter::ClipDecorationsStripe(const NGTextFragmentPaintInfo&,
                                                 float upper,
                                                 float stripe_width,
                                                 float dilation) {
  // Nothing to do.
}

void NGTextCombinePainter::PaintDecorations(const PaintInfo& paint_info,
                                            const TextPaintStyle& text_style) {
  // Setup arguments for painting text decorations
  const absl::optional<AppliedTextDecoration> selection_text_decoration;
  TextDecorationInfo decoration_info(
      text_frame_rect_.offset, text_frame_rect_.size.inline_size, style_,
      /* inline_context */ nullptr, selection_text_decoration);

  const NGTextDecorationOffset decoration_offset(style_);

  // Paint underline and overline text decorations
  PaintUnderOrOverLineDecorations(NGTextFragmentPaintInfo{}, decoration_offset,
                                  decoration_info, ~TextDecorationLine::kNone,
                                  paint_info, text_style);

  // Paint line through if needed
  PaintDecorationsOnlyLineThrough(decoration_info, paint_info, text_style);
}

void NGTextCombinePainter::PaintEmphasisMark(const TextPaintStyle& text_style,
                                             const Font& emphasis_mark_font) {
  DCHECK_NE(style_.GetTextEmphasisMark(), TextEmphasisMark::kNone);
  SetEmphasisMark(style_.TextEmphasisMarkString(),
                  style_.GetTextEmphasisPosition());
  DCHECK(emphasis_mark_font.GetFontDescription().IsVerticalBaseline());
  DCHECK(emphasis_mark_);
  const SimpleFontData* const font_data = font_.PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return;
  }
  if (text_style.emphasis_mark_color != text_style.fill_color) {
    // See virtual/text-antialias/emphasis-combined-text.html
    graphics_context_.SetFillColor(text_style.emphasis_mark_color);
  }

  const auto font_ascent = font_data->GetFontMetrics().Ascent();
  const TextRun placeholder_text_run(&kIdeographicFullStopCharacter, 1);
  const gfx::PointF emphasis_mark_text_origin(
      text_frame_rect_.LineLeft().ToFloat(),
      text_frame_rect_.LineOver().ToFloat() + font_ascent +
          emphasis_mark_offset_);
  const TextRunPaintInfo text_run_paint_info(placeholder_text_run);
  graphics_context_.DrawEmphasisMarks(
      emphasis_mark_font, text_run_paint_info, emphasis_mark_,
      emphasis_mark_text_origin,
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kForeground));
}

}  // namespace blink
