/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
// the layout. It does not own child boxes nor trigger their layout, which it is
// a responsibility of the box that establishes this formatting context. This
// class merely knows how to update the position of the subsequent children
// passed to it.
//
// Due to the horizontal and vertical alignment, used values of "left" and "top"
// can only be calculated when the line ends. To ensure that the line box has
// completed all calculations, |UpdateUsedTops| must be called after
// |TryUpdateUsedLeftAndMaybeSplit| or |UpdateUsedLeftAndMaybeOverflow| have
// been called for every child box.
class LineBox {
 public:
  LineBox(LayoutUnit top, bool position_children_relative_to_baseline,
          const scoped_refptr<cssom::PropertyValue>& line_height,
          const render_tree::FontMetrics& font_metrics,
          bool should_collapse_leading_white_space,
          bool should_collapse_trailing_white_space,
          const LayoutParams& layout_params, BaseDirection base_direction,
          const scoped_refptr<cssom::PropertyValue>& text_align,
          const scoped_refptr<cssom::PropertyValue>& white_space,
          const scoped_refptr<cssom::PropertyValue>& font_size,
          LayoutUnit indent_offset, LayoutUnit ellipsis_width);

  LayoutUnit top() const { return top_; }

  // Attempts to calculate the position and size of the given child box
  // if the box or a part of it fits on the line. Some parts of the calculation
  // are asynchronous, so the used values will be undefined until |EndUpdates|
  // is called.
  //
  // Returns false if the box does not fit on the line, even when split.
  //
  // If the child box had to be split in order to fit on the line, the part
  // of the box after the split is stored in |child_box_after_split|. The box
  // that establishes this formatting context must re-insert the returned part
  // right after the original child box.
  bool TryBeginUpdateRectAndMaybeSplit(
      Box* child_box, scoped_refptr<Box>* child_box_after_split);
  // Asynchronously calculates the position and size of the given child box,
  // ignoring the possible overflow. The used values will be undefined until
  // |EndUpdates| is called.
  void BeginUpdateRectAndMaybeOverflow(Box* child_box);
  // Asynchronously estimates the static position of the given child box.
  // In CSS 2.1 the static position is only defined for absolutely positioned
  // boxes. The used values will be undefined until |EndUpdates| is called.
  void BeginEstimateStaticPosition(Box* child_box);
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

  bool IsEllipsisPlaced() const;
  math::Vector2dF GetEllipsisCoordinates() const;

  // Line boxes that contain no text, no preserved white space, no inline
  // elements with non-zero margins, padding, or borders, and no other in-flow
  // content must be treated as zero-height line boxes for the purposes
  // of determining the positions of any elements inside of them, and must be
  // treated as not existing for any other purpose.
  //   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  bool line_exists() const { return line_exists_; }

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

  void BeginUpdatePosition(Box* child_box);

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
  const bool is_text_wrapping_disabled_;
  const LayoutUnit indent_offset_;
  const LayoutUnit ellipsis_width_;

  bool at_end_;
  bool line_exists_;

  // Non-owned list of child boxes.
  //
  // Horizontal and vertical alignments make it impossible to calculate
  // positions of children before all children are known.
  typedef std::vector<Box*> ChildBoxes;
  ChildBoxes child_boxes_;

  base::optional<size_t> first_non_collapsed_child_box_index_;
  base::optional<size_t> last_non_collapsed_child_box_index_;

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
