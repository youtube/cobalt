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

#include "cobalt/layout/inline_container_box.h"

#include "cobalt/layout/line_box.h"
#include "cobalt/layout/used_style.h"

namespace cobalt {
namespace layout {

InlineContainerBox::InlineContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : ContainerBox(computed_style, transitions, used_style_provider),
      should_collapse_leading_white_space_(false),
      should_collapse_trailing_white_space_(false),
      has_leading_white_space_(false),
      has_trailing_white_space_(false),
      is_collapsed_(false),
      justifies_line_existence_(false),
      baseline_offset_from_margin_box_top_(0),
      used_font_(used_style_provider->GetUsedFont(
          computed_style->font_family(), computed_style->font_size(),
          computed_style->font_style(), computed_style->font_weight())),
      update_size_results_valid_(false) {}

InlineContainerBox::~InlineContainerBox() {}

Box::Level InlineContainerBox::GetLevel() const { return kInlineLevel; }

bool InlineContainerBox::TryAddChild(scoped_ptr<Box>* child_box) {
  switch ((*child_box)->GetLevel()) {
    case kBlockLevel:
      if (!(*child_box)->IsAbsolutelyPositioned()) {
        // Only inline-level boxes are allowed as in-flow children of an inline
        // container box.
        return false;
      }
      // Fall through if out-of-flow.

    case kInlineLevel:
      // If the inline container box already triggers a line break, then no
      // additional children can be added to it.
      if (DoesTriggerLineBreak()) {
        return false;
      }

      PushBackDirectChild(child_box->Pass());
      return true;

    default:
      NOTREACHED();
      return false;
  }
}

scoped_ptr<ContainerBox> InlineContainerBox::TrySplitAtEnd() {
  scoped_ptr<ContainerBox> box_after_split(new InlineContainerBox(
      computed_style(), transitions(), used_style_provider()));

  // When an inline box is split, margins, borders, and padding have no visual
  // effect where the split occurs.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  // TODO(***REMOVED***): Implement the above comment.

  return box_after_split.Pass();
}

bool InlineContainerBox::ValidateUpdateSizeInputs(
    const LayoutParams& params) {
  // Also take into account mutable local state about (at least) whether white
  // space should be collapsed or not.
  if (ContainerBox::ValidateUpdateSizeInputs(params) &&
      update_size_results_valid_) {
    return true;
  } else {
    update_size_results_valid_ = true;
    return false;
  }
}

void InlineContainerBox::UpdateContentSizeAndMargins(
    const LayoutParams& layout_params) {
  // Lay out child boxes as a one line without width constraints and white space
  // trimming.
  LineBox line_box(0, computed_style()->line_height(),
                   used_font_->GetFontMetrics(),
                   should_collapse_leading_white_space_,
                   should_collapse_trailing_white_space_, layout_params,
                   computed_style()->text_align());

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    line_box.BeginUpdateRectAndMaybeOverflow(child_box);
  }
  line_box.EndUpdates();

  // Although the spec says:
  //
  // The "width" property does not apply.
  //   http://www.w3.org/TR/CSS21/visudet.html#inline-width
  //
  // ...it is not the entire truth. It merely means that we have to ignore
  // the computed value of "width". Instead we use the shrink-to-fit width of
  // a hypothetical line box that contains all children. Later on this allow
  // to apply the following rule:
  //
  // When an inline box exceeds the width of a line box, it is split into
  // several boxes.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting
  set_width(line_box.shrink_to_fit_width());

  base::optional<float> maybe_margin_left = GetUsedMarginLeftIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<float> maybe_margin_right = GetUsedMarginRightIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  // A computed value of "auto" for "margin-left" or "margin-right" becomes
  // a used value of "0".
  //   http://www.w3.org/TR/CSS21/visudet.html#inline-width
  set_margin_left(maybe_margin_left.value_or(0.0f));
  set_margin_right(maybe_margin_right.value_or(0.0f));

  // The "height" property does not apply. The height of the content area should
  // be based on the font, but this specification does not specify how. [...]
  // However, we suggest that the height is chosen such that the content area
  // is just high enough for [...] the maximum ascenders and descenders, of all
  // the fonts in the element.
  //   http://www.w3.org/TR/CSS21/visudet.html#inline-non-replaced
  //
  // Above definition of used height matches the height of hypothetical line box
  // that contains all children.
  set_height(line_box.height());

  base::optional<float> maybe_margin_top = GetUsedMarginTopIfNotAuto(
      computed_style(), layout_params.containing_block_size);
  base::optional<float> maybe_margin_bottom = GetUsedMarginBottomIfNotAuto(
      computed_style(), layout_params.containing_block_size);

  // Vertical margins will not have any effect on non-replaced inline elements.
  //   http://www.w3.org/TR/CSS21/box.html#margin-properties
  set_margin_top(0);
  set_margin_bottom(0);

  has_leading_white_space_ = line_box.HasLeadingWhiteSpace();
  has_trailing_white_space_ = line_box.HasTrailingWhiteSpace();
  is_collapsed_ = line_box.IsCollapsed();
  justifies_line_existence_ =
      line_box.line_exists() || HasNonZeroMarginOrBorderOrPadding();

  baseline_offset_from_margin_box_top_ = margin_top() + border_top_width() +
                                         padding_top() +
                                         line_box.baseline_offset_from_top();
}

scoped_ptr<Box> InlineContainerBox::TrySplitAt(float available_width,
                                               bool allow_overflow) {
  DCHECK_GT(GetMarginBoxWidth(), available_width);

  available_width -= margin_left() + border_left_width() + padding_left();
  if (!allow_overflow && available_width < 0) {
    return scoped_ptr<Box>();
  }

  // Leave first N children that fit completely in the available width in this
  // box. The first child that does not fit within the width may also be split
  // and partially left in this box. Additionally, if |allow_overflow| is true,
  // then overflows past the available width are allowed until a child with a
  // used width greater than 0 has been added.
  ChildBoxes::const_iterator child_box_iterator;
  for (child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    float margin_box_width_of_child_box = child_box->GetMarginBoxWidth();

    // Split the first child that overflows the available width.
    // Leave its part before the split in this box.
    if (available_width < margin_box_width_of_child_box) {
      scoped_ptr<Box> child_box_after_split =
          child_box->TrySplitAt(available_width, allow_overflow);
      if (child_box_after_split) {
        ++child_box_iterator;
        child_box_iterator =
            InsertDirectChild(child_box_iterator, child_box_after_split.Pass());
      } else if (allow_overflow) {
        // Unable to split the child, but overflow is allowed, so increment
        // |child_box_iterator| because the whole first child box is being left
        // in this box.
        ++child_box_iterator;
      }

      break;
    }

    available_width -= margin_box_width_of_child_box;

    // Only continue allowing overflow if the box that was added
    // does not contribute to the line box.
    allow_overflow = allow_overflow && !child_box->JustifiesLineExistence();
  }

  // The first child cannot be split, so this box cannot be split either.
  if (child_box_iterator == child_boxes().begin()) {
    return scoped_ptr<Box>();
  }
  // Either:
  //   - All children fit but the right edge overflows.
  //   - The last child overflows, and happens to be the first one that
  //     justifies line existence and the overflow is allowed.
  // Anyway, this box cannot be split.
  //
  // TODO(***REMOVED***): If all children fit but the right edge overflows,
  //               go backwards and try splitting children just before their
  //               right edge.
  if (child_box_iterator == child_boxes().end()) {
    DCHECK(padding_right() + border_right_width() + margin_right() >
               available_width ||
           (allow_overflow &&
            child_boxes().back()->GetMarginBoxWidth() > available_width));
    return scoped_ptr<Box>();
  }

  return SplitAtIterator(child_box_iterator);
}

scoped_ptr<Box> InlineContainerBox::TrySplitAtSecondBidiLevelRun() {
  const int kInvalidLevel = -1;
  int last_level = kInvalidLevel;

  ChildBoxes::const_iterator child_box_iterator;
  for (child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    int current_level = child_box->GetBidiLevel().value_or(last_level);

    // If the last level isn't equal to the current level, then check on whether
    // or not the last level is kInvalidLevel. If it is, then no initial value
    // has been set yet. Otherwise, the intersection of two bidi levels has been
    // found where a split should occur.
    if (last_level != current_level) {
      if (last_level == kInvalidLevel) {
        last_level = current_level;
      } else {
        break;
      }
    }

    // Try to split the child box's internals.
    scoped_ptr<Box> child_box_after_split =
        child_box->TrySplitAtSecondBidiLevelRun();
    if (child_box_after_split) {
      ++child_box_iterator;
      child_box_iterator =
          InsertDirectChild(child_box_iterator, child_box_after_split.Pass());
      break;
    }
  }

  // If the iterator reached the end, then no split was found.
  if (child_box_iterator == child_boxes().end()) {
    return scoped_ptr<Box>();
  }

  return SplitAtIterator(child_box_iterator);
}

base::optional<int> InlineContainerBox::GetBidiLevel() const {
  if (!child_boxes().empty()) {
    ChildBoxes::const_iterator child_box_iterator = child_boxes().begin();
    return (*child_box_iterator)->GetBidiLevel();
  }

  return base::nullopt;
}

void InlineContainerBox::SetShouldCollapseLeadingWhiteSpace(
    bool should_collapse_leading_white_space) {
  if (should_collapse_leading_white_space_ !=
      should_collapse_leading_white_space) {
    should_collapse_leading_white_space_ = should_collapse_leading_white_space;
    update_size_results_valid_ = false;
  }
}

void InlineContainerBox::SetShouldCollapseTrailingWhiteSpace(
    bool should_collapse_trailing_white_space) {
  if (should_collapse_trailing_white_space_ !=
      should_collapse_trailing_white_space) {
    should_collapse_trailing_white_space_ =
        should_collapse_trailing_white_space;
    update_size_results_valid_ = false;
  }
}

bool InlineContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return has_leading_white_space_;
}

bool InlineContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return has_trailing_white_space_;
}

bool InlineContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());
  DCHECK_EQ(width(), width());  // Width should not be NaN.

  return is_collapsed_;
}

bool InlineContainerBox::JustifiesLineExistence() const {
  return justifies_line_existence_;
}

bool InlineContainerBox::DoesTriggerLineBreak() const {
  return !child_boxes().empty() &&
         (*child_boxes().rbegin())->DoesTriggerLineBreak();
}

bool InlineContainerBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

float InlineContainerBox::GetBaselineOffsetFromTopMarginEdge() const {
  return baseline_offset_from_margin_box_top_;
}

bool InlineContainerBox::IsTransformable() const { return false; }

#ifdef COBALT_BOX_DUMP_ENABLED

void InlineContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineContainerBox ";
}

void InlineContainerBox::DumpProperties(std::ostream* stream) const {
  ContainerBox::DumpProperties(stream);

  *stream << std::boolalpha
          << "has_leading_white_space=" << has_leading_white_space_ << " "
          << "has_trailing_white_space=" << has_trailing_white_space_ << " "
          << "is_collapsed=" << is_collapsed_ << " "
          << "justifies_line_existence=" << justifies_line_existence_ << " "
          << std::noboolalpha;
}

#endif  // COBALT_BOX_DUMP_ENABLED

scoped_ptr<Box> InlineContainerBox::SplitAtIterator(
    ChildBoxes::const_iterator child_split_iterator) {
  // TODO(***REMOVED***): When an inline box is split, margins, borders, and padding
  //               have no visual effect where the split occurs.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting

  // Move the children after the split into a new box.
  scoped_ptr<InlineContainerBox> box_after_split(new InlineContainerBox(
      computed_style(), transitions(), used_style_provider()));

  box_after_split->MoveChildrenFrom(box_after_split->child_boxes().end(), this,
                                    child_split_iterator, child_boxes().end());

#ifdef _DEBUG
  // Make sure that the |UpdateContentSizeAndMargins| is called afterwards.

  set_width(std::numeric_limits<float>::quiet_NaN());
  set_height(std::numeric_limits<float>::quiet_NaN());

  set_margin_left(std::numeric_limits<float>::quiet_NaN());
  set_margin_top(std::numeric_limits<float>::quiet_NaN());
  set_margin_right(std::numeric_limits<float>::quiet_NaN());
  set_margin_bottom(std::numeric_limits<float>::quiet_NaN());
#endif  // _DEBUG

  return box_after_split.PassAs<Box>();
}

}  // namespace layout
}  // namespace cobalt
