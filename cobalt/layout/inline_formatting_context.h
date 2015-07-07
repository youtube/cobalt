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

#ifndef LAYOUT_INLINE_FORMATTING_CONTEXT_H_
#define LAYOUT_INLINE_FORMATTING_CONTEXT_H_

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"
#include "cobalt/layout/box.h"
#include "cobalt/layout/formatting_context.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/size_f.h"

namespace cobalt {
namespace layout {

class LineBox;

// In an inline formatting context, boxes are laid out horizontally, one
// after the other, beginning at the top of a containing block. When several
// inline-level boxes cannot fit horizontally within a single line box,
// they are distributed among two or more vertically-stacked line boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
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
      const LayoutParams& layout_params, float x_height,
      const scoped_refptr<cssom::PropertyValue>& text_align);
  ~InlineFormattingContext() OVERRIDE;

  // Asynchronously updates used values of "left" and "top" for the given child
  // box. The child's position is in undefined state until |EndQueries| is
  // called.
  //
  // If the child box had to be split in order to fit on the line,
  // the part of the box after the split is returned. The box that establishes
  // this formatting context must re-insert the returned part right after
  // the original child box and pass this part in the next call to
  // |QueryUsedPositionAndMaybeSplit|, so that it can be split again if needed.
  scoped_ptr<Box> QueryUsedRectAndMaybeSplit(Box* child_box);
  // Ensures that the calculation of used values of "left" and "top" for all
  // previously seen child boxes is completed.
  void EndQueries();

  // WARNING: All public methods below may be called only after |EndQueries|.

 private:
  void OnLineBoxDestroying();

  const LayoutParams layout_params_;

  // The inline formatting context only keeps the last line box, which may be
  // NULL if no child boxes were seen.
  scoped_ptr<LineBox> line_box_;

  // Number of lines boxes that affect the layout.
  int line_count_;

  // X-height of the font in the parent box.
  float x_height_;

  // The alignment to be used in child boxes.
  const scoped_refptr<cssom::PropertyValue> text_align_;

  DISALLOW_COPY_AND_ASSIGN(InlineFormattingContext);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_INLINE_FORMATTING_CONTEXT_H_
