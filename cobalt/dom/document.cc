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

#include "cobalt/dom/document.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_implementation.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/location.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/text.h"
#include "cobalt/dom/ui_event.h"

namespace cobalt {
namespace dom {

Document::Document(HTMLElementContext* html_element_context,
                   const Options& options)
    : ALLOW_THIS_IN_INITIALIZER_LIST(Node(this)),
      html_element_context_(html_element_context),
      implementation_(new DOMImplementation()),
      location_(new Location(options.url)),
      url_(options.url),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          style_sheets_(new cssom::StyleSheetList(this))),
      loading_counter_(0),
      should_dispatch_load_event_(true),
      rule_matches_dirty_(true),
      computed_style_dirty_(true) {
  DCHECK(url_.is_empty() || url_.is_valid());
  // Call OnInsertedIntoDocument() immediately to ensure that the Document
  // object itself is considered to be "in the document".
  OnInsertedIntoDocument();
}

std::string Document::node_name() const {
  static const char kDocumentName[] = "#document";
  return kDocumentName;
}

scoped_refptr<Element> Document::document_element() {
  return first_element_child();
}

scoped_refptr<DOMImplementation> Document::implementation() {
  return implementation_;
}

scoped_refptr<HTMLCollection> Document::GetElementsByTagName(
    const std::string& local_name) const {
  return HTMLCollection::CreateWithElementsByTagName(this, local_name);
}

scoped_refptr<HTMLCollection> Document::GetElementsByClassName(
    const std::string& class_names) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_names);
}

scoped_refptr<Element> Document::CreateElement(const std::string& local_name) {
  if (IsXMLDocument()) {
    return new Element(this, local_name);
  } else {
    std::string lower_local_name = local_name;
    StringToLowerASCII(&lower_local_name);
    DCHECK(html_element_context_);
    DCHECK(html_element_context_->html_element_factory());
    return html_element_context_->html_element_factory()->CreateHTMLElement(
        this, lower_local_name);
  }
}

scoped_refptr<Text> Document::CreateTextNode(const std::string& text) {
  return new Text(this, text);
}

scoped_refptr<Event> Document::CreateEvent(
    const std::string& interface_name,
    script::ExceptionState* exception_state) {
  // http://www.w3.org/TR/2015/WD-dom-20150428/#dom-document-createevent
  // The match of interface name is case-insensitive.
  if (strcasecmp(interface_name.c_str(), "event") == 0 ||
      strcasecmp(interface_name.c_str(), "events") == 0) {
    return new Event(Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "uievent") == 0 ||
             strcasecmp(interface_name.c_str(), "uievents") == 0) {
    return new UIEvent(Event::Uninitialized);
  }

  DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
  // Return value will be ignored.
  return NULL;
}

scoped_refptr<Element> Document::GetElementById(const std::string& id) const {
  NodeDescendantsIterator iterator(this);

  // TODO(***REMOVED***): Consider optimizing this method by replacing the linear
  // search with a constant time lookup.
  Node* child = iterator.First();
  while (child) {
    scoped_refptr<Element> element = child->AsElement();
    if (element && element->id() == id) {
      return element;
    }
    child = iterator.Next();
  }
  return NULL;
}

scoped_refptr<Location> Document::location() const { return location_; }

scoped_refptr<HTMLBodyElement> Document::body() const { return body_.get(); }

// Algorithm for set_body:
//   http://www.w3.org/TR/html5/dom.html#dom-document-body
void Document::set_body(const scoped_refptr<HTMLBodyElement>& value) {
  // 1. If the new value is not a body or frameset element, then throw a
  //    HierarchyRequestError exception and abort these steps.
  if (value->tag_name() != HTMLBodyElement::kTagName) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return;
  }

  // 2. Otherwise, if the new value is the same as the body element, do nothing.
  //    Abort these steps.
  scoped_refptr<HTMLBodyElement> current_body = body();
  if (current_body == value) return;

  // 3. Otherwise, if the body element is not null, then replace that element
  //    with the new value in the DOM, as if the root element's replaceChild()
  //    method had been called with the new value and the incumbent body element
  //    as its two arguments respectively, then abort these steps.
  // 4. Otherwise, if there is no root element, throw a HierarchyRequestError
  //    exception and abort these steps.
  // 5. Otherwise, the body element is null, but there's a root element. Append
  //    the new value to the root element.
  scoped_refptr<HTMLHtmlElement> current_html = html();
  if (!current_html) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return;
  }
  if (current_body) {
    current_html->ReplaceChild(value, current_body);
  } else {
    current_html->AppendChild(value);
  }
}

scoped_refptr<HTMLHeadElement> Document::head() const { return head_.get(); }

scoped_refptr<Element> Document::active_element() const {
  if (!active_element_) {
    return body();
  } else {
    return active_element_.get();
  }
}

scoped_refptr<Element> Document::QuerySelector(const std::string& selectors) {
  return QuerySelectorInternal(selectors, html_element_context_->css_parser());
}

void Document::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Document::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

scoped_refptr<HTMLHtmlElement> Document::html() const { return html_.get(); }

void Document::SetBody(HTMLBodyElement* body) {
  if (body) {
    DCHECK(!body_);
    body_ = base::AsWeakPtr(body);
  } else {
    DCHECK(body_);
    body_.reset();
  }
}

void Document::SetHead(HTMLHeadElement* head) {
  if (head) {
    DCHECK(!head_);
    head_ = base::AsWeakPtr(head);
  } else {
    DCHECK(head_);
    head_.reset();
  }
}

void Document::SetHtml(HTMLHtmlElement* html) {
  if (html) {
    DCHECK(!html_);
    html_ = base::AsWeakPtr(html);
  } else {
    DCHECK(html_);
    html_.reset();
  }
}

void Document::SetActiveElement(Element* active_element) {
  if (active_element) {
    active_element_ = base::AsWeakPtr(active_element);
  } else {
    active_element_.reset();
  }
}

void Document::IncreaseLoadingCounter() { ++loading_counter_; }

void Document::DecreaseLoadingCounterAndMaybeDispatchLoadEvent(
    bool load_succeeded) {
  if (!load_succeeded) {
    // If the document or any dependent script failed loading, load event
    // shouldn't be dispatched.
    should_dispatch_load_event_ = false;
  } else {
    DCHECK_GT(loading_counter_, 0);
    loading_counter_--;
    if (loading_counter_ == 0 && should_dispatch_load_event_) {
      DCHECK(MessageLoop::current());
      should_dispatch_load_event_ = false;
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(base::IgnoreResult(&Document::DispatchEvent),
                                base::AsWeakPtr<Document>(this),
                                make_scoped_refptr(new Event("load"))));

      // After all JavaScript OnLoad event handlers have executed, signal to any
      // Document observers know that a load event has occurred.
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&Document::SignalOnLoadToObservers,
                                base::AsWeakPtr<Document>(this)));
    }
  }
}

void Document::AddObserver(DocumentObserver* observer) {
  observers_.AddObserver(observer);
}

void Document::RemoveObserver(DocumentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Document::SignalOnLoadToObservers() {
  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnLoad());
}

void Document::RecordMutation() {
  TRACE_EVENT0("cobalt::dom", "Document::RecordMutation()");

  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnMutation());
}

void Document::OnCSSMutation() {
  // Something in the document's CSS rules has been modified, but we don't know
  // what, so set the flag indicating that rule matching needs to be done.
  rule_matches_dirty_ = true;
  computed_style_dirty_ = true;

  RecordMutation();
}

void Document::OnDOMMutation() {
  // Something in the document's DOM has been modified, but we don't know what,
  // so set the flag indicating that rule matching needs to be done.
  rule_matches_dirty_ = true;
  computed_style_dirty_ = true;

  RecordMutation();
}

void Document::OnElementInlineStyleMutation() {
  computed_style_dirty_ = true;

  RecordMutation();
}

void Document::UpdateMatchingRules(
    const scoped_refptr<cssom::CSSStyleDeclarationData>& root_computed_style,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet) {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateMatchingRules()");

  if (rule_matches_dirty_) {
    // The following TRACE_EVENT is tracked by the benchmarking system, if
    // you change it, please also update the appspot database that watches this:
    //   ***REMOVED***steel-build-stats-doc
    TRACE_EVENT0("cobalt::dom", "UpdateMatchingRules()");
    EvaluateStyleSheetMediaRules(root_computed_style, user_agent_style_sheet,
                                 style_sheets());
    UpdateStyleSheetRuleIndexes(user_agent_style_sheet, style_sheets());
    html()->UpdateMatchingRulesRecursively(user_agent_style_sheet,
                                           style_sheets());

    rule_matches_dirty_ = false;
  }
}

void Document::UpdateComputedStyles(
    const scoped_refptr<cssom::CSSStyleDeclarationData>& root_computed_style,
    const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet) {
  TRACE_EVENT0("cobalt::dom", "Document::UpdateComputedStyles()");

  UpdateMatchingRules(root_computed_style, user_agent_style_sheet);

  if (computed_style_dirty_) {
    // Determine the official time that this style change event took place. This
    // is needed (as opposed to repeatedly calling base::Time::Now()) because
    // all animations that may be triggered here must start at the exact same
    // time if they were triggered in the same style change event.
    //   http://www.w3.org/TR/css3-transitions/#starting
    base::TimeDelta style_change_event_time =
        base::Time::Now() - base::Time::UnixEpoch();

    // The following TRACE_EVENT is tracked by the benchmarking system, if
    // you change it, please also update the appspot database that watches this:
    //   ***REMOVED***steel-build-stats-doc
    TRACE_EVENT0("cobalt::layout", "UpdateComputedStyle");
    html()->UpdateComputedStyleRecursively(root_computed_style,
                                           style_change_event_time, true);

    computed_style_dirty_ = false;
  }
}

Document::~Document() {}

}  // namespace dom
}  // namespace cobalt
