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

#ifndef LAYOUT_BOX_GENERATOR_H_
#define LAYOUT_BOX_GENERATOR_H_

#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace layout {

class ContainingBlock;
class UsedStyleProvider;

// Given the node, generates corresponding boxes recursively.
// As a side-effect, computed styles of processed elements are updated.
class BoxGenerator : public dom::NodeVisitor {
 public:
  BoxGenerator(ContainingBlock* containing_block,
               UsedStyleProvider* used_style_provider);

  void set_is_root(bool is_root) { is_root_ = is_root; }

  void Visit(dom::Comment* comment) OVERRIDE;
  void Visit(dom::Document* document) OVERRIDE;
  void Visit(dom::Element* element) OVERRIDE;
  void Visit(dom::Text* text) OVERRIDE;

 private:
  // Element with computed value of a "display" property being "block" or
  // "inline-block" establishes the new containing block for its descendants.
  // Element with "display: inline;" continue to use the old containing block.
  //   http://www.w3.org/TR/CSS2/visuren.html#containing-block
  ContainingBlock* GetOrGenerateContainingBlock(
      const scoped_refptr<cssom::CSSStyleDeclaration>& computed_style);

  void GenerateWordBox(
      std::string::const_iterator* text_iterator,
      const std::string::const_iterator& text_end_iterator,
      const scoped_refptr<cssom::CSSStyleDeclaration>& parent_computed_style);

  void GenerateWhitespaceBox(
      std::string::const_iterator* text_iterator,
      const std::string::const_iterator& text_end_iterator,
      const scoped_refptr<cssom::CSSStyleDeclaration>& parent_computed_style);

  ContainingBlock* const containing_block_;
  UsedStyleProvider* const used_style_provider_;
  bool is_root_;

  DISALLOW_COPY_AND_ASSIGN(BoxGenerator);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BOX_GENERATOR_H_
