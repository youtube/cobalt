// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_LINE_BOX_H_
#define COBALT_LAYOUT_LINE_BOX_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/layout/base_direction.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

// The rectangular area that contains the boxes that form a line is called
// a line box.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
//
// Note that the line box is not an actual box. To maintain consistency, we
// follow the nomenclature of CSS 2.1 specification. But we do not derive
// |LineBox| from |Box| because, despite the name, a line box is much closer
// to block and inline formatting contexts than to a box.
//
// A line box is a short-lived object that is constructed and destroyed during
// the layout. It does not own child boxes nor trigger their layout, which is a
// responsibility of the box that establishes this formatting context. This
// class merely knows how to determine which children to include on the line,
// where to wrap those children, and how to calculate the positions of those
// children.
//
// Due to bidirectional reordering and the horizontal and vertical alignment,
// used values of "left" and "top" can only be calculated when the line ends. To
// ensure that the line box has completed all calculations, |EndUpdates| must be
// called.
class LineBox {
 public:
  LineBox(LayoutUnit top, bool position_children_relative_to_baseline,
          const scoped_refptr<cssom::PropertyValue>& line_height,
          const render_tree::FontMetrics& font_metrics,
          bool should_collapse_leading_white_space,
          bool should_collapse_trailing_white_space,
          const LayoutParams& layout_params, BaseDirection base_direction,
          const scoped_refptr<cssom::PropertyValue>& text_align,
          const scoped_refptr<cssom::PropertyValue>& font_size,
          LayoutUnit indent_offset, LayoutUnit ellipsis_width);

  LayoutUnit top() const { return top_; }

  // Attempt to add the child box to the line, which may cause a line wrap to
  // occur if the box overflows the line and a usable wrap location is available
  // among the child boxes. When this occurs, a box is returned. This signifies
  // the last child box included on the line before the wrap and can be the
  // current child box or any previously added one. All boxes that were
  // previously added after the returned box must be re-inserted, as they were
  // not successfully placed on the line.
  //
  // The returned box can potentially be split as a result of the line wrap, in
  // which case, the portion after the split will be accessible via the child
  // box's |GetSplitSibling| call. This split sibling should be the first box
  // added the next time |TryAddChildAndMaybeWrap| is called, followed by any
  // additional child boxes that were not placed on the line.
  //
  // This call asynchronously calculates the positions and sizes of the added
  // child boxes. The used values will be undefined until |EndUpdates| is
  // called.
  Box* TryAddChildAndMaybeWrap(Box* child_box);
  // Asynchronously adds the given child box to the line, ignoring any possible
  // overflow. The used values will be undefined until |EndUpdates| is called.
  void BeginAddChildAndMaybeOverflow(Box* child_box);
  // Ensures that the calculation of used values for all previously seen child
  // boxes is completed.
  void EndUpdates();

  // WARNING: All public methods below may be called only after |EndUpdates|.

  // Whether the line starts with a white space.
  bool HasLeadingWhiteSpace() const;
  // Whether the line ends with a white space.
  bool HasTrailingWhiteSpace() const;
  // Whether all boxes on the line are collapsed.
  bool IsCollapsed() const;

  // Line boxes that contain no text, no preserved white space, no inline
  // elements with non-zero margins, padding, or borders, and no other in-flow
  // content must be treated as zero-height line boxes for the purposes
  // of determining the positions of any elements inside of them, and must be
  // treated as not existing for any other purpose.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  bool LineExists() const;
  // Returns the first box justifing the line's existence.
  // NOTE: This includes absolutely positioned children.
  size_t GetFirstBoxJustifyingLineExistenceIndex() const;

  bool IsEllipsisPlaced() const;
  math::Vector2dF GetEllipsisCoordinates() const;

  // Used to calculate the width of an inline container box.
  LayoutUnit shrink_to_fit_width() const { return shrink_to_fit_width_; }

  // Used to calculate the "auto" height of the box that establishes this
  // formatting context.
  LayoutUnit height() const { return height_; }

  // Returns the vertical offset of the baseline from the top of the line box.
  // May return non-zero values even for empty line boxes, because of the strut.
  //   https://www.w3.org/TR/CSS21/visudet.html#strut
  LayoutUnit baseline_offset_from_top() const {
    return baseline_offset_from_top_;
  }

 private:
  enum HorizontalAlignment {
    kLeftHorizontalAlignment,
    kCenterHorizontalAlignment,
    kRightHorizontalAlignment,
  };

  LayoutUnit GetAvailableWidth() const;
  void UpdateSizePreservingTrailingWhiteSpace(Box* child_box);
  bool ShouldCollapseLeadingWhiteSpaceInNextChildBox() const;
  void CollapseTrailingWhiteSpace();
  void RestoreTrailingWhiteSpace();

  bool TryAddChildWithinAvailableWidth(Box* child_box);
  bool TryWrapOverflowingBoxAndMaybeAddSplitChild(
      WrapAtPolicy wrap_at_policy,
      WrapOpportunityPolicy wrap_opportunity_policy, Box* child_box);
  bool TryWrapChildrenAtLastOpportunity(
      WrapOpportunityPolicy wrap_opportunity_policy);

  // Asynchronously estimates the static position of the given child box.
  // In CSS 2.1 the static position is only defined for absolutely positioned
  // boxes. The used values will be undefined until |EndUpdates| is called.
  void BeginEstimateStaticPositionForAbsolutelyPositionedChild(Box* child_box);

  void BeginAddChildInternal(Box* child_box);

  void ReverseChildBoxesByBidiLevels();
  void ReverseChildBoxesMeetingBidiLevelThreshold(int level);

  void UpdateChildBoxLeftPositions();
  void SetLineBoxHeightFromChildBoxes();
  void UpdateChildBoxTopPositions();
  void MaybePlaceEllipsis();

  LayoutUnit GetHeightAboveMiddleAlignmentPoint(Box* child_box);
  HorizontalAlignment ComputeHorizontalAlignment() const;

  const LayoutUnit top_;
  const bool position_children_relative_to_baseline_;
  const scoped_refptr<cssom::PropertyValue> line_height_;
  const render_tree::FontMetrics font_metrics_;
  const bool should_collapse_leading_white_space_;
  const bool should_collapse_trailing_white_space_;
  const LayoutParams layout_params_;
  const BaseDirection base_direction_;
  const scoped_refptr<cssom::PropertyValue> text_align_;
  const scoped_refptr<cssom::PropertyValue> font_size_;
  const LayoutUnit indent_offset_;
  const LayoutUnit ellipsis_width_;

  bool has_overflowed_;
  bool at_end_;

  // Non-owned list of child boxes.
  //
  // Horizontal and vertical alignments make it impossible to calculate
  // positions of children before all children are known.
  typedef std::vector<Box*> ChildBoxes;
  ChildBoxes child_boxes_;

  int num_absolutely_positioned_boxes_before_first_box_justifying_line_;

  // Accessing boxes indicated by these indices are only valid before
  // EndUpdates() is called, because the positions of the boxes may change
  // during bidirectional sorting.
  base::Optional<size_t> first_box_justifying_line_existence_index_;
  base::Optional<size_t> first_non_collapsed_child_box_index_;
  base::Optional<size_t> last_non_collapsed_child_box_index_;

  // These flags are set when EndUpdates() is called. This allows the leading
  // and trailing white space state of the line to be accessible even after
  // the boxes have been moved as a result of bidirectional sorting.
  base::Optional<bool> has_leading_white_space_;
  base::Optional<bool> has_trailing_white_space_;

  LayoutUnit shrink_to_fit_width_;
  LayoutUnit height_;
  LayoutUnit baseline_offset_from_top_;

  bool is_ellipsis_placed_;
  LayoutUnit placed_ellipsis_offset_;

  DISALLOW_COPY_AND_ASSIGN(LineBox);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_LINE_BOX_H_
