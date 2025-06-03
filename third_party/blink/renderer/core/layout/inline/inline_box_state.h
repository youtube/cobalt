// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BOX_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BOX_STATE_H_

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/fonts/font_height.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

class InlineItem;
class LogicalLineItems;
class ShapeResultView;
struct InlineItemResult;

// Fragments that require the layout position/size of ancestor are packed in
// this struct.
struct PendingPositions {
  unsigned fragment_start;
  unsigned fragment_end;
  FontHeight metrics;
  EVerticalAlign vertical_align;
};

// Represents the current box while InlineLayoutAlgorithm performs layout.
// Used 1) to cache common values for a box, and 2) to layout children that
// require ancestor position or size.
// This is a transient object only while building line boxes in a block.
struct InlineBoxState {
  DISALLOW_NEW();

 public:
  unsigned fragment_start = 0;
  const InlineItem* item = nullptr;
  Member<const ComputedStyle> style;

  // Points to style->GetFont(), or |scaled_font| in an SVG <text>.
  const Font* font;
  // A storage of SVG scaled font. Do not touch this outside of
  // InitializeFont().
  absl::optional<Font> scaled_font;

  // SVG scaling factor for this box. We use a font of which size is
  // css-specified-size * scaling_factor.
  float scaling_factor;

  // The united metrics for the current box. This includes all objects in this
  // box, including descendants, and adjusted by placement properties such as
  // 'vertical-align'.
  FontHeight metrics = FontHeight::Empty();

  // The metrics of the font for this box. This includes leadings determined by
  // by the `text-box-trim` and 'line-height' properties.
  FontHeight text_metrics = FontHeight::Empty();

  // The distance between the text-top and the baseline for this box. The
  // text-top does not include leadings.
  LayoutUnit text_top;

  // The height of the text fragments.
  LayoutUnit text_height;

  // SVG alignment-baseline presentation property resolved to a FontBaseline.
  FontBaseline alignment_type = FontBaseline::kAlphabeticBaseline;

  // These values are to create a box fragment. Set only when needs_box_fragment
  // is set.
  bool has_start_edge = false;
  bool has_end_edge = false;
  LayoutUnit margin_inline_start;
  LayoutUnit margin_inline_end;
  LineBoxStrut borders;
  LineBoxStrut padding;

  Vector<PendingPositions> pending_descendants;
  bool include_used_fonts = false;
  bool has_box_placeholder = false;
  bool needs_box_fragment = false;
  bool is_svg_text = false;

  // If you add new data members, update the move constructor.

  InlineBoxState() = default;
  // Needs the move constructor for Vector<InlineBoxState>.
  InlineBoxState(const InlineBoxState&& state);
  InlineBoxState(const InlineBoxState&) = delete;
  InlineBoxState& operator=(const InlineBoxState&) = delete;

  void Trace(Visitor* visitor) const { visitor->Trace(style); }

  // Reset |style|, |is_svg_text|, |font|, |scaled_font|, |scaling_factor|, and
  // |alignment_type|.
  void ResetStyle(const ComputedStyle& style_ref,
                  bool is_svg,
                  const LayoutObject& layout_object);

  // True if this box has a metrics, including pending ones. Pending metrics
  // will be activated in |EndBoxState()|.
  bool HasMetrics() const {
    return !metrics.IsEmpty() || !pending_descendants.empty();
  }

  // Compute text metrics for a box. All text in a box share the same
  // metrics.
  // The computed metrics is included into the line height of the current box.
  void ComputeTextMetrics(const ComputedStyle&,
                          const Font& fontref,
                          FontBaseline ifc_baseline);
  void EnsureTextMetrics(const ComputedStyle&,
                         const Font& fontref,
                         FontBaseline ifc_baseline);
  void ResetTextMetrics();

  void AccumulateUsedFonts(const ShapeResultView*);

  // 'text-top' offset for 'vertical-align'.
  LayoutUnit TextTop(FontBaseline baseline_type) const;

  // Returns if the text style can be added without open-tag.
  // Text with different font or vertical-align needs to be wrapped with an
  // inline box.
  bool CanAddTextOfStyle(const ComputedStyle&) const;

#if DCHECK_IS_ON()
  void CheckSame(const InlineBoxState&) const;
#endif
};

// Represents the inline tree structure. This class provides:
// 1) Allow access to fragments belonging to the current box.
// 2) Performs layout when the positin/size of a box was computed.
// 3) Cache common values for a box.
class CORE_EXPORT InlineLayoutStateStack {
  STACK_ALLOCATED();

 public:
  // The box state for the line box.
  InlineBoxState& LineBoxState() { return stack_.front(); }

  void SetIsEmptyLine(bool is_empty_line) { is_empty_line_ = is_empty_line; }

  // Initialize the box state stack for a new line.
  // @return The initial box state for the line.
  InlineBoxState* OnBeginPlaceItems(const InlineNode node,
                                    const ComputedStyle&,
                                    FontBaseline,
                                    bool line_height_quirk,
                                    LogicalLineItems* line_box);

  // Push a box state stack.
  InlineBoxState* OnOpenTag(const NGConstraintSpace&,
                            const InlineItem&,
                            const InlineItemResult&,
                            FontBaseline baseline_type,
                            const LogicalLineItems&);
  // This variation adds a box placeholder to |line_box|.
  InlineBoxState* OnOpenTag(const NGConstraintSpace&,
                            const InlineItem&,
                            const InlineItemResult&,
                            FontBaseline baseline_type,
                            LogicalLineItems* line_box);

  // Pop a box state stack.
  InlineBoxState* OnCloseTag(const NGConstraintSpace& space,
                             LogicalLineItems*,
                             InlineBoxState*,
                             FontBaseline);

  // Compute all the pending positioning at the end of a line.
  void OnEndPlaceItems(const NGConstraintSpace& space,
                       LogicalLineItems*,
                       FontBaseline);

  void OnBlockInInline(const FontHeight& metrics, LogicalLineItems* line_box);

  bool HasBoxFragments() const { return !box_data_list_.empty(); }

  // Notify when child is inserted at |index| to adjust child indexes.
  void ChildInserted(unsigned index);

  // This class keeps indexes to fragments in the line box, and that only
  // appending is allowed. Call this function to move all such data to the line
  // box, so that outside of this class can reorder fragments in the line box.
  void PrepareForReorder(LogicalLineItems*);

  // When reordering was complete, call this function to re-construct the box
  // data from the line box. Callers must call |PrepareForReorder()| before
  // reordering.
  void UpdateAfterReorder(LogicalLineItems*);

  // Compute inline positions of fragments and boxes.
  LayoutUnit ComputeInlinePositions(LogicalLineItems*,
                                    LayoutUnit position,
                                    bool ignore_box_margin_border_padding);

  void ApplyRelativePositioning(const NGConstraintSpace&, LogicalLineItems*);
  // Create box fragments. This function turns a flat list of children into
  // a box tree.
  void CreateBoxFragments(const NGConstraintSpace&,
                          LogicalLineItems*,
                          bool is_opaque);

#if DCHECK_IS_ON()
  void CheckSame(const InlineLayoutStateStack&) const;
#endif

 private:
  // End of a box state, either explicitly by close tag, or implicitly at the
  // end of a line.
  void EndBoxState(const NGConstraintSpace&,
                   InlineBoxState*,
                   LogicalLineItems*,
                   FontBaseline);

  void AddBoxFragmentPlaceholder(InlineBoxState*,
                                 LogicalLineItems*,
                                 FontBaseline);
  void AddBoxData(const NGConstraintSpace&, InlineBoxState*, LogicalLineItems*);

  enum PositionPending { kPositionNotPending, kPositionPending };

  // Compute vertical position for the 'vertical-align' property.
  // The timing to apply varies by values; some values apply at the layout of
  // the box was computed. Other values apply when the layout of the parent or
  // the line box was computed.
  // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
  // https://www.w3.org/TR/css-inline-3/#propdef-vertical-align
  PositionPending ApplyBaselineShift(InlineBoxState*,
                                     LogicalLineItems*,
                                     FontBaseline);

  // Computes an offset that will align the |box| with its 'alignment-baseline'
  // relative to the baseline of the line box. This takes into account both the
  // 'dominant-baseline' and 'alignment-baseline' of |box| and its parent.
  LayoutUnit ComputeAlignmentBaselineShift(const InlineBoxState* box);

  // Compute the metrics for when 'vertical-align' is 'top' and 'bottom' from
  // |pending_descendants|.
  FontHeight MetricsForTopAndBottomAlign(const InlineBoxState&,
                                         const LogicalLineItems&) const;

  // Data for a box fragment. See AddBoxFragmentPlaceholder().
  // This is a transient object only while building a line box.
  struct BoxData {
    BoxData(unsigned start,
            unsigned end,
            const InlineItem* item,
            LogicalSize size)
        : fragment_start(start),
          fragment_end(end),
          item(item),
          rect(LogicalOffset(), size) {}

    BoxData(const BoxData& other, unsigned start, unsigned end)
        : fragment_start(start),
          fragment_end(end),
          item(other.item),
          rect(other.rect) {}

    void SetFragmentRange(unsigned start_index, unsigned end_index) {
      fragment_start = start_index;
      fragment_end = end_index;
    }

    // The range of child fragments this box contains.
    unsigned fragment_start;
    unsigned fragment_end;

    const InlineItem* item;
    LogicalRect rect;

    bool has_line_left_edge = false;
    bool has_line_right_edge = false;
    LineBoxStrut borders;
    LineBoxStrut padding;
    // |CreateBoxFragment()| needs margin, border+padding, and the sum of them.
    LayoutUnit margin_line_left;
    LayoutUnit margin_line_right;
    LayoutUnit margin_border_padding_line_left;
    LayoutUnit margin_border_padding_line_right;

    unsigned parent_box_data_index = 0;
    unsigned fragmented_box_data_index = 0;

    void UpdateFragmentEdges(Vector<BoxData, 4>& list);

    const NGLayoutResult* CreateBoxFragment(const NGConstraintSpace&,
                                            LogicalLineItems*,
                                            bool is_opaque = false);
  };

  // Update start/end of the first BoxData found at |index|.
  //
  // If inline fragmentation is found, a new BoxData is added.
  //
  // Returns the index to process next. It should be given to the next call to
  // this function.
  unsigned UpdateBoxDataFragmentRange(LogicalLineItems*,
                                      unsigned index,
                                      Vector<BoxData>* fragmented_boxes);

  // Update edges of inline fragmented boxes.
  void UpdateFragmentedBoxDataEdges(Vector<BoxData>* fragmented_boxes);

  HeapVector<InlineBoxState, 4> stack_;
  Vector<BoxData, 4> box_data_list_;

  bool is_empty_line_ = false;
  bool has_block_in_inline_ = false;
  bool is_svg_text_ = false;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::InlineBoxState)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_INLINE_BOX_STATE_H_
