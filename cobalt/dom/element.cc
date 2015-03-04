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
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_serializer.h"
#include "cobalt/dom/named_node_map.h"

namespace cobalt {
namespace dom {

// static
scoped_refptr<Element> Element::Create() {
  return make_scoped_refptr(new Element());
}

bool Element::HasAttributes() const { return !attribute_map_.empty(); }

scoped_refptr<NamedNodeMap> Element::attributes() {
  scoped_refptr<NamedNodeMap> named_node_map = named_node_map_.get();
  if (!named_node_map) {
    // Create a new instance and store a weak reference.
    named_node_map = NamedNodeMap::Create(this);
    named_node_map_ = named_node_map->AsWeakPtr();
  }
  return named_node_map;
}

const std::string& Element::tag_name() const {
  static const std::string kElementName("#element");
  return kElementName;
}

scoped_refptr<DOMTokenList> Element::class_list() {
  scoped_refptr<DOMTokenList> class_list = class_list_.get();
  if (!class_list) {
    // Create a new instance and store a weak reference.
    class_list = DOMTokenList::Create(this, "class");
    class_list_ = class_list->AsWeakPtr();
  }
  return class_list;
}

std::string Element::inner_html() const {
  std::stringstream html_stream;

  HTMLSerializer serializer(&html_stream);
  serializer.SerializeDescendantsOnly(this);

  return html_stream.str();
}

// Algorithm for GetAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-getattribute
base::optional<std::string> Element::GetAttribute(
    const std::string& name) const {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string name_lower_case = name;
  StringToLowerASCII(&name_lower_case);

  // 2. Return the value of the attribute in element's attribute list whose
  //    namespace is namespace and local name is localName, if it has one, and
  //    null otherwise.
  AttributeMap::const_iterator iter = attribute_map_.find(name_lower_case);
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
  std::string name_lower_case = name;
  StringToLowerASCII(&name_lower_case);

  // 3. Let attribute be the first attribute in the context object's attribute
  //    list whose name is name, or null if there is no such attribute.
  // 4. If attribute is null, create an attribute whose local name is name and
  //    value is value, and then append this attribute to the context object and
  //    terminate these steps.
  // 5. Change attribute from context object to value.
  attribute_map_[name_lower_case] = value;

  // Custom, not in any spec.
  // Changing the class name may affect the contents of proxy objects.
  if (name_lower_case == "class") {
    UpdateNodeGeneration();
  }
  if (named_node_map_) {
    named_node_map_->SetAttributeInternal(name_lower_case, value);
  }
}

// Algorithm for RemoveAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-removeattribute
void Element::RemoveAttribute(const std::string& name) {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string name_lower_case = name;
  StringToLowerASCII(&name_lower_case);

  // 2. Remove the first attribute from the context object whose name is name,
  //    if any.
  AttributeMap::iterator iter = attribute_map_.find(name_lower_case);
  if (iter == attribute_map_.end()) {
    return;
  }
  attribute_map_.erase(iter);

  // Custom, not in any spec.
  // Changing the class name may affect the contents of proxy objects.
  if (name_lower_case == "class") {
    UpdateNodeGeneration();
  }
  if (named_node_map_) {
    named_node_map_->RemoveAttributeInternal(name_lower_case);
  }
}

// Algorithm for HasAttribute:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#dom-element-hasattribute
bool Element::HasAttribute(const std::string& name) const {
  // 1. If the context object is in the HTML namespace and its node document is
  //    an HTML document, let name be converted to ASCII lowercase.
  std::string name_lower_case = name;
  StringToLowerASCII(&name_lower_case);

  // 2. Return true if the context object has an attribute whose name is name,
  //    and false otherwise.
  AttributeMap::const_iterator iter = attribute_map_.find(name_lower_case);
  return iter != attribute_map_.end();
}

scoped_refptr<HTMLCollection> Element::GetElementsByClassName(
    const std::string& class_name) const {
  return HTMLCollection::CreateWithElementsByClassName(this, class_name);
}

scoped_refptr<HTMLCollection> Element::GetElementsByTagName(
    const std::string& tag_name) const {
  return HTMLCollection::CreateWithElementsByTagName(this, tag_name);
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

scoped_refptr<HTMLElement> Element::AsHTMLElement() { return NULL; }

Element::Element() {}

Element::~Element() {}

}  // namespace dom
}  // namespace cobalt
