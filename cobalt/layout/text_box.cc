/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/layout/text_box.h"

#include "cobalt/layout/used_style.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace layout {

TextBox::TextBox(
    ContainingBlock* containing_block,
    const scoped_refptr<cssom::CSSStyleDeclaration>& computed_style,
    UsedStyleProvider* converter, const base::StringPiece& text)
    : Box(containing_block, computed_style, converter),
      text_(text),
      trimmed_(false) {}

void TextBox::Layout(const LayoutOptions& options) {
  trimmed_ = options.beginning_of_line && text_ == " ";
  if (trimmed_) {
    used_frame().set_size(math::SizeF());
  } else {
    scoped_refptr<render_tree::Font> used_font =
        used_style_provider()->GetUsedFont(computed_style()->font_family(),
                                           computed_style()->font_size());
    // TODO(***REMOVED***): Figure out how to handle text bounds with negative x().
    //               Does it mean that the first letter like "W" should overlap
    //               with the preceding text or we should additionally shift
    //               entire word by -x()?
    // TODO(***REMOVED***): Figure out why Skia returns zero bounds for whitespace
    //               characters.
    // TODO(***REMOVED***): Line height should be calculated from font metrics,
    //               not from actual text bounds.
    used_frame() = used_font->GetBounds(text_ == " " ? "i" : text_.as_string());
  }
}

void TextBox::AddToRenderTree(
    render_tree::CompositionNode::Builder* composition_node_builder) {
  if (trimmed_) {
    return;
  }

  Box::AddToRenderTree(composition_node_builder);

  scoped_refptr<render_tree::Font> used_font =
      used_style_provider()->GetUsedFont(computed_style()->font_family(),
                                         computed_style()->font_size());
  render_tree::ColorRGBA used_color = GetUsedColor(computed_style()->color());
  composition_node_builder->AddChild(
      new render_tree::TextNode(text_.as_string(), used_font, used_color),
      GetTransform());
}

}  // namespace layout
}  // namespace cobalt
