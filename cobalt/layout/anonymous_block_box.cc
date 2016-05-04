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
    BaseDirection base_direction, UsedStyleProvider* used_style_provider)
    : BlockContainerBox(css_computed_style_declaration, base_direction,
                        used_style_provider),
      used_font_(used_style_provider->GetUsedFontList(
          css_computed_style_declaration->data()->font_family(),
          css_computed_style_declaration->data()->font_size(),
          css_computed_style_declaration->data()->font_style(),
          css_computed_style_declaration->data()->font_weight())) {}

Box::Level AnonymousBlockBox::GetLevel() const { return kBlockLevel; }

AnonymousBlockBox* AnonymousBlockBox::AsAnonymousBlockBox() { return this; }

void AnonymousBlockBox::SplitBidiLevelRuns() {
  ContainerBox::SplitBidiLevelRuns();

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end();) {
    Box* child_box = *child_box_iterator;
    ++child_box_iterator;

    scoped_refptr<Box> child_box_after_split =
        child_box->TrySplitAtSecondBidiLevelRun();
    if (child_box_after_split) {
      child_box_iterator =
          InsertDirectChild(child_box_iterator, child_box_after_split);
    }
  }
}

bool AnonymousBlockBox::HasTrailingLineBreak() const {
  return !child_boxes().empty() && child_boxes().back()->HasTrailingLineBreak();
}

void AnonymousBlockBox::RenderAndAnimateContent(
    render_tree::CompositionNode::Builder* border_node_builder,
    render_tree::animations::NodeAnimationsMap::Builder*
        node_animations_map_builder) const {
  ContainerBox::RenderAndAnimateContent(border_node_builder,
                                        node_animations_map_builder);

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
      char16 ellipsis_value = used_font_->GetEllipsisValue();
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

bool AnonymousBlockBox::TryAddChild(const scoped_refptr<Box>& /*child_box*/) {
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

scoped_ptr<FormattingContext> AnonymousBlockBox::UpdateRectOfInFlowChildBoxes(
    const LayoutParams& child_layout_params) {
  // Check to see if ellipses are enabled:
  // If they are, then retrieve the ellipsis width for the font and reset the
  // ellipsis boxes on all child boxes because they are no longer valid.
  // Otherwise, set the width to 0, which indicates that ellipses are not being
  // used. In this case, child boxes do not need to have ellipses reset, as they
  // could not have previously been set.
  float ellipsis_width;
  if (AreEllipsesEnabled()) {
    ellipsis_width = used_font_->GetEllipsisWidth();

    for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
         child_box_iterator != child_boxes().end(); ++child_box_iterator) {
      (*child_box_iterator)->ResetEllipses();
    }
  } else {
    ellipsis_width = 0;
  }

  // Lay out child boxes in the normal flow.
  //   https://www.w3.org/TR/CSS21/visuren.html#normal-flow
  scoped_ptr<InlineFormattingContext> inline_formatting_context(
      new InlineFormattingContext(
          computed_style()->line_height(), used_font_->GetFontMetrics(),
          child_layout_params, GetBaseDirection(),
          computed_style()->text_align(), computed_style()->white_space(),
          computed_style()->font_size(),
          GetUsedLength(computed_style()->text_indent()),
          LayoutUnit(ellipsis_width)));

  for (Boxes::const_iterator child_box_iterator = child_boxes().begin();
       child_box_iterator != child_boxes().end();) {
    Box* child_box = *child_box_iterator;
    ++child_box_iterator;

    if (child_box->IsAbsolutelyPositioned()) {
      inline_formatting_context->BeginEstimateStaticPosition(child_box);
    } else {
      scoped_refptr<Box> child_box_after_split =
          inline_formatting_context->BeginUpdateRectAndMaybeSplit(child_box);
      if (child_box_after_split) {
        // Re-insert the rest of the child box and attempt to lay it out in
        // the next iteration of the loop.
        // TODO(***REMOVED***): If every child box is split, this becomes an O(N^2)
        //               operation where N is the number of child boxes.
        //               Consider using a new vector instead and swap its
        //               contents after the layout is done.
        child_box_iterator =
            InsertDirectChild(child_box_iterator, child_box_after_split);
      }
    }
  }
  inline_formatting_context->EndUpdates();
  ellipses_coordinates_ = inline_formatting_context->GetEllipsesCoordinates();
  return inline_formatting_context.PassAs<FormattingContext>();
}

bool AnonymousBlockBox::AreEllipsesEnabled() const {
  return parent()->computed_style()->overflow() ==
             cssom::KeywordValue::GetHidden() &&
         parent()->computed_style()->text_overflow() ==
             cssom::KeywordValue::GetEllipsis();
}

}  // namespace layout
}  // namespace cobalt
