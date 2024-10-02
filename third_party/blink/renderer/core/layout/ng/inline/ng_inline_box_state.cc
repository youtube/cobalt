// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

FontHeight ComputeEmphasisMarkOutsets(const ComputedStyle& style,
                                      const Font& font) {
  if (style.GetTextEmphasisMark() == TextEmphasisMark::kNone)
    return FontHeight::Empty();

  LayoutUnit emphasis_mark_height =
      LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
  DCHECK_GE(emphasis_mark_height, LayoutUnit());
  return style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver
             ? FontHeight(emphasis_mark_height, LayoutUnit())
             : FontHeight(LayoutUnit(), emphasis_mark_height);
}

}  // namespace

NGInlineBoxState::NGInlineBoxState(const NGInlineBoxState&& state)
    : fragment_start(state.fragment_start),
      item(state.item),
      style(state.style),
      scaled_font(state.scaled_font),
      scaling_factor(state.scaling_factor),
      metrics(state.metrics),
      text_metrics(state.text_metrics),
      text_top(state.text_top),
      text_height(state.text_height),
      alignment_type(state.alignment_type),
      has_start_edge(state.has_start_edge),
      has_end_edge(state.has_end_edge),
      margin_inline_start(state.margin_inline_start),
      margin_inline_end(state.margin_inline_end),
      borders(state.borders),
      padding(state.padding),
      pending_descendants(std::move(state.pending_descendants)),
      include_used_fonts(state.include_used_fonts),
      has_box_placeholder(state.has_box_placeholder),
      needs_box_fragment(state.needs_box_fragment),
      is_svg_text(state.is_svg_text) {
  if (state.scaled_font)
    font = &*scaled_font;
  else
    font = state.font;
}

void NGInlineBoxState::ResetStyle(const ComputedStyle& style_ref,
                                  bool is_svg,
                                  const LayoutObject& layout_object) {
  style = &style_ref;
  is_svg_text = is_svg;
  if (!is_svg_text) {
    scaling_factor = 1.0f;
    font = &style->GetFont();
    return;
  }
  scaled_font.emplace();
  LayoutSVGInlineText::ComputeNewScaledFontForStyle(
      layout_object, scaling_factor, *scaled_font);
  font = &*scaled_font;
  switch (style_ref.AlignmentBaseline()) {
    case EAlignmentBaseline::kAuto:
    case EAlignmentBaseline::kBaseline:
      alignment_type = style_ref.GetFontBaseline();
      break;
    case EAlignmentBaseline::kBeforeEdge:
    case EAlignmentBaseline::kTextBeforeEdge:
      alignment_type = FontBaseline::kTextOverBaseline;
      break;
    case EAlignmentBaseline::kMiddle:
      alignment_type = FontBaseline::kXMiddleBaseline;
      break;
    case EAlignmentBaseline::kCentral:
      alignment_type = FontBaseline::kCentralBaseline;
      break;
    case EAlignmentBaseline::kAfterEdge:
    case EAlignmentBaseline::kTextAfterEdge:
      alignment_type = FontBaseline::kTextUnderBaseline;
      break;
    case EAlignmentBaseline::kIdeographic:
      alignment_type = FontBaseline::kIdeographicUnderBaseline;
      break;
    case EAlignmentBaseline::kAlphabetic:
      alignment_type = FontBaseline::kAlphabeticBaseline;
      break;
    case EAlignmentBaseline::kHanging:
      alignment_type = FontBaseline::kHangingBaseline;
      break;
    case EAlignmentBaseline::kMathematical:
      alignment_type = FontBaseline::kMathBaseline;
      break;
  }
}

void NGInlineBoxState::ComputeTextMetrics(const ComputedStyle& styleref,
                                          const Font& fontref,
                                          FontBaseline ifc_baseline) {
  const auto baseline_type =
      styleref.CssDominantBaseline() == EDominantBaseline::kAuto
          ? ifc_baseline
          : styleref.GetFontBaseline();
  if (const SimpleFontData* font_data = fontref.PrimaryFont()) {
    if (is_svg_text) {
      text_metrics =
          font_data->GetFontMetrics().GetFloatFontHeight(baseline_type);
    } else {
      text_metrics = font_data->GetFontMetrics().GetFontHeight(baseline_type);
    }
  } else {
    text_metrics = FontHeight();
  }
  text_top = -text_metrics.ascent;
  text_height = text_metrics.LineHeight();

  FontHeight emphasis_marks_outsets =
      ComputeEmphasisMarkOutsets(styleref, fontref);
  if (emphasis_marks_outsets.IsEmpty()) {
    text_metrics.AddLeading(styleref.ComputedLineHeightAsFixed(fontref));
  } else {
    FontHeight emphasis_marks_metrics = text_metrics;
    emphasis_marks_metrics += emphasis_marks_outsets;
    text_metrics.AddLeading(styleref.ComputedLineHeightAsFixed(fontref));
    text_metrics.Unite(emphasis_marks_metrics);
    // TODO: Is this correct to include into text_metrics? How do we use
    // text_metrics after this point?
  }

  metrics.Unite(text_metrics);

  include_used_fonts = styleref.LineHeight().IsNegative();
}

void NGInlineBoxState::ResetTextMetrics() {
  metrics = text_metrics = FontHeight::Empty();
  text_top = text_height = LayoutUnit();
}

void NGInlineBoxState::EnsureTextMetrics(const ComputedStyle& styleref,
                                         const Font& fontref,
                                         FontBaseline ifc_baseline) {
  if (text_metrics.IsEmpty())
    ComputeTextMetrics(styleref, fontref, ifc_baseline);
}

void NGInlineBoxState::AccumulateUsedFonts(
    const ShapeResultView* shape_result) {
  const auto baseline_type = style->GetFontBaseline();
  HashSet<const SimpleFontData*> fallback_fonts;
  shape_result->FallbackFonts(&fallback_fonts);
  for (const SimpleFontData* const fallback_font : fallback_fonts) {
    FontHeight fallback_metrics =
        fallback_font->GetFontMetrics().GetFontHeight(baseline_type);
    fallback_metrics.AddLeading(
        fallback_font->GetFontMetrics().FixedLineSpacing());
    metrics.Unite(fallback_metrics);
  }
}

LayoutUnit NGInlineBoxState::TextTop(FontBaseline baseline_type) const {
  if (!text_metrics.IsEmpty())
    return text_top;
  if (const SimpleFontData* font_data = font->PrimaryFont())
    return -font_data->GetFontMetrics().FixedAscent(baseline_type);
  NOTREACHED();
  return LayoutUnit();
}

bool NGInlineBoxState::CanAddTextOfStyle(
    const ComputedStyle& text_style) const {
  if (text_style.VerticalAlign() != EVerticalAlign::kBaseline)
    return false;
  DCHECK(style);
  if (style == &text_style || &style->GetFont() == &text_style.GetFont() ||
      style->GetFont().PrimaryFont() == text_style.GetFont().PrimaryFont())
    return true;
  return false;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnBeginPlaceItems(
    const NGInlineNode node,
    const ComputedStyle& line_style,
    FontBaseline baseline_type,
    bool line_height_quirk,
    NGLogicalLineItems* line_box) {
  has_block_in_inline_ = false;
  is_svg_text_ = node.IsSvgText();
  if (stack_.empty()) {
    // For the first line, push a box state for the line itself.
    stack_.resize(1);
    NGInlineBoxState* box = &stack_.back();
    box->fragment_start = 0;
  } else {
    // For the following lines, clear states that are not shared across lines.
    for (NGInlineBoxState& box : stack_) {
      box.fragment_start = line_box->size();
      if (box.needs_box_fragment) {
        DCHECK_NE(&box, stack_.begin());
        AddBoxFragmentPlaceholder(&box, line_box, baseline_type);
      }
      if (!line_height_quirk)
        box.metrics = box.text_metrics;
      else
        box.ResetTextMetrics();
      if (box.has_start_edge) {
        // Existing box states are wrapped before they were closed, and hence
        // they do not have start edges, unless 'box-decoration-break: clone'.
        box.has_start_edge =
            box.needs_box_fragment &&
            box.style->BoxDecorationBreak() == EBoxDecorationBreak::kClone;
      }
      DCHECK(box.pending_descendants.empty());
    }
  }

  DCHECK(box_data_list_.empty());

  // Initialize the box state for the line box.
  NGInlineBoxState& line_box_state = LineBoxState();
  if (line_box_state.style != &line_style) {
    line_box_state.ResetStyle(line_style, node.IsSvgText(),
                              *node.GetLayoutBox());

    // Use a "strut" (a zero-width inline box with the element's font and
    // line height properties) as the initial metrics for the line box.
    // https://drafts.csswg.org/css2/visudet.html#strut
    if (!line_height_quirk) {
      line_box_state.ComputeTextMetrics(line_style, *line_box_state.font,
                                        baseline_type);
    }
  }

  return &stack_.back();
}

NGInlineBoxState* NGInlineLayoutStateStack::OnOpenTag(
    const NGConstraintSpace& space,
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    FontBaseline baseline_type,
    NGLogicalLineItems* line_box) {
  NGInlineBoxState* box =
      OnOpenTag(space, item, item_result, baseline_type, *line_box);
  box->needs_box_fragment = item.ShouldCreateBoxFragment();
  if (box->needs_box_fragment)
    AddBoxFragmentPlaceholder(box, line_box, baseline_type);
  return box;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnOpenTag(
    const NGConstraintSpace& space,
    const NGInlineItem& item,
    const NGInlineItemResult& item_result,
    FontBaseline baseline_type,
    const NGLogicalLineItems& line_box) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  stack_.resize(stack_.size() + 1);
  NGInlineBoxState* box = &stack_.back();
  box->fragment_start = line_box.size();
  box->ResetStyle(style, is_svg_text_, *item.GetLayoutObject());
  box->item = &item;
  box->has_start_edge = true;
  box->margin_inline_start = item_result.margins.inline_start;
  box->margin_inline_end = item_result.margins.inline_end;
  box->borders = item_result.borders;
  box->padding = item_result.padding;
  if (space.IsInsideRepeatableContent()) {
    // Avoid culled inlines when inside repeatable content (fixed-positioned
    // elements when printing and fragmented tables with headers and footers).
    // We cannot represent them correctly as culled.
    if (auto* layout_inline = DynamicTo<LayoutInline>(item.GetLayoutObject()))
      layout_inline->SetShouldCreateBoxFragment();
  }
  return box;
}

NGInlineBoxState* NGInlineLayoutStateStack::OnCloseTag(
    const NGConstraintSpace& space,
    NGLogicalLineItems* line_box,
    NGInlineBoxState* box,
    FontBaseline baseline_type) {
  DCHECK_EQ(box, &stack_.back());
  box->has_end_edge = true;
  EndBoxState(space, box, line_box, baseline_type);
  // TODO(kojii): When the algorithm restarts from a break token, the stack may
  // underflow. We need either synthesize a missing box state, or push all
  // parents on initialize.
  stack_.pop_back();
  return &stack_.back();
}

void NGInlineLayoutStateStack::OnEndPlaceItems(const NGConstraintSpace& space,
                                               NGLogicalLineItems* line_box,
                                               FontBaseline baseline_type) {
  for (auto& box : base::Reversed(stack_)) {
    if (!box.has_end_edge && box.needs_box_fragment &&
        box.style->BoxDecorationBreak() == EBoxDecorationBreak::kClone)
      box.has_end_edge = true;
    EndBoxState(space, &box, line_box, baseline_type);
  }

  // Up to this point, the offset of inline boxes are stored in placeholder so
  // that |ApplyBaselineShift()| can compute offset for both children and boxes.
  // Copy the final offset to |box_data_list_|.
  for (BoxData& box_data : box_data_list_) {
    const NGLogicalLineItem& placeholder = (*line_box)[box_data.fragment_start];
    DCHECK(placeholder.IsPlaceholder());
    box_data.rect.offset = placeholder.rect.offset;
  }
}

void NGInlineLayoutStateStack::EndBoxState(const NGConstraintSpace& space,
                                           NGInlineBoxState* box,
                                           NGLogicalLineItems* line_box,
                                           FontBaseline baseline_type) {
  if (box->needs_box_fragment)
    AddBoxData(space, box, line_box);

  PositionPending position_pending =
      ApplyBaselineShift(box, line_box, baseline_type);

  // We are done here if there is no parent box.
  if (box == stack_.begin())
    return;
  NGInlineBoxState& parent_box = *std::prev(box);

  // Unite the metrics to the parent box.
  if (position_pending == kPositionNotPending)
    parent_box.metrics.Unite(box->metrics);
}

void NGInlineLayoutStateStack::OnBlockInInline(const FontHeight& metrics,
                                               NGLogicalLineItems* line_box) {
  DCHECK(!has_block_in_inline_);
  has_block_in_inline_ = true;

  for (NGInlineBoxState& box : stack_)
    box.metrics = metrics;

  // Update the metrics in placeholders.
  const LayoutUnit line_height = metrics.LineHeight();
  for (NGLogicalLineItem& item : *line_box) {
    DCHECK(item.IsPlaceholder());
    item.rect.offset.block_offset = LayoutUnit();
    item.rect.size.block_size = line_height;
  }
}

// Crete a placeholder for a box fragment.
// We keep a flat list of fragments because it is more suitable for operations
// such as ApplyBaselineShift. Later, CreateBoxFragments() creates box fragments
// from placeholders.
void NGInlineLayoutStateStack::AddBoxFragmentPlaceholder(
    NGInlineBoxState* box,
    NGLogicalLineItems* line_box,
    FontBaseline baseline_type) {
  DCHECK(box != stack_.begin() &&
         box->item->Type() != NGInlineItem::kAtomicInline);
  box->has_box_placeholder = true;

  LayoutUnit block_offset;
  LayoutUnit block_size;
  if (!is_empty_line_) {
    // The inline box should have the height of the font metrics without the
    // line-height property. Compute from style because |box->metrics| includes
    // the line-height property.
    FontHeight metrics;
    if (const auto* font_data = box->font->PrimaryFont()) {
      metrics =
          is_svg_text_
              ? font_data->GetFontMetrics().GetFloatFontHeight(baseline_type)
              : font_data->GetFontMetrics().GetFontHeight(baseline_type);
    }

    // Extend the block direction of the box by borders and paddings. Inline
    // direction is already included into positions in NGLineBreaker.
    block_offset =
        -metrics.ascent - (box->borders.line_over + box->padding.line_over);
    block_size = metrics.LineHeight() + box->borders.BlockSum() +
                 box->padding.BlockSum();
  }
  line_box->AddChild(block_offset, block_size);
  DCHECK((*line_box)[line_box->size() - 1].IsPlaceholder());
}

// Add a |BoxData|, for each close-tag that needs a box fragment.
void NGInlineLayoutStateStack::AddBoxData(const NGConstraintSpace& space,
                                          NGInlineBoxState* box,
                                          NGLogicalLineItems* line_box) {
  DCHECK(box->needs_box_fragment);
  DCHECK(box->style);
  const ComputedStyle& style = *box->style;
  NGLogicalLineItem& placeholder = (*line_box)[box->fragment_start];
  DCHECK(placeholder.IsPlaceholder());
  const unsigned fragment_end = line_box->size();
  DCHECK(box->item);
  BoxData& box_data = box_data_list_.emplace_back(
      box->fragment_start, fragment_end, box->item, placeholder.Size());
  box_data.borders = box->borders;
  box_data.padding = box->padding;
  if (box->has_start_edge) {
    box_data.has_line_left_edge = true;
    box_data.margin_line_left = box->margin_inline_start;
    box_data.margin_border_padding_line_left = box->margin_inline_start +
                                               box->borders.inline_start +
                                               box->padding.inline_start;
  } else {
    box_data.borders.inline_start = LayoutUnit();
    box_data.padding.inline_start = LayoutUnit();
  }
  if (box->has_end_edge) {
    box_data.has_line_right_edge = true;
    box_data.margin_line_right = box->margin_inline_end;
    box_data.margin_border_padding_line_right = box->margin_inline_end +
                                                box->borders.inline_end +
                                                box->padding.inline_end;
  } else {
    box_data.borders.inline_end = LayoutUnit();
    box_data.padding.inline_end = LayoutUnit();
  }
  if (IsRtl(style.Direction())) {
    std::swap(box_data.has_line_left_edge, box_data.has_line_right_edge);
    std::swap(box_data.margin_line_left, box_data.margin_line_right);
    std::swap(box_data.margin_border_padding_line_left,
              box_data.margin_border_padding_line_right);
  }

  DCHECK((*line_box)[box->fragment_start].IsPlaceholder());
  DCHECK_GT(fragment_end, box->fragment_start);
  if (fragment_end > box->fragment_start + 1)
    return;

  // Do not defer creating a box fragment if this is an empty inline box.
  // An empty box fragment is still flat that we do not have to defer.
  // Also, placeholders cannot be reordred if empty.
  placeholder.rect.offset.inline_offset += box_data.margin_line_left;
  placeholder.rect.offset +=
      ComputeRelativeOffsetForInline(space, *box_data.item->Style());
  LayoutUnit advance = box_data.margin_border_padding_line_left +
                       box_data.margin_border_padding_line_right;
  box_data.rect.size.inline_size =
      advance - box_data.margin_line_left - box_data.margin_line_right;
  placeholder.layout_result = box_data.CreateBoxFragment(space, line_box);
  placeholder.inline_size = advance;
  DCHECK(!placeholder.children_count);
  box_data_list_.pop_back();
}

void NGInlineLayoutStateStack::ChildInserted(unsigned index) {
  for (NGInlineBoxState& state : stack_) {
    if (state.fragment_start >= index)
      ++state.fragment_start;
    DCHECK(state.pending_descendants.empty());
  }
  for (BoxData& box_data : box_data_list_) {
    if (box_data.fragment_start >= index)
      ++box_data.fragment_start;
    if (box_data.fragment_end >= index)
      ++box_data.fragment_end;
  }
}

void NGInlineLayoutStateStack::PrepareForReorder(NGLogicalLineItems* line_box) {
  // There's nothing to do if no boxes.
  if (box_data_list_.empty())
    return;

  // Set indexes of BoxData to the children of the line box.
  unsigned box_data_index = 0;
  for (const BoxData& box_data : box_data_list_) {
    box_data_index++;
    DCHECK((*line_box)[box_data.fragment_start].IsPlaceholder());
    for (unsigned i = box_data.fragment_start; i < box_data.fragment_end; i++) {
      NGLogicalLineItem& child = (*line_box)[i];
      unsigned child_box_data_index = child.box_data_index;
      if (!child_box_data_index) {
        child.box_data_index = box_data_index;
        continue;
      }

      // This |box_data| has child boxes. Set up |parent_box_data_index| to
      // represent the box nesting structure.
      while (child_box_data_index != box_data_index) {
        BoxData* child_box_data = &box_data_list_[child_box_data_index - 1];
        child_box_data_index = child_box_data->parent_box_data_index;
        if (!child_box_data_index) {
          child_box_data->parent_box_data_index = box_data_index;
          break;
        }
      }
    }
  }
}

void NGInlineLayoutStateStack::UpdateAfterReorder(
    NGLogicalLineItems* line_box) {
  // There's nothing to do if no boxes.
  if (box_data_list_.empty())
    return;

  // Compute start/end of boxes from the children of the line box.
  // Clear start/end first.
  for (BoxData& box_data : box_data_list_)
    box_data.fragment_start = box_data.fragment_end = 0;

  // Scan children and update start/end from their box_data_index.
  Vector<BoxData> fragmented_boxes;
  for (unsigned index = 0; index < line_box->size();)
    index = UpdateBoxDataFragmentRange(line_box, index, &fragmented_boxes);

  // If any inline fragmentation occurred due to BiDi reorder, append them and
  // adjust box edges.
  if (UNLIKELY(!fragmented_boxes.empty()))
    UpdateFragmentedBoxDataEdges(&fragmented_boxes);

#if DCHECK_IS_ON()
  // Check all BoxData have ranges.
  for (const BoxData& box_data : box_data_list_) {
    DCHECK_NE(box_data.fragment_end, 0u);
    DCHECK_GT(box_data.fragment_end, box_data.fragment_start);
  }
  // Check all |box_data_index| were migrated to BoxData.
  for (const NGLogicalLineItem& child : *line_box) {
    DCHECK_EQ(child.box_data_index, 0u);
  }
#endif
}

unsigned NGInlineLayoutStateStack::UpdateBoxDataFragmentRange(
    NGLogicalLineItems* line_box,
    unsigned index,
    Vector<BoxData>* fragmented_boxes) {
  // Find the first line box item that should create a box fragment.
  for (; index < line_box->size(); index++) {
    NGLogicalLineItem* start = &(*line_box)[index];
    const unsigned box_data_index = start->box_data_index;
    if (!box_data_index)
      continue;
    // |box_data_list_[box_data_index - 1]| is the box for |start| child.
    // Avoid keeping a pointer to the |BoxData| because it maybe invalidated as
    // we add to |box_data_list_|.

    // As |box_data_index| is converted to start/end of BoxData, update
    // |box_data_index| to the parent box, or to 0 if no parent boxes.
    // This allows including this box to the nested parent box.
    start->box_data_index =
        box_data_list_[box_data_index - 1].parent_box_data_index;

    // Find the end line box item.
    const unsigned start_index = index;
    for (index++; index < line_box->size(); index++) {
      NGLogicalLineItem* end = &(*line_box)[index];

      // If we found another box that maybe included in this box, update it
      // first. Updating will change |end->box_data_index| so that we can
      // determine if it should be included into this box or not.
      // It also changes other BoxData, but not the one we're dealing with here
      // because the update is limited only when its |box_data_index| is lower.
      while (end->box_data_index && end->box_data_index < box_data_index) {
        UpdateBoxDataFragmentRange(line_box, index, fragmented_boxes);
      }

      if (box_data_index != end->box_data_index)
        break;
      end->box_data_index =
          box_data_list_[box_data_index - 1].parent_box_data_index;
    }

    // If this is the first range for this BoxData, set it.
    if (!box_data_list_[box_data_index - 1].fragment_end) {
      box_data_list_[box_data_index - 1].SetFragmentRange(start_index, index);
    } else {
      // This box is fragmented by BiDi reordering. Add a new BoxData for the
      // fragmented range.
      BoxData& fragmented_box = fragmented_boxes->emplace_back(
          box_data_list_[box_data_index - 1], start_index, index);
      fragmented_box.fragmented_box_data_index = box_data_index;
    }
    // If this box has parent boxes, we need to process it again.
    if (box_data_list_[box_data_index - 1].parent_box_data_index)
      return start_index;
    return index;
  }
  return index;
}

void NGInlineLayoutStateStack::UpdateFragmentedBoxDataEdges(
    Vector<BoxData>* fragmented_boxes) {
  DCHECK(!fragmented_boxes->empty());
  // Append in the descending order of |fragmented_box_data_index| because the
  // indices will change as boxes are inserted into |box_data_list_|.
  std::sort(fragmented_boxes->begin(), fragmented_boxes->end(),
            [](const BoxData& a, const BoxData& b) {
              if (a.fragmented_box_data_index != b.fragmented_box_data_index) {
                return a.fragmented_box_data_index <
                       b.fragmented_box_data_index;
              }
              DCHECK_NE(a.fragment_start, b.fragment_start);
              return a.fragment_start < b.fragment_start;
            });
  for (BoxData& fragmented_box : base::Reversed(*fragmented_boxes)) {
    // Insert the fragmented box to right after the box it was fragmented from.
    // The order in the |box_data_list_| is critical when propagating child
    // fragment data such as OOF to ancestors.
    const unsigned insert_at = fragmented_box.fragmented_box_data_index;
    DCHECK_GT(insert_at, 0u);
    fragmented_box.fragmented_box_data_index = 0;
    box_data_list_.insert(insert_at, fragmented_box);

    // Adjust box data indices by the insertion.
    for (BoxData& box_data : box_data_list_) {
      if (box_data.fragmented_box_data_index >= insert_at)
        ++box_data.fragmented_box_data_index;
    }

    // Set the index of the last fragment to the original box. This is needed to
    // update fragment edges.
    const unsigned fragmented_from = insert_at - 1;
    if (!box_data_list_[fragmented_from].fragmented_box_data_index)
      box_data_list_[fragmented_from].fragmented_box_data_index = insert_at;
  }

  // Move the line-right edge to the last fragment.
  for (BoxData& box_data : box_data_list_) {
    if (box_data.fragmented_box_data_index)
      box_data.UpdateFragmentEdges(box_data_list_);
  }
}

void NGInlineLayoutStateStack::BoxData::UpdateFragmentEdges(
    Vector<BoxData, 4>& list) {
  DCHECK(fragmented_box_data_index);

  // If this box has the right edge, move it to the last fragment.
  if (has_line_right_edge) {
    BoxData& last = list[fragmented_box_data_index];
    last.has_line_right_edge = true;
    last.margin_line_right = margin_line_right;
    last.margin_border_padding_line_right = margin_border_padding_line_right;
    last.padding.inline_end = padding.inline_end;

    has_line_right_edge = false;
    margin_line_right = margin_border_padding_line_right = padding.inline_end =
        LayoutUnit();
  }
}

LayoutUnit NGInlineLayoutStateStack::ComputeInlinePositions(
    NGLogicalLineItems* line_box,
    LayoutUnit position,
    bool ignore_box_margin_border_padding) {
  // At this point, children are in the visual order, and they have their
  // origins at (0, 0). Accumulate inline offset from left to right.
  for (NGLogicalLineItem& child : *line_box) {
    child.margin_line_left = child.rect.offset.inline_offset;
    child.rect.offset.inline_offset += position;
    // Box margins/boders/paddings will be processed later.
    // TODO(kojii): we could optimize this if the reordering did not occur.
    if (!child.HasFragment())
      continue;
    position += child.inline_size;
  }

  if (box_data_list_.empty())
    return position;

  if (!ignore_box_margin_border_padding) {
    // Adjust child offsets for margin/border/padding of inline boxes.
    for (BoxData& box_data : box_data_list_) {
      unsigned start = box_data.fragment_start;
      unsigned end = box_data.fragment_end;
      DCHECK_GT(end, start);

      if (box_data.margin_border_padding_line_left) {
        line_box->MoveInInlineDirection(
            box_data.margin_border_padding_line_left, start, line_box->size());
        position += box_data.margin_border_padding_line_left;
      }

      if (box_data.margin_border_padding_line_right) {
        line_box->MoveInInlineDirection(
            box_data.margin_border_padding_line_right, end, line_box->size());
        position += box_data.margin_border_padding_line_right;
      }
    }
  }

  // Compute positions and sizes of inline boxes.
  //
  // Accumulate margin/border/padding of boxes for each child, to place nested
  // parent boxes relative to the leaf (text or atomic inline) child.
  struct LinePadding {
    LayoutUnit line_left;
    LayoutUnit line_right;
  };
  Vector<LinePadding, 32> accumulated_padding(line_box->size());
  for (BoxData& box_data : box_data_list_) {
    // Compute line-left and line-right edge of this box by accomodating
    // border/padding of this box and margin/border/padding of descendants
    // boxes, while accumulating its margin/border/padding.
    unsigned start = box_data.fragment_start;
    NGLogicalLineItem& start_child = (*line_box)[start];
    LayoutUnit line_left_offset =
        start_child.rect.offset.inline_offset - start_child.margin_line_left;
    LinePadding& start_padding = accumulated_padding[start];

    DCHECK_GT(box_data.fragment_end, start);
    unsigned last = box_data.fragment_end - 1;
    NGLogicalLineItem& last_child = (*line_box)[last];
    LayoutUnit line_right_offset = last_child.rect.offset.inline_offset -
                                   last_child.margin_line_left +
                                   last_child.inline_size;
    LinePadding& last_padding = accumulated_padding[last];

    if (!ignore_box_margin_border_padding) {
      start_padding.line_left += box_data.margin_border_padding_line_left;
      last_padding.line_right += box_data.margin_border_padding_line_right;
      line_left_offset += box_data.margin_line_left;
      line_right_offset -= box_data.margin_line_right;
    }

    line_left_offset -= start_padding.line_left;
    line_right_offset += last_padding.line_right;

    box_data.rect.offset.inline_offset = line_left_offset;
    box_data.rect.size.inline_size = line_right_offset - line_left_offset;
  }

  return position;
}

void NGInlineLayoutStateStack::ApplyRelativePositioning(
    const NGConstraintSpace& space,
    NGLogicalLineItems* line_box) {
  if (box_data_list_.empty())
    return;

  // The final position of any inline boxes, (<span>, etc) are stored on
  // |BoxData::rect|. As we don't have a mapping from |NGLogicalLineItem| to
  // |BoxData| we store the accumulated relative offsets, and then apply the
  // final adjustment at the end of this function.
  Vector<LogicalOffset, 32> accumulated_offsets(line_box->size());

  for (BoxData& box_data : box_data_list_) {
    unsigned start = box_data.fragment_start;
    unsigned end = box_data.fragment_end;
    const LogicalOffset relative_offset =
        ComputeRelativeOffsetForInline(space, *box_data.item->Style());

    // Move all children for this box.
    for (unsigned index = start; index < end; index++) {
      auto& child = (*line_box)[index];
      child.rect.offset += relative_offset;
      accumulated_offsets[index] += relative_offset;
    }
  }

  // Apply the final accumulated relative position offset for each box.
  for (BoxData& box_data : box_data_list_)
    box_data.rect.offset += accumulated_offsets[box_data.fragment_start];
}

void NGInlineLayoutStateStack::CreateBoxFragments(
    const NGConstraintSpace& space,
    NGLogicalLineItems* line_box,
    bool is_opaque) {
  DCHECK(!box_data_list_.empty());

  for (BoxData& box_data : box_data_list_) {
    unsigned start = box_data.fragment_start;
    unsigned end = box_data.fragment_end;
    DCHECK_GT(end, start);
    NGLogicalLineItem* child = &(*line_box)[start];
    DCHECK(box_data.item->ShouldCreateBoxFragment());
    const NGLayoutResult* box_fragment =
        box_data.CreateBoxFragment(space, line_box, is_opaque);
    if (child->IsPlaceholder()) {
      child->layout_result = std::move(box_fragment);
      child->rect = box_data.rect;
      child->children_count = end - start;
      continue;
    }

    // |AddBoxFragmentPlaceholder| adds a placeholder at |fragment_start|, but
    // bidi reordering may move it. Insert in such case.
    line_box->InsertChild(start, std::move(box_fragment), box_data.rect,
                          end - start + 1);
    ChildInserted(start + 1);
  }

  box_data_list_.clear();
}

const NGLayoutResult* NGInlineLayoutStateStack::BoxData::CreateBoxFragment(
    const NGConstraintSpace& space,
    NGLogicalLineItems* line_box,
    bool is_opaque) {
  DCHECK(item);
  DCHECK(item->Style());
  const ComputedStyle& style = *item->Style();

  NGFragmentGeometry fragment_geometry;
  fragment_geometry.border_box_size = {
      rect.size.inline_size.ClampNegativeToZero(), rect.size.block_size};
  fragment_geometry.border =
      NGBoxStrut(borders, IsFlippedLinesWritingMode(style.GetWritingMode()));
  fragment_geometry.padding =
      NGBoxStrut(padding, IsFlippedLinesWritingMode(style.GetWritingMode()));

  // Because children are already in the visual order, use LTR for the
  // fragment builder so that it should not transform the coordinates for RTL.
  NGBoxFragmentBuilder box(item->GetLayoutObject(), &style, space,
                           {style.GetWritingMode(), TextDirection::kLtr});
  box.SetInitialFragmentGeometry(fragment_geometry);
  box.SetBoxType(NGPhysicalFragment::kInlineBox);
  box.SetStyleVariant(item->StyleVariant());

  if (UNLIKELY(is_opaque)) {
    box.SetIsOpaque();
    box.SetSidesToInclude({false, false, false, false});
  } else {
    // Inline boxes have block start/end borders, even when its containing block
    // was fragmented. Fragmenting a line box in block direction is not
    // supported today.
    box.SetSidesToInclude(
        {true, has_line_right_edge, true, has_line_left_edge});
  }

  for (unsigned i = fragment_start; i < fragment_end; i++) {
    NGLogicalLineItem& child = (*line_box)[i];

    // If |child| has a fragment created by previous |CreateBoxFragment|, skip
    // children that were already added to |child|.
    if (child.children_count)
      i += child.children_count - 1;

    if (child.out_of_flow_positioned_box) {
      DCHECK(item->GetLayoutObject()->IsLayoutInline());
      NGBlockNode oof_box(
          To<LayoutBox>(child.out_of_flow_positioned_box.Get()));

      // child.offset is the static position wrt. the linebox. As we are adding
      // this as a child of an inline level fragment, we adjust the static
      // position to be relative to this fragment.
      LogicalOffset static_offset = child.rect.offset - rect.offset;

      box.AddOutOfFlowInlineChildCandidate(oof_box, static_offset,
                                           child.container_direction);
      child.out_of_flow_positioned_box = nullptr;
      continue;
    }

    // Propagate any OOF-positioned descendants from any atomic-inlines, etc.
    if (child.layout_result) {
      box.PropagateChildData(child.layout_result->PhysicalFragment(),
                             child.rect.offset - rect.offset -
                                 ComputeRelativeOffsetForInline(
                                     space, child.PhysicalFragment()->Style()),
                             ComputeRelativeOffsetForOOFInInline(
                                 space, child.PhysicalFragment()->Style()));
    }

    // |NGFragmentItems| has a flat list of all descendants, except
    // OOF-positioned descendants. We still create a |NGPhysicalBoxFragment|,
    // but don't add children to it and keep them in the flat list.
  }

  // Inline boxes that produce DisplayItemClient should do full paint
  // invalidations.
  item->GetLayoutObject()->SetShouldDoFullPaintInvalidation();

  box.MoveOutOfFlowDescendantCandidatesToDescendants();
  return box.ToInlineBoxFragment();
}

NGInlineLayoutStateStack::PositionPending
NGInlineLayoutStateStack::ApplyBaselineShift(NGInlineBoxState* box,
                                             NGLogicalLineItems* line_box,
                                             FontBaseline baseline_type) {
  // The `vertical-align` property should not apply to the line wrapper for
  // block-in-inline.
  if (UNLIKELY(has_block_in_inline_)) {
    DCHECK(box->pending_descendants.empty());
    return kPositionNotPending;
  }

  // Some 'vertical-align' values require the size of their parents. Align all
  // such descendant boxes that require the size of this box; they are queued in
  // |pending_descendants|.
  LayoutUnit baseline_shift;
  if (!box->pending_descendants.empty()) {
    bool has_top_or_bottom = false;
    for (NGPendingPositions& child : box->pending_descendants) {
      // In quirks mode, metrics is empty if no content.
      if (child.metrics.IsEmpty())
        child.metrics = FontHeight();
      switch (child.vertical_align) {
        case EVerticalAlign::kTextTop:
          baseline_shift = child.metrics.ascent + box->TextTop(baseline_type);
          break;
        case EVerticalAlign::kTextBottom:
          if (const SimpleFontData* font_data = box->font->PrimaryFont()) {
            LayoutUnit text_bottom =
                font_data->GetFontMetrics().FixedDescent(baseline_type);
            baseline_shift = text_bottom - child.metrics.descent;
            break;
          }
          NOTREACHED();
          break;
        case EVerticalAlign::kTop:
        case EVerticalAlign::kBottom:
          has_top_or_bottom = true;
          continue;
        default:
          NOTREACHED();
          continue;
      }
      child.metrics.Move(baseline_shift);
      box->metrics.Unite(child.metrics);
      line_box->MoveInBlockDirection(baseline_shift, child.fragment_start,
                                     child.fragment_end);
    }
    // `top` and `bottom` need to be applied after all other values are applied,
    // because they align to the maximum metrics, but the maximum metrics may
    // depend on other pending descendants for this box.
    if (has_top_or_bottom) {
      FontHeight max = MetricsForTopAndBottomAlign(*box, *line_box);
      for (NGPendingPositions& child : box->pending_descendants) {
        switch (child.vertical_align) {
          case EVerticalAlign::kTop:
            baseline_shift = child.metrics.ascent - max.ascent;
            break;
          case EVerticalAlign::kBottom:
            baseline_shift = max.descent - child.metrics.descent;
            break;
          case EVerticalAlign::kTextTop:
          case EVerticalAlign::kTextBottom:
            continue;
          default:
            NOTREACHED();
            continue;
        }
        child.metrics.Move(baseline_shift);
        box->metrics.Unite(child.metrics);
        line_box->MoveInBlockDirection(baseline_shift, child.fragment_start,
                                       child.fragment_end);
      }
    }
    box->pending_descendants.clear();
  }

  const ComputedStyle& style = *box->style;
  EVerticalAlign vertical_align = style.VerticalAlign();
  if (!is_svg_text_ && vertical_align == EVerticalAlign::kBaseline)
    return kPositionNotPending;

  if (UNLIKELY(box->item &&
               IsA<LayoutNGTextCombine>(box->item->GetLayoutObject()))) {
    // Text content in text-combine-upright:all is layout in horizontally, so
    // we don't need to move text combine box.
    // See "text-combine-shrink-to-fit.html".
    return kPositionNotPending;
  }

  // Check if there are any fragments to move.
  unsigned fragment_end = line_box->size();
  if (box->fragment_start == fragment_end)
    return kPositionNotPending;

  // SVG <text> supports not |vertical-align| but |baseline-shift|.
  // https://drafts.csswg.org/css-inline/#propdef-vertical-align says
  // |vertical-align| is a shorthand property of |baseline-shift| and
  // |alignment-baseline|. However major browsers have never supported
  // |vertical-align| in SVG <text>. Also, the shift amount computation
  // for |baseline-shift| is not same as one for |vertical-align|.
  // For now we follow the legacy behavior. If we'd like to follow the
  // standard, first we should add a UseCounter for non-zero
  // |baseline-shift|.
  if (is_svg_text_) {
    switch (style.BaselineShiftType()) {
      case EBaselineShiftType::kLength: {
        const Length& length = style.BaselineShift();
        // ValueForLength() should be called with unscaled values.
        baseline_shift =
            LayoutUnit(-SVGLengthContext::ValueForLength(
                           length, style,
                           box->font->GetFontDescription().ComputedPixelSize() /
                               box->scaling_factor) *
                       box->scaling_factor);
        break;
      }
      case EBaselineShiftType::kSub:
        if (const auto* font_data = box->font->PrimaryFont()) {
          baseline_shift =
              LayoutUnit(font_data->GetFontMetrics().FloatHeight() / 2);
        }
        break;
      case EBaselineShiftType::kSuper:
        if (const auto* font_data = box->font->PrimaryFont()) {
          baseline_shift =
              LayoutUnit(-font_data->GetFontMetrics().FloatHeight() / 2);
        }
        break;
    }
    baseline_shift += ComputeAlignmentBaselineShift(box);
    if (!box->metrics.IsEmpty())
      box->metrics.Move(baseline_shift);
    line_box->MoveInBlockDirection(baseline_shift, box->fragment_start,
                                   fragment_end);
    return kPositionNotPending;
  }

  // 'vertical-align' aligns boxes relative to themselves, to their parent
  // boxes, or to the line box, depends on the value.
  // Because |box| is an item in |stack_|, |box[-1]| is its parent box.
  // If this box doesn't have a parent; i.e., this box is a line box,
  // 'vertical-align' has no effect.
  DCHECK(box >= stack_.begin() && box < stack_.end());
  if (box == stack_.begin())
    return kPositionNotPending;
  NGInlineBoxState& parent_box = box[-1];

  switch (vertical_align) {
    case EVerticalAlign::kSub:
      baseline_shift = parent_box.style->ComputedFontSizeAsFixed() / 5 + 1;
      break;
    case EVerticalAlign::kSuper:
      baseline_shift = -(parent_box.style->ComputedFontSizeAsFixed() / 3 + 1);
      break;
    case EVerticalAlign::kLength: {
      // 'Percentages: refer to the 'line-height' of the element itself'.
      // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
      const Length& length = style.GetVerticalAlignLength();
      LayoutUnit line_height = length.IsPercentOrCalc()
                                   ? style.ComputedLineHeightAsFixed()
                                   : box->text_metrics.LineHeight();
      baseline_shift = -ValueForLength(length, line_height);
      break;
    }
    case EVerticalAlign::kMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      if (const SimpleFontData* parent_font_data =
              parent_box.style->GetFont().PrimaryFont()) {
        baseline_shift -= LayoutUnit::FromFloatRound(
            parent_font_data->GetFontMetrics().XHeight() / 2);
      }
      break;
    case EVerticalAlign::kBaselineMiddle:
      baseline_shift = (box->metrics.ascent - box->metrics.descent) / 2;
      break;
    case EVerticalAlign::kTop:
    case EVerticalAlign::kBottom: {
      // 'top' and 'bottom' require the layout size of the nearest ancestor that
      // has 'top' or 'bottom', or the line box if none.
      NGInlineBoxState* ancestor = &parent_box;
      for (; ancestor != stack_.begin(); --ancestor) {
        if (ancestor->style->VerticalAlign() == EVerticalAlign::kTop ||
            ancestor->style->VerticalAlign() == EVerticalAlign::kBottom)
          break;
      }
      ancestor->pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
    }
    default:
      // Other values require the layout size of the parent box.
      parent_box.pending_descendants.push_back(NGPendingPositions{
          box->fragment_start, fragment_end, box->metrics, vertical_align});
      return kPositionPending;
  }
  if (!box->metrics.IsEmpty())
    box->metrics.Move(baseline_shift);
  line_box->MoveInBlockDirection(baseline_shift, box->fragment_start,
                                 fragment_end);
  return kPositionNotPending;
}

LayoutUnit NGInlineLayoutStateStack::ComputeAlignmentBaselineShift(
    const NGInlineBoxState* box) {
  LayoutUnit result;
  if (const auto* font_data = box->font->PrimaryFont()) {
    const FontMetrics& metrics = font_data->GetFontMetrics();
    result = metrics.FixedAscent(box->style->GetFontBaseline()) -
             metrics.FixedAscent(box->alignment_type);
  }

  if (box == stack_.begin())
    return result;
  if (const auto* font_data = box[-1].font->PrimaryFont()) {
    const FontMetrics& parent_metrics = font_data->GetFontMetrics();
    result -= parent_metrics.FixedAscent(box[-1].style->GetFontBaseline()) -
              parent_metrics.FixedAscent(box[-1].alignment_type);
  }

  return result;
}

FontHeight NGInlineLayoutStateStack::MetricsForTopAndBottomAlign(
    const NGInlineBoxState& box,
    const NGLogicalLineItems& line_box) const {
  DCHECK(!box.pending_descendants.empty());

  // |metrics| is the bounds of "aligned subtree", that is, bounds of
  // descendants that are not 'vertical-align: top' nor 'bottom'.
  // https://drafts.csswg.org/css2/visudet.html#propdef-vertical-align
  FontHeight metrics = box.metrics;

  // BoxData contains inline boxes to be created later. Take them into account.
  for (const BoxData& box_data : box_data_list_) {
    // Except when the box has `vertical-align: top` or `bottom`.
    DCHECK(box_data.item->Style());
    const ComputedStyle& style = *box_data.item->Style();
    EVerticalAlign vertical_align = style.VerticalAlign();
    if (vertical_align == EVerticalAlign::kTop ||
        vertical_align == EVerticalAlign::kBottom)
      continue;

    // |block_offset| is the top position when the baseline is at 0.
    const NGLogicalLineItem& placeholder = line_box[box_data.fragment_start];
    DCHECK(placeholder.IsPlaceholder());
    LayoutUnit box_ascent = -placeholder.rect.offset.block_offset;
    FontHeight box_metrics(box_ascent,
                           box_data.rect.size.block_size - box_ascent);
    // The top/bottom of inline boxes should not include their paddings.
    box_metrics.ascent -= box_data.padding.line_over;
    box_metrics.descent -= box_data.padding.line_under;
    // Include the line-height property. The inline box has the height of the
    // font metrics without the line-height included.
    box_metrics.AddLeading(style.ComputedLineHeightAsFixed());
    metrics.Unite(box_metrics);
  }

  // In quirks mode, metrics is empty if no content.
  if (metrics.IsEmpty())
    metrics = FontHeight();

  // If the height of a box that has 'vertical-align: top' or 'bottom' exceeds
  // the height of the "aligned subtree", align the edge to the "aligned
  // subtree" and extend the other edge.
  FontHeight max = metrics;
  for (const NGPendingPositions& child : box.pending_descendants) {
    if ((child.vertical_align == EVerticalAlign::kTop ||
         child.vertical_align == EVerticalAlign::kBottom) &&
        child.metrics.LineHeight() > max.LineHeight()) {
      if (child.vertical_align == EVerticalAlign::kTop) {
        max = FontHeight(metrics.ascent,
                         child.metrics.LineHeight() - metrics.ascent);
      } else if (child.vertical_align == EVerticalAlign::kBottom) {
        max = FontHeight(child.metrics.LineHeight() - metrics.descent,
                         metrics.descent);
      }
    }
  }
  return max;
}

#if DCHECK_IS_ON()
void NGInlineLayoutStateStack::CheckSame(
    const NGInlineLayoutStateStack& other) const {
  // At the beginning of each line, box_data_list_ should be empty.
  DCHECK_EQ(box_data_list_.size(), 0u);
  DCHECK_EQ(other.box_data_list_.size(), 0u);

  DCHECK_EQ(stack_.size(), other.stack_.size());
  for (unsigned i = 0; i < stack_.size(); i++) {
    stack_[i].CheckSame(other.stack_[i]);
  }
}

void NGInlineBoxState::CheckSame(const NGInlineBoxState& other) const {
  DCHECK_EQ(fragment_start, other.fragment_start);
  DCHECK_EQ(item, other.item);
  DCHECK_EQ(style, other.style);

  DCHECK_EQ(metrics, other.metrics);
  DCHECK_EQ(text_metrics, other.text_metrics);
  DCHECK_EQ(text_top, other.text_top);
  DCHECK_EQ(text_height, other.text_height);
  if (!text_metrics.IsEmpty()) {
    // |include_used_fonts| will be computed when computing |text_metrics|.
    DCHECK_EQ(include_used_fonts, other.include_used_fonts);
  }

  DCHECK_EQ(needs_box_fragment, other.needs_box_fragment);

  DCHECK_EQ(has_start_edge, other.has_start_edge);
  // |has_end_edge| may not match because it will be computed in |OnCloseTag|.

  DCHECK_EQ(margin_inline_start, other.margin_inline_start);
  DCHECK_EQ(margin_inline_end, other.margin_inline_end);
  DCHECK_EQ(borders, other.borders);
  DCHECK_EQ(padding, other.padding);

  // At the beginning of each line, box_data_list_pending_descendants should be
  // empty.
  DCHECK_EQ(pending_descendants.size(), 0u);
  DCHECK_EQ(other.pending_descendants.size(), 0u);
}
#endif

}  // namespace blink
