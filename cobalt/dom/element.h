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

#ifndef DOM_ELEMENT_H_
#define DOM_ELEMENT_H_

#include "base/optional.h"
#include "base/string_piece.h"
#include "cobalt/cssom/css_style_declaration.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

class DOMTokenList;
class HTMLCollection;
class HTMLElement;
class HTMLElementFactory;
class NamedNodeMap;

// The Element interface represents an object of a Document. This interface
// describes methods and properties common to all kinds of elements.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-element
class Element : public Node {
 public:
  typedef base::hash_map<std::string, std::string> AttributeMap;

  Element();
  explicit Element(HTMLElementFactory* html_element_factory);

  // Web API: Node
  //
  bool HasAttributes() const OVERRIDE;

  const std::string& node_name() const OVERRIDE { return tag_name(); }
  NodeType node_type() const OVERRIDE { return Node::kElementNode; }

  // Web API: Element
  //

  // Note: tag_name is implemented differently from the spec.
  // To save memory, there's no member variable storing the tag name. For an
  // Element that is not an HTMLElement, tag_name() always returns "#element".
  // For an HTMLElement, each subclass will be responsible for reporting its
  // own tag name.
  virtual const std::string& tag_name() const;

  std::string id() const { return GetAttribute("id").value_or(""); }
  void set_id(const std::string& value) { SetAttribute("id", value); }

  std::string class_name() const { return GetAttribute("class").value_or(""); }
  void set_class_name(const std::string& value) {
    SetAttribute("class", value);
  }

  scoped_refptr<DOMTokenList> class_list();

  scoped_refptr<NamedNodeMap> attributes();

  base::optional<std::string> GetAttribute(const std::string& name) const;
  void SetAttribute(const std::string& name, const std::string& value);
  void RemoveAttribute(const std::string& name);
  bool HasAttribute(const std::string& name) const;

  scoped_refptr<HTMLCollection> GetElementsByClassName(
      const std::string& class_name) const;
  scoped_refptr<HTMLCollection> GetElementsByTagName(
      const std::string& tag_name) const;

  // Web API: DOM Parsing and Serialization (partial interface)
  // This interface is extended in the spec DOM Parsing and Serialization.
  //   http://www.w3.org/TR/2014/CR-DOM-Parsing-20140617/#extensions-to-the-element-interface
  //
  std::string inner_html() const;
  void set_inner_html(const std::string& inner_html);

  // Web API: Selectors API Level 1 (partial interface)
  // This interface is extended in the spec Selectors API Level 1.
  //   http://www.w3.org/TR/selectors-api/#interface-definitions
  //
  // TODO(***REMOVED***): Implement QuerySelector|All.

  // Custom, not in any spec.
  //

  // Returns a map that holds the actual attributes of the element.
  const AttributeMap& attribute_map() const { return attribute_map_; }

  bool IsElement() const OVERRIDE { return true; }

  scoped_refptr<Element> AsElement() OVERRIDE { return this; }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  void Accept(ConstNodeVisitor* visitor) const OVERRIDE;

  virtual scoped_refptr<HTMLElement> AsHTMLElement();

  // TODO(***REMOVED***): Remove this when auto type binding is supported for JS.
  // b/19898684 is fired to track this issue.
  // This function is just for HTMLElement.
  virtual const scoped_refptr<cssom::CSSStyleDeclaration>& style() {
    return style_;
  }

 protected:
  ~Element() OVERRIDE;

  // Getting and setting boolean attribute.
  //   http://www.w3.org/TR/html5/infrastructure.html#boolean-attribute
  bool GetBooleanAttribute(const std::string& name) const;
  void SetBooleanAttribute(const std::string& name, bool value);

 private:
  // Callback for error when parsing inner HTML.
  void InnerHTMLError(const std::string& error);

  // A map that holds the actual element attributes.
  AttributeMap attribute_map_;
  // A weak pointer to a NamedNodeMap that proxies the actual attributes.
  // This heavy weight object is kept in memory only when needed by the user.
  base::WeakPtr<NamedNodeMap> named_node_map_;
  // A weak pointer to a DOMTOkenList containing the the classes of the element.
  // This heavy weight object is kept in memory only when needed by the user.
  base::WeakPtr<DOMTokenList> class_list_;

  // Reference to HTML element factory, used for setting inner HTML attribute.
  HTMLElementFactory* html_element_factory_;

  // TODO(***REMOVED***): Remove style_ when auto type binding is supported for JS.
  scoped_refptr<cssom::CSSStyleDeclaration> style_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_ELEMENT_H_
