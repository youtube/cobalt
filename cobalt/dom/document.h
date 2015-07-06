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

#ifndef DOM_DOCUMENT_H_
#define DOM_DOCUMENT_H_

#include <string>

#include "base/observer_list.h"
#include "base/string_piece.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/node.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

class Attr;
class Element;
class HTMLBodyElement;
class HTMLCollection;
class HTMLElement;
class HTMLElementContext;
class HTMLHeadElement;
class HTMLHtmlElement;
class Location;
class Text;

class DocumentObserver {
 public:
  // Called at most once, when document and all referred resources are loaded.
  virtual void OnLoad() = 0;

  // Called each time when the document or one of its descendants is changed.
  virtual void OnMutation() = 0;

 protected:
  virtual ~DocumentObserver() {}
};

// The Document interface serves as an entry point into the web page's content
// (the DOM tree, including elements such as <head> and <body>) and provides
// functionality which is global to the document.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-document
//
// In the spec, "A document is assumed to be an XML document unless it is
// flagged as being an HTML document". In Cobalt it is always considered as HTML
// document.
class Document : public Node, public cssom::MutationObserver {
 public:
  struct Options {
    Options() {}
    explicit Options(const GURL& url_value) : url(url_value) {}

    GURL url;
  };

  Document(HTMLElementContext* html_element_context, const Options& options);

  // Web API: Node
  //
  NodeType node_type() const OVERRIDE { return Node::kDocumentNode; }
  const std::string& node_name() const OVERRIDE;

  scoped_refptr<Node> InsertBefore(
      const scoped_refptr<Node>& new_child,
      const scoped_refptr<Node>& reference_child) OVERRIDE;

  // Web API: Document
  //
  const std::string& url() const { return url_.spec(); }
  const std::string& document_uri() const { return url_.spec(); }

  scoped_refptr<Element> document_element();

  scoped_refptr<HTMLCollection> GetElementsByTagName(
      const std::string& tag_name) const;
  scoped_refptr<HTMLCollection> GetElementsByClassName(
      const std::string& class_name) const;

  scoped_refptr<Element> CreateElement();
  scoped_refptr<Element> CreateElement(const std::string& tag_name);
  scoped_refptr<Text> CreateTextNode(const std::string& text);
  scoped_refptr<Event> CreateEvent(const std::string& interface_name);

  // Web API: NonElementParentNode (implements)
  //   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-nonelementparentnode
  //
  scoped_refptr<Element> GetElementById(const std::string& id) const;

  // Web API: HTML5 (partial interface)
  //   http://www.w3.org/TR/html5/dom.html#the-document-object
  //
  scoped_refptr<Location> location() const;

  scoped_refptr<HTMLBodyElement> body() const;
  void set_body(const scoped_refptr<HTMLBodyElement>& value);

  scoped_refptr<HTMLHeadElement> head() const;

  // Web API: CSS Object Model (partial interface)
  //   http://dev.w3.org/csswg/cssom/#extensions-to-the-document-interface
  const scoped_refptr<cssom::StyleSheetList>& style_sheets() const {
    return style_sheets_;
  }

  // Web API: Selectors API (partial interface)
  // This interface is extended in the spec Selectors API.
  //   http://www.w3.org/TR/selectors-api2/#interface-definitions
  //
  scoped_refptr<Element> QuerySelector(const std::string& selectors);

  // Web API: GlobalEventHandlers (implements)
  //   http://www.w3.org/TR/html5/webappapis.html#event-handlers
  scoped_refptr<EventListener> onload();
  void set_onload(const scoped_refptr<EventListener>& event_listener);

  // Custom, not in any spec: Node.
  //
  bool IsDocument() const OVERRIDE { return true; }

  scoped_refptr<Document> AsDocument() OVERRIDE { return this; }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  void Accept(ConstNodeVisitor* visitor) const OVERRIDE;

  scoped_refptr<Node> Duplicate() const OVERRIDE {
    return new Document(html_element_context_, Options(url_));
  }

  // Custom, not in any spec.
  //
  void AddObserver(DocumentObserver* observer);
  void RemoveObserver(DocumentObserver* observer);

  scoped_refptr<HTMLHtmlElement> html() const;

  // Count all ongoing loadings, including document itself and its dependent
  // resources, and dispatch OnLoad() if necessary.
  void IncreaseLoadingCounter();
  void DecreaseLoadingCounterAndMaybeDispatchOnLoad();

  void set_should_dispatch_on_load(bool value) {
    should_dispatch_on_load_ = value;
  }

  GURL& url_as_gurl() { return url_; }

  void SignalOnLoadToObservers();

  // Must be called by all descendants of the document on their modification.
  // TODO(***REMOVED***): Provide more granularity, model after mutation observers
  //               (see http://www.w3.org/TR/dom/#mutation-observers).
  void RecordMutation();

  // From cssom::MutationObserver.
  void OnMutation() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(Document);

 private:
  ~Document() OVERRIDE;

  // These functions are called when body, head, html elements are attached to/
  // detached from the document. This causes these elements to be friend classes
  // of Document.
  void SetBodyInternal(HTMLBodyElement* value);
  void SetHeadInternal(HTMLHeadElement* value);
  void SetHtmlInternal(HTMLHtmlElement* value);

  // Reference to HTML element Factory.
  HTMLElementContext* html_element_context_;
  // Associated location obejct.
  scoped_refptr<Location> location_;
  // Weak references to the elements that according to the spec should only
  // appear once in the document.
  base::WeakPtr<HTMLBodyElement> body_;
  base::WeakPtr<HTMLHeadElement> head_;
  base::WeakPtr<HTMLHtmlElement> html_;
  // URL of the document.
  GURL url_;
  // List of CSS style sheets.
  scoped_refptr<cssom::StyleSheetList> style_sheets_;
  // The number of ongoing loadings.
  int loading_counter_;
  // Whether OnLoad event should be dispatched.
  bool should_dispatch_on_load_;
  // List of document observers.
  ObserverList<DocumentObserver> observers_;

  friend class HTMLBodyElement;
  friend class HTMLHeadElement;
  friend class HTMLHtmlElement;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_DOCUMENT_H_
