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
// limitations under the License.

#ifndef COBALT_DOM_HTML_ELEMENT_H_
#define COBALT_DOM_HTML_ELEMENT_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "cobalt/base/token.h"
#include "cobalt/cssom/animation_set.h"
#include "cobalt/cssom/css_computed_style_declaration.h"
#include "cobalt/cssom/css_declared_style_declaration.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_transition_set.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/css_animations_adapter.h"
#include "cobalt/dom/css_transitions_adapter.h"
#include "cobalt/dom/dom_rect_list.h"
#include "cobalt/dom/dom_stat_tracker.h"
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
class HTMLTitleElement;
class HTMLUnknownElement;
class HTMLVideoElement;

// The enum Directionality is used to track the explicit direction of the html
// element:
// https://dev.w3.org/html5/spec-preview/global-attributes.html#the-directionality
// NOTE: Value "auto" is not supported.
enum Directionality {
  kNoExplicitDirectionality,
  kLeftToRightDirectionality,
  kRightToLeftDirectionality,
};

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
//   https://www.w3.org/TR/html5/dom.html#htmlelement
class HTMLElement : public Element, public cssom::MutationObserver {
 public:
  typedef cssom::SelectorTree::NodeSet<12> MatchingNodes;
  typedef cssom::SelectorTree::NodeSet<40> DescendantPotentialNodes;
  typedef cssom::SelectorTree::NodeSet<8> FollowingSiblingPotentialNodes;

  struct RuleMatchingState {
    MatchingNodes matching_nodes;
    DescendantPotentialNodes descendant_potential_nodes;
    FollowingSiblingPotentialNodes following_sibling_potential_nodes;
  };

  enum AncestorsAreDisplayed {
    kAncestorsAreDisplayed,
    kAncestorsAreNotDisplayed,
  };

  // Web API: HTMLElement
  //
  std::string dir() const;
  void set_dir(const std::string& value);

  scoped_refptr<DOMStringMap> dataset();

  int32 tab_index() const;
  void set_tab_index(int32 tab_index);

  void Focus();
  void Blur();

  // Web API: ElementCSSInlineStyle (implements)
  //   https://www.w3.org/TR/2013/WD-cssom-20131205/#elementcssinlinestyle
  const scoped_refptr<cssom::CSSDeclaredStyleDeclaration>& style() {
    return style_;
  }

  // Web API: CSSOM View Module: Extensions to the Element Interface (partial
  // interface)
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-element-interface
  scoped_refptr<DOMRectList> GetClientRects() override;
  float client_top() override;
  float client_left() override;
  float client_width() override;
  float client_height() override;

  // Web API: CSSOM View Module: Extensions to the HTMLElement Interface
  // (partial interface)
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-htmlelement-interface
  Element* offset_parent();
  float offset_top();
  float offset_left();
  float offset_width();
  float offset_height();

  // Custom, not in any spec: Node.
  scoped_refptr<Node> Duplicate() const override;

  // Custom, not in any spec: Element.
  scoped_refptr<HTMLElement> AsHTMLElement() override { return this; }

  base::optional<std::string> GetStyleAttribute() const override;
  void SetStyleAttribute(const std::string& value) override;
  void RemoveStyleAttribute() override;

  // Custom, not in any spec.
  //
  // From cssom::CSSStyleDeclaration::MutationObserver.
  void OnCSSMutation() override;

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
  virtual scoped_refptr<HTMLTitleElement> AsHTMLTitleElement();
  virtual scoped_refptr<HTMLUnknownElement> AsHTMLUnknownElement();
  virtual scoped_refptr<HTMLVideoElement> AsHTMLVideoElement();

  // Returns the directionality of the element, which is based upon the
  // underlying "dir" attribute, and is updated when the attribute changes.
  // https://dev.w3.org/html5/spec-preview/global-attributes.html#the-directionalityy.
  Directionality directionality() const { return directionality_; }

  // Rule matching related methods.
  //
  // Returns the cached matching rules of this element.
  cssom::RulesWithCascadePrecedence* matching_rules() {
    return &matching_rules_;
  }
  // Returns the rule matching state of this element.
  RuleMatchingState* rule_matching_state() { return &rule_matching_state_; }
  // Invalidates the matching rules and rule matching state in this element,
  // its descendants and its siblings.
  void InvalidateMatchingRulesRecursively();

  // Computed style related methods.
  //
  // Used by layout engine to cache the computed values.
  // See https://www.w3.org/TR/css-cascade-3/#computed for the definition of
  // computed value.
  scoped_refptr<cssom::CSSComputedStyleDeclaration>&
  css_computed_style_declaration() {
    return css_computed_style_declaration_;
  }
  const scoped_refptr<const cssom::CSSComputedStyleData>& computed_style()
      const {
    return css_computed_style_declaration_->data();
  }

  void MarkDisplayNoneOnNodeAndDescendants() override;
  void PurgeCachedBackgroundImagesOfNodeAndDescendants() override;
  void InvalidateComputedStylesOfNodeAndDescendants() override;
  void InvalidateLayoutBoxesOfNodeAndAncestors() override;
  void InvalidateLayoutBoxesOfNodeAndDescendants() override;
  void InvalidateLayoutBoxSizes() override;
  void InvalidateLayoutBoxCrossReferences() override;
  void InvalidateLayoutBoxRenderTreeNodes() override;

  // Updates the cached computed style of this element and its descendants.
  void UpdateComputedStyleRecursively(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          parent_computed_style,
      const scoped_refptr<const cssom::CSSComputedStyleData>&
          root_computed_style,
      const base::TimeDelta& style_change_event_time, bool ancestors_were_valid,
      int current_element_depth);

  // Updates the cached computed style of this element.
  void UpdateComputedStyle(
      const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
          parent_computed_style_declaration,
      const scoped_refptr<const cssom::CSSComputedStyleData>&
          root_computed_style,
      const base::TimeDelta& style_change_event_time,
      AncestorsAreDisplayed ancestor_is_displayed);

  // Layout box related methods.
  //
  // The LayoutContainerBox gives the HTML Element an interface to the container
  // box that result from it. The BoxList is set when layout is performed for a
  // node.
  void set_layout_boxes(scoped_ptr<LayoutBoxes> layout_boxes) {
    layout_boxes_ = layout_boxes.Pass();
  }

  LayoutBoxes* layout_boxes() const { return layout_boxes_.get(); }

  PseudoElement* pseudo_element(PseudoElementType type) const {
    DCHECK(type < kMaxPseudoElementType);
    return pseudo_elements_[type].get();
  }

  void set_pseudo_element(PseudoElementType type,
                          scoped_ptr<PseudoElement> pseudo_element) {
    DCHECK_EQ(this, pseudo_element->parent_element());
    DCHECK(type < kMaxPseudoElementType);
    pseudo_elements_[type] = pseudo_element.Pass();
  }

  bool computed_style_valid() const { return computed_style_valid_; }
  bool descendant_computed_styles_valid() const {
    return descendant_computed_styles_valid_;
  }
  bool matching_rules_valid() const { return matching_rules_valid_; }
  void set_matching_rules_valid() { matching_rules_valid_ = true; }

  // Returns whether the element has been designated.
  //   https://www.w3.org/TR/selectors4/#hover-pseudo
  bool IsDesignated() const;

  // Returns whether the element can be designated by a pointer.
  //   https://www.w3.org/TR/SVG11/interact.html#PointerEventsProperty
  bool CanbeDesignatedByPointerIfDisplayed() const;

  // Returns true if this node and all of its ancestors do NOT have display set
  // to 'none'.
  bool IsDisplayed() const;

  DEFINE_WRAPPABLE_TYPE(HTMLElement);

 protected:
  HTMLElement(Document* document, base::Token local_name);
  ~HTMLElement() override;

  void OnInsertedIntoDocument() override;
  void OnRemovedFromDocument() override;

  void CopyDirectionality(const HTMLElement& other);

  // HTMLElement keeps a pointer to the dom stat tracker to ensure that it can
  // make stat updates even after its weak pointer to its document has been
  // deleted. This is protected because some derived classes need access to it.
  DomStatTracker* const dom_stat_tracker_;

 private:
  // From Node.
  void OnMutation() override;

  // From Element.
  void OnSetAttribute(const std::string& name,
                      const std::string& value) override;
  void OnRemoveAttribute(const std::string& name) override;

  bool IsFocusable();
  bool HasTabindexFocusFlag() const;
  bool IsBeingRendered();

  void RunFocusingSteps();
  void RunUnFocusingSteps();

  // This both updates the directionality based upon the string value and
  // invalidates layout box caching if the value has changed.
  // NOTE1: Value "auto" is not supported.
  // NOTE2: Cobalt does not support either the CSS 'direction" or "unicode-bidi'
  // properties, and instead relies entirely upon the 'dir' attribute for
  // determining directionality of elements. As a result of this, setting the
  // directionality does not invalidate the computed style.
  void SetDirectionality(const std::string& value);

  // Invalidates the matching rules and rule matching state in this element and
  // its descendants. In the case where this is the the initial invalidation,
  // it will also invalidate the rule matching state of its siblings.
  void InvalidateMatchingRulesRecursivelyInternal(bool is_initial_invalidation);

  // Clear the list of active background images, and notify the animated image
  // tracker to stop the animations.
  void ClearActiveBackgroundImages();

  void UpdateCachedBackgroundImagesFromComputedStyle();

  // This will be called when the image data associated with this element's
  // computed style's background-image property is loaded.
  void OnBackgroundImageLoaded();

  // Returns true if the element is the root element as defined in
  // https://www.w3.org/TR/html5/semantics.html#the-root-element.
  bool IsRootElement();

  // Purge the cached background images on only this node.
  void PurgeCachedBackgroundImages();

  bool locked_for_focus_;

  // The directionality of the html element is determined by the 'dir'
  // attribute.
  // https://dev.w3.org/html5/spec-preview/global-attributes.html#the-directionality
  // NOTE1: Value "auto" is not supported.
  // NOTE2: Cobalt does not support either the CSS 'direction" or "unicode-bidi'
  // properties, and instead relies entirely upon the 'dir' attribute for
  // determining directionality. Inheritance of directionality occurs via the
  // base direction of the parent element's paragraph.
  Directionality directionality_;

  // The inline style specified via attribute's in the element's HTML tag, or
  // through JavaScript (accessed via style() defined above).
  scoped_refptr<cssom::CSSDeclaredStyleDeclaration> style_;

  // Keeps track of whether the HTML element's current computed style is out
  // of date or not.
  bool computed_style_valid_;
  // Keeps track of whether the HTML element's descendants' computed styles are
  // out of date or not.
  bool descendant_computed_styles_valid_;

  // Indicates whether this node has an ancestor which has display set to none
  // or not. This value gets updated when computed style is updated.
  AncestorsAreDisplayed ancestors_are_displayed_;

  scoped_refptr<cssom::CSSComputedStyleDeclaration>
      css_computed_style_declaration_;

  dom::CSSTransitionsAdapter transitions_adapter_;
  cssom::TransitionSet css_transitions_;

  dom::CSSAnimationsAdapter animations_adapter_;
  cssom::AnimationSet css_animations_;

  // The following fields are used in rule matching.
  cssom::RulesWithCascadePrecedence old_matching_rules_;
  cssom::RulesWithCascadePrecedence matching_rules_;
  RuleMatchingState rule_matching_state_;
  bool matching_rules_valid_;

  // This contains information about the boxes generated from the element.
  scoped_ptr<LayoutBoxes> layout_boxes_;

  scoped_ptr<PseudoElement> pseudo_elements_[kMaxPseudoElementType];
  base::WeakPtr<DOMStringMap> dataset_;

  std::vector<GURL> active_background_images_;

  // |cached_background_images_| contains a list of CachedImage references for
  // all images referenced by the computed value for the background_image CSS
  // property and a image loaded handle to add and remove image loaded callback.
  // We maintain it here to indicate to the resource caching system
  // that the images are currently in-use, and should not be purged.
  loader::image::CachedImageReferenceVector cached_background_images_;

  // HTMLElement is a friend of Animatable so that animatable can insert and
  // remove animations into HTMLElement's set of animations.
  friend class DOMAnimatable;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_ELEMENT_H_
