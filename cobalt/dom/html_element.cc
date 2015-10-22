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

#include "cobalt/dom/html_element.h"

#include "base/string_number_conversions.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/cascaded_style.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/specified_style.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_string_map.h"
#include "cobalt/dom/focus_event.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_br_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_heading_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_image_element.h"
#include "cobalt/dom/html_link_element.h"
#include "cobalt/dom/html_meta_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/rule_matching.h"

namespace cobalt {
namespace dom {

scoped_refptr<DOMStringMap> HTMLElement::dataset() {
  if (!dataset_) {
    dataset_ = new DOMStringMap(this);
  }
  return dataset_;
}

int32 HTMLElement::tab_index() const {
  int32 tabindex;
  bool success =
      base::StringToInt32(GetAttribute("tabindex").value_or(""), &tabindex);
  if (!success) {
    LOG(WARNING) << "Element's tabindex is not an integer.";
  }
  return tabindex;
}

void HTMLElement::set_tab_index(int32 tab_index) {
  SetAttribute("tabindex", base::Int32ToString(tab_index));
}

void HTMLElement::Focus() {
  if (!IsFocusable()) {
    return;
  }

  Element* old_active_element = owner_document()->active_element();
  if (old_active_element == this->AsElement()) {
    return;
  }

  owner_document()->SetActiveElement(this);

  if (old_active_element) {
    old_active_element->DispatchEvent(new FocusEvent("blur", this));
  }

  DispatchEvent(new FocusEvent("focus", old_active_element));
}

void HTMLElement::Blur() {
  if (owner_document()->active_element() == this->AsElement()) {
    owner_document()->SetActiveElement(NULL);

    DispatchEvent(new FocusEvent("blur", NULL));
  }
}

scoped_refptr<Node> HTMLElement::Duplicate() const {
  DCHECK(owner_document()->html_element_context()->html_element_factory());
  scoped_refptr<HTMLElement> new_html_element =
      owner_document()
          ->html_element_context()
          ->html_element_factory()
          ->CreateHTMLElement(owner_document(), tag_name());
  new_html_element->CopyAttributes(*this);

  return new_html_element;
}

void HTMLElement::OnCSSMutation() {
  // Invalidate the computed style of this node.
  computed_style_valid_ = false;

  owner_document()->OnElementInlineStyleMutation();
}

void HTMLElement::OnBackgroundImageLoaded() {
  owner_document()->RecordMutation();
}

scoped_refptr<HTMLAnchorElement> HTMLElement::AsHTMLAnchorElement() {
  return NULL;
}

scoped_refptr<HTMLBodyElement> HTMLElement::AsHTMLBodyElement() { return NULL; }

scoped_refptr<HTMLBRElement> HTMLElement::AsHTMLBRElement() { return NULL; }

scoped_refptr<HTMLDivElement> HTMLElement::AsHTMLDivElement() { return NULL; }

scoped_refptr<HTMLHeadElement> HTMLElement::AsHTMLHeadElement() { return NULL; }

scoped_refptr<HTMLHeadingElement> HTMLElement::AsHTMLHeadingElement() {
  return NULL;
}

scoped_refptr<HTMLHtmlElement> HTMLElement::AsHTMLHtmlElement() { return NULL; }

scoped_refptr<HTMLImageElement> HTMLElement::AsHTMLImageElement() {
  return NULL;
}

scoped_refptr<HTMLLinkElement> HTMLElement::AsHTMLLinkElement() { return NULL; }

scoped_refptr<HTMLMetaElement> HTMLElement::AsHTMLMetaElement() { return NULL; }

scoped_refptr<HTMLParagraphElement> HTMLElement::AsHTMLParagraphElement() {
  return NULL;
}

scoped_refptr<HTMLScriptElement> HTMLElement::AsHTMLScriptElement() {
  return NULL;
}

scoped_refptr<HTMLSpanElement> HTMLElement::AsHTMLSpanElement() { return NULL; }

scoped_refptr<HTMLStyleElement> HTMLElement::AsHTMLStyleElement() {
  return NULL;
}

scoped_refptr<HTMLUnknownElement> HTMLElement::AsHTMLUnknownElement() {
  return NULL;
}

scoped_refptr<HTMLVideoElement> HTMLElement::AsHTMLVideoElement() {
  return NULL;
}

namespace {

scoped_refptr<cssom::CSSStyleDeclarationData>
PromoteMatchingRulesToComputedStyle(
    cssom::RulesWithCascadePriority* matching_rules,
    cssom::GURLMap* property_name_to_base_url_map,
    const scoped_refptr<const cssom::CSSStyleDeclarationData>& inline_style,
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        parent_computed_style,
    const base::TimeDelta& style_change_event_time,
    cssom::TransitionSet* transitions,
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        previous_computed_style) {
  scoped_refptr<cssom::CSSStyleDeclarationData> computed_style =
      new cssom::CSSStyleDeclarationData();

  // Get the element's inline styles.
  computed_style->AssignFrom(*inline_style);

  // Select the winning value for each property by performing the cascade,
  // that is, apply values from matching rules on top of inline style, taking
  // into account rule specificity and location in the source file, as well as
  // property declaration importance.
  cssom::PromoteToCascadedStyle(computed_style, matching_rules,
                                property_name_to_base_url_map);

  // Up to this point many properties may lack a value. Perform defaulting
  // in order to ensure that every property has a value. Resolve "initial" and
  // "inherit" keywords, initialize properties with missing or semantically
  // invalid values with default values.
  cssom::PromoteToSpecifiedStyle(computed_style, parent_computed_style);

  // Lastly, absolutize the values, if possible. Convert length units and
  // percentages into pixels, convert color keywords into RGB triplets,
  // and so on. For certain properties, like "font-family", computed value is
  // the same as specified value. Declarations that cannot be absolutized
  // easily, like "width: auto;", will be resolved during layout.
  cssom::PromoteToComputedStyle(computed_style, parent_computed_style,
                                property_name_to_base_url_map);

  if (previous_computed_style) {
    // Now that we have updated our computed style, compare it to the previous
    // style and see if we need to adjust our animations.
    transitions->UpdateTransitions(style_change_event_time,
                                   *previous_computed_style, *computed_style);
  }

  return computed_style;
  // Cache the results of the computed style calculation.
}

bool NewComputedStyleInvalidatesLayoutBoxes(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        old_computed_style,
    const scoped_refptr<cssom::CSSStyleDeclarationData>& new_computed_style) {
  // FIXE(***REMOVED***): Only invalidate layout boxes when a property that is used for
  // box generation is modified. We currently have to also invalidate when any
  // inheritable property is modified, because AnonymousBlockBox and TextBox use
  // GetComputedStyleOfAnonymousBox() to store a copy of them that won't
  // automatically get updated when the style() in a ComputedStyleState gets
  // updated.
  return !old_computed_style->display()->Equals(
             *new_computed_style->display()) ||
         !old_computed_style->content()->Equals(
             *new_computed_style->content()) ||
         !old_computed_style->text_transform()->Equals(
             *new_computed_style->text_transform()) ||
         !old_computed_style->white_space()->Equals(
             *new_computed_style->white_space()) ||
         !old_computed_style->color()->Equals(*new_computed_style->color()) ||
         !old_computed_style->font_family()->Equals(
             *new_computed_style->font_family()) ||
         !old_computed_style->font_size()->Equals(
             *new_computed_style->font_size()) ||
         !old_computed_style->font_style()->Equals(
             *new_computed_style->font_style()) ||
         !old_computed_style->font_weight()->Equals(
             *new_computed_style->font_weight()) ||
         !old_computed_style->line_height()->Equals(
             *new_computed_style->line_height()) ||
         !old_computed_style->overflow_wrap()->Equals(
             *new_computed_style->overflow_wrap()) ||
         !old_computed_style->tab_size()->Equals(
             *new_computed_style->tab_size()) ||
         !old_computed_style->text_align()->Equals(
             *new_computed_style->text_align()) ||
         !old_computed_style->text_indent()->Equals(
             *new_computed_style->text_indent()) ||
         !old_computed_style->text_transform()->Equals(
             *new_computed_style->text_transform()) ||
         !old_computed_style->visibility()->Equals(
             *new_computed_style->visibility()) ||
         !old_computed_style->white_space()->Equals(
             *new_computed_style->white_space());
}

}  // namespace

void HTMLElement::UpdateComputedStyle(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        parent_computed_style,
    const base::TimeDelta& style_change_event_time) {
  scoped_refptr<Document> document = owner_document();
  DCHECK(document) << "Element should be attached to document in order to "
                      "participate in layout.";

  // TODO(***REMOVED***): It maybe helpful to generalize this mapping framework in the
  // future to allow more data and context about where a cssom::PropertyValue
  // came from.
  cssom::GURLMap property_name_to_base_url_map;
  property_name_to_base_url_map[cssom::kBackgroundImagePropertyName] =
      document->url_as_gurl();

  scoped_refptr<cssom::CSSStyleDeclarationData> new_computed_style =
      PromoteMatchingRulesToComputedStyle(
          matching_rules(), &property_name_to_base_url_map, style_->data(),
          parent_computed_style, style_change_event_time, transitions(),
          computed_style());

  // If there is no previous computed style, there should also be no layout
  // boxes, and nothing has to be invalidated.
  bool invalidate_layout_boxes = false;
  DCHECK(computed_style() || NULL == layout_boxes());
  if (computed_style() && NewComputedStyleInvalidatesLayoutBoxes(
                              computed_style(), new_computed_style)) {
    invalidate_layout_boxes = true;
  }
  set_computed_style(new_computed_style);

  // Update cached background images after resolving the urls in
  // background_image CSS property of the computed style, so we have all the
  // information to get the cached background images.
  UpdateCachedBackgroundImagesFromComputedStyle();

  // Promote the matching rules for all known pseudo elements.
  for (int pseudo_element_type = 0; pseudo_element_type < kMaxPseudoElementType;
       ++pseudo_element_type) {
    if (pseudo_elements_[pseudo_element_type]) {
      scoped_refptr<cssom::CSSStyleDeclarationData>
          pseudo_element_computed_style = PromoteMatchingRulesToComputedStyle(
              pseudo_elements_[pseudo_element_type]->matching_rules(),
              &property_name_to_base_url_map, style_->data(), computed_style(),
              style_change_event_time,
              pseudo_elements_[pseudo_element_type]->transitions(),
              pseudo_elements_[pseudo_element_type]->computed_style());

      if (!invalidate_layout_boxes &&
          pseudo_elements_[pseudo_element_type]->computed_style() &&
          NewComputedStyleInvalidatesLayoutBoxes(
              pseudo_elements_[pseudo_element_type]->computed_style(),
              pseudo_element_computed_style)) {
        invalidate_layout_boxes = true;
      }
      pseudo_elements_[pseudo_element_type]->set_computed_style(
          pseudo_element_computed_style);
    }
  }

  if (invalidate_layout_boxes) {
    InvalidateLayoutBoxesFromNodeAndAncestors();
    InvalidateLayoutBoxesFromNodeAndDescendants();
  }

  computed_style_valid_ = true;
}

void HTMLElement::UpdateComputedStyleRecursively(
    const scoped_refptr<const cssom::CSSStyleDeclarationData>&
        parent_computed_style,
    const base::TimeDelta& style_change_event_time, bool ancestors_were_valid) {
  bool is_valid = ancestors_were_valid && computed_style_valid_;
  if (!is_valid) {
    UpdateComputedStyle(parent_computed_style, style_change_event_time);
    DCHECK(computed_style_valid_);
  }

  // Update computed style for this element's descendants.
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    DCHECK(html_element);
    html_element->UpdateComputedStyleRecursively(
        computed_style(), style_change_event_time, is_valid);
  }
}

void HTMLElement::ClearMatchingRules() {
  matching_rules_.clear();
  rule_matching_state_.matching_nodes.clear();
  rule_matching_state_.descendant_potential_nodes.clear();
  rule_matching_state_.following_sibling_potential_nodes.clear();
  for (int pseudo_element_type = 0; pseudo_element_type < kMaxPseudoElementType;
       ++pseudo_element_type) {
    if (pseudo_elements_[pseudo_element_type]) {
      pseudo_elements_[pseudo_element_type]->ClearMatchingRules();
    }
  }
  computed_style_valid_ = false;
}

void HTMLElement::SetLayoutBoxes(scoped_ptr<LayoutBoxes> layout_boxes) {
#if defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  if (!owner_document()->partial_layout_is_enabled()) {
    layout_boxes_.reset();
    return;
  }
#endif  // defined(ENABLE_PARTIAL_LAYOUT_CONTROL)
  layout_boxes_ = layout_boxes.Pass();
}

void HTMLElement::InvalidateLayoutBoxesFromNodeAndAncestors() {
  layout_boxes_.reset();
  Node::InvalidateLayoutBoxesFromNodeAndAncestors();
}

void HTMLElement::InvalidateLayoutBoxesFromNodeAndDescendants() {
  layout_boxes_.reset();
  Node::InvalidateLayoutBoxesFromNodeAndDescendants();
}

HTMLElement::HTMLElement(Document* document)
    : Element(document),
      style_(new cssom::CSSStyleDeclaration(
          document->html_element_context()->css_parser())),
      computed_style_valid_(false),
      computed_style_state_(new cssom::ComputedStyleState()) {
  style_->set_mutation_observer(this);
}

HTMLElement::HTMLElement(Document* document, const std::string& tag_name)
    : Element(document, tag_name),
      style_(new cssom::CSSStyleDeclaration(
          document->html_element_context()->css_parser())),
      computed_style_valid_(false),
      computed_style_state_(new cssom::ComputedStyleState()) {
  style_->set_mutation_observer(this);
}

void HTMLElement::OnParserStartTag(
    const base::SourceLocation& opening_tag_location) {
  UNREFERENCED_PARAMETER(opening_tag_location);
  base::optional<std::string> style_attribute = GetAttribute("style");
  if (style_attribute) {
    style_->set_css_text(*style_attribute);
  }
}

HTMLElement::~HTMLElement() {}

void HTMLElement::UpdateCachedBackgroundImagesFromComputedStyle() {
  scoped_refptr<cssom::PropertyValue> background_image =
      computed_style()->background_image();
  // Don't fetch or cache the image if no image is specified or the display of
  // this element is turned off.
  if (background_image != cssom::KeywordValue::GetNone() &&
      computed_style()->display() != cssom::KeywordValue::GetNone()) {
    cssom::PropertyListValue* property_list_value =
        base::polymorphic_downcast<cssom::PropertyListValue*>(
            background_image.get());

    loader::image::CachedImageReferenceVector cached_images;
    cached_images.reserve(property_list_value->value().size());

    for (size_t i = 0; i < property_list_value->value().size(); ++i) {
      // TODO(***REMOVED***): Using visitors when linear gradient is supported.
      cssom::AbsoluteURLValue* absolute_url =
          base::polymorphic_downcast<cssom::AbsoluteURLValue*>(
              property_list_value->value()[i].get());
      if (absolute_url->value().is_valid()) {
        scoped_refptr<loader::image::CachedImage> cached_image =
            html_element_context()->image_cache()->CreateCachedResource(
                absolute_url->value());
        base::Closure loaded_callback = base::Bind(
            &HTMLElement::OnBackgroundImageLoaded, base::Unretained(this));
        cached_images.push_back(
            new loader::image::CachedImageReferenceWithCallbacks(
                cached_image, loaded_callback, base::Closure()));
      }
    }

    cached_background_images_ = cached_images.Pass();
  } else {
    cached_background_images_.clear();
  }
}

}  // namespace dom
}  // namespace cobalt
