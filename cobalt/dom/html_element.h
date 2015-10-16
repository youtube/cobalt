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

#ifndef DOM_HTML_ELEMENT_H_
#define DOM_HTML_ELEMENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "cobalt/cssom/computed_style_state.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/layout_boxes.h"
#include "cobalt/dom/pseudo_element.h"
#include "cobalt/loader/image/image_cache.h"

namespace cobalt {
namespace dom {

class DOMStringMap;
class HTMLAnchorElement;
class HTMLBodyElement;
class HTMLBRElement;
class HTMLDivElement;
class HTMLElementContext;
class HTMLHeadElement;
class HTMLHeadingElement;
class HTMLHtmlElement;
class HTMLImageElement;
class HTMLLinkElement;
class HTMLMetaElement;
class HTMLParagraphElement;
class HTMLScriptElement;
class HTMLSpanElement;
class HTMLStyleElement;
class HTMLUnknownElement;
class HTMLVideoElement;

// The enum PseudoElementType is used to track the type of pseudo element
enum PseudoElementType {
  kAfterPseudoElementType,
  kBeforePseudoElementType,
  kMaxPseudoElementType,
  kNotPseudoElementType = kMaxPseudoElementType,
  kMaxAnyElementType,
};

// The basic interface, from which all the HTML elements' interfaces inherit,
// and which must be used by elements that have no additional requirements.
//   http://www.w3.org/TR/html5/dom.html#htmlelement
class HTMLElement : public Element, public cssom::MutationObserver {
 public:
  struct RuleMatchingState {
    cssom::SelectorTree::NodeSet matching_nodes;
    cssom::SelectorTree::NodeSet descendant_potential_nodes;
    cssom::SelectorTree::NodeSet following_sibling_potential_nodes;
  };

  // Web API: HTMLElement
  //
  scoped_refptr<DOMStringMap> dataset();

  int32 tab_index() const;
  void set_tab_index(int32 tab_index);

  void Focus();
  void Blur();

  // Web API: ElementCSSInlineStyle (implements)
  //   http://www.w3.org/TR/2013/WD-cssom-20131205/#elementcssinlinestyle
  const scoped_refptr<cssom::CSSStyleDeclaration>& style() { return style_; }

  // Custom, not in any spec: Node.
  scoped_refptr<Node> Duplicate() const OVERRIDE;

  // Custom, not in any spec: Element.
  void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) OVERRIDE;
  scoped_refptr<HTMLElement> AsHTMLElement() OVERRIDE { return this; }

  // Custom, not in any spec.
  //
  // From cssom::CSSStyleDeclaration::MutationObserver.
  void OnCSSMutation() OVERRIDE;

  // Safe type conversion methods that will downcast to the required type if
  // possible or return NULL otherwise.
  virtual scoped_refptr<HTMLAnchorElement> AsHTMLAnchorElement();
  virtual scoped_refptr<HTMLBodyElement> AsHTMLBodyElement();
  virtual scoped_refptr<HTMLBRElement> AsHTMLBRElement();
  virtual scoped_refptr<HTMLDivElement> AsHTMLDivElement();
  virtual scoped_refptr<HTMLHeadElement> AsHTMLHeadElement();
  virtual scoped_refptr<HTMLHeadingElement> AsHTMLHeadingElement();
  virtual scoped_refptr<HTMLHtmlElement> AsHTMLHtmlElement();
  virtual scoped_refptr<HTMLImageElement> AsHTMLImageElement();
  virtual scoped_refptr<HTMLLinkElement> AsHTMLLinkElement();
  virtual scoped_refptr<HTMLMetaElement> AsHTMLMetaElement();
  virtual scoped_refptr<HTMLParagraphElement> AsHTMLParagraphElement();
  virtual scoped_refptr<HTMLScriptElement> AsHTMLScriptElement();
  virtual scoped_refptr<HTMLSpanElement> AsHTMLSpanElement();
  virtual scoped_refptr<HTMLStyleElement> AsHTMLStyleElement();
  virtual scoped_refptr<HTMLUnknownElement> AsHTMLUnknownElement();
  virtual scoped_refptr<HTMLVideoElement> AsHTMLVideoElement();

  // Rule matching related methods.
  //
  // Returns pointer to cached rule matching results.
  cssom::RulesWithCascadePriority* matching_rules() { return &matching_rules_; }
  // Returns rule matching state.
  RuleMatchingState* rule_matching_state() { return &rule_matching_state_; }
  // Returns whether the matching rules are valid.
  bool matching_rules_valid() const { return matching_rules_valid_; }
  // Sets the flag indicating matching rules are valid.
  void SetMatchingRulesValid() { matching_rules_valid_ = true; }
  // Invalidates the matching rules and rule matching state in this element.
  void InvalidateMatchingRules();

  // Computed style related methods.
  //
  // Used by layout engine to cache the computed values.
  // See http://www.w3.org/TR/css-cascade-3/#computed for the definition of
  // computed value.
  scoped_refptr<cssom::ComputedStyleState>& computed_style_state() {
    return computed_style_state_;
  }
  scoped_refptr<const cssom::CSSStyleDeclarationData> computed_style() const {
    return computed_style_state_->style();
  }
  void set_computed_style(
      scoped_refptr<cssom::CSSStyleDeclarationData> computed_style) {
    computed_style_state_->set_style(computed_style);
  }
  // Updates the cached computed style of one HTML element.
  //   http://www.w3.org/TR/css-cascade-3/#value-stages
  void UpdateComputedStyle(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>&
          parent_computed_style,
      const base::TimeDelta& style_change_event_time);
  // Calls UpdateComputedStyle() on itself and all descendants.
  void UpdateComputedStyleRecursively(
      const scoped_refptr<const cssom::CSSStyleDeclarationData>&
          parent_computed_style,
      const base::TimeDelta& style_change_event_time,
      bool ancestors_were_valid);

  // Layout box related methods.
  //
  // The LayoutContainerBox gives the HTML Element an interface to the container
  // box that result from it. The BoxList is set when layout is performed for a
  // node.
  void SetLayoutBoxes(scoped_ptr<LayoutBoxes> layout_boxes);
  LayoutBoxes* layout_boxes() const { return layout_boxes_.get(); }
  void InvalidateLayoutBoxesFromNodeAndAncestors() OVERRIDE;
  void InvalidateLayoutBoxesFromNodeAndDescendants() OVERRIDE;

  // Determines whether this element is focusable.
  virtual bool IsFocusable() { return HasAttribute("tabindex"); }

  cssom::TransitionSet* transitions() const {
    return computed_style_state_->transitions();
  }

  PseudoElement* pseudo_element(PseudoElementType type) const {
    DCHECK(type < kMaxPseudoElementType);
    return pseudo_elements_[type].get();
  }

  void set_pseudo_element(PseudoElementType type, PseudoElement* element) {
    DCHECK(type < kMaxPseudoElementType);
    return pseudo_elements_[type].reset(element);
  }

  DEFINE_WRAPPABLE_TYPE(HTMLElement);

 protected:
  explicit HTMLElement(Document* document);
  HTMLElement(Document* document, const std::string& tag_name);
  ~HTMLElement() OVERRIDE;

 private:
  // From Node.
  void OnInsertBefore(const scoped_refptr<Node>& new_child,
                      const scoped_refptr<Node>& reference_child) OVERRIDE;
  void OnRemoveChild(const scoped_refptr<Node>& node) OVERRIDE;

  // From Element.
  void OnSetAttribute(const std::string& name,
                      const std::string& value) OVERRIDE;
  void OnRemoveAttribute(const std::string& name) OVERRIDE;

  void UpdateCachedBackgroundImagesFromComputedStyle();

  // This will be called when the image data associated with this element's
  // computed style's background-image property is loaded.
  void OnBackgroundImageLoaded();

  // The inline style specified via attribute's in the element's HTML tag, or
  // through JavaScript (accessed via style() defined above).
  scoped_refptr<cssom::CSSStyleDeclaration> style_;

  // Keeps track of whether the HTML element's current computed style is out
  // of date or not.
  bool computed_style_valid_;

  scoped_refptr<cssom::ComputedStyleState> computed_style_state_;

  // The following fields are used in rule matching.
  cssom::RulesWithCascadePriority matching_rules_;
  RuleMatchingState rule_matching_state_;

  // This contains information about the boxes generated from the element.
  scoped_ptr<LayoutBoxes> layout_boxes_;

  bool matching_rules_valid_;

  cssom::TransitionSet transitions_;
  scoped_ptr<PseudoElement> pseudo_elements_[kMaxPseudoElementType];
  scoped_refptr<DOMStringMap> dataset_;

  // |cached_background_images_| contains a list of CachedImage references for
  // all images referenced by the computed value for the background_image CSS
  // property and a image loaded handle to add and remove image loaded callback.
  // We maintain it here to indicate to the resource caching system
  // that the images are currently in-use, and should not be purged.
  loader::image::CachedImageReferenceVector cached_background_images_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_ELEMENT_H_
