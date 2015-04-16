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

#include "cobalt/layout/box_generator.h"

#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_value_visitor.h"
#include "cobalt/cssom/transform_list_value.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/text.h"
#include "cobalt/layout/computed_style.h"
#include "cobalt/layout/containing_block.h"
#include "cobalt/layout/html_elements.h"
#include "cobalt/layout/specified_style.h"
#include "cobalt/layout/text_box.h"

namespace cobalt {
namespace layout {

BoxGenerator::BoxGenerator(
    ContainingBlock* containing_block,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet,
    UsedStyleProvider* used_style_provider)
    : containing_block_(containing_block),
      used_style_provider_(used_style_provider),
      user_agent_style_sheet_(user_agent_style_sheet),
      is_root_(false) {}

void BoxGenerator::Visit(dom::Element* element) {
  scoped_refptr<dom::HTMLElement> html_element = element->AsHTMLElement();
  DCHECK_NE(scoped_refptr<dom::HTMLElement>(), html_element);

  // If element is the root of layout tree, use the style of initial containing
  // block as the style of its parent.
  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style;
  if (is_root_) {
    parent_computed_style = containing_block_->computed_style();
  } else {
    // Only HTML elements should participate in layout.
    scoped_refptr<dom::Node> parent_node = html_element->parent_node();
    DCHECK_NE(scoped_refptr<dom::Node>(), parent_node);
    scoped_refptr<dom::Element> parent_element = parent_node->AsElement();
    DCHECK_NE(scoped_refptr<dom::Element>(), parent_element);
    scoped_refptr<dom::HTMLElement> parent_html_element =
        parent_element->AsHTMLElement();
    DCHECK_NE(scoped_refptr<dom::HTMLElement>(), parent_html_element);

    parent_computed_style = parent_html_element->computed_style()->data();
  }

  UpdateComputedStyleOf(html_element, parent_computed_style,
                        user_agent_style_sheet_);

  if (html_element->computed_style()->data()->display() ==
      cssom::KeywordValue::GetNone()) {
    // If the element has the display property of its style set to "none",
    // then it should not participate in layout, and we are done here.
    // http://www.w3.org/TR/CSS21/visuren.html#display-prop
    return;
  }

  ContainingBlock* child_containing_block =
      GetOrGenerateContainingBlock(html_element->computed_style()->data());

  // Generate child boxes.
  for (scoped_refptr<dom::Node> child_node = html_element->first_child();
       child_node; child_node = child_node->next_sibling()) {
    BoxGenerator child_box_generator(child_containing_block,
                                     user_agent_style_sheet_,
                                     used_style_provider_);
    child_node->Accept(&child_box_generator);
  }
}

// Comment does not generate boxes.
void BoxGenerator::Visit(dom::Comment* /*comment*/) {}

// Document should not participate in layout.
void BoxGenerator::Visit(dom::Document* /*document*/) { NOTREACHED(); }

// Text node produces a list of alternating whitespace and text boxes.
// TODO(b/19716102): Implement word breaking according to Unicode algorithm.
void BoxGenerator::Visit(dom::Text* text) {
  scoped_refptr<cssom::CSSStyleDeclarationData> parent_computed_style =
      text->parent_node()
          ->AsElement()
          ->AsHTMLElement()
          ->computed_style()
          ->data();

  std::string::const_iterator text_iterator = text->text().begin();
  std::string::const_iterator text_end_iterator = text->text().end();

  // Try to start with whitespace boxes.
  if (text_iterator != text_end_iterator && IsAsciiWhitespace(*text_iterator)) {
    GenerateWhitespaceBox(&text_iterator, text_end_iterator,
                          parent_computed_style);
  }

  while (true) {
    if (text_iterator == text_end_iterator) {
      break;
    }
    GenerateWordBox(&text_iterator, text_end_iterator, parent_computed_style);

    if (text_iterator == text_end_iterator) {
      break;
    }
    GenerateWhitespaceBox(&text_iterator, text_end_iterator,
                          parent_computed_style);
  }
}

ContainingBlock* BoxGenerator::GetOrGenerateContainingBlock(
    const scoped_refptr<cssom::CSSStyleDeclarationData>& computed_style) {
  // Check to see if we should create a containing block based on the value
  // of the "display" property.
  // Computed value of "display" property is guaranteed to be the keyword.
  cssom::KeywordValue* display =
      base::polymorphic_downcast<cssom::KeywordValue*>(
          computed_style->display().get());
  switch (display->value()) {
    case cssom::KeywordValue::kBlock:
    case cssom::KeywordValue::kInlineBlock: {
      return GenerateContainingBlock(computed_style);
    } break;

    case cssom::KeywordValue::kInline: {
      return containing_block_;
    } break;

    case cssom::KeywordValue::kAuto:
    case cssom::KeywordValue::kHidden:
    case cssom::KeywordValue::kInherit:
    case cssom::KeywordValue::kInitial:
    case cssom::KeywordValue::kNone:
    case cssom::KeywordValue::kVisible:
    default:
      NOTREACHED();
      return NULL;
  }
}

const scoped_refptr<cssom::CSSStyleDeclarationData>&
BoxGenerator::GetAnonymousInlineBoxStyle(const scoped_refptr<
    cssom::CSSStyleDeclarationData>& parent_computed_style) {
  if (!anonymous_inline_box_style_) {
    anonymous_inline_box_style_ = new cssom::CSSStyleDeclarationData();
    PromoteToSpecifiedStyle(anonymous_inline_box_style_, parent_computed_style);
    PromoteToComputedStyle(anonymous_inline_box_style_, parent_computed_style);
  }
  return anonymous_inline_box_style_;
}

ContainingBlock* BoxGenerator::GenerateContainingBlock(
    const scoped_refptr<cssom::CSSStyleDeclarationData>& computed_style) {
  scoped_ptr<ContainingBlock> child_containing_block(new ContainingBlock(
      containing_block_, computed_style, used_style_provider_));
  ContainingBlock* saved_child_containing_block = child_containing_block.get();
  containing_block_->AddChildBox(child_containing_block.PassAs<Box>());
  return saved_child_containing_block;
}

void BoxGenerator::GenerateWordBox(
    std::string::const_iterator* text_iterator,
    const std::string::const_iterator& text_end_iterator,
    const scoped_refptr<cssom::CSSStyleDeclarationData>&
        parent_computed_style) {
  std::string::const_iterator word_start_iterator = *text_iterator;
  std::string::const_iterator& word_end_iterator = *text_iterator;

  while (word_end_iterator != text_end_iterator &&
         !IsAsciiWhitespace(*word_end_iterator)) {
    ++word_end_iterator;
  }
  DCHECK(word_start_iterator != word_end_iterator);

  scoped_ptr<TextBox> word_box(new TextBox(
      containing_block_, GetAnonymousInlineBoxStyle(parent_computed_style),
      used_style_provider_,
      base::StringPiece(word_start_iterator, word_end_iterator)));
  containing_block_->AddChildBox(word_box.PassAs<Box>());
}

void BoxGenerator::GenerateWhitespaceBox(
    std::string::const_iterator* text_iterator,
    const std::string::const_iterator& text_end_iterator,
    const scoped_refptr<cssom::CSSStyleDeclarationData>&
        parent_computed_style) {
  std::string::const_iterator whitespace_start_iterator = *text_iterator;
  std::string::const_iterator& whitespace_end_iterator = *text_iterator;

  while (whitespace_end_iterator != text_end_iterator &&
         IsAsciiWhitespace(*whitespace_end_iterator)) {
    ++whitespace_end_iterator;
  }
  DCHECK(whitespace_start_iterator != whitespace_end_iterator);

  scoped_ptr<TextBox> whitespace_box(new TextBox(
      containing_block_, GetAnonymousInlineBoxStyle(parent_computed_style),
      used_style_provider_, " "));
  // TODO(***REMOVED***): Do not add whitespace box if the last child is already
  //               a whitespace box.
  containing_block_->AddChildBox(whitespace_box.PassAs<Box>());
}

}  // namespace layout
}  // namespace cobalt
