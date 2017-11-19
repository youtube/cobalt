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

#include "cobalt/dom/named_node_map.h"

#include <algorithm>
#include <iterator>

#include "base/optional.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

NamedNodeMap::NamedNodeMap(const scoped_refptr<Element>& element)
    : element_(element) {
  DCHECK(element_);
  GlobalStats::GetInstance()->Add(this);
}

// The length attribute's getter must return the attribute list's size.
//  https://dom.spec.whatwg.org/#dom-namednodemap-length
unsigned int NamedNodeMap::length() const {
  return static_cast<unsigned int>(element_->attribute_map().size());
}

// Algorithm for Item:
//   https://dom.spec.whatwg.org/#dom-namednodemap-item
scoped_refptr<Attr> NamedNodeMap::Item(unsigned int item) {
  // 1. If index is equal to or greater than context object's attribute list's
  // size, then return null.
  if (item >= element_->attribute_map().size()) {
    return NULL;
  }

  // 2. Otherwise, return context object's attribute list[index].
  Element::AttributeMap::const_iterator iter =
      element_->attribute_map().begin();
  std::advance(iter, item);
  return GetOrCreateAttr(iter->first);
}

// The getNamedItem(qualifiedName) method, when invoked, must return the result
// of getting an attribute given qualifiedName and element.
//   https://dom.spec.whatwg.org/#dom-namednodemap-getnameditem
scoped_refptr<Attr> NamedNodeMap::GetNamedItem(const std::string& name) const {
  // 1. if element is in the HTML namespace and its node document is an HTML
  // document, then set qualifiedName to qualifiedName in ASCII lowercase.
  // 2. Return the first attribute in element's attribute list whose qualified
  // name is qualifiedName, and null otherwise.
  if (!element_->HasAttribute(name)) {
    return NULL;
  }
  return GetOrCreateAttr(name);
}

// The setNamedItem(attr) and setNamedItemNS(attr) methods, when invoked, must
// return the result of setting an attribute given attr and element.
//   https://dom.spec.whatwg.org/#dom-namednodemap-setnameditem
scoped_refptr<Attr> NamedNodeMap::SetNamedItem(
    const scoped_refptr<Attr>& attribute) {
  // Custom, not in any spec.
  if (!attribute) {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }

  // To set an attribute given an attr and element, run these steps:
  //   https://dom.spec.whatwg.org/#concept-element-attributes-set

  // 1. If attr's element is neither null nor element, throw an
  // InUseAttributeError.
  if (attribute->container() && attribute->container() != this) {
    // TODO: Throw JS InUseAttributeError.
    return NULL;
  }

  // 2. Let oldAttr be the result of getting an attribute given attr's
  // namespace, attr's local name, and element.
  // 3. If oldAttr is attr, return attr.
  // 4. If oldAttr is non-null, replace it by attr in element.
  // 5. Otherwise, append attr to element.
  const std::string& name = attribute->name();
  scoped_refptr<Attr> old_attr;
  if (element_->HasAttribute(name)) {
    old_attr = GetOrCreateAttr(name);

    if (attribute->value() == old_attr->value()) {
      return old_attr;
    }
    old_attr->set_container(NULL);
  }
  attribute->set_container(this);
  proxy_attributes_[name] = attribute->AsWeakPtr();
  // Inform the element about the new attribute. This should trigger a call to
  // NamedNodeMap::SetAttributeInternal and continue the update there.
  element_->SetAttribute(name, attribute->value());

  // 6. Return oldAttr.
  return old_attr;
}

// Algorithm for RemoveNamedItem:
//  https://dom.spec.whatwg.org/#dom-namednodemap-removenameditem
scoped_refptr<Attr> NamedNodeMap::RemoveNamedItem(const std::string& name) {
  // 1. Let attr be the result of removing an attribute given qualifiedName and
  // element.
  // 2. If attr is null, then throw a NotFoundError.
  scoped_refptr<Attr> attr;
  if (element_->HasAttribute(name)) {
    // Get the previous attribute that was associated with 'name' and detach it
    // from NamedNodeMap.
    attr = GetOrCreateAttr(name);
    attr->set_container(NULL);
  } else {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }
  // Inform the element about the removed attribute. This should trigger a call
  // to NamedNodeMap::RemoveAttributeInternal and continue the update there.
  element_->RemoveAttribute(name);

  // 3. Return attr.
  return attr;
}

bool NamedNodeMap::CanQueryNamedProperty(const std::string& name) const {
  return element_->HasAttribute(name);
}

void NamedNodeMap::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) const {
  const Element::AttributeMap& attribute_map = element_->attribute_map();
  for (Element::AttributeMap::const_iterator iter = attribute_map.begin();
       iter != attribute_map.end(); ++iter) {
    enumerator->AddProperty(iter->first);
  }
}

void NamedNodeMap::SetAttributeInternal(const std::string& name,
                                        const std::string& value) {
  // Update the associated Attr object.
  NameToAttrMap::iterator attribute_iter = proxy_attributes_.find(name);
  if (attribute_iter != proxy_attributes_.end() && attribute_iter->second) {
    attribute_iter->second->SetValueInternal(value);
  }
}

void NamedNodeMap::RemoveAttributeInternal(const std::string& name) {
  // Make sure to detach the proxy attribute from NamedNodeMap.
  NameToAttrMap::iterator iter = proxy_attributes_.find(name);
  if (iter != proxy_attributes_.end()) {
    if (iter->second) {
      iter->second->set_container(NULL);
    }
    proxy_attributes_.erase(iter);
  }
}

scoped_refptr<Element> NamedNodeMap::element() const { return element_; }

void NamedNodeMap::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(element_);
  tracer->TraceValues(proxy_attributes_);
}

NamedNodeMap::~NamedNodeMap() { GlobalStats::GetInstance()->Remove(this); }

scoped_refptr<Attr> NamedNodeMap::GetOrCreateAttr(
    const std::string& name) const {
  TRACK_MEMORY_SCOPE("DOM");
  NameToAttrMap::iterator iter = proxy_attributes_.find(name);
  if (iter != proxy_attributes_.end() && iter->second) {
    return iter->second.get();
  }

  // We don't have an existing Attr object so we create a new once instead.
  base::optional<std::string> value = element_->GetAttribute(name);
  scoped_refptr<Attr> attribute = new Attr(name, value.value_or(""), this);
  proxy_attributes_[name] = attribute->AsWeakPtr();

  return attribute;
}

}  // namespace dom
}  // namespace cobalt
