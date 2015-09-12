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
#include "cobalt/dom/rule_matching.h"
#include "cobalt/script/exception_state.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

class Attr;
class DOMImplementation;
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
  std::string node_name() const OVERRIDE;

  // Web API: Document
  //
  scoped_refptr<DOMImplementation> implementation();
  const std::string& url() const { return url_.spec(); }
  const std::string& document_uri() const { return url_.spec(); }

  scoped_refptr<Element> document_element();

  scoped_refptr<HTMLCollection> GetElementsByTagName(
      const std::string& local_name) const;
  scoped_refptr<HTMLCollection> GetElementsByClassName(
      const std::string& class_names) const;

  scoped_refptr<Element> CreateElement(const std::string& local_name);
  scoped_refptr<Text> CreateTextNode(const std::string& data);
  scoped_refptr<Event> CreateEvent(const std::string& interface_name,
                                   script::ExceptionState* exception_state);

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

  scoped_refptr<Element> active_element() const;

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
  virtual bool IsXMLDocument() const { return false; }

  HTMLElementContext* html_element_context() const {
    return html_element_context_;
  }

  const GURL& url_as_gurl() const { return url_; }

  scoped_refptr<HTMLHtmlElement> html() const;

  // These functions are for setting weak references to certain elements in the
  // document.
  void SetBody(HTMLBodyElement* body);
  void SetHead(HTMLHeadElement* head);
  void SetHtml(HTMLHtmlElement* html);
  void SetActiveElement(Element* active_element);

  // Count all ongoing loadings, including document itself and its dependent
  // resources, and dispatch OnLoad() if necessary.
  void IncreaseLoadingCounter();
  void DecreaseLoadingCounterAndMaybeDispatchLoadEvent(bool load_succeeded);

  // Utilities related to DocumentObserver.
  void AddObserver(DocumentObserver* observer);
  void RemoveObserver(DocumentObserver* observer);
  void SignalOnLoadToObservers();

  // Must be called by all descendants of the document on their modification.
  // TODO(***REMOVED***): Provide more granularity, model after mutation observers
  //               (see http://www.w3.org/TR/dom/#mutation-observers).
  void RecordMutation();

  // From cssom::MutationObserver.
  void OnCSSMutation() OVERRIDE;

  // Called when the DOM is mutated in some way.
  void OnDOMMutation();

  // Called when the inline style of an element is modified.
  void OnElementInlineStyleMutation();

  // Called to update the rule indexes of all style sheets when needed.
  void UpdateRuleIndexes(
      const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet);

  // Scans the user agent style sheet and all style sheets in the document's
  // style sheet list and updates the cached matching rules of the document's
  // elements by performing rule matching. Only a subset of selectors is
  // supported as specified here:
  //   http://***REMOVED***cobalt-css#heading=h.s82z8u3l3se
  // Those selectors that are supported are implemented after Selectors Level 4.
  //   http://www.w3.org/TR/selectors4/
  void UpdateMatchingRules(
      const scoped_refptr<cssom::CSSStyleDeclarationData>& root_computed_style,
      const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet);

  // Updates the computed styles of all of this document's HTML elements.
  void UpdateComputedStyles(
      const scoped_refptr<cssom::CSSStyleDeclarationData>& root_computed_style,
      const scoped_refptr<cssom::CSSStyleSheet>& user_agent_style_sheet);

  DEFINE_WRAPPABLE_TYPE(Document);

 protected:
  ~Document() OVERRIDE;

 private:
  // Reference to HTML element context.
  HTMLElementContext* html_element_context_;
  // Associated DOM implementation obejct.
  scoped_refptr<DOMImplementation> implementation_;
  // Associated location obejct.
  scoped_refptr<Location> location_;
  // URL of the document.
  GURL url_;
  // List of CSS style sheets.
  scoped_refptr<cssom::StyleSheetList> style_sheets_;
  // The number of ongoing loadings.
  int loading_counter_;
  // Whether the load event should be dispatched when loading counter hits zero.
  bool should_dispatch_load_event_;
  // Indicates if rule matching/computed style is dirty and needs to be
  // recomputed before the next layout.
  bool rule_matches_dirty_;
  bool computed_style_dirty_;

  // Weak references to the certain elements in the document.
  base::WeakPtr<HTMLBodyElement> body_;
  base::WeakPtr<HTMLHeadElement> head_;
  base::WeakPtr<HTMLHtmlElement> html_;
  base::WeakPtr<Element> active_element_;
  // List of document observers.
  ObserverList<DocumentObserver> observers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_DOCUMENT_H_
