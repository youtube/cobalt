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

#include "cobalt/dom/element.h"

#include "base/string_util.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/selector.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/serializer.h"
#include "cobalt/dom/text.h"

namespace cobalt {
namespace dom {

Element::Element(Document* document) : Node(document) {}

Element::Element(Document* document, const std::string& tag_name)
    : Node(document), tag_name_(tag_name) {}

base::optional<std::string> Element::text_content() const {
  std::string content;

  const Node* child = first_child();
  while (child) {
    if (child->IsText() || child->IsElement()) {
      content.append(child->text_content().value());
    }
    child = child->next_sibling();
  }

  return content;
}

void Element::set_text_content(
    const base::optional<std::string>& text_content) {
  // Remove all children and replace them with a single Text node.
  while (HasChildNodes()) {
    RemoveChild(first_child());
  }
  std::string new_text_content = text_content.value_or("");
  if (!new_text_content.empty()) {
    AppendChild(new Text(owner_document(), new_text_content));
  }
}

bool Element::HasAttributes() const { return !attribute_map_.empty(); }

scoped_refptr<NamedNodeMap> Element::attributes() {
  scoped_refptr<NamedNodeMap> named_node_map = named_node_map_.get();
  if (!named_node_map) {
    // Create a new instance and store a weak reference.
    named_node_map = new NamedNodeMap(this);
    named_node_map_ = named_node_map->AsWeakPtr();
  }
  return named_node_map;
}

scoped_refptr<DOMTokenList> Element::class_list() {
  scoped_refptr<DOMTokenList> class_list = class_list_.get();
  if (!class_list) {
    // Create a new instance and store a weak reference.
    class_list = new DOMTokenList(this, "class");
    class_list_ = class_list->AsWeakPtr();
  }
  return class_list;
}

// Algorithm for inner_html:
//   http://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
std::string Element::inner_html() const {
  std::ostringstream oss;
  Serializer serializer(&oss);
  serializer.SerializeDescendantsOnly(this);
  return oss.str();
}

// Algorithm for set_inner_html:
//   http://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
void Element::set_inner_html(const std::string& inner_html) {
  // 1. Let fragment be the result of invoking the fragment parsing algorithm
  // with the new value as markup, and the context object as the context
  // element.
  // 2. Replace all with fragment within the context object.
  // Remove all children.
  scoped_refptr<Node> child = first_child();
  while (child) {
    scoped_refptr<Node> next_child = child->next_sibling();
    RemoveChild(child);
    child = next_child;
  }

  // Use the DOM parser to parse the HTML input and generate children nodes.
  // TODO(***REMOVED***): Replace "Element" in the source location with the name
  //               of actual class, like "HTMLDivElement".
  html_element_context()->dom_parser()->ParseDocumentFragment(
      inner_html, owner_document(), this, NULL,
      base::SourceLocation("[object Element]", 1, 1));
}

// Algorithm for outer_html:
//   http://www.w3.org/TR/DOM-Parsing/#widl-Element-innerHTML
std::string Element::outer_html() const {
  std::ostringstream oss;
  Serializer serializer(&oss);
  serializer.Serialize(this);
  return oss.str();
}

// Algorithm for set_outer_html:
//   http://www.w3.org/TR/DOM-Parsing/#widl-Element-outerHTML
void Element::set_outer_html(const std::string& outer_html) {
  // 1. Let parent be the context object's parent.
  scoped_refptr<Node> parent = parent_node();

  // 2. If parent is null, terminate these steps. There would be no way to
  // obtain a reference to the nodes created even if the remaining steps were
  // run.
  if (!parent) {
    return;
  }

  // 3. If parent is a Document, throw a DOMException with name
  // "NoModificationAllowedError" exception.
  if (parent->IsDocument()) {
    // TODO(***REMOVED***): Throw JS NoModificationAllowedError.
    return;
  }

  // 4. Not needed by Cobalt.

  // 5. Let fragment be the result of invoking the fragment parsing algorithm
  // with the new value as markup, and parent as the context element.
  // 6. Replace the context object with fragment within the context object's
  // parent.
  // Remove this node from its parent.
  scoped_refptr<Node> reference = next_sibling();
  parent->RemoveChild(this);

  // Use the DOM parser to parse the HTML input and generate children nodes.
  // TODO(***REMOVED***): Replace "Element" in the source location with the name
  //               of actual class, like "HTMLDivElement".
  html_element_context()->dom_parser()->ParseDocumentFragment(
      outer_html, owner_document(), parent, reference,
      base::SourceLocation("[object Element]", 1, 1));
}

scoped_refptr<Element> Element::QuerySelector(const std::string& selectors) {
  return QuerySelectorInternal(selectors, html_element_context()->css_parser());
}

// Algorithm for GetAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-getattribute
base::optional<std::string> Element::GetAttribute(
    const std::string& name) const {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (!owner_document()->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Return the value of the attribute in element's attribute list whose
  //    namespace is namespace and local name is localName, if it has one, and
  //    null otherwise.
  AttributeMap::const_iterator iter = attribute_map_.find(attr_name);
  if (iter != attribute_map_.end()) {
    return iter->second;
  }
  return base::nullopt;
}

// Algorithm for SetAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-setattribute
void Element::SetAttribute(const std::string& name, const std::string& value) {
  // 1. Not needed by Cobalt.

  // 2. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (!owner_document()->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 3. Let attribute be the first attribute in the context object's attribute
  //    list whose name is name, or null if there is no such attribute.
  // 4. If attribute is null, create an attribute whose local name is name and
  //    value is value, and then append this attribute to the context object and
  //    terminate these steps.
  // 5. Change attribute from context object to value.
  attribute_map_[attr_name] = value;

  // Custom, not in any spec.
  // Changing the class name may affect the contents of proxy objects.
  if (attr_name == "class") {
    UpdateNodeGeneration();
  }
  if (named_node_map_) {
    named_node_map_->SetAttributeInternal(attr_name, value);
  }

  owner_document()->OnDOMMutation();
}

// Algorithm for RemoveAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-removeattribute
void Element::RemoveAttribute(const std::string& name) {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (!owner_document()->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Remove the first attribute from the context object whose name is name,
  //    if any.
  AttributeMap::iterator iter = attribute_map_.find(attr_name);
  if (iter == attribute_map_.end()) {
    return;
  }
  attribute_map_.erase(iter);

  // Custom, not in any spec.
  // Changing the class name may affect the contents of proxy objects.
  if (attr_name == "class") {
    UpdateNodeGeneration();
  }
  if (named_node_map_) {
    named_node_map_->RemoveAttributeInternal(attr_name);
  }

  owner_document()->OnDOMMutation();
}

// Algorithm for HasAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-hasattribute
bool Element::HasAttribute(const std::string& name) const {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string attr_name = name;
  if (!owner_document()->IsXMLDocument()) {
    StringToLowerASCII(&attr_name);
  }

  // 2. Return true if the context object has an attribute whose name is name,
  //    and false otherwise.
  AttributeMap::const_iterator iter = attribute_map_.find(attr_name);
  return iter != attribute_map_.end();
}

scoped_refptr<HTMLCollection> Element::GetElementsByTagName(
    const std::string& tag_name) const {
  return HTMLCollection::CreateWithElementsByTagName(this, tag_name);
}

scoped_refptr<HTMLCollection> Element::GetElementsByClassName(
    const std::string& class_name) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_name);
}

bool Element::GetBooleanAttribute(const std::string& name) const {
  return HasAttribute(name);
}

void Element::SetBooleanAttribute(const std::string& name, bool value) {
  if (value) {
    SetAttribute(name, "");
  } else {
    RemoveAttribute(name);
  }
}

void Element::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Element::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

scoped_refptr<Node> Element::Duplicate() const {
  Element* new_element = new Element(owner_document());
  new_element->attribute_map_ = attribute_map_;
  return new_element;
}

bool Element::IsEmpty() {
  scoped_refptr<Node> child = first_child();
  while (child) {
    if (!child->IsComment()) {
      return false;
    }
    child = child->next_sibling();
  }
  return true;
}

scoped_refptr<HTMLElement> Element::AsHTMLElement() { return NULL; }

Element::~Element() {}

HTMLElementContext* Element::html_element_context() {
  return owner_document()->html_element_context();
}

void Element::HTMLParseError(const std::string& error) {
  // TODO(***REMOVED***): Report line / column number.
  LOG(WARNING) << "Error when parsing inner HTML or outer HTML: " << error;
}

}  // namespace dom
}  // namespace cobalt
