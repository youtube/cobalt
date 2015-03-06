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

#ifndef LAYOUT_TEXT_BOX_H_
#define LAYOUT_TEXT_BOX_H_

#include "base/string_piece.h"
#include "cobalt/layout/box.h"

namespace cobalt {
namespace layout {

// Represents a whitespace if created with text == " ",
// otherwise represents a word.
//
// Whitespace boxes at the beginning and at the end of the line are "trimmed",
// so they have zero width during layout and do not produce render tree nodes,
// as per http://www.w3.org/TR/css3-text/#white-space-phase-2.
class TextBox : public Box {
 public:
  TextBox(ContainingBlock* containing_block,
          const scoped_refptr<cssom::CSSStyleDeclaration>& computed_style,
          UsedStyleProvider* converter, const base::StringPiece& text);

  void Layout(const LayoutOptions& options) OVERRIDE;

  void AddToRenderTree(
      render_tree::CompositionNode::Builder* composition_node_builder) OVERRIDE;

 private:
  const base::StringPiece text_;
  bool trimmed_;
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_TEXT_BOX_H_
