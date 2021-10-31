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

#include <memory>

#include "cobalt/layout/anonymous_block_box.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/layout/inline_formatting_context.h"
#include "cobalt/layout/used_style.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace layout {

AnonymousBlockBox::AnonymousBlockBox(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        css_computed_style_declaration,
    BaseDirection base_direction, UsedStyleProvider* used_style_provider,
    LayoutStatTracker* layout_stat_tracker)
    : BlockContainerBox(css_computed_style_declaration, base_direction,
                        used_style_provider, layout_stat_tracker),
      used_font_(used_style_provider->GetUsedFontList(
          css_computed_style_declaration->data()->font_family(),
          css_computed_style_declaration->data()->font_size(),
          css_computed_style_declaration->data()->font_style(),
          css_computed_style_declaration->data()->font_weight())) {}

Box::Level AnonymousBlockBox::GetLevel() const { return kBlockLevel; }
Box::MarginCollapsingStatus AnonymousBlockBox::GetMarginCollapsingStatus()
    const {
  // If all enclosed boxes are absolutely-positioned, ignore it for
  // margin-collapse.
  if (std::all_of(child_boxes().begin(), child_boxes().end(),
                  [](Box* b) { return b->IsAbsolutelyPositioned(); })) {
    return kIgnore;
  }

  // If any enclosed block is inline-level, break collapsing model for
  // parent/siblings.
  return kSeparateAdjoiningMargins;
}

AnonymousBlockBox* AnonymousBlockBox::AsAnonymousBlockBox() { return this; }
const AnonymousBlockBox* AnonymousBlockBox::AsAnonymousBlockBox() const {
  return this;
}

void AnonymousBlockBox::SplitBidiLevelRuns() {
  ContainerBox::SplitBidiLevelRuns();

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end();) {
    Box* child_box = child_box_iterator->get();
    if (child_box->TrySplitAtSecondBidiLevelRun()) {
      child_box_iterator = InsertSplitSiblingOfDirectChild(child_box_iterator);
    } else {
      ++child_box_iterator;
    }
  }
}

bool AnonymousBlockBox::HasTrailingLineBreak() const {
  return !child_boxes().empty() && child_boxes().back()->HasTrailingLineBreak();
}

void AnonymousBlockBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    ContainerBox* stacking_context) const {
  ContainerBox::RenderAndAnimateContent(border_node_builder, stacking_context);

  if (computed_style()->visibility() != cssom::KeywordValue::GetVisible()) {
    return;
  }

  // Only add the ellipses to the render tree if the font is visible. The font
  // is treated as transparent if a font is currently being downloaded and
  // hasn't timed out: "In cases where textual content is loaded before
  // downloadable fonts are available, user agents may... render text
  // transparently with fallback fonts to avoid a flash of text using a fallback
  // font. In cases where the font download fails user agents must display text,
  // simply leaving transparent text is considered non-conformant behavior."
  //   https://www.w3.org/TR/css3-fonts/#font-face-loading
  if (!ellipses_coordinates_.empty() && used_font_->IsVisible()) {
    render_tree::ColorRGBA used_color = GetUsedColor(computed_style()->color());

    // Only render the ellipses if the color is not completely transparent.
    if (used_color.a() > 0.0f) {
      base::char16 ellipsis_value = used_font_->GetEllipsisValue();
      scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
          used_font_->CreateGlyphBuffer(&ellipsis_value, 1, false);

      for (EllipsesCoordinates::const_iterator iterator =
               ellipses_coordinates_.begin();
           iterator != ellipses_coordinates_.end(); ++iterator) {
        const math::Vector2dF& ellipsis_coordinates = *iterator;

        // The render tree API considers text coordinates to be a position of
        // a baseline, offset the text node accordingly.
        border_node_builder->AddChild(new render_tree::TextNode(
            ellipsis_coordinates, glyph_buffer, used_color));
      }
    }
  }
}

bool AnonymousBlockBox::TryAddChild(const scoped_refptr<Box>& child_box) {
  NOTREACHED();
  return false;
}

void AnonymousBlockBox::AddInlineLevelChild(
    const scoped_refptr<Box>& child_box) {
  DCHECK(child_box->GetLevel() == kInlineLevel ||
         child_box->IsAbsolutelyPositioned());
  PushBackDirectChild(child_box);
}

#ifdef COBALT_BOX_DUMP_ENABLED

void AnonymousBlockBox::DumpClassName(std::ostream* stream) const {
  *stream << "AnonymousBlockBox ";
}

#endif  // COBALT_BOX_DUMP_ENABLED

std::unique_ptr<FormattingContext>
AnonymousBlockBox::UpdateRectOfInFlowChildBoxes(
    const LayoutParams& child_layout_params) {
  // Do any processing needed prior to ellipsis placement on all of the
  // children.
  for (Boxes::const_iterator child_ellipsis_iterator = child_boxes().begin();
       child_ellipsis_iterator != child_boxes().end();
       ++child_ellipsis_iterator) {
    (*child_ellipsis_iterator)->DoPreEllipsisPlacementProcessing();
  }

  // If ellipses are enabled then retrieve the ellipsis width for the font;
  // otherwise, set the width to 0, which indicates that ellipses are not being
  // used.
  float ellipsis_width =
      AreEllipsesEnabled() ? used_font_->GetEllipsisWidth() : 0;

  // Lay out child boxes in the normal flow.
  //   https://www.w3.org/TR/CSS21/visuren.html#normal-flow
  std::unique_ptr<InlineFormattingContext> inline_formatting_context(
      new InlineFormattingContext(
          computed_style()->line_height(), used_font_->GetFontMetrics(),
          child_layout_params, base_direction(), computed_style()->text_align(),
          computed_style()->font_size(),
          GetUsedLength(computed_style()->text_indent()),
          LayoutUnit(ellipsis_width)));

  Boxes::const_iterator child_box_iterator = child_boxes().begin();
  while (child_box_iterator != child_boxes().end()) {
    Box* child_box = child_box_iterator->get();

    // Attempt to add the child box to the inline formatting context.
    Box* child_box_before_wrap =
        inline_formatting_context->TryAddChildAndMaybeWrap(child_box);
    // If |child_box_before_wrap| is non-NULL, then trying to add the child
    // box caused a line wrap to occur, and |child_box_before_wrap| is set to
    // the last box that was successfully placed on the line. This can
    // potentially be any of the child boxes previously added. Any boxes
    // following the returned box, including ones that were previously
    // added, still need to be added to the inline formatting context.
    if (child_box_before_wrap) {
      // Iterate backwards until the last box added to the line is found, and
      // then increment the iterator, so that it is pointing at the location
      // of the first box to add the next time through the loop.
      while (child_box_iterator->get() != child_box_before_wrap) {
        --child_box_iterator;
      }

      // If |child_box_before_wrap| has a split sibling, then this potentially
      // means that a split occurred during the wrap, and a new box needs to
      // be added to the container (this will also need to be the first box
      // added to the inline formatting context).
      //
      // If the split sibling is from a previous split, then it would have
      // already been added to the line and |child_box_iterator| should
      // be currently pointing at it. If this is not the case, then we know
      // that this is a new box produced during the wrap, and it must be
      // added to the container. This will be the first box added during
      // the next iteration of the loop.
      Box* split_child_after_wrap = child_box_before_wrap->GetSplitSibling();
      Boxes::const_iterator next_child_box_iterator = child_box_iterator + 1;
      if (split_child_after_wrap &&
          (next_child_box_iterator == child_boxes().end() ||
           next_child_box_iterator->get() != split_child_after_wrap)) {
        child_box_iterator =
            InsertSplitSiblingOfDirectChild(child_box_iterator);
        continue;
      }
    }

    ++child_box_iterator;
  }
  inline_formatting_context->EndUpdates();
  ellipses_coordinates_ = inline_formatting_context->GetEllipsesCoordinates();

  // Do any processing needed following ellipsis placement on all of the
  // children.
  for (Boxes::const_iterator child_ellipsis_iterator = child_boxes().begin();
       child_ellipsis_iterator != child_boxes().end();
       ++child_ellipsis_iterator) {
    (*child_ellipsis_iterator)->DoPostEllipsisPlacementProcessing();
  }

  return std::unique_ptr<FormattingContext>(
      inline_formatting_context.release());
}

bool AnonymousBlockBox::AreEllipsesEnabled() const {
  return parent()->computed_style()->overflow() ==
             cssom::KeywordValue::GetHidden() &&
         parent()->computed_style()->text_overflow() ==
             cssom::KeywordValue::GetEllipsis();
}

}  // namespace layout
}  // namespace cobalt
