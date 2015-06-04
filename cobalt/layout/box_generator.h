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
 * limitations under the License.`
 */

#ifndef LAYOUT_BOX_GENERATOR_H_
#define LAYOUT_BOX_GENERATOR_H_

#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/dom/node.h"
#include "third_party/icu/public/common/unicode/brkiter.h"

namespace cobalt {
namespace layout {

class Box;
class ContainerBox;
class UsedStyleProvider;

// In the visual formatting model, each element in the document tree generates
// zero or more boxes.
//   http://www.w3.org/TR/CSS21/visuren.html#box-gen
//
// A box generator recursively visits an HTML subtree that starts with a given
// element, creates a matching forest of boxes, and returns zero or more root
// boxes.
//
// As a side-effect, computed styles of visited HTML elements are updated.
class BoxGenerator : public dom::NodeVisitor {
 public:
  BoxGenerator(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>&
          parent_computed_style,
      const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
      const UsedStyleProvider* used_style_provider,
      icu::BreakIterator* line_break_iterator,
      const base::Time& style_change_event_time,
      ContainerBox* containing_box_for_absolute);
  ~BoxGenerator();

  void Visit(dom::Comment* comment) OVERRIDE;
  void Visit(dom::Document* document) OVERRIDE;
  void Visit(dom::Element* element) OVERRIDE;
  void Visit(dom::Text* text) OVERRIDE;

  // The result of a box generator is zero or more root boxes.
  typedef ScopedVector<Box> Boxes;
  Boxes PassBoxes();

 private:
  const scoped_refptr<const cssom::CSSStyleDeclarationData>
      parent_computed_style_;
  const scoped_refptr<cssom::CSSStyleSheet> user_agent_style_sheet_;
  const UsedStyleProvider* const used_style_provider_;
  icu::BreakIterator* const line_break_iterator_;

  Boxes boxes_;
  const base::Time style_change_event_time_;

  // Maintains a reference to the box that should be used as the containing
  // block for absolutely positioned elements (e.g. their first ancestor
  // that is positioned).
  ContainerBox* containing_box_for_absolute_;

  DISALLOW_COPY_AND_ASSIGN(BoxGenerator);
};

}  // namespace layout
}  // namespace cobalt

#endif  // LAYOUT_BOX_GENERATOR_H_
