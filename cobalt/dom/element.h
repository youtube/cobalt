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

#ifndef COBALT_DOM_ELEMENT_H_
#define COBALT_DOM_ELEMENT_H_

#include <string>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "cobalt/base/token.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/node.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web_animations/animation_set.h"

namespace cobalt {
namespace dom {

class DOMRect;
class DOMRectList;
class DOMTokenList;
class HTMLCollection;
class HTMLElement;
class HTMLElementContext;
class NamedNodeMap;

// The Element interface represents an object of a Document. This interface
// describes methods and properties common to all kinds of elements.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#interface-element
class Element : public Node {
 public:
  // NOTE1: The array size of base::SmallMap and the decision to use
  // base::hash_map as the underlying container type are based on extensive
  // performance testing. Do not change these unless additional profiling data
  // justifies it.
  // NOTE2: Using base::SmallMap rather than base::hash_map also results in
  // substantial memory gains when live videos are played. These videos trigger
  // the creation of XML documents with over 20k elements, of which over 99%
  // contain a single attribute. By using base::SmallMap, these single attribute
  // elements are contained within arrays rather than hash_maps.
  typedef base::SmallMap<base::hash_map<std::string, std::string>, 1>
      AttributeMap;

  explicit Element(Document* document);
  Element(Document* document, base::Token local_name);

  // Web API: Node
  //
  base::Token node_name() const override { return tag_name(); }
  NodeType node_type() const override { return Node::kElementNode; }

  base::optional<std::string> text_content() const override;
  void set_text_content(
      const base::optional<std::string>& text_content) override;

  bool HasAttributes() const override;

  // Web API: Element
  //
  base::Token local_name() const { return local_name_; }

  base::Token tag_name() const;

  base::Token id() const { return id_attribute_; }
  void set_id(const std::string& value) { SetAttribute("id", value); }

  std::string class_name() const { return GetAttribute("class").value_or(""); }
  void set_class_name(const std::string& value) {
    SetAttribute("class", value);
  }

  const scoped_refptr<DOMTokenList>& class_list();
  scoped_refptr<NamedNodeMap> attributes();

  base::optional<std::string> GetAttribute(const std::string& name) const;
  void SetAttribute(const std::string& name, const std::string& value);
  void RemoveAttribute(const std::string& name);
  bool HasAttribute(const std::string& name) const;

  scoped_refptr<HTMLCollection> GetElementsByTagName(
      const std::string& local_name) const;
  scoped_refptr<HTMLCollection> GetElementsByClassName(
      const std::string& class_name) const;

  // Web API: CSSOM View Module: Extensions to the Element Interface (partial
  // interface)
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#extensions-to-the-element-interface
  scoped_refptr<DOMRect> GetBoundingClientRect();
  virtual scoped_refptr<DOMRectList> GetClientRects();
  virtual float client_top();
  virtual float client_left();
  virtual float client_width();
  virtual float client_height();

  // Web API: DOM Parsing and Serialization (partial interface)
  //   https://www.w3.org/TR/DOM-Parsing/#extensions-to-the-element-interface
  //
  std::string inner_html() const;
  void set_inner_html(const std::string& inner_html);
  std::string outer_html(script::ExceptionState* exception_state) const;
  void set_outer_html(const std::string& outer_html,
                      script::ExceptionState* exception_state);

  // Web API: Pointer Events: Extensions to the Element Interface (partial
  // interface)
  //   https://www.w3.org/TR/2015/REC-pointerevents-20150224/#extensions-to-the-element-interface
  void SetPointerCapture(int32_t pointer_id,
                         script::ExceptionState* exception_state);
  void ReleasePointerCapture(int32_t pointer_id,
                             script::ExceptionState* exception_state);
  bool HasPointerCapture(int32_t pointer_id);

  // Custom, not in any spec: Node.
  //
  Element* AsElement() override { return this; }

  void Accept(NodeVisitor* visitor) override;
  void Accept(ConstNodeVisitor* visitor) const override;

  scoped_refptr<Node> Duplicate() const override;

  // Custom, not in any spec.
  //

  // Returns whether the element has no children at all except comments or
  // processing instructions.
  //   https://www.w3.org/TR/selectors4/#empty-pseudo
  bool IsEmpty();

  // Returns whether the element has focus.
  //   https://www.w3.org/TR/selectors4/#focus-pseudo
  bool HasFocus();

  // Returns a map that holds the actual attributes of the element.
  const AttributeMap& attribute_map() const { return attribute_map_; }

  // These are called when the element is generated by the parser, rather than
  // by javascript.
  // opening_tag_location points to ">" of opening tag.
  virtual void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) {
    UNREFERENCED_PARAMETER(opening_tag_location);
  }
  virtual void OnParserEndTag() {}

  // Used to ensure that the style attribute value reflects the style
  // declaration.
  //   https://www.w3.org/TR/html5/dom.html#the-style-attribute
  virtual base::optional<std::string> GetStyleAttribute() const;
  virtual void SetStyleAttribute(const std::string& value);
  virtual void RemoveStyleAttribute();

  // Adds all style sheets contained within the this element and its descendants
  // to the style sheet vector. The style sheets are added in depth-first
  // pre-order.
  void CollectStyleSheetsOfElementAndDescendants(
      cssom::StyleSheetVector* style_sheets) const;

  virtual scoped_refptr<HTMLElement> AsHTMLElement();

  const scoped_refptr<web_animations::AnimationSet>& animations() {
    return animations_;
  }

  DEFINE_WRAPPABLE_TYPE(Element);

 protected:
  ~Element() override;

  // Getting and setting boolean attribute.
  //   https://www.w3.org/TR/html5/infrastructure.html#boolean-attribute
  bool GetBooleanAttribute(const std::string& name) const;
  void SetBooleanAttribute(const std::string& name, bool value);

  void CopyAttributes(const Element& other);

  HTMLElementContext* html_element_context();

 private:
  // From EventTarget.
  std::string GetDebugName() override;

  virtual void OnSetAttribute(const std::string& /* name */,
                              const std::string& /* value */) {}
  virtual void OnRemoveAttribute(const std::string& /* name */) {}

  // Adds this element's style sheet to the style sheet vector. By default, this
  // function does nothing, but is implemented by element subclasses that
  // generate style sheets (HTMLStyleElement and HTMLLinkElement).
  virtual void CollectStyleSheet(
      cssom::StyleSheetVector* /*style_sheets*/) const {}

  // Callback for error when parsing inner / outer HTML.
  void HTMLParseError(const std::string& error);

  // Local name of the element.
  base::Token local_name_;
  // A map that holds the actual element attributes.
  AttributeMap attribute_map_;
  // The "id" attribute for this element. Stored here in addition to being
  // stored in |attribute_map_| as an optimization for id().
  base::Token id_attribute_;
  // A weak pointer to a NamedNodeMap that proxies the actual attributes.
  // This heavy weight object is kept in memory only when needed by the user.
  base::WeakPtr<NamedNodeMap> named_node_map_;
  // Lazily created list of CSS classes.
  // After creation this is kept in memory because of the significant negative
  // performance impact of repeatedly recreating it.
  scoped_refptr<DOMTokenList> class_list_;

  // A set of all animations currently applied to this element.
  scoped_refptr<web_animations::AnimationSet> animations_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ELEMENT_H_
