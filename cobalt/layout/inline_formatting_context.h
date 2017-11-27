// Copyright 2015 Google Inc. All Rights Reserved.
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
// that establishes this formatting context. This class merely knows how to
// update the position of children passed to it.
//
// To ensure that the inline formatting context has completed all calculations,
// |EndUpdates| must be called after all of the child boxes have been
// successfully added.
class InlineFormattingContext : public FormattingContext {
 public:
  InlineFormattingContext(
      const scoped_refptr<cssom::PropertyValue>& line_height,
      const render_tree::FontMetrics& font_metrics,
      const LayoutParams& layout_params, BaseDirection base_direction,
      const scoped_refptr<cssom::PropertyValue>& text_align,
      const scoped_refptr<cssom::PropertyValue>& font_size,
      LayoutUnit text_indent_offset, LayoutUnit ellipsis_width);
  ~InlineFormattingContext() override;

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
  // additional child boxes that were not already placed on the line.
  //
  // This call asynchronously calculates the positions and sizes of the added
  // child boxes. The used values will be undefined until |EndUpdates| is
  // called.
  Box* TryAddChildAndMaybeWrap(Box* child_box);
  // Ensures that the calculation of used values for all previously seen child
  // boxes is completed.
  void EndUpdates();

  const std::vector<math::Vector2dF>& GetEllipsesCoordinates() const;

  // WARNING: All public getters from |FormattingContext| may be called only
  //          after |EndUpdates|.

 private:
  void CreateLineBox();
  void DestroyLineBox();

  const scoped_refptr<cssom::PropertyValue> line_height_;
  const render_tree::FontMetrics font_metrics_;
  const LayoutParams layout_params_;
  const BaseDirection base_direction_;
  const scoped_refptr<cssom::PropertyValue> text_align_;
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
