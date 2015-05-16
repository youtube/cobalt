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

namespace cobalt {
namespace layout {

InlineContainerBox::InlineContainerBox(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& computed_style,
    const cssom::TransitionSet* transitions,
    const UsedStyleProvider* used_style_provider)
    : ContainerBox(computed_style, transitions, used_style_provider),
      justifies_line_existence_(false),
      height_above_baseline_(0) {}

InlineContainerBox::~InlineContainerBox() {}

Box::Level InlineContainerBox::GetLevel() const { return kInlineLevel; }

bool InlineContainerBox::TryAddChild(scoped_ptr<Box>* child_box) {
  switch ((*child_box)->GetLevel()) {
    case kBlockLevel:
      // An inline container box may only contain inline-level boxes.
      return false;
    case kInlineLevel:
      child_boxes_.push_back(child_box->release());
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

void InlineContainerBox::UpdateUsedSize(const LayoutParams& layout_params) {
  // Lay out child boxes as a one line without width constraints and white space
  // trimming.
  LineBox line_box(0, LineBox::kShouldNotTrimWhiteSpace, layout_params);

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    line_box.QueryUsedRectAndMaybeOverflow(child_box);
  }
  line_box.EndQueries();

  // TODO(***REMOVED***): Set to true if the box has non-zero margins, padding,
  //               or borders.
  justifies_line_existence_ = line_box.line_exists();

  set_used_width(line_box.GetShrinkToFitWidth());
  set_used_height(line_box.used_height());
  height_above_baseline_ = line_box.height_above_baseline();
}

scoped_ptr<Box> InlineContainerBox::TrySplitAt(float available_width) {
  // Leave first N children that fit completely into the available width
  // in this box.
  ChildBoxes::iterator child_box_iterator;
  for (child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;

    // Split the first child that overflows the available width.
    // Leave its part before the split in this box.
    if (available_width < child_box->used_width()) {
      scoped_ptr<Box> child_box_after_split =
          child_box->TrySplitAt(available_width);
      if (child_box_after_split) {
        ++child_box_iterator;
        child_box_iterator = child_boxes_.insert(
            child_box_iterator, child_box_after_split.release());
      }
      break;
    }

    available_width -= child_box->used_width();
  }

  // The first child cannot be split, so this box cannot be split either.
  if (child_box_iterator == child_boxes_.begin()) {
    return scoped_ptr<Box>();
  }

  // Move the children after the split into a new box.
  scoped_ptr<InlineContainerBox> box_after_split(new InlineContainerBox(
      computed_style(), transitions(), used_style_provider()));
  box_after_split->child_boxes_.insert(box_after_split->child_boxes_.end(),
                                       child_box_iterator, child_boxes_.end());
  child_boxes_.weak_erase(child_box_iterator, child_boxes_.end());

  // TODO(***REMOVED***): When an inline box is split, margins, borders, and padding
  //               have no visual effect where the split occurs.
  //   http://www.w3.org/TR/CSS21/visuren.html#inline-formatting

  InvalidateUsedHeight();

  return box_after_split.Pass();
}

// TODO(***REMOVED***): All white space processing methods have an O(N) worst-case
//               complexity. Figure out how to make them O(1).

bool InlineContainerBox::IsCollapsed() const {
  DCHECK_EQ(kInlineLevel, GetLevel());

  return FindFirstNonCollapsedChildBox() == child_boxes_.end();
}

bool InlineContainerBox::HasLeadingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());

  ChildBoxes::const_iterator child_box_iterator =
      FindFirstNonCollapsedChildBox();
  return child_box_iterator != child_boxes_.end() &&
         (*child_box_iterator)->HasLeadingWhiteSpace();
}

bool InlineContainerBox::HasTrailingWhiteSpace() const {
  DCHECK_EQ(kInlineLevel, GetLevel());

  ChildBoxes::const_iterator child_box_iterator =
      FindLastNonCollapsedChildBox();
  return child_box_iterator != child_boxes_.end() &&
         (*child_box_iterator)->HasTrailingWhiteSpace();
}

void InlineContainerBox::CollapseLeadingWhiteSpace() {
  DCHECK_EQ(kInlineLevel, GetLevel());

  // Did the inline container box collapse completely?
  ChildBoxes::const_iterator child_box_iterator =
      FindFirstNonCollapsedChildBox();
  if (child_box_iterator == child_boxes_.end()) {
    return;
  }

  // Is there any work to do?
  Box* child_box = *child_box_iterator;
  if (!child_box->HasLeadingWhiteSpace()) {
    return;
  }

  child_box->CollapseLeadingWhiteSpace();
  InvalidateUsedWidth();
}

void InlineContainerBox::CollapseTrailingWhiteSpace() {
  DCHECK_EQ(kInlineLevel, GetLevel());

  // Did the inline container box collapse completely?
  ChildBoxes::const_iterator child_box_iterator =
      FindLastNonCollapsedChildBox();
  if (child_box_iterator == child_boxes_.end()) {
    return;
  }

  // Is there any work to do?
  Box* child_box = *child_box_iterator;
  if (!child_box->HasTrailingWhiteSpace()) {
    return;
  }

  child_box->CollapseTrailingWhiteSpace();
  InvalidateUsedWidth();
}

bool InlineContainerBox::JustifiesLineExistence() const {
  // TODO(***REMOVED***): Return true if has non-zero margins, padding, or borders.
  return justifies_line_existence_;
}

bool InlineContainerBox::AffectsBaselineInBlockFormattingContext() const {
  NOTREACHED() << "Should only be called in a block formatting context.";
  return true;
}

float InlineContainerBox::GetHeightAboveBaseline() const {
  return height_above_baseline_;
}

void InlineContainerBox::AddContentToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    Box* child_box = *child_box_iterator;
    child_box->AddToRenderTree(composition_node_builder,
                               node_animations_map_builder);
  }
}

bool InlineContainerBox::IsTransformable() const { return false; }

void InlineContainerBox::DumpClassName(std::ostream* stream) const {
  *stream << "InlineContainerBox ";
}

void InlineContainerBox::DumpChildrenWithIndent(std::ostream* stream,
                                                int indent) const {
  ContainerBox::DumpChildrenWithIndent(stream, indent);

  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    const Box* child_box = *child_box_iterator;
    child_box->DumpWithIndent(stream, indent);
  }
}

InlineContainerBox::ChildBoxes::const_iterator
InlineContainerBox::FindFirstNonCollapsedChildBox() const {
  for (ChildBoxes::const_iterator child_box_iterator = child_boxes_.begin();
       child_box_iterator != child_boxes_.end(); ++child_box_iterator) {
    const Box* child_box = *child_box_iterator;
    if (!child_box->IsCollapsed()) {
      return child_box_iterator;
    }
  }
  return child_boxes_.end();
}

InlineContainerBox::ChildBoxes::const_iterator
InlineContainerBox::FindLastNonCollapsedChildBox() const {
  ChildBoxes::const_iterator child_box_iterator = child_boxes_.end();
  if (child_box_iterator != child_boxes_.begin()) {
    do {
      --child_box_iterator;
      const Box* child_box = *child_box_iterator;
      if (!child_box->IsCollapsed()) {
        return child_box_iterator;
      }
    } while (child_box_iterator != child_boxes_.begin());
  }
  return child_boxes_.end();
}

}  // namespace layout
}  // namespace cobalt
