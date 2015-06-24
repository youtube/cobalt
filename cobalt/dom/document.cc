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
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
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

Document::Document(HTMLElementFactory* html_element_factory,
                   const Options& options)
    : html_element_factory_(html_element_factory),
      location_(new Location(options.url)),
      url_(options.url),
      style_sheets_(new cssom::StyleSheetList(this)),
      loading_counter_(0),
      should_dispatch_on_load_(true) {
  DCHECK(url_.is_empty() || url_.is_valid());
}

const std::string& Document::node_name() const {
  static const std::string kDocumentName("#document");
  return kDocumentName;
}

scoped_refptr<Element> Document::document_element() {
  return first_element_child();
}

scoped_refptr<Node> Document::InsertBefore(
    const scoped_refptr<Node>& new_child,
    const scoped_refptr<Node>& reference_child) {
  Node::InsertBefore(new_child, reference_child);
  // After inserting a child to Document, the child needs to be attached to the
  // Document. This is because Document's owner_document is NULL, so the
  // attachment won't happen in Node::InsertBefore(). Same is true for
  // the mutation recording.
  new_child->AttachToDocument(this);
  RecordMutation();
  return new_child;
}

scoped_refptr<HTMLCollection> Document::GetElementsByClassName(
    const std::string& class_name) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_name);
}
scoped_refptr<HTMLCollection> Document::GetElementsByTagName(
    const std::string& tag_name) const {
  return HTMLCollection::CreateWithElementsByTagName(this, tag_name);
}

scoped_refptr<Element> Document::CreateElement() { return new Element(); }

scoped_refptr<Element> Document::CreateElement(const std::string& tag_name) {
  DCHECK(html_element_factory_);
  return html_element_factory_->CreateHTMLElement(tag_name);
}

scoped_refptr<Text> Document::CreateTextNode(const std::string& text) {
  return new Text(text);
}

scoped_refptr<Event> Document::CreateEvent(const std::string& interface_name) {
  // http://www.w3.org/TR/2015/WD-dom-20150428/#dom-document-createevent
  // The match of interface name is case-insensitive.
  if (strcasecmp(interface_name.c_str(), "event") == 0 ||
      strcasecmp(interface_name.c_str(), "events") == 0) {
    return new Event(Event::Uninitialized);
  } else if (strcasecmp(interface_name.c_str(), "uievent") == 0 ||
             strcasecmp(interface_name.c_str(), "uievents") == 0) {
    return new UIEvent(Event::Uninitialized);
  }

  // TODO(***REMOVED***): throw NotSupportedError exception.
  NOTREACHED();
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

scoped_refptr<Element> Document::QuerySelector(const std::string& selectors) {
  return QuerySelectorInternal(selectors, html_element_factory_->css_parser());
}

scoped_refptr<EventListener> Document::onload() {
  return GetAttributeEventListener("load");
}

void Document::set_onload(const scoped_refptr<EventListener>& listener) {
  SetAttributeEventListener("load", listener);
}

void Document::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Document::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

void Document::AddObserver(DocumentObserver* observer) {
  observers_.AddObserver(observer);
}

void Document::RemoveObserver(DocumentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Document::IncreaseLoadingCounter() { ++loading_counter_; }

void Document::DecreaseLoadingCounterAndMaybeDispatchOnLoad() {
  DCHECK_GT(loading_counter_, 0);
  if (--loading_counter_ == 0 && should_dispatch_on_load_) {
    should_dispatch_on_load_ = false;
    // TODO(***REMOVED***): We should also fire event on onload attribute when set.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(base::IgnoreResult(&Document::DispatchEvent),
                              base::AsWeakPtr<Document>(this),
                              make_scoped_refptr(new Event("load"))));

    // After all JavaScript OnLoad event handlers have executed, signal to any
    // Document observers know that an OnLoad event has occurred.
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&Document::SignalOnLoadToObservers,
                              base::AsWeakPtr<Document>(this)));
  }
}

void Document::SignalOnLoadToObservers() {
  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnLoad());
}

void Document::RecordMutation() {
  TRACE_EVENT0("cobalt::dom", "Document::RecordMutation()");
  FOR_EACH_OBSERVER(DocumentObserver, observers_, OnMutation());
}

void Document::OnMutation() { RecordMutation(); }

Document::~Document() {}

scoped_refptr<HTMLHtmlElement> Document::html() const { return html_.get(); }

void Document::SetBodyInternal(HTMLBodyElement* value) {
  if (value) {
    DCHECK(!body_);
    body_ = base::AsWeakPtr(value);
  } else {
    DCHECK(body_);
    body_.reset();
  }
}

void Document::SetHeadInternal(HTMLHeadElement* value) {
  if (value) {
    DCHECK(!head_);
    head_ = base::AsWeakPtr(value);
  } else {
    DCHECK(head_);
    head_.reset();
  }
}

void Document::SetHtmlInternal(HTMLHtmlElement* value) {
  if (value) {
    DCHECK(!html_);
    html_ = base::AsWeakPtr(value);
  } else {
    DCHECK(html_);
    html_.reset();
  }
}

}  // namespace dom
}  // namespace cobalt
