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

#ifndef COBALT_LAYOUT_INLINE_FORMATTING_CONTEXT_H_
#define COBALT_LAYOUT_INLINE_FORMATTING_CONTEXT_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/formatting_context.h"
#include "cobalt/layout/layout_unit.h"
#include "cobalt/layout/paragraph.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/font.h"

namespace cobalt {
namespace layout {

class LineBox;

// In an inline formatting context, boxes are laid out horizontally, one
// after the other, beginning at the top of a containing block. When several
// inline-level boxes cannot fit horizontally within a single line box,
// they are distributed among two or more vertically-stacked line boxes.
//   https://www.w3.org/TR/CSS21/visuren.html#inline-formatting
//
// An inline formatting context is a short-lived object that is constructed
// and destroyed during the layout. The inline formatting context does not own
// child boxes nor triggers their layout - it is a responsibility of the box
// that establishes this formatting context. This class merely knows how
// to update the position of the subsequent children passed to it.
//
// Due to the vertical alignment, used values of "left" and "top" are calculated
// at different times. To ensure that the inline formatting context has
// completed all calculations, |UpdateUsedTops| must be called after
// |UpdateUsedLeftAndMaybeSplit| has been called for every child box.
class InlineFormattingContext : public FormattingContext {
 public:
  InlineFormattingContext(
      const scoped_refptr<cssom::PropertyValue>& line_height,
      const render_tree::FontMetrics& font_metrics,
      const LayoutParams& layout_params, BaseDirection base_direction,
      const scoped_refptr<cssom::PropertyValue>& text_align,
      const scoped_refptr<cssom::PropertyValue>& white_space,
      const scoped_refptr<cssom::PropertyValue>& font_size,
      LayoutUnit text_indent_offset, LayoutUnit ellipsis_width);
  ~InlineFormattingContext() OVERRIDE;

  // Asynchronously calculates the position and size of the given child box and
  // updates the internal state in the preparation for the next child. The used
  // values will be undefined until |EndUpdates| is called.
  //
  // If the child box had to be split in order to fit on the line,
  // the part of the box after the split is returned. The box that establishes
  // this formatting context must re-insert the returned part right after
  // the original child box and pass this part in the next call to
  // |BeginUpdateRectAndMaybeSplit|, so that it can be split again if needed.
  scoped_refptr<Box> BeginUpdateRectAndMaybeSplit(Box* child_box);
  // Asynchronously estimates the static position of the given child box.
  // In CSS 2.1 the static position is only defined for absolutely positioned
  // boxes. The position is undefined until |EndUpdates| is called.
  void BeginEstimateStaticPosition(Box* child_box);
  // Ensures that the calculation of used values for all previously seen child
  // boxes is completed.
  void EndUpdates();

  const std::vector<math::Vector2dF>& GetEllipsesCoordinates() const;

  // WARNING: All public getters from |FormattingContext| may be called only
  //          after |EndUpdates|.

 private:
  bool TryBeginUpdateRectAndMaybeCreateLineBox(
      Box* child_box, scoped_refptr<Box>* child_box_after_split);

  void CreateLineBox();
  void DestroyLineBox();

  const scoped_refptr<cssom::PropertyValue> line_height_;
  const render_tree::FontMetrics font_metrics_;
  const LayoutParams layout_params_;
  const BaseDirection base_direction_;
  const scoped_refptr<cssom::PropertyValue> text_align_;
  const scoped_refptr<cssom::PropertyValue> white_space_;
  const scoped_refptr<cssom::PropertyValue> font_size_;
  const LayoutUnit text_indent_offset_;
  const LayoutUnit ellipsis_width_;

  // The inline formatting context only keeps the last line box, which may be
  // NULL if no child boxes were seen.
  scoped_ptr<LineBox> line_box_;

  // Number of lines boxes that affect the layout.
  int line_count_;

  // A width of the block container box when all possible line breaks are made.
  LayoutUnit preferred_min_width_;

  std::vector<math::Vector2dF> ellipses_coordinates_;

  DISALLOW_COPY_AND_ASSIGN(InlineFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_INLINE_FORMATTING_CONTEXT_H_
