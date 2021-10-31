// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_element.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "cobalt/base/console_log.h"
#include "cobalt/base/tokens.h"
#include "cobalt/cssom/absolute_url_value.h"
#include "cobalt/cssom/cascaded_style.h"
#include "cobalt/cssom/computed_style.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/selector_tree.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_animatable.h"
#include "cobalt/dom/dom_string_map.h"
#include "cobalt/dom/focus_event.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_audio_element.h"
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
#include "cobalt/dom/lottie_player.h"
#include "cobalt/dom/rule_matching.h"
#include "cobalt/dom/text.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/resource_cache.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf8.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {

namespace {

// If an element has a "tabindex" attribute less than -1, it will be considered
// a UI navigation focus item. The web spec states that all negative tabindex
// values are treated the same. However, since some apps may want to treat
// certain elements as HTML focusable but not UI navigation focus items, the
// commonly used value of -1 is reserved.
// https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/tabindex
const int32 kUiNavFocusTabIndexThreshold = -2;

// This custom data attribute can be used to force UI navigation to remain
// focused on the element for a specified duration.
const char kUiNavFocusDurationAttribute[] = "data-cobalt-ui-nav-focus-duration";

// https://www.w3.org/TR/resource-timing-1/#dom-performanceresourcetiming-initiatortype
const char* kPerformanceResourceTimingInitiatorType = "img";

void UiNavCallbackHelper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::Callback<void(SbTimeMonotonic)> callback) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(callback, SbTimeGetMonotonicNow()));
}

struct NonTrivialStaticFields {
  NonTrivialStaticFields() {
    cssom::PropertyKeyVector computed_style_invalidation_properties;
    cssom::PropertyKeyVector layout_box_invalidation_properties;
    cssom::PropertyKeyVector size_invalidation_properties;
    cssom::PropertyKeyVector cross_references_invalidation_properties;

    for (int i = 0; i <= cssom::kMaxLonghandPropertyKey; ++i) {
      cssom::PropertyKey property_key = static_cast<cssom::PropertyKey>(i);

      if (cssom::GetPropertyImpactsChildComputedStyle(property_key) ==
          cssom::kImpactsChildComputedStyleYes) {
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

 private:
  DISALLOW_COPY_AND_ASSIGN(NonTrivialStaticFields);
};

base::LazyInstance<NonTrivialStaticFields>::DestructorAtExit
    non_trivial_static_fields = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void HTMLElement::RuleMatchingState::Clear() {
  if (is_set) {
    is_set = false;

    parent_matching_nodes.clear();
    parent_descendant_nodes.clear();

    previous_sibling_matching_nodes.clear();
    previous_sibling_following_sibling_nodes.clear();

    matching_nodes_parent_nodes.clear();
    matching_nodes.clear();

    are_descendant_nodes_dirty = true;
    descendant_nodes.clear();

    are_following_sibling_nodes_dirty = true;
    following_sibling_nodes.clear();
  }
}

std::string HTMLElement::dir() const {
  // The dir attribute is limited to only known values. On getting, dir must
  // return the conforming value associated with the state the attribute is in,
  // or the empty string if the attribute is in a state with no associated
  // keyword value.
  // https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#the-dir-attribute
  if (dir_ == kDirAuto) {
    return "auto";
  } else if (dir_ == kDirLeftToRight) {
    return "ltr";
  } else if (dir_ == kDirRightToLeft) {
    return "rtl";
  } else {
    return "";
  }
}

void HTMLElement::set_dir(const std::string& value) {
  // Funnel through OnSetAttribute.
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
  if (tabindex_) {
    return *tabindex_;
  }
  LOG(WARNING) << "Element's tabindex is not valid.";
  // The default value is 0 for focusable elements.
  // https://html.spec.whatwg.org/multipage/interaction.html#attr-tabindex
  return 0;
}

void HTMLElement::set_tab_index(int32 tab_index) {
  SetAttribute("tabindex", base::Int32ToString(tab_index));
}

// Algorithm for Focus:
//   https://www.w3.org/TR/html50/editing.html#dom-focus
void HTMLElement::Focus() {
  // 1. If the element is marked as locked for focus, then abort these steps.
  if (locked_for_focus_) {
    return;
  }

  // 2. Mark the element as locked for focus.
  locked_for_focus_ = true;

  // 3. Run the focusing steps for the element.
  RunFocusingSteps();

  // 4. Unmark the element as locked for focus.
  locked_for_focus_ = false;

  // Custom, not in any spec.
  Document* document = node_document();
  if (document) {
    document->OnFocusChange();
  }
}

// Algorithm for Blur:
//   https://www.w3.org/TR/html50/editing.html#dom-blur
void HTMLElement::Blur() {
  // The blur() method, when invoked, should run the unfocusing steps for the
  // element on which the method was called instead. User agents may selectively
  // or uniformly ignore calls to this method for usability reasons.
  RunUnFocusingSteps();

  // Custom, not in any spec.
  Document* document = node_document();
  if (document) {
    document->OnFocusChange();
  }
}

// Algorithm for GetClientRects:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-getclientrects
scoped_refptr<DOMRectList> HTMLElement::GetClientRects() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element on which it was invoked does not have an associated
  // layout box return an empty DOMRectList object and stop this algorithm.
  if (!layout_boxes_) {
    return base::WrapRefCounted(new DOMRectList());
  }

  // The remaining steps are implemented in LayoutBoxes::GetClientRects().
  return layout_boxes_->GetClientRects();
}

// Algorithm for client_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-element-clienttop
float HTMLElement::client_top() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInline()) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInline()) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInline()) {
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element has no associated CSS layout box or if the CSS layout box
  // is inline, return zero.
  if (!layout_boxes_ || layout_boxes_->IsInline()) {
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

// Algorithm for scrollWidth:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrollwidth
int32 HTMLElement::scroll_width() {
  // 1. Let document be the element's node document.
  // 2. If document is not the active document, return zero and terminate
  //    these steps.
  if (!node_document() || !node_document()->window()) {
    return 0;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  int32 element_scroll_width = 0;
  if (layout_boxes_) {
    element_scroll_width = static_cast<int32>(
        layout_boxes_->GetScrollArea(directionality()).width());
  }

  // 3. Let viewport width be the width of the viewport excluding the width of
  //    the scroll bar, if any, or zero if there is no viewport.
  // 4. If the element is the root element and document is not in quirks mode
  //    return max(viewport scrolling area width, viewport width).
  // 5. If the element is the HTML body element, document is in quirks mode
  //    and the element is not potentially scrollable, return max(viewport
  //    scrolling area width, viewport width).
  if (IsRootElement()) {
    return std::max(node_document()->viewport_size().width(),
                    element_scroll_width);
  }

  // 6. If the element does not have any associated CSS layout box return zero
  //    and terminate these steps.
  // 7. Return the width of the element's scrolling area.
  return element_scroll_width;
}

// Algorithm for scrollHeight:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrollheight
int32 HTMLElement::scroll_height() {
  // 1. Let document be the element's node document.
  // 2. If document is not the active document, return zero and terminate
  //    these steps.
  if (!node_document() || !node_document()->window()) {
    return 0;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  int32 element_scroll_height = 0;
  if (layout_boxes_) {
    element_scroll_height = static_cast<int32>(
        layout_boxes_->GetScrollArea(directionality()).height());
  }

  // 3. Let viewport height be the height of the viewport excluding the height
  //    of the scroll bar, if any, or zero if there is no viewport.
  // 4. If the element is the root element and document is not in quirks mode
  //    return max(viewport scrolling area height, viewport height).
  // 5. If the element is the HTML body element, document is in quirks mode
  //    and the element is not potentially scrollable, return max(viewport
  //    scrolling area height, viewport height).
  if (IsRootElement()) {
    return std::max(node_document()->viewport_size().height(),
                    element_scroll_height);
  }

  // 6. If the element does not have any associated CSS layout box return zero
  //    and terminate these steps.
  // 7. Return the height of the element's scrolling area.
  return element_scroll_height;
}

// Algorithm for scrollLeft:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrollleft
// For RTL, the rightmost content is visible when scrollLeft == 0, and
// scrollLeft values are <= 0. Chrome does not behave this way currently, but
// it is the spec that has been adopted.
//   https://readable-email.org/list/www-style/topic/cssom-view-value-of-scrollleft-in-rtl-situations-is-completely-busted-across-browsers
float HTMLElement::scroll_left() {
  // This is only partially implemented and will only work for elements with
  // UI navigation containers.

  // 1. Let document be the element's node document.
  // 2. If document is not the active document, return zero and terminate these
  //    steps.
  // 3. Let window be the value of document's defaultView attribute.
  // 4. If window is null, return zero and terminate these steps.
  if (!node_document() || !node_document()->window()) {
    return 0.0f;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  if (!ui_nav_item_ || !ui_nav_item_->IsContainer()) {
    return 0.0f;
  }

  // 5. If the element is the root element and document is in quirks mode,
  //    return zero and terminate these steps.
  // 6. If the element is the root element return the value of scrollX on
  //    window.
  // 7. If the element is the HTML body element, document is in quirks mode, and
  //    the element is not potentially scrollable, return the value of scrollX
  //    on window.

  // 8. If the element does not have any associated CSS layout box, return zero
  //    and terminate these steps.
  if (!layout_boxes_) {
    return 0.0f;
  }

  // 9. Return the x-coordinate of the scrolling area at the alignment point
  //    with the left of the padding edge of the element.
  float left, top;
  ui_nav_item_->GetContentOffset(&left, &top);
  return left;
}

// Algorithm for scrollTop:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrolltop
float HTMLElement::scroll_top() {
  // This is only partially implemented and will only work for elements with
  // UI navigation containers.

  // 1. Let document be the element's node document.
  // 2. If document is not the active document, return zero and terminate these
  //    steps.
  // 3. Let window be the value of document's defaultView attribute.
  // 4. If window is null, return zero and terminate these steps.
  if (!node_document() || !node_document()->window()) {
    return 0.0f;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  if (!ui_nav_item_ || !ui_nav_item_->IsContainer()) {
    return 0.0f;
  }

  // 5. If the element is the root element and document is in quirks mode,
  //    return zero and terminate these steps.
  // 6. If the element is the root element return the value of scrollY on
  //    window.
  // 7. If the element is the HTML body element, document is in quirks mode, and
  //    the element is not potentially scrollable, return the value of scrollY
  //    on window.

  // 8. If the element does not have any associated CSS layout box, return zero
  //    and terminate these steps.
  if (!layout_boxes_) {
    return 0.0f;
  }

  // 9. Return the y-coordinate of the scrolling area at the alignment point
  //    with the top of the padding edge of the element.
  float left, top;
  ui_nav_item_->GetContentOffset(&left, &top);
  return top;
}

// Algorithm for scrollLeft:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrollleft
void HTMLElement::set_scroll_left(float x) {
  // This is only partially implemented and will only work for elements with
  // UI navigation containers.

  // 1. Let x be the given value.
  // 2. Normalize non-finite values for x.

  // 3. Let document be the element's node document.
  // 4. If document is not the active document, terminate these steps.
  // 5. Let window be the value of document's defaultView attribute.
  // 6. If window is null, terminate these steps.
  if (!node_document() || !node_document()->window()) {
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  if (!ui_nav_item_ || !ui_nav_item_->IsContainer()) {
    CLOG(WARNING, debugger_hooks())
        << "scrollLeft only works on HTML elements with 'overflow' set to "
        << "'scroll' or 'auto'";
    return;
  }

  // 7. If the element is the root element and document is in quirks mode,
  //    terminate these steps.
  // 8. If the element is the root element invoke scroll() on window with x as
  //    first argument and scrollY on window as second argument, and terminate
  //    these steps.
  // 9. If the element is the HTML body element, document is in quirks mode, and
  //    the element is not potentially scrollable, invoke scroll() on window
  //    with x as first argument and scrollY on window as second argument, and
  //    terminate these steps.

  // 10. If the element does not have any associated CSS layout box, the element
  //     has no associated scrolling box, or the element has no overflow,
  //     terminate these steps.
  if (!layout_boxes_ ||
      scroll_width() <= layout_boxes_->GetPaddingEdgeWidth()) {
    // Make sure the UI navigation container is set to the expected 0.
    x = 0.0f;
  }

  // 11. Scroll the element to x,scrollTop, with the scroll behavior being
  //     "auto".
  float left, top;
  ui_nav_item_->GetContentOffset(&left, &top);
  ui_nav_item_->SetContentOffset(x, top);
}

// Algorithm for scrollTop:
//   https://www.w3.org/TR/cssom-view-1/#dom-element-scrolltop
void HTMLElement::set_scroll_top(float y) {
  // This is only partially implemented and will only work for elements with
  // UI navigation containers.

  // 1. Let y be the given value.
  // 2. Normalize non-finite values for y.

  // 3. Let document be the element's node document.
  // 4. If document is not the active document, terminate these steps.
  // 5. Let window be the value of document's defaultView attribute.
  // 6. If window is null, terminate these steps.
  if (!node_document() || !node_document()->window()) {
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  if (!ui_nav_item_ || !ui_nav_item_->IsContainer()) {
    CLOG(WARNING, debugger_hooks())
        << "scrollTop only works on HTML elements with 'overflow' set to "
        << "'scroll' or 'auto'";
    return;
  }

  // 7. If the element is the root element and document is in quirks mode,
  //    terminate these steps.
  // 8. If the element is the root element invoke scroll() on window with
  //    scrollX on window as first argument and y as second argument, and
  //    terminate these steps.
  // 9. If the element is the HTML body element, document is in quirks mode, and
  //    the element is not potentially scrollable, invoke scroll() on window
  //    with scrollX as first argument and y as second argument, and terminate
  //    these steps.

  // 10. If the element does not have any associated CSS layout box, the element
  //     has no associated scrolling box, or the element has no overflow,
  //     terminate these steps.
  if (!layout_boxes_ ||
      scroll_height() <= layout_boxes_->GetPaddingEdgeHeight()) {
    // Make sure the UI navigation container is set to the expected 0.
    y = 0.0f;
  }

  // 11. Scroll the element to scrollLeft,y, with the scroll behavior being
  //     "auto".
  float left, top;
  ui_nav_item_->GetContentOffset(&left, &top);
  ui_nav_item_->SetContentOffset(left, y);
}

// Algorithm for offsetParent:
//   https://drafts.csswg.org/date/2019-10-11/cssom-view/#extensions-to-the-htmlelement-interface
Element* HTMLElement::offset_parent() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
    return NULL;
  }

  // 2. Return the nearest ancestor element of the element for which at least
  //    one of the following is true and terminate this algorithm if such an
  //    ancestor is found:
  //    . The element is a containing block of absolutely-positioned descendants
  //      (regardless of whether there are any absolutely-positioned
  //       descendants).
  //    . It is the HTML body element.
  for (Node* ancestor_node = parent_node(); ancestor_node;
       ancestor_node = ancestor_node->parent_node()) {
    Element* ancestor_element = ancestor_node->AsElement();
    if (!ancestor_element) {
      continue;
    }
    HTMLElement* ancestor_html_element = ancestor_element->AsHTMLElement();
    if (!ancestor_html_element) {
      continue;
    }
    DCHECK(ancestor_html_element->computed_style());
    if (ancestor_html_element->AsHTMLBodyElement() ||
        ancestor_html_element->computed_style()
            ->IsContainingBlockForPositionAbsoluteElements()) {
      return ancestor_element;
    }
  }

  // 3. Return null.
  return NULL;
}

// Algorithm for offset_top:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsettop
float HTMLElement::offset_top() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
         offset_parent_html_element->layout_boxes()->GetPaddingEdgeOffset().y();
}

// Algorithm for offset_left:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetleft
float HTMLElement::offset_left() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
         offset_parent_html_element->layout_boxes()->GetPaddingEdgeOffset().x();
}

// Algorithm for offset_width:
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#dom-htmlelement-offsetwidth
float HTMLElement::offset_width() {
  DCHECK(node_document());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  if (!layout_boxes_) {
    if (IsRootElement()) {
      node_document()->OnRootElementUnableToProvideOffsetDimensions();
    }
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  node_document()->DoSynchronousLayout();

  // 1. If the element does not have any associated CSS layout box return zero
  // and terminate this algorithm.
  if (!layout_boxes_) {
    if (IsRootElement()) {
      node_document()->OnRootElementUnableToProvideOffsetDimensions();
    }
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
          ->CreateHTMLElement(document, local_name());
  new_html_element->CopyAttributes(*this);
  new_html_element->style_->AssignFrom(*this->style_);

  // Copy cached attributes.
  new_html_element->tabindex_ = tabindex_;
  new_html_element->dir_ = dir_;

  return new_html_element;
}

base::Optional<std::string> HTMLElement::GetStyleAttribute() const {
  base::Optional<std::string> value = Element::GetStyleAttribute();
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
    PostToDispatchEventName(FROM_HERE, base::Tokens::error());
  }
}

void HTMLElement::RemoveStyleAttribute() {
  style_->set_css_text("", NULL);
  Element::RemoveStyleAttribute();
}

void HTMLElement::OnCSSMutation() {
  // Invalidate the computed style of this node.
  computed_style_valid_ = false;
  descendant_computed_styles_valid_ = false;

  // Remove the style attribute value from the Element.
  Element::RemoveStyleAttribute();

  node_document()->OnElementInlineStyleMutation();
}

scoped_refptr<HTMLAnchorElement> HTMLElement::AsHTMLAnchorElement() {
  return NULL;
}

scoped_refptr<HTMLAudioElement> HTMLElement::AsHTMLAudioElement() {
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

scoped_refptr<HTMLMediaElement> HTMLElement::AsHTMLMediaElement() {
  return NULL;
}

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

scoped_refptr<LottiePlayer> HTMLElement::AsLottiePlayer() { return NULL; }

void HTMLElement::ClearRuleMatchingState() {
  ClearRuleMatchingStateInternal(true /*invalidate_descendants*/);
}

void HTMLElement::ClearRuleMatchingStateInternal(bool invalidate_descendants) {
  rule_matching_state_.Clear();
  matching_rules_valid_ = false;

  if (invalidate_descendants) {
    InvalidateMatchingRulesRecursivelyInternal(true /*is_initial_element*/);
  }
}

void HTMLElement::ClearRuleMatchingStateOnElementAndAncestors(
    bool invalidate_matching_rules) {
  Element* parent_element = this->parent_element();
  HTMLElement* parent_html_element =
      parent_element ? parent_element->AsHTMLElement() : NULL;
  if (parent_html_element) {
    parent_html_element->ClearRuleMatchingStateOnElementAndAncestors(
        invalidate_matching_rules);
  }
  ClearRuleMatchingStateInternal(invalidate_matching_rules &&
                                 parent_html_element == NULL);
}

void HTMLElement::ClearRuleMatchingStateOnElementAndDescendants() {
  ClearRuleMatchingStateInternal(false /* invalidate_descendants*/);
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->ClearRuleMatchingStateOnElementAndDescendants();
    }
  }
}

void HTMLElement::ClearRuleMatchingStateOnElementAndSiblingsAndDescendants() {
  HTMLElement::ClearRuleMatchingStateOnElementAndDescendants();
  for (Element* element = previous_element_sibling(); element;
       element = element->previous_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->ClearRuleMatchingStateOnElementAndDescendants();
    }
  }
  for (Element* element = next_element_sibling(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->ClearRuleMatchingStateOnElementAndDescendants();
    }
  }
}

void HTMLElement::InvalidateMatchingRulesRecursively() {
  InvalidateMatchingRulesRecursivelyInternal(true /*is_initial_element*/);
}

void HTMLElement::InvalidateMatchingRulesRecursivelyInternal(
    bool is_initial_element) {
  matching_rules_valid_ = false;

  // Invalidate matching rules on all children.
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->InvalidateMatchingRulesRecursivelyInternal(
          false /*is_initial_element*/);
    }
  }

  // Invalidate matching rules on all following siblings if this is the initial
  // element and sibling combinators are used; if this is not the initial
  // element, then these will already be handled by a previous call.
  if (is_initial_element &&
      node_document()->selector_tree()->has_sibling_combinators()) {
    for (Element* element = next_element_sibling(); element;
         element = element->next_element_sibling()) {
      HTMLElement* html_element = element->AsHTMLElement();
      if (html_element) {
        html_element->InvalidateMatchingRulesRecursivelyInternal(
            false /*is_initial_element*/);
      }
    }
  }
}

void HTMLElement::UpdateMatchingRules() { UpdateElementMatchingRules(this); }

void HTMLElement::UpdateMatchingRulesRecursively() {
  UpdateMatchingRules();
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->UpdateMatchingRulesRecursively();
    }
  }
}

void HTMLElement::OnMatchingRulesModified() {
  dom_stat_tracker_->OnUpdateMatchingRules();
  computed_style_valid_ = false;
}

void HTMLElement::OnPseudoElementMatchingRulesModified() {
  pseudo_elements_computed_styles_valid_ = false;
}

void HTMLElement::UpdateComputedStyleRecursively(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const base::TimeDelta& style_change_event_time, bool ancestors_were_valid,
    int current_element_depth) {
  int max_depth = node_document()->dom_max_element_depth();
  if (max_depth > 0 && current_element_depth >= max_depth) {
    return;
  }

  // Update computed styles for this element if any aren't valid.
  bool is_valid = ancestors_were_valid && AreComputedStylesValid();
  if (!is_valid) {
    UpdateComputedStyle(parent_computed_style_declaration, root_computed_style,
                        style_change_event_time, kAncestorsAreDisplayed);
    UpdateUiNavigation();
    node_document()->set_ui_nav_needs_layout(true);
  } else if (ui_nav_needs_update_) {
    UpdateUiNavigation();
  }

  // Do not update computed style for descendants of "display: none" elements,
  // since they do not participate in layout. Note the "display: none" elements
  // themselves still need to have their computed style updated, in case the
  // value of display is changed.
  if (computed_style()->display() == cssom::KeywordValue::GetNone()) {
    return;
  }

  // Update computed style for this element's descendants. Note that if
  // descendant_computed_styles_valid_ flag is not set, the ancestors should
  // still be considered invalid, which forces the computes styles to be updated
  // on all children.
  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      html_element->UpdateComputedStyleRecursively(
          css_computed_style_declaration(), root_computed_style,
          style_change_event_time,
          is_valid && descendant_computed_styles_valid_,
          current_element_depth + 1);
    }
  }

  descendant_computed_styles_valid_ = true;
}

void HTMLElement::MarkNotDisplayedOnNodeAndDescendants() {
  // While we do want to clear the animations immediately, we also want to
  // ensure that they are also reset starting with the next computed style
  // update.  This ensures that for example a transition will not be triggered
  // on the next computed style update.
  ancestors_are_displayed_ = kAncestorsAreNotDisplayed;

  PurgeCachedBackgroundImages();

  if (!css_animations_.empty() || !css_transitions_.empty()) {
    css_transitions_.Clear();
    css_animations_.Clear();
    computed_style_valid_ = false;
    descendant_computed_styles_valid_ = false;
  }

  // Also clear out all animations/transitions on pseudo elements.
  for (auto& pseudo_element : pseudo_elements_) {
    if (pseudo_element) {
      pseudo_element->css_transitions()->Clear();
      pseudo_element->css_animations()->Clear();
    }
  }

  ReleaseUiNavigationItem();
  MarkNotDisplayedOnDescendants();
}

void HTMLElement::PurgeCachedBackgroundImagesOfNodeAndDescendants() {
  PurgeCachedBackgroundImages();
  PurgeCachedBackgroundImagesOfDescendants();
}

void HTMLElement::PurgeCachedBackgroundImages() {
  ClearActiveBackgroundImages();
  if (!cached_background_images_.empty()) {
    cached_background_images_.clear();
    computed_style_valid_ = false;
    descendant_computed_styles_valid_ = false;
  }
}

bool HTMLElement::IsDisplayed() const {
  return ancestors_are_displayed_ == kAncestorsAreDisplayed &&
         computed_style()->display() != cssom::KeywordValue::GetNone();
}

bool HTMLElement::IsRootElement() {
  // The html element represents the root of an HTML document.
  //   https://www.w3.org/TR/2014/REC-html5-20141028/semantics.html#the-root-element
  return AsHTMLHtmlElement().get() != NULL;
}

void HTMLElement::InvalidateComputedStylesOfNodeAndDescendants() {
  computed_style_valid_ = false;
  descendant_computed_styles_valid_ = false;
  InvalidateComputedStylesOfDescendants();
}

void HTMLElement::InvalidateLayoutBoxesOfNodeAndAncestors() {
  InvalidateLayoutBoxes();
  InvalidateLayoutBoxesOfAncestors();
}

void HTMLElement::InvalidateLayoutBoxesOfNodeAndDescendants() {
  InvalidateLayoutBoxes();
  InvalidateLayoutBoxesOfDescendants();
}

void HTMLElement::InvalidateLayoutBoxSizes() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateSizes();

    for (auto& pseudo_element : pseudo_elements_) {
      if (pseudo_element && pseudo_element->layout_boxes()) {
        pseudo_element->layout_boxes()->InvalidateSizes();
      }
    }
  }
}

void HTMLElement::InvalidateLayoutBoxCrossReferences() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateCrossReferences();

    for (auto& pseudo_element : pseudo_elements_) {
      if (pseudo_element && pseudo_element->layout_boxes()) {
        pseudo_element->layout_boxes()->InvalidateCrossReferences();
      }
    }
  }
}

void HTMLElement::InvalidateLayoutBoxRenderTreeNodes() {
  if (layout_boxes_) {
    layout_boxes_->InvalidateRenderTreeNodes();

    for (auto& pseudo_element : pseudo_elements_) {
      if (pseudo_element && pseudo_element->layout_boxes()) {
        pseudo_element->layout_boxes()->InvalidateRenderTreeNodes();
      }
    }
  }
}

void HTMLElement::InvalidateLayoutBoxes() {
  layout_boxes_.reset();
  for (auto& pseudo_element : pseudo_elements_) {
    if (pseudo_element) {
      pseudo_element->reset_layout_boxes();
    }
  }
  directionality_ = base::nullopt;
}

void HTMLElement::OnUiNavBlur(SbTimeMonotonic time) {
  if (node_document() && node_document()->ui_nav_focus_element() == this) {
    if (node_document()->TrySetUiNavFocusElement(nullptr, time)) {
      Blur();
    }
  }
}

void HTMLElement::OnUiNavFocus(SbTimeMonotonic time) {
  // Suppress the focus event if this is already focused -- i.e. the HTMLElement
  // initiated the focus change that resulted in this call to OnUiNavFocus.
  if (node_document() && node_document()->ui_nav_focus_element() != this) {
    if (node_document()->TrySetUiNavFocusElement(this, time)) {
      Focus();
    }
  }
}

void HTMLElement::OnUiNavScroll(SbTimeMonotonic /* time */) {
  Document* document = node_document();
  scoped_refptr<Window> window(document ? document->window() : nullptr);
  DispatchEvent(new UIEvent(base::Tokens::scroll(), Event::kBubbles,
                            Event::kNotCancelable, window));
}

HTMLElement::HTMLElement(Document* document, base::Token local_name)
    : Element(document, local_name),
      dom_stat_tracker_(document->html_element_context()->dom_stat_tracker()),
      locked_for_focus_(false),
      dir_(kDirNotDefined),
      style_(new cssom::CSSDeclaredStyleDeclaration(
          document->html_element_context()->css_parser())),
      computed_style_valid_(false),
      pseudo_elements_computed_styles_valid_(false),
      descendant_computed_styles_valid_(false),
      ancestors_are_displayed_(kAncestorsAreDisplayed),
      css_computed_style_declaration_(new cssom::CSSComputedStyleDeclaration()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          transitions_adapter_(new DOMAnimatable(this))),
      css_transitions_(&transitions_adapter_),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          animations_adapter_(new DOMAnimatable(this))),
      css_animations_(&animations_adapter_),
      matching_rules_valid_(false) {
  css_computed_style_declaration_->set_animations(animations());
  style_->set_mutation_observer(this);
  dom_stat_tracker_->OnHtmlElementCreated();
}

HTMLElement::~HTMLElement() {
  ReleaseUiNavigationItem();

  if (IsInDocument()) {
    dom_stat_tracker_->OnHtmlElementRemovedFromDocument();
  }
  dom_stat_tracker_->OnHtmlElementDestroyed();

  style_->set_mutation_observer(NULL);
}

void HTMLElement::OnInsertedIntoDocument() {
  Node::OnInsertedIntoDocument();
  dom_stat_tracker_->OnHtmlElementInsertedIntoDocument();
}

void HTMLElement::OnRemovedFromDocument() {
  Node::OnRemovedFromDocument();
  dom_stat_tracker_->OnHtmlElementRemovedFromDocument();

  // When an element that is focused stops being a focusable element, or stops
  // being focused without another element being explicitly focused in its
  // stead, the user agent should synchronously run the unfocusing steps for the
  // affected element only.
  // For example, this might happen because the element is removed from its
  // Document, or has a hidden attribute added. It would also happen to an input
  // element when the element gets disabled.
  //   https://www.w3.org/TR/html50/editing.html#unfocusing-steps
  Document* document = node_document();
  DCHECK(document);
  if (document->active_element() == this->AsElement()) {
    RunUnFocusingSteps();
    document->OnFocusChange();
  }

  // Only clear the rule matching on this element. Descendants will be handled
  // by OnRemovedFromDocument() being called on them from
  // Node::OnRemovedFromDocument().
  ClearRuleMatchingStateInternal(false /*invalidate_descendants*/);

  ReleaseUiNavigationItem();
}

void HTMLElement::OnMutation() { InvalidateMatchingRulesRecursively(); }

void HTMLElement::OnSetAttribute(const std::string& name,
                                 const std::string& value) {
  // Be sure to update HTMLElement::Duplicate() to copy over values as needed.
  if (name == "dir") {
    SetDir(value);
  } else if (name == "tabindex") {
    SetTabIndex(value);
  } else if (name == kUiNavFocusDurationAttribute) {
    SetUiNavFocusDuration(value);
  }

  // Always clear the matching state when an attribute changes. Any attribute
  // changing can potentially impact the matching rules.
  ClearRuleMatchingState();
}

void HTMLElement::OnRemoveAttribute(const std::string& name) {
  if (name == "dir") {
    SetDir("");
  } else if (name == "tabindex") {
    SetTabIndex("");
  } else if (name == kUiNavFocusDurationAttribute) {
    SetUiNavFocusDuration("");
  }

  // Always clear the matching state when an attribute changes. Any attribute
  // changing can potentially impact the matching rules.
  ClearRuleMatchingState();
}

// Algorithm for IsFocusable:
//   https://www.w3.org/TR/html50/editing.html#focusable
bool HTMLElement::IsFocusable() {
  return HasTabindexFocusFlag() && IsBeingRendered();
}

// Algorithm for HasTabindexFocusFlag:
//  https://www.w3.org/TR/html50/editing.html#specially-focusable
bool HTMLElement::HasTabindexFocusFlag() const { return tabindex_.has_value(); }

// An element is being rendered if it has any associated CSS layout boxes, SVG
// layout boxes, or some equivalent in other styling languages.
//   https://www.w3.org/TR/html50/rendering.html#being-rendered
bool HTMLElement::IsBeingRendered() {
  Document* document = node_document();
  if (!document) {
    return false;
  }

  if (!document->UpdateComputedStyleOnElementAndAncestor(this)) {
    return false;
  }
  DCHECK(computed_style());

  return IsDisplayed() &&
         computed_style()->visibility() == cssom::KeywordValue::GetVisible();
}

// Algorithm for RunFocusingSteps:
//   https://www.w3.org/TR/html50/editing.html#focusing-steps
void HTMLElement::RunFocusingSteps() {
  // 1. If the element is not in a Document, or if the element's Document has
  // no browsing context, or if the element's Document's browsing context has no
  // top-level browsing context, or if the element is not focusable, or if the
  // element is already focused, then abort these steps.
  Document* document = node_document();
  if (!document || !document->HasBrowsingContext() || !IsFocusable()) {
    return;
  }
  Element* old_active_element = document->active_element();
  if (old_active_element == this) {
    return;
  }

  // 2. If focusing the element will remove the focus from another element,
  // then run the unfocusing steps for that element.
  if (old_active_element && old_active_element->AsHTMLElement()) {
    old_active_element->AsHTMLElement()->RunUnFocusingSteps();
  }

  // focusin: A user agent MUST dispatch this event when an event target is
  // about to receive focus. This event type MUST be dispatched before the
  // element is given focus. The event target MUST be the element which is about
  // to receive focus. This event type is similar to focus, but is dispatched
  // before focus is shifted, and does bubble.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-focusin
  DispatchEvent(new FocusEvent(base::Tokens::focusin(), Event::kBubbles,
                               Event::kNotCancelable, document->window(),
                               this));

  // 3. Make the element the currently focused element in its top-level browsing
  // context.
  document->SetActiveElement(this);

  // 4. Not needed by Cobalt.

  // 5. Fire a simple event named focus at the element.
  // focus: A user agent MUST dispatch this event when an event target receives
  // focus. The focus MUST be given to the element before the dispatch of this
  // event type. This event type is similar to focusin, but is dispatched after
  // focus is shifted, and does not bubble.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-focus
  DispatchEvent(new FocusEvent(base::Tokens::focus(), Event::kNotBubbles,
                               Event::kNotCancelable, document->window(),
                               this));

  // Custom, not in any spec.
  ClearRuleMatchingState();
}

// Algorithm for RunUnFocusingSteps:
//   https://www.w3.org/TR/html50/editing.html#unfocusing-steps
void HTMLElement::RunUnFocusingSteps() {
  // 1. Not needed by Cobalt.

  // focusout: A user agent MUST dispatch this event when an event target is
  // about to lose focus. This event type MUST be dispatched before the element
  // loses focus. The event target MUST be the element which is about to lose
  // focus. This event type is similar to blur, but is dispatched before focus
  // is shifted, and does bubble.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-focusout
  Document* document = node_document();
  scoped_refptr<Window> window(document ? document->window() : NULL);
  DispatchEvent(new FocusEvent(base::Tokens::focusout(), Event::kBubbles,
                               Event::kNotCancelable, window, this));

  // 2. Unfocus the element.
  if (document && document->active_element() == this->AsElement()) {
    document->SetActiveElement(NULL);
  }

  // 3. Fire a simple event named blur at the element.
  // blur: A user agent MUST dispatch this event when an event target loses
  // focus. The focus MUST be taken from the element before the dispatch of this
  // event type. This event type is similar to focusout, but is dispatched after
  // focus is shifted, and does not bubble.
  //   https://www.w3.org/TR/2016/WD-uievents-20160804/#event-type-blur
  DispatchEvent(new FocusEvent(base::Tokens::blur(), Event::kNotBubbles,
                               Event::kNotCancelable, document->window(),
                               this));

  // Custom, not in any spec.
  ClearRuleMatchingState();
}

void HTMLElement::UpdateUiNavigationFocus() {
  if (!node_document()) {
    return;
  }

  // Set the focus item for the UI navigation system. Search up the DOM tree to
  // find the nearest ancestor that is a UI navigation item if needed. Do this
  // step before dispatching events as the event handlers may make UI navigation
  // changes.
  for (Node* node = this; node; node = node->parent_node()) {
    Element* element = node->AsElement();
    if (!element) {
      continue;
    }
    HTMLElement* html_element = element->AsHTMLElement();
    if (!html_element) {
      continue;
    }
    if (!html_element->ui_nav_item_ ||
        html_element->ui_nav_item_->IsContainer()) {
      continue;
    }

    // Updating the UI navigation focus element has the additional effect of
    // suppressing the Blur call for the previously focused HTMLElement and the
    // Focus call for this HTMLElement as a result of OnUiNavBlur / OnUiNavFocus
    // callbacks that result from initiating the UI navigation focus change.
    if (node_document()->TrySetUiNavFocusElement(html_element,
                                                 SbTimeGetMonotonicNow())) {
      html_element->ui_nav_item_->Focus();
    }
    break;
  }
}

void HTMLElement::SetDir(const std::string& value) {
  // https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#the-dir-attribute
  auto previous_dir = dir_;
  if (value == "auto") {
    dir_ = kDirAuto;
  } else if (value == "ltr") {
    dir_ = kDirLeftToRight;
  } else if (value == "rtl") {
    dir_ = kDirRightToLeft;
  } else {
    dir_ = kDirNotDefined;

    // Reset the attribute so that element.getAttribute('dir') returns the
    // same thing as element.dir.
    if (value.size() > 0) {
      LOG(WARNING) << "Unsupported value '" << value << "' for attribute 'dir'";
      SetAttribute("dir", "");
    }
  }

  if (dir_ != previous_dir) {
    InvalidateLayoutBoxesOfNodeAndAncestors();
    InvalidateLayoutBoxesOfDescendants();
  }
}

void HTMLElement::SetTabIndex(const std::string& value) {
  int32 tabindex;
  if (base::StringToInt32(value, &tabindex)) {
    tabindex_ = tabindex;
  } else {
    tabindex_ = base::nullopt;
  }

  // Changing the tabindex may trigger a UI navigation change.
  ui_nav_needs_update_ = true;
}

void HTMLElement::SetUiNavFocusDuration(const std::string& value) {
  double duration;
  if (base::StringToDouble(value, &duration)) {
    ui_nav_focus_duration_ = static_cast<float>(duration);
    if (!std::isfinite(*ui_nav_focus_duration_) ||
        *ui_nav_focus_duration_ < 0.0f) {
      ui_nav_focus_duration_ = 0.0f;
    }
    if (ui_nav_item_) {
      ui_nav_item_->SetFocusDuration(*ui_nav_focus_duration_);
    }
  } else {
    ui_nav_focus_duration_ = base::nullopt;
    if (ui_nav_item_) {
      ui_nav_item_->SetFocusDuration(0.0f);
    }
  }
}

namespace {
// This is similar to base rtl.h's GetStringDirection; however, this takes a
// utf8 string and only pays attention to L, AL, and R character types.
HTMLElement::DirState GetStringDirection(const std::string& utf8_string) {
  int32_t length = static_cast<int32_t>(utf8_string.length());
  for (int32_t index = 0; index < length;) {
    int32_t ch;
    U8_NEXT(utf8_string.data(), index, length, ch);
    if (ch < 0) {
      LOG(ERROR) << "Unable to determine directionality of " << utf8_string;
      break;
    }

    int32_t property = u_getIntPropertyValue(ch, UCHAR_BIDI_CLASS);
    if (property == U_LEFT_TO_RIGHT) {
      return HTMLElement::kDirLeftToRight;
    }
    if (property == U_RIGHT_TO_LEFT || property == U_RIGHT_TO_LEFT_ARABIC) {
      return HTMLElement::kDirRightToLeft;
    }
  }
  return HTMLElement::kDirNotDefined;
}
}  // namespace

// This is similar to dir_state() except it will resolve kDirAuto to
// kDirLeftToRight or kDirRightToLeft according to the spec:
//   https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#the-directionality
// If "dir" was not defined for this element, then this function will return
// kDirNotDefined.
HTMLElement::DirState HTMLElement::GetUsedDirState() {
  // If the element's dir attribute is in the auto state
  // If the element is a bdi element and the dir attribute is not in a defined
  //   state (i.e. it is not present or has an invalid value)
  if (dir_ != kDirAuto) {
    return dir_;
  }

  // Find the first character in tree order that matches the following criteria:
  //   The character is from a Text node that is a descendant of the element
  //     whose directionality is being determined.
  //   The character is of bidirectional character type L, AL, or R. [BIDI]
  //   The character is not in a Text node that has an ancestor element that is
  //     a descendant of the element whose directionality is being determined
  //     and that is either:
  //       A bdi element.
  //       A script element.
  //       A style element.
  //       A textarea element.
  //       An element with a dir attribute in a defined state.
  //   If such a character is found and it is of bidirectional character type
  //     AL or R, the directionality of the element is 'rtl'.
  //   If such a character is found and it is of bidirectional character type
  //     L, the directionality of the element is 'ltr'.

  // A tree is a finite hierarchical tree structure. In tree order is preorder,
  // depth-first traversal of a tree.
  //   https://dom.spec.whatwg.org/#concept-tree-order
  std::vector<Node*> stack;

  // Add children in reverse order so that pop_back() will result in preorder
  // depth-first traversal.
  for (Node* child_node = last_child(); child_node;
       child_node = child_node->previous_sibling()) {
    stack.push_back(child_node);
  }

  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();

    Text* text = node->AsText();
    if (text) {
      // If the text has strong directionality, then return it.
      DirState dir = GetStringDirection(text->text());
      if (dir != kDirNotDefined) {
        return dir;
      }
    }

    // Traverse children only if this is not:
    //   A bdi element.
    //   A script element.
    //   A style element.
    //   A textarea element.
    //   An element with a dir attribute in a defined state.
    Element* element = node->AsElement();
    if (element) {
      HTMLElement* html_element = element->AsHTMLElement();
      if (html_element) {
        if (html_element->AsHTMLScriptElement() ||
            html_element->AsHTMLStyleElement() ||
            html_element->dir_state() != kDirNotDefined) {
          continue;
        }
      }
    }

    for (Node* child_node = node->last_child(); child_node;
         child_node = child_node->previous_sibling()) {
      stack.push_back(child_node);
    }
  }

  // Otherwise, if the element is a document element, the directionality of
  //   the element is 'ltr'.
  if (IsDocumentElement()) {
    return kDirLeftToRight;
  }

// Although the spec says to use the parent's directionality, the W3C test
// (the-dir-attribute-069.html) says to default to LTR. Chrome follows the
// W3C expectation, so follow Chrome. Additional discussion here:
//   https://github.com/w3c/i18n-drafts/issues/235
// The following code block which implements the spec is left for reference.
#if 0
  // Otherwise, the directionality of the element is the same as the element's
  //   parent element's directionality.
  for (Node* ancestor_node = parent_node(); ancestor_node;
       ancestor_node = ancestor_node->parent_node()) {
    Element* ancestor_element = ancestor_node->AsElement();
    if (!ancestor_element) {
      continue;
    }
    HTMLElement* ancestor_html_element = ancestor_element->AsHTMLElement();
    if (!ancestor_html_element) {
      continue;
    }
    if (ancestor_html_element->dir_state() == kDirNotDefined) {
      continue;
    }
    return ancestor_html_element->GetUsedDirState();
  }
#endif

  return kDirLeftToRight;
}

// Algorithm:
//   https://html.spec.whatwg.org/commit-snapshots/ebcac971c2add28a911283899da84ec509876c44/#the-directionality
Directionality HTMLElement::directionality() {
  // Use the cached value if available.
  if (directionality_) {
    return *directionality_;
  }

  // The directionality of an element (any element, not just an HTML element)
  // is either 'ltr' or 'rtl', and is determined as per the first appropriate
  // set of steps from the following list:

  // If the element's dir attribute is in the ltr state
  // If the element is a document element and the dir attribute is not in a
  //   defined state (i.e. it is not present or has an invalid value)
  // If the element is an input element whose type attribute is in the
  //   Telephone state, and the dir attribute is not in a defined state (i.e.
  //   it is not present or has an invalid value)
  // --> The directionality of the element is 'ltr'.
  if (dir_ == kDirLeftToRight) {
    directionality_ = kLeftToRightDirectionality;
    return *directionality_;
  }
  // [Case of undefined 'dir' is handled later in this function.]

  // If the element's dir attribute is in the rtl state
  // --> The directionality of the element is 'rtl'.
  if (dir_ == kDirRightToLeft) {
    directionality_ = kRightToLeftDirectionality;
    return *directionality_;
  }

  // If the element is an input element whose type attribute is in the Text,
  //   Search, Telephone, URL, or E-mail state, and the dir attribute is in the
  //   auto state
  // If the element is a textarea element and the dir attribute is in the auto
  //   state
  // --> Cobalt does not support these element types.

  // If the element's dir attribute is in the auto state
  // If the element is a bdi element and the dir attribute is not in a defined
  //   state (i.e. it is not present or has an invalid value)
  // --> Find the first character in tree order that matches the following
  //       criteria:
  //       The character is from a Text node that is a descendant of the
  //         element whose directionality is being determined.
  //       The character is of bidirectional character type L, AL, or R. [BIDI]
  //       The character is not in a Text node that has an ancestor element
  //         that is a descendant of the element whose directionality is being
  //         determined and that is either:
  //           A bdi element.
  //           A script element.
  //           A style element.
  //           A textarea element.
  //           An element with a dir attribute in a defined state.
  //     If such a character is found and it is of bidirectional character type
  //       AL or R, the directionality of the element is 'rtl'.
  //     If such a character is found and it is of bidirectional character type
  //       L, the directionality of the element is 'ltr'.
  //     Otherwise, if the element is a document element, the directionality of
  //       the element is 'ltr'.
  //     Otherwise, the directionality of the element is the same as the
  //       element's parent element's directionality.

  // If the element has a parent element and the dir attribute is not in a
  //   defined state (i.e. it is not present or has an invalid value)
  // --> The directionality of the element is the same as the element's parent
  //       element's directionality.
  for (Node* ancestor_node = parent_node(); ancestor_node;
       ancestor_node = ancestor_node->parent_node()) {
    Element* ancestor_element = ancestor_node->AsElement();
    if (!ancestor_element) {
      continue;
    }
    HTMLElement* ancestor_html_element = ancestor_element->AsHTMLElement();
    if (!ancestor_html_element) {
      continue;
    }
    directionality_ = ancestor_html_element->directionality();
    return *directionality_;
  }

  directionality_ = kLeftToRightDirectionality;
  return *directionality_;
}

namespace {

scoped_refptr<cssom::CSSComputedStyleData> PromoteMatchingRulesToComputedStyle(
    cssom::RulesWithCascadePrecedence* matching_rules,
    cssom::GURLMap* property_key_to_base_url_map,
    const scoped_refptr<const cssom::CSSDeclaredStyleData>& inline_style,
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const ViewportSize& viewport_size) {
  // Select the winning value for each property by performing the cascade,
  // that is, apply values from matching rules on top of inline style, taking
  // into account rule specificity and location in the source file, as well as
  // property declaration importance.
  scoped_refptr<cssom::MutableCSSComputedStyleData> computed_style =
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
      viewport_size.width_height(), property_key_to_base_url_map);

  return computed_style;
}

void PossiblyActivateAnimations(
    const scoped_refptr<const cssom::CSSComputedStyleData>&
        previous_computed_style,
    const scoped_refptr<const cssom::CSSComputedStyleData>& new_computed_style,
    const base::TimeDelta& style_change_event_time,
    cssom::TransitionSet* css_transitions, cssom::AnimationSet* css_animations,
    const cssom::CSSKeyframesRule::NameMap& keyframes_map,
    HTMLElement::AncestorsAreDisplayed old_ancestors_are_displayed,
    HTMLElement::AncestorsAreDisplayed new_ancestors_are_displayed,
    bool* animations_modified) {
  // Calculate some helper values to help determine if we are transitioning from
  // not being displayed to being displayed, or vice-versa.  Animations should
  // not be playing if we are not being displayed.
  bool old_is_displayed =
      old_ancestors_are_displayed == HTMLElement::kAncestorsAreDisplayed &&
      (previous_computed_style &&
       previous_computed_style->display() != cssom::KeywordValue::GetNone());
  bool new_is_displayed =
      new_ancestors_are_displayed == HTMLElement::kAncestorsAreDisplayed &&
      new_computed_style->display() != cssom::KeywordValue::GetNone();

  if (new_is_displayed) {
    // Don't start any transitions if we are transitioning from display: none.
    if (previous_computed_style && old_is_displayed) {
      // Now that we have updated our computed style, compare it to the previous
      // style and see if we need to adjust our animations.
      css_transitions->UpdateTransitions(style_change_event_time,
                                         *previous_computed_style,
                                         *new_computed_style);
    }
    // Update the set of currently running animations and track whether or not
    // the animations changed.
    *animations_modified = css_animations->Update(
        style_change_event_time, *new_computed_style, keyframes_map);
  } else {
    if (old_is_displayed) {
      css_transitions->Clear();
      css_animations->Clear();
    } else {
      DCHECK(css_transitions->empty());
      DCHECK(css_animations->empty());
    }
  }
}

// Flags tracking which cached values must be invalidated.
struct UpdateComputedStyleInvalidationFlags {
  UpdateComputedStyleInvalidationFlags()
      : mark_descendants_as_display_none(false),
        invalidate_computed_styles_of_descendants(false),
        invalidate_layout_boxes(false),
        invalidate_sizes(false),
        invalidate_cross_references(false),
        invalidate_render_tree_nodes(false) {}

  bool mark_descendants_as_display_none;
  bool invalidate_computed_styles_of_descendants;
  bool invalidate_layout_boxes;
  bool invalidate_sizes;
  bool invalidate_cross_references;
  bool invalidate_render_tree_nodes;
};

bool NewComputedStyleMarksDescendantsAsDisplayNone(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style) {
  return old_computed_style->display() != cssom::KeywordValue::GetNone() &&
         new_computed_style->display() == cssom::KeywordValue::GetNone();
}

bool NewComputedStyleInvalidatesComputedStylesOfDescendants(
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

enum IsPseudoElement {
  kIsNotPseudoElement,
  kIsPseudoElement,
};

void UpdateInvalidationFlagsFromNewComputedStyle(
    const scoped_refptr<const cssom::CSSComputedStyleData>& old_computed_style,
    const scoped_refptr<cssom::CSSComputedStyleData>& new_computed_style,
    bool animations_modified, IsPseudoElement is_pseudo_element,
    UpdateComputedStyleInvalidationFlags* flags) {
  if (old_computed_style) {
    if (!flags->mark_descendants_as_display_none &&
        is_pseudo_element == kIsNotPseudoElement &&
        NewComputedStyleMarksDescendantsAsDisplayNone(old_computed_style,
                                                      new_computed_style)) {
      flags->mark_descendants_as_display_none = true;
    }
    if (!flags->invalidate_computed_styles_of_descendants &&
        NewComputedStyleInvalidatesComputedStylesOfDescendants(
            old_computed_style, new_computed_style)) {
      flags->invalidate_computed_styles_of_descendants = true;
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

void DoComputedStyleUpdate(
    cssom::RulesWithCascadePrecedence* matching_rules,
    cssom::GURLMap* property_key_to_base_url_map,
    const scoped_refptr<const cssom::CSSDeclaredStyleData>& inline_style,
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const ViewportSize& viewport_size,
    const scoped_refptr<const cssom::CSSComputedStyleData>&
        previous_computed_style,
    const base::TimeDelta& style_change_event_time,
    cssom::TransitionSet* css_transitions, cssom::AnimationSet* css_animations,
    const cssom::CSSKeyframesRule::NameMap& keyframes_map,
    HTMLElement::AncestorsAreDisplayed old_ancestors_are_displayed,
    HTMLElement::AncestorsAreDisplayed new_ancestors_are_displayed,
    IsPseudoElement is_pseudo_element,
    UpdateComputedStyleInvalidationFlags* invalidation_flags,
    cssom::CSSComputedStyleDeclaration* css_computed_style_declaration) {
  bool animations_modified = false;

  scoped_refptr<cssom::CSSComputedStyleData> new_computed_style =
      PromoteMatchingRulesToComputedStyle(
          matching_rules, property_key_to_base_url_map, inline_style,
          parent_computed_style_declaration, root_computed_style,
          viewport_size);

  PossiblyActivateAnimations(previous_computed_style, new_computed_style,
                             style_change_event_time, css_transitions,
                             css_animations, keyframes_map,
                             old_ancestors_are_displayed,
                             new_ancestors_are_displayed, &animations_modified);

  UpdateInvalidationFlagsFromNewComputedStyle(
      previous_computed_style, new_computed_style, animations_modified,
      is_pseudo_element, invalidation_flags);

  css_computed_style_declaration->SetData(new_computed_style);
}
}  // namespace

void HTMLElement::UpdateComputedStyle(
    const scoped_refptr<cssom::CSSComputedStyleDeclaration>&
        parent_computed_style_declaration,
    const scoped_refptr<const cssom::CSSComputedStyleData>& root_computed_style,
    const base::TimeDelta& style_change_event_time,
    AncestorsAreDisplayed ancestors_are_displayed) {
  Document* document = node_document();
  DCHECK(document) << "Element should be attached to document in order to "
                      "participate in layout.";

  // Verify that the matching rules for this element are valid. They should have
  // been updated prior to UpdateComputedStyle() being called.
  DCHECK(matching_rules_valid_);
  // If there is no previous computed style, there should also be no layout
  // boxes.
  DCHECK(computed_style() || NULL == layout_boxes());

  dom_stat_tracker_->OnUpdateComputedStyle();

  // The computed style must be generated if either the computed style is
  // invalid or no computed style has been created yet.
  bool generate_computed_style = !computed_style_valid_ || !computed_style();

  // If any declared properties inherited from the parent are no longer valid,
  // then a new computed style must be generated with the updated inherited
  // values.
  if (!generate_computed_style &&
      !computed_style()->AreDeclaredPropertiesInheritedFromParentValid()) {
    generate_computed_style = true;
  }

  // It is possible for computed style to have been updated on this element even
  // if its ancestors were set to display: none.  If this has changed, we would
  // need to update our computed style again, even if nothing else has changed.
  if (!generate_computed_style &&
      ancestors_are_displayed_ == kAncestorsAreNotDisplayed &&
      ancestors_are_displayed == kAncestorsAreDisplayed) {
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

  // We record this now before we make changes to the computed style and use
  // it later for the pseudo element computed style updates.
  bool old_is_displayed = computed_style() && IsDisplayed();

  if (generate_computed_style) {
    dom_stat_tracker_->OnGenerateHtmlElementComputedStyle();
    DoComputedStyleUpdate(
        matching_rules(), &property_key_to_base_url_map, style_->data(),
        parent_computed_style_declaration, root_computed_style,
        document->viewport_size(), computed_style(), style_change_event_time,
        &css_transitions_, &css_animations_, document->keyframes_map(),
        ancestors_are_displayed_, ancestors_are_displayed, kIsNotPseudoElement,
        &invalidation_flags, css_computed_style_declaration_);

    // Update cached background images after resolving the urls in
    // background_image CSS property of the computed style, so we have all the
    // information to get the cached background images.
    UpdateCachedBackgroundImagesFromComputedStyle();
  } else {
    // Update the inherited data if a new style was not generated. The ancestor
    // data with inherited properties may have changed.
    css_computed_style_declaration_->UpdateInheritedData();
  }

  // Update the displayed status of our ancestors.
  ancestors_are_displayed_ = ancestors_are_displayed;

  // Process pseudo elements. They must have their computed style generated if
  // either their owning HTML element's style was just generated or their
  // computed style is invalid (this occurs when their matching rules change).
  for (int type = 0; type < kMaxPseudoElementType; ++type) {
    PseudoElement* type_pseudo_element =
        pseudo_element(PseudoElementType(type));
    if (type_pseudo_element) {
      if (generate_computed_style ||
          type_pseudo_element->computed_style_invalid()) {
        dom_stat_tracker_->OnGeneratePseudoElementComputedStyle();
        DoComputedStyleUpdate(
            type_pseudo_element->matching_rules(),
            &property_key_to_base_url_map, NULL,
            css_computed_style_declaration(), root_computed_style,
            document->viewport_size(), type_pseudo_element->computed_style(),
            style_change_event_time, type_pseudo_element->css_transitions(),
            type_pseudo_element->css_animations(), document->keyframes_map(),
            old_is_displayed ? kAncestorsAreDisplayed
                             : kAncestorsAreNotDisplayed,
            IsDisplayed() ? kAncestorsAreDisplayed : kAncestorsAreNotDisplayed,
            kIsPseudoElement, &invalidation_flags,
            type_pseudo_element->css_computed_style_declaration());
        type_pseudo_element->clear_computed_style_invalid();
      } else {
        // Update the inherited data if a new style was not generated. The
        // ancestor data with inherited properties may have changed.
        type_pseudo_element->css_computed_style_declaration()
            ->UpdateInheritedData();
      }
    }
  }

  if (invalidation_flags.mark_descendants_as_display_none) {
    MarkNotDisplayedOnDescendants();
  }
  if (invalidation_flags.invalidate_computed_styles_of_descendants) {
    InvalidateComputedStylesOfDescendants();
  }

  if (invalidation_flags.invalidate_layout_boxes) {
    InvalidateLayoutBoxesOfNodeAndAncestors();
    InvalidateLayoutBoxesOfDescendants();
  } else {
    if (invalidation_flags.invalidate_sizes) {
      InvalidateLayoutBoxSizes();
    }
    if (invalidation_flags.invalidate_cross_references) {
      InvalidateLayoutBoxCrossReferences();
    }
    if (invalidation_flags.invalidate_render_tree_nodes) {
      InvalidateLayoutBoxRenderTreeNodes();
    }
  }

  computed_style_valid_ = true;
  pseudo_elements_computed_styles_valid_ = true;
}

void HTMLElement::CollectHTMLMediaElementsRecursively(
    std::vector<HTMLMediaElement*>* html_media_elements,
    int current_element_depth) {
  int max_depth = node_document()->dom_max_element_depth();
  if (max_depth > 0 && current_element_depth >= max_depth) {
    return;
  }

  for (Element* element = first_element_child(); element;
       element = element->next_element_sibling()) {
    HTMLElement* html_element = element->AsHTMLElement();
    if (html_element) {
      HTMLMediaElement* media_html_element = html_element->AsHTMLMediaElement();
      if (media_html_element) {
        html_media_elements->push_back(media_html_element);
      }
      html_element->CollectHTMLMediaElementsRecursively(
          html_media_elements, current_element_depth + 1);
    }
  }
}

void HTMLElement::SetPseudoElement(
    PseudoElementType type, std::unique_ptr<PseudoElement> pseudo_element) {
  DCHECK_EQ(this, pseudo_element->parent_element());
  DCHECK(type < kMaxPseudoElementType);
  pseudo_elements_[type] = std::move(pseudo_element);
  pseudo_elements_computed_styles_valid_ = false;
}

bool HTMLElement::AreComputedStylesValid() const {
  return computed_style_valid_ && pseudo_elements_computed_styles_valid_;
}

bool HTMLElement::IsDesignated() const {
  Document* document = node_document();
  if (document) {
    scoped_refptr<Element> element = document->indicated_element();
    while (element) {
      if (element.get() == this) {
        return true;
      }
      // The parent of an element that is :hover is also in that state.
      //  https://www.w3.org/TR/selectors4/#hover-pseudo
      element = element->parent_element();
    }
  }
  return false;
}

bool HTMLElement::CanBeDesignatedByPointerIfDisplayed() const {
  return computed_style()->pointer_events() != cssom::KeywordValue::GetNone() &&
         computed_style()->visibility() == cssom::KeywordValue::GetVisible();
}

void HTMLElement::UpdateUiNavigation() {
  ui_nav_needs_update_ = false;

  base::Optional<ui_navigation::NativeItemType> ui_nav_item_type;
  if (tabindex_ && *tabindex_ <= kUiNavFocusTabIndexThreshold &&
      computed_style()->pointer_events() != cssom::KeywordValue::GetNone()) {
    ui_nav_item_type = ui_navigation::kNativeItemTypeFocus;
  } else if (computed_style()->overflow() == cssom::KeywordValue::GetAuto() ||
             computed_style()->overflow() == cssom::KeywordValue::GetScroll()) {
    ui_nav_item_type = ui_navigation::kNativeItemTypeContainer;
  }

  if (ui_nav_item_type && IsDisplayed() && node_document()) {
    ui_navigation::NativeItemDir ui_nav_item_dir;
    ui_nav_item_dir.is_left_to_right =
        directionality() == kLeftToRightDirectionality;
    ui_nav_item_dir.is_top_to_bottom = true;

    if (ui_nav_item_) {
      if (ui_nav_item_->GetType() == *ui_nav_item_type) {
        // Keep using the existing navigation item.
        ui_nav_item_->SetDir(ui_nav_item_dir);
        return;
      }
      // The current navigation item isn't of the correct type. Disable it so
      // that callbacks won't be invoked for it. The object will be destroyed
      // when all references to it are released.
      ReleaseUiNavigationItem();
    }

    ui_nav_item_ = new ui_navigation::NavItem(
        *ui_nav_item_type,
        base::Bind(
            &UiNavCallbackHelper, base::MessageLoop::current()->task_runner(),
            base::Bind(&HTMLElement::OnUiNavBlur, base::AsWeakPtr(this))),
        base::Bind(
            &UiNavCallbackHelper, base::MessageLoop::current()->task_runner(),
            base::Bind(&HTMLElement::OnUiNavFocus, base::AsWeakPtr(this))),
        base::Bind(
            &UiNavCallbackHelper, base::MessageLoop::current()->task_runner(),
            base::Bind(&HTMLElement::OnUiNavScroll, base::AsWeakPtr(this))));
    ui_nav_item_->SetDir(ui_nav_item_dir);
    if (ui_nav_focus_duration_) {
      ui_nav_item_->SetFocusDuration(*ui_nav_focus_duration_);
    }

    node_document()->AddUiNavigationElement(this);
    node_document()->set_ui_nav_needs_layout(true);
    InvalidateLayoutBoxRenderTreeNodes();
    if (layout_boxes_) {
      layout_boxes_->SetUiNavItem(ui_nav_item_);
    }
  } else if (ui_nav_item_) {
    // This navigation item is no longer relevant.
    ReleaseUiNavigationItem();
    InvalidateLayoutBoxRenderTreeNodes();
    if (layout_boxes_) {
      layout_boxes_->SetUiNavItem(ui_nav_item_);
    }
  }
}

void HTMLElement::ReleaseUiNavigationItem() {
  if (ui_nav_item_) {
    // Disable the UI navigation item so it won't receive anymore callbacks
    // while being released.
    if (node_document()) {
      node_document()->RemoveUiNavigationElement(this);
      node_document()->set_ui_nav_needs_layout(true);
      if (node_document()->ui_nav_focus_element() == this) {
        if (node_document()->TrySetUiNavFocusElement(nullptr,
                                                     SbTimeGetMonotonicNow())) {
          ui_nav_item_->UnfocusAll();
        }
      }
    }
    ui_nav_item_->SetEnabled(false);
    ui_nav_item_ = nullptr;
  }
}

void HTMLElement::ClearActiveBackgroundImages() {
  if (html_element_context() &&
      html_element_context()->animated_image_tracker()) {
    for (std::vector<GURL>::iterator it = active_background_images_.begin();
         it != active_background_images_.end(); ++it) {
      html_element_context()->animated_image_tracker()->DecreaseURLCount(*it);
    }
  }
  active_background_images_.clear();
}

void HTMLElement::UpdateCachedBackgroundImagesFromComputedStyle() {
  ClearActiveBackgroundImages();

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

      // Skip invalid URL.
      GURL absolute_url = base::polymorphic_downcast<cssom::AbsoluteURLValue*>(
                              property_list_value->value()[i].get())
                              ->value();
      if (!absolute_url.is_valid()) {
        continue;
      }

      active_background_images_.push_back(absolute_url);
      html_element_context()->animated_image_tracker()->IncreaseURLCount(
          absolute_url);

      scoped_refptr<loader::image::CachedImage> cached_image =
          html_element_context()->image_cache()->GetOrCreateCachedResource(
              absolute_url, loader::Origin());
      base::Closure loaded_callback = base::Bind(
          &HTMLElement::OnBackgroundImageLoaded, base::Unretained(this));
      cached_images.emplace_back(
          new loader::image::CachedImageReferenceWithCallbacks(
              cached_image, loaded_callback, base::Closure()));
    }

    cached_background_images_ = std::move(cached_images);
  } else {
    // Clear the previous cached background image if the display is "none".
    cached_background_images_.clear();
  }
}

void HTMLElement::GetLoadTimingInfoAndCreateResourceTiming() {
  if (html_element_context()->performance() == nullptr) return;
  for (auto& cached_background_image : cached_background_images_) {
    scoped_refptr<loader::CachedResourceBase> cached_image =
        cached_background_image->GetCachedResource();
    if (cached_image == nullptr) continue;

    if (!cached_image->get_resource_timing_created_flag()) {
      html_element_context()->performance()->CreatePerformanceResourceTiming(
          cached_image->GetLoadTimingInfo(),
          kPerformanceResourceTimingInitiatorType, cached_image->url().spec());
      cached_image->set_resource_timing_created_flag(true);
    }
  }
}

void HTMLElement::OnBackgroundImageLoaded() {
  node_document()->RecordMutation();
  InvalidateLayoutBoxRenderTreeNodes();
  // GetLoadTimingInfo from cached resource and create resource timing.
  GetLoadTimingInfoAndCreateResourceTiming();
}

}  // namespace dom
}  // namespace cobalt
