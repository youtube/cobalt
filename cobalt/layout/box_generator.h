// Copyright 2014 Google Inc. All Rights Reserved.
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
// limitations under the License.`

#ifndef COBALT_LAYOUT_BOX_GENERATOR_H_
#define COBALT_LAYOUT_BOX_GENERATOR_H_

#include "base/memory/scoped_vector.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/node.h"
#include "cobalt/layout/box.h"
#include "third_party/icu/source/common/unicode/brkiter.h"

namespace cobalt {

namespace dom {
class HTMLVideoElement;
}  // namespace dom

namespace layout {

class Box;
class LayoutStatTracker;
class Paragraph;
class UsedStyleProvider;

// In the visual formatting model, each element in the document tree generates
// zero or more boxes.
//   https://www.w3.org/TR/CSS21/visuren.html#box-gen
//
// A box generator recursively visits an HTML subtree that starts with a given
// element, creates a matching forest of boxes, and returns zero or more root
// boxes.
//
// As a side-effect, computed styles of visited HTML elements are updated.
class BoxGenerator : public dom::NodeVisitor {
 public:
  struct Context {
    Context(UsedStyleProvider* used_style_provider,
            LayoutStatTracker* layout_stat_tracker,
            icu::BreakIterator* line_break_iterator,
            icu::BreakIterator* character_break_iterator,
            dom::HTMLElement* ignore_background_element,
            int dom_max_element_depth)
        : used_style_provider(used_style_provider),
          layout_stat_tracker(layout_stat_tracker),
          line_break_iterator(line_break_iterator),
          character_break_iterator(character_break_iterator),
          ignore_background_element(ignore_background_element),
          dom_max_element_depth(dom_max_element_depth) {}
    UsedStyleProvider* used_style_provider;
    LayoutStatTracker* layout_stat_tracker;
    icu::BreakIterator* line_break_iterator;
    icu::BreakIterator* character_break_iterator;

    // The HTML and BODY tags may have had their background style properties
    // propagated up to the initial containing block.  If so, we should not
    // re-use those properties on that element.  This value will track that
    // element.
    dom::HTMLElement* ignore_background_element;

    // The maximum element depth for layout.
    int dom_max_element_depth;
  };

  BoxGenerator(const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
                   parent_css_computed_style_declaration,
               // Parent animations are passed separately in order to enable
               // grandparent inheritance of color property animations when the
               // parent is a pseudo element.
               // TODO: Remove this parameter when full support for
               // animation inheritance is implemented.
               const scoped_refptr<const web_animations::AnimationSet>&
                   parent_animations,
               scoped_refptr<Paragraph>* paragraph,
               const int dom_element_depth_, const Context* context);
  ~BoxGenerator();

  void Visit(dom::CDATASection* cdata_section) override;
  void Visit(dom::Comment* comment) override;
  void Visit(dom::Document* document) override;
  void Visit(dom::DocumentType* document_type) override;
  void Visit(dom::Element* element) override;
  void Visit(dom::Text* text) override;

  const Boxes& boxes() const { return boxes_; }

 private:
  void VisitVideoElement(dom::HTMLVideoElement* video_element);
  void VisitBrElement(dom::HTMLBRElement* br_element);
  void VisitNonReplacedElement(dom::HTMLElement* html_element);

  void AppendChildBoxToLine(const scoped_refptr<Box>& child_box);
  void AppendPseudoElementToLine(dom::HTMLElement* html_element,
                                 dom::PseudoElementType pseudo_element_type);

  const scoped_refptr<cssom::CSSComputedStyleDeclaration>
      parent_css_computed_style_declaration_;
  const scoped_refptr<const web_animations::AnimationSet>& parent_animations_;
  scoped_refptr<Paragraph>* paragraph_;
  // The current element depth.
  const int dom_element_depth_;
  const Context* context_;

  scoped_refptr<dom::HTMLElement> generating_html_element_;

  // The result of a box generator is zero or more root boxes.
  Boxes boxes_;

  DISALLOW_COPY_AND_ASSIGN(BoxGenerator);
};

}  // namespace layout
}  // namespace cobalt

#endif  // COBALT_LAYOUT_BOX_GENERATOR_H_
