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

#include <map>

#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/user_log.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/cascaded_style.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_animatable.h"
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
#include "cobalt/dom/html_title_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/html_video_element.h"
#include "cobalt/dom/rule_matching.h"

namespace cobalt {
namespace dom {

namespace {

// This struct manages the user log information for Node count.
struct HtmlElementCountLog {
 public:
  HtmlElementCountLog() : count(0) {
    base::UserLog::Register(base::UserLog::kHtmlElementCountIndex,
                            "HtmlElementCnt", &count, sizeof(count));
  }
  ~HtmlElementCountLog() {
    base::UserLog::Deregister(base::UserLog::kHtmlElementCountIndex);
  }

  int count;
};

struct NonTrivialStaticFields {
  NonTrivialStaticFields() {
    cssom::PropertyKeyVector computed_style_invalidation_properties;
    cssom::PropertyKeyVector layout_box_invalidation_properties;
    cssom::PropertyKeyVector size_invalidation_properties;
    cssom::PropertyKeyVector cross_references_invalidation_properties;

    for (int i = 0; i <= cssom::kMaxLonghandPropertyKey; ++i) {
      cssom::PropertyKey property_key = static_cast<cssom::PropertyKey>(i);

      if (cssom::GetPropertyImpactsChildDeclaredStyle(property_key) ==
          cssom::kImpactsChildDeclaredStyleYes) {
        computed_style_invalidation_properties.push_back(property_key);
      }

      // TODO: Revisit inherited property handling. Currently, all boxes are
      // invalidated if an inherited property changes, but now that inherited
      // properties dynamically update, this is likely no longer necessary.
      if (cssom::GetPropertyInheritance(property_key) == cssom::kInheritedYes ||
          cssom::GetPropertyImpactsBoxGeneration(property_key) ==
              cssom::kImpactsBoxGenerationYes) {
        layout_box_invalidation_properties.push_back(property_key);
      } else {
        if (cssom::GetPropertyImpactsBoxSizes(property_key) ==
            cssom::kImpactsBoxSizesYes) {
          size_invalidation_properties.push_back(property_key);
        }
        if (cssom::GetPropertyImpactsBoxCrossReferences(property_key) ==
            cssom::kImpactsBoxCrossReferencesYes) {
          cross_references_invalidation_properties.push_back(property_key);
        }
      }
    }

    computed_style_invalidation_property_checker =
        cssom::CSSComputedStyleData::PropertySetMatcher(
            computed_style_invalidation_properties);
    layout_box_invalidation_property_checker =
        cssom::CSSComputedStyleData::PropertySetMatcher(
            layout_box_invalidation_properties);
    size_invalidation_property_checker =
        cssom::CSSComputedStyleData::PropertySetMatcher(
            size_invalidation_properties);
    cross_references_invalidation_property_checker =
        cssom::CSSComputedStyleData::PropertySetMatcher(
            cross_references_invalidation_properties);
  }

  cssom::CSSComputedStyleData::PropertySetMatcher
      computed_style_invalidation_property_checker;
  cssom::CSSComputedStyleData::PropertySetMatcher
      layout_box_invalidation_property_checker;
  cssom::CSSComputedStyleData::PropertySetMatcher
      size_invalidation_property_checker;
  cssom::CSSComputedStyleData::PropertySetMatcher
      cross_references_invalidation_property_checker;

  HtmlElementCountLog html_element_count_log;

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

base::LazyInstance<NonTrivialStaticFields> non_trivial_static_fields =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

std::string HTMLElement::dir() const {
  // The dir attribute is limited to only known values. On getting, dir must
  // return the conforming value associated with the state the attribute is in,
  // or the empty string if the attribute is in a state with no associated
  // keyword value.
  // https://dev.w3.org/html5/spec-preview/global-attributes.html#the-directionality
  // https://dev.w3.org/html5/spec-preview/common-dom-interfaces.html#limited-to-only-known-values
  // NOTE: Value "auto" is not supported.
  if (directionality_ == kLeftToRightDirectionality) {
    return "ltr";
  } else if (directionality_ == kRightToLeftDirectionality) {
    return "rtl";
  } else {
    return "";
  }
}

void HTMLElement::set_dir(const std::string& value) {
  // The dir attribute is limited to only known values. On setting, the dir
  // attribute must be set to the specified new value.
  // https://dev.w3.org/html5/spec-preview/global-attributes.html#the-directionality
  // https://dev.w3.org/html5/spec-preview/common-dom-interfaces.html#limited-to-only-known-values
  SetAttribute("dir", value);
}

scoped_refptr<DOMStringMap> HTMLElement::dataset() {
  scoped_refptr<DOMStringMap> dataset(dataset_);
  if (!dataset) {
    // Create a new instance and store a weak reference.
    dataset = new DOMStringMap(this);
    dataset_ = dataset->AsWeakPtr();
  }
  return dataset;
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

  Document* document = node_document();
  Element* old_active_element = document->active_element();
  if (old_active_element == this->AsElement()) {
    return;
  }

  document->SetActiveElement(this);

  if (old_active_element) {
    old_active_element->DispatchEvent(
        new FocusEvent(base::Tokens::blur(), this));
  }

  DispatchEvent(new FocusEvent(base::Tokens::focus(), old_active_element));
}

void HTMLElement::Blur() {
  Document* document = node_document();
  if (document->active_element() == this->AsElement()) {
    document->SetActiveElement(NULL);

    DispatchEvent(new FocusEvent(base::Tokens::blur(), NULL));
  }
}

// Algorithm for GetClientRects:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-getclientrects
scoped_refptr<DOMRectList> HTMLElement::GetClientRects() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element on which it was invoked does not have an associated
  // layout box return an empty DOMRectList object and stop this algorithm.
  if (!layout_boxes_) {
    return make_scoped_refptr(new DOMRectList());
  }

  // The remaining steps are implemented in LayoutBoxes::GetClientRects().
  return layout_boxes_->GetClientRects();
}

// Algorithm for client_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clienttop
float HTMLElement::client_top() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInlineLevel()) {
    return 0.0f;
  }
  // 2. Return the computed value of the 'border-top-width' property plus the
  // height of any scrollbar rendered between the top padding edge and the top
  // border edge, ignoring any transforms that apply to the element and its
  // ancestors.
  return layout_boxes_->GetBorderTopWidth();
}

// Algorithm for client_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientleft
float HTMLElement::client_left() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInlineLevel()) {
    return 0.0f;
  }
  // 2. Return the computed value of the 'border-left-width' property plus the
  // width of any scrollbar rendered between the left padding edge and the left
  // border edge, ignoring any transforms that apply to the element and its
  // ancestors.
  return layout_boxes_->GetBorderLeftWidth();
}

// Algorithm for client_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientwidth
float HTMLElement::client_width() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInlineLevel()) {
    return 0.0f;
  }

  // 2. If the element is the root element, return the viewport width.
  if (IsRootElement()) {
    return layout_boxes_->GetMarginEdgeWidth();
  }

  // 3. Return the width of the padding edge, ignoring any transforms that apply
  // to the element and its ancestors.
  return layout_boxes_->GetPaddingEdgeWidth();
}

// Algorithm for client_height:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clientheight
float HTMLElement::client_height() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInlineLevel()) {
    return 0.0f;
  }

  // 2. If the element is the root element, return the viewport height.
  if (IsRootElement()) {
    return layout_boxes_->GetMarginEdgeHeight();
  }

  // Return the height of the padding edge, ignoring any transforms that apply
  // to the element and its ancestors.
  return layout_boxes_->GetPaddingEdgeHeight();
}

// Algorithm for offsetParent:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetparent
scoped_refptr<Element> HTMLElement::offset_parent() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If any of the following holds true return null and terminate this
  //    algorithm:
  //    . The element does not have an associated CSS layout box.
  //    . The element is the root element.
  //    . The element is the HTML body element.
  //    . The element's computed value of the 'position' property is 'fixed'.
  if (!layout_boxes_ || IsRootElement() || AsHTMLBodyElement() ||
      !computed_style() ||
      computed_style()->position() == cssom::KeywordValue::GetFixed()) {
    return scoped_refptr<Element>();
  }

  // 2. Return the nearest ancestor element of the element for which at least
  //    one of the following is true and terminate this algorithm if such an
  //    ancestor is found:
  //    . The computed value of the 'position' property is not 'static'.
  //    . It is the HTML body element.
  for (scoped_refptr<Node> ancestor_node = parent_node(); ancestor_node;
       ancestor_node = ancestor_node->parent_node()) {
    scoped_refptr<Element> ancestor_element = ancestor_node->AsElement();
    if (!ancestor_element) {
      continue;
    }
    scoped_refptr<HTMLElement> ancestor_html_element =
        ancestor_element->AsHTMLElement();
    if (!ancestor_html_element) {
      continue;
    }
    DCHECK(ancestor_html_element->computed_style());
    if (ancestor_html_element->AsHTMLBodyElement() ||
        ancestor_html_element->computed_style()->position() !=
            cssom::KeywordValue::GetStatic()) {
      return ancestor_element;
    }
  }

  // 3. Return null.
  return scoped_refptr<Element>();
}

// Algorithm for offset_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsettop
float HTMLElement::offset_top() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element is the HTML body element or does not have any associated
  // CSS layout box return zero and terminate this algorithm.
  if (!layout_boxes_ || AsHTMLBodyElement()) {
    return 0.0f;
  }

  // 2. If the offsetParent of the element is null return the y-coordinate of
  // the top border edge of the first CSS layout box associated with the
  // element, relative to the initial containing block origin, ignoring any
  // transforms that apply to the element and its ancestors, and terminate this
  // algorithm.
  scoped_refptr<Element> offset_parent_element = offset_parent();
  if (!offset_parent_element) {
    return layout_boxes_->GetBorderEdgeTop();
  }

  // 3. Return the result of subtracting the y-coordinate of the top padding
  // edge of the first CSS layout box associated with the offsetParent of the
  // element from the y-coordinate of the top border edge of the first CSS
  // layout box associated with the element, relative to the initial containing
  // block origin, ignoring any transforms that apply to the element and its
  // ancestors.
  scoped_refptr<HTMLElement> offset_parent_html_element =
      offset_parent_element->AsHTMLElement();
  if (!offset_parent_html_element) {
    return layout_boxes_->GetBorderEdgeTop();
  }

  DCHECK(offset_parent_html_element->layout_boxes());
  return layout_boxes_->GetBorderEdgeTop() -
         offset_parent_html_element->layout_boxes()->GetPaddingEdgeTop();
}

// Algorithm for offset_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetleft
float HTMLElement::offset_left() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element is the HTML body element or does not have any associated
  // CSS layout box return zero and terminate this algorithm.
  if (!layout_boxes_ || AsHTMLBodyElement()) {
    return 0.0f;
  }

  // 2. If the offsetParent of the element is null return the x-coordinate of
  // the left border edge of the first CSS layout box associated with the
  // element, relative to the initial containing block origin, ignoring any
  // transforms that apply to the element and its ancestors, and terminate this
  // algorithm.
  scoped_refptr<Element> offset_parent_element = offset_parent();
  if (!offset_parent_element) {
    return layout_boxes_->GetBorderEdgeLeft();
  }

  // 3. Return the result of subtracting the x-coordinate of the left padding
  // edge of the first CSS layout box associated with the offsetParent of the
  // element from the x-coordinate of the left border edge of the first CSS
  // layout box associated with the element, relative to the initial containing
  // block origin, ignoring any transforms that apply to the element and its
  // ancestors.
  scoped_refptr<HTMLElement> offset_parent_html_element =
      offset_parent_element->AsHTMLElement();
  if (!offset_parent_html_element) {
    return layout_boxes_->GetBorderEdgeLeft();
  }

  DCHECK(offset_parent_html_element->layout_boxes());
  return layout_boxes_->GetBorderEdgeLeft() -
         offset_parent_html_element->layout_boxes()->GetPaddingEdgeLeft();
}

// Algorithm for offset_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetwidth
float HTMLElement::offset_width() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  if (!layout_boxes_) {
    return 0.0f;
  }

  // 2. Return the border edge width of the first CSS layout box associated with
  // the element, ignoring any transforms that apply to the element and its
  // ancestors.
  return layout_boxes_->GetBorderEdgeWidth();
}

// Algorithm for offset_height:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetheight
float HTMLElement::offset_height() {
  DCHECK(node_document());
  node_document()->DoSynchronousLayout();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  if (!layout_boxes_) {
    return 0.0f;
  }

  // 2. Return the border edge height of the first CSS layout box associated
  // with the element, ignoring any transforms that apply to the element and its
  // ancestors.
  return layout_boxes_->GetBorderEdgeHeight();
}

scoped_refptr<Node> HTMLElement::Duplicate() const {
  Document* document = node_document();
  DCHECK(document->html_element_context()->html_element_factory());
  scoped_refptr<HTMLElement> new_html_element =
      document->html_element_context()
          ->html_element_factory()
          ->CreateHTMLElement(document, tag_name());
  new_html_element->CopyAttributes(*this);
  new_html_element->CopyDirectionality(*this);
  new_html_element->style_->AssignFrom(*this->style_);

  return new_html_element;
}

base::optional<std::string> HTMLElement::GetStyleAttribute() const {
  base::optional<std::string> value = Element::GetStyleAttribute();
  return value.value_or(style_->css_text(NULL));
}

void HTMLElement::SetStyleAttribute(const std::string& value) {
  Document* document = node_document();
  CspDelegate* csp_delegate = document->csp_delegate();
  if (value.empty() ||
      csp_delegate->AllowInline(
          CspDelegate::kStyle,
          base::SourceLocation(GetSourceLocationName(), 1, 1), value)) {
    style_->set_css_text(value, NULL);
    Element::SetStyleAttribute(value);
  } else {
    // Report a violation.
    PostToDispatchEvent(FROM_HERE, base::Tokens::error());
  }
}

void HTMLElement::RemoveStyleAttribute() {
  style_->set_css_text("", NULL);
  Element::RemoveStyleAttribute();
}

void HTMLElement::OnCSSMutation() {
  // Invalidate the computed style of this node.
  computed_style_valid_ = false;

  // Remove the style attribute value from the Element.
  Element::RemoveStyleAttribute();

  node_document()->OnElementInlineStyleMutation();
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

scoped_refptr<HTMLTitleElement> HTMLElement::AsHTMLTitleElement() {
  return NULL;
}

scoped_refptr<HTMLUnknownElement> HTMLElement::AsHTMLUnknownElement() {
  return NULL;
}

scoped_refptr<HTMLVideoElement> HTMLElement::AsHTMLVideoElement() {
  return NULL;
}

void HTMLElement::InvalidateMatchingRulesRecursively() {
  if (!matching_rules_valid_) {
    return;
  }

  matching_rules_valid_ = false;

  // Move |matching_rules_| into |old_matching_rules_|. This is used for
  // determining whether or not the matching rules actually changed when they
  // are updated.
  old_matching_rules_.swap(matching_rules_);

  matching_rules_->clear();
  rule_matching_state_.matching_nodes.clear();
  rule_matching_state_.descendant_potential_nodes.clear();
  rule_matching_state_.following_sibling_potential_nodes.clear();
  for (int pseudo_element_type = 0; pseudo_element_type < kMaxPseudoElementType;
       ++pseudo_element_type) {
    if (pseudo_elements_[pseudo_element_type]) {
      pseudo_elements_[pseudo_element_type]->ClearMatchingRules();
    }
  }

  // Invalidate matching rules on all children.
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->InvalidateMatchingRulesRecursively();
    }
  }

  // Invalidate matching rules on all following siblings if sibling combinators
  // are used.
  if (node_document()->selector_tree()->has_sibling_combinators()) {
    for (Element* element = next_element_sibling(); element;
         element = element->next_element_sibling()) {
      HTMLElement* html_element = element->AsHTMLElement();
      if (html_element) {
        html_element->InvalidateMatchingRulesRecursively();
      }
    }
  }
}

void HTMLElement::InvalidateComputedStylesRecursively() {
  computed_style_valid_ = false;
  Node::InvalidateComputedStylesRecursively();
}

void HTMLElement::UpdateComputedStyleRecursively(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const base::TimeDelta& style_change_event_time, bool ancestors_were_valid) {
  // Update computed style for this element.
  bool is_valid =
      ancestors_were_valid && matching_rules_valid_ && computed_style_valid_;
  if (!is_valid) {
    UpdateComputedStyle(parent_computed_style_declaration, root_computed_style,
                        style_change_event_time);
  }

  // Do not update computed style for descendants of "display: none" elements,
  // since they do not participate in layout. Note the "display: node" elements
  // themselves still need to have their computed style updated, in case the
  // value of display is changed.
  if (computed_style()->display() == cssom::KeywordValue::GetNone()) {
    return;
  }

  // Update computed style for this element's descendants.
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->UpdateComputedStyleRecursively(
          css_computed_style_declaration(), root_computed_style,
          style_change_event_time, is_valid);
    }
  }
}

void HTMLElement::InvalidateLayoutBoxesFromNodeAndAncestors() {
  layout_boxes_.reset();
  cached_background_images_.clear();
  Node::InvalidateLayoutBoxesFromNodeAndAncestors();
}

void HTMLElement::InvalidateLayoutBoxesFromNodeAndDescendants() {
  layout_boxes_.reset();
  cached_background_images_.clear();
  Node::InvalidateLayoutBoxesFromNodeAndDescendants();
}

void HTMLElement::InvalidateLayoutBoxSizesFromNode() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateSizes();
  }
}

void HTMLElement::InvalidateLayoutBoxCrossReferencesFromNode() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateCrossReferences();
  }
}

void HTMLElement::InvalidateRenderTreeNodesFromNode() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateRenderTreeNodes();
  }
}

HTMLElement::HTMLElement(Document* document, base::Token tag_name)
    : Element(document, tag_name),
      directionality_(kNoExplicitDirectionality),
      style_(new cssom::CSSDeclaredStyleDeclaration(
          document->html_element_context()->css_parser())),
      computed_style_valid_(false),
      css_computed_style_declaration_(new cssom::CSSComputedStyleDeclaration()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          transitions_adapter_(new DOMAnimatable(this))),
      css_transitions_(&transitions_adapter_),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          animations_adapter_(new DOMAnimatable(this))),
      css_animations_(&animations_adapter_),
      old_matching_rules_(new cssom::RulesWithCascadePrecedence()),
      matching_rules_(new cssom::RulesWithCascadePrecedence()),
      matching_rules_valid_(false),
      dom_stat_tracker_(document->html_element_context()->dom_stat_tracker()) {
  css_computed_style_declaration_->set_animations(animations());
  style_->set_mutation_observer(this);
  ++(non_trivial_static_fields.Get().html_element_count_log.count);
  dom_stat_tracker_->OnHtmlElementCreated();
}

HTMLElement::~HTMLElement() {
  --(non_trivial_static_fields.Get().html_element_count_log.count);
  dom_stat_tracker_->OnHtmlElementDestroyed();
  style_->set_mutation_observer(NULL);
}

void HTMLElement::CopyDirectionality(const HTMLElement& other) {
  directionality_ = other.directionality_;
}

void HTMLElement::OnMutation() { InvalidateMatchingRulesRecursively(); }

void HTMLElement::OnSetAttribute(const std::string& name,
                                 const std::string& value) {
  if (name == "class" || name == "id") {
    InvalidateMatchingRulesRecursively();
  } else if (name == "dir") {
    SetDirectionality(value);
  }
}

void HTMLElement::OnRemoveAttribute(const std::string& name) {
  if (name == "class" || name == "id") {
    InvalidateMatchingRulesRecursively();
  } else if (name == "dir") {
    SetDirectionality("");
  }
}

void HTMLElement::SetDirectionality(const std::string& value) {
  // NOTE: Value "auto" is not supported.
  Directionality previous_directionality = directionality_;
  if (value == "ltr") {
    directionality_ = kLeftToRightDirectionality;
  } else if (value == "rtl") {
    directionality_ = kRightToLeftDirectionality;
  } else {
    directionality_ = kNoExplicitDirectionality;
  }

  if (directionality_ != previous_directionality) {
    InvalidateLayoutBoxesFromNodeAndAncestors();
    InvalidateLayoutBoxesFromNodeAndDescendants();
  }
}

namespace {

scoped_refptr<cssom::CSSComputedStyleData> PromoteMatchingRulesToComputedStyle(
    cssom::RulesWithCascadePrecedence* matching_rules,
    cssom::GURLMap* property_key_to_base_url_map,
    const scoped_refptr<const cssom::CSSDeclaredStyleData>& inline_style,
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const math::Size& viewport_size,
    const base::TimeDelta& style_change_event_time,
    const scoped_refptr<const cssom::CSSComputedStyleData>&
        previous_computed_style,
    cssom::TransitionSet* css_transitions, cssom::AnimationSet* css_animations,
    const cssom::CSSKeyframesRule::NameMap& keyframes_map,
    bool* animations_modified) {
  // Select the winning value for each property by performing the cascade,
  // that is, apply values from matching rules on top of inline style, taking
  // into account rule specificity and location in the source file, as well as
  // property declaration importance.
  scoped_refptr<cssom::CSSComputedStyleData> computed_style =
      PromoteToCascadedStyle(inline_style, matching_rules,
                             property_key_to_base_url_map);

  // Lastly, absolutize the values, if possible. Start by resolving "initial"
  // and "inherit" keywords (which gives us what the specification refers to
  // as "specified style").  Then, convert length units and percentages into
  // pixels, convert color keywords into RGB triplets, and so on.  For certain
  // properties, like "font-family", computed value is the same as specified
  // value. Declarations that cannot be absolutized easily, like "width: auto;",
  // will be resolved during layout.
  cssom::PromoteToComputedStyle(
      computed_style, parent_computed_style_declaration, root_computed_style,
      viewport_size, property_key_to_base_url_map);

  if (previous_computed_style) {
    // Now that we have updated our computed style, compare it to the previous
    // style and see if we need to adjust our animations.
    css_transitions->UpdateTransitions(
        style_change_event_time, *previous_computed_style, *computed_style);
  }
  // Update the set of currently running animations and track whether or not the
  // animations changed.
  *animations_modified = css_animations->Update(style_change_event_time,
                                                *computed_style, keyframes_map);

  return computed_style;
}

// Flags tracking which cached values must be invalidated.
struct UpdateComputedStyleInvalidationFlags {
  UpdateComputedStyleInvalidationFlags()
      : invalidate_descendant_computed_styles(false),
        invalidate_layout_boxes(false),
        invalidate_sizes(false),
        invalidate_cross_references(false),
        invalidate_render_tree_nodes(false) {}

  bool invalidate_descendant_computed_styles;
  bool invalidate_layout_boxes;
  bool invalidate_sizes;
  bool invalidate_cross_references;
  bool invalidate_render_tree_nodes;
};

bool NewComputedStyleInvalidatesDescendantComputedStyles(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style) {
  return !non_trivial_static_fields.Get()
              .computed_style_invalidation_property_checker
              .DoDeclaredPropertiesMatch(old_computed_style,
                                         new_computed_style);
}

bool NewComputedStyleInvalidatesLayoutBoxes(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style) {
  return !non_trivial_static_fields.Get()
              .layout_box_invalidation_property_checker
              .DoDeclaredPropertiesMatch(old_computed_style,
                                         new_computed_style);
}

bool NewComputedStyleInvalidatesSizes(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style) {
  return !non_trivial_static_fields.Get()
              .size_invalidation_property_checker.DoDeclaredPropertiesMatch(
                  old_computed_style, new_computed_style);
}

bool NewComputedStyleInvalidatesCrossReferences(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style) {
  return !non_trivial_static_fields.Get()
              .cross_references_invalidation_property_checker
              .DoDeclaredPropertiesMatch(old_computed_style,
                                         new_computed_style);
}

void UpdateInvalidationFlagsForNewComputedStyle(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style,
    bool animations_modified, UpdateComputedStyleInvalidationFlags* flags) {
  if (old_computed_style) {
    if (!flags->invalidate_descendant_computed_styles &&
        NewComputedStyleInvalidatesDescendantComputedStyles(
            old_computed_style, new_computed_style)) {
      flags->invalidate_descendant_computed_styles = true;
      flags->invalidate_layout_boxes = true;
    } else if (!flags->invalidate_layout_boxes) {
      if (NewComputedStyleInvalidatesLayoutBoxes(old_computed_style,
                                                 new_computed_style)) {
        flags->invalidate_layout_boxes = true;
      } else {
        if (!flags->invalidate_sizes &&
            NewComputedStyleInvalidatesSizes(old_computed_style,
                                             new_computed_style)) {
          flags->invalidate_sizes = true;
          flags->invalidate_render_tree_nodes = true;
        }
        if (!flags->invalidate_cross_references &&
            NewComputedStyleInvalidatesCrossReferences(old_computed_style,
                                                       new_computed_style)) {
          flags->invalidate_cross_references = true;
          flags->invalidate_render_tree_nodes = true;
        }

        flags->invalidate_render_tree_nodes =
            flags->invalidate_render_tree_nodes || animations_modified ||
            !new_computed_style->DoDeclaredPropertiesMatch(old_computed_style);
      }
    }
  }
}

}  // namespace

void HTMLElement::UpdateComputedStyle(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const base::TimeDelta& style_change_event_time) {
  Document* document = node_document();
  DCHECK(document) << "Element should be attached to document in order to "
                      "participate in layout.";

  // If there is no previous computed style, there should also be no layout
  // boxes.
  DCHECK(computed_style() || NULL == layout_boxes());

  dom_stat_tracker_->OnUpdateComputedStyle();

  // The computed style must be generated if either the computed style is
  // invalid or no computed style has been created yet.
  bool generate_computed_style = !computed_style_valid_ || !computed_style();

  // Update matching rules if necessary.
  if (!matching_rules_valid_) {
    dom_stat_tracker_->OnUpdateMatchingRules();
    UpdateMatchingRules(this);
    matching_rules_valid_ = true;

    // Check for whether the matching rules have changed. If they have, then a
    // new computed style must be generated from them.
    if (!generate_computed_style && *old_matching_rules_ != *matching_rules_) {
      generate_computed_style = true;
    }
  }

  // If any declared properties inherited from the parent are no longer valid,
  // then a new computed style must be generated with the updated inherited
  // values.
  if (!generate_computed_style &&
      !computed_style()->AreDeclaredPropertiesInheritedFromParentValid()) {
    generate_computed_style = true;
  }

  // TODO: It maybe helpful to generalize this mapping framework in the
  // future to allow more data and context about where a cssom::PropertyValue
  // came from.
  cssom::GURLMap property_key_to_base_url_map;
  property_key_to_base_url_map[cssom::kBackgroundImageProperty] =
      document->url_as_gurl();

  // Flags tracking which cached values must be invalidated.
  UpdateComputedStyleInvalidationFlags invalidation_flags;

  if (generate_computed_style) {
    bool animations_modified = false;

    scoped_refptr<cssom::CSSComputedStyleData> new_computed_style =
        PromoteMatchingRulesToComputedStyle(
            matching_rules(), &property_key_to_base_url_map, style_->data(),
            parent_computed_style_declaration, root_computed_style,
            document->viewport_size(), style_change_event_time,
            computed_style(), &css_transitions_, &css_animations_,
            document->keyframes_map(), &animations_modified);

    UpdateInvalidationFlagsForNewComputedStyle(
        computed_style(), new_computed_style, animations_modified,
        &invalidation_flags);

    css_computed_style_declaration_->SetData(new_computed_style);

    // Update cached background images after resolving the urls in
    // background_image CSS property of the computed style, so we have all the
    // information to get the cached background images.
    UpdateCachedBackgroundImagesFromComputedStyle();
  } else {
    // Update the inherited data if a new style was not generated. The ancestor
    // data with inherited properties may have changed.
    css_computed_style_declaration_->UpdateInheritedData();
  }

  // NOTE: Currently, pseudo elements computed styles are always generated. If
  // this becomes a performance bottleneck, change the logic so that it only
  // occurs when needed.

  // Promote the matching rules for all known pseudo elements.
  for (int pseudo_element_type = 0; pseudo_element_type < kMaxPseudoElementType;
       ++pseudo_element_type) {
    if (pseudo_elements_[pseudo_element_type]) {
      bool animations_modified = false;

      scoped_refptr<cssom::CSSComputedStyleData> pseudo_element_computed_style =
          PromoteMatchingRulesToComputedStyle(
              pseudo_elements_[pseudo_element_type]->matching_rules(),
              &property_key_to_base_url_map, style_->data(),
              css_computed_style_declaration(), root_computed_style,
              document->viewport_size(), style_change_event_time,
              pseudo_elements_[pseudo_element_type]->computed_style(),
              pseudo_elements_[pseudo_element_type]->css_transitions(),
              pseudo_elements_[pseudo_element_type]->css_animations(),
              document->keyframes_map(), &animations_modified);

      UpdateInvalidationFlagsForNewComputedStyle(
          pseudo_elements_[pseudo_element_type]->computed_style(),
          pseudo_element_computed_style, animations_modified,
          &invalidation_flags);

      pseudo_elements_[pseudo_element_type]
          ->css_computed_style_declaration()
          ->SetData(pseudo_element_computed_style);
    }
  }

  if (invalidation_flags.invalidate_descendant_computed_styles) {
    InvalidateComputedStylesRecursively();
  }

  if (invalidation_flags.invalidate_layout_boxes) {
    InvalidateLayoutBoxesFromNodeAndAncestors();
    InvalidateLayoutBoxesFromNodeAndDescendants();
  } else {
    if (invalidation_flags.invalidate_sizes) {
      InvalidateLayoutBoxSizesFromNode();
    }
    if (invalidation_flags.invalidate_cross_references) {
      InvalidateLayoutBoxCrossReferencesFromNode();
    }
    if (invalidation_flags.invalidate_render_tree_nodes) {
      InvalidateRenderTreeNodesFromNode();
    }
  }

  computed_style_valid_ = true;
}

void HTMLElement::UpdateCachedBackgroundImagesFromComputedStyle() {
  // Don't fetch or cache the image if the display of this element is turned
  // off.
  if (computed_style()->display() != cssom::KeywordValue::GetNone()) {
    scoped_refptr<cssom::PropertyValue> background_image =
        computed_style()->background_image();

    cssom::PropertyListValue* property_list_value =
        base::polymorphic_downcast<cssom::PropertyListValue*>(
            background_image.get());

    loader::image::CachedImageReferenceVector cached_images;
    for (size_t i = 0; i < property_list_value->value().size(); ++i) {
      // Skip this image if it is not an absolute URL.
      if (property_list_value->value()[i]->GetTypeId() !=
          base::GetTypeId<cssom::AbsoluteURLValue>()) {
        continue;
      }

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
                cached_image, loaded_callback, base::Closure(),
                base::Closure()));
      }
    }

    cached_background_images_ = cached_images.Pass();
  } else {
    // Clear the previous cached background image if the display is "none".
    cached_background_images_.clear();
  }
}

void HTMLElement::OnBackgroundImageLoaded() {
  node_document()->RecordMutation();
  InvalidateRenderTreeNodesFromNode();
}

bool HTMLElement::IsRootElement() {
  // The html element represents the root of an HTML document.
  //   https://www.w3.org/TR/2014/REC-html5-20141028/semantics.html#the-root-element
  return AsHTMLHtmlElement() != NULL;
}

}  // namespace dom
}  // namespace cobalt
