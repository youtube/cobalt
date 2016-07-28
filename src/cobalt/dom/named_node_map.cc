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

#include "cobalt/dom/named_node_map.h"

#include <algorithm>

#include "cobalt/dom/attr.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"

namespace cobalt {
namespace dom {

NamedNodeMap::NamedNodeMap(const scoped_refptr<Element>& element)
    : element_(element) {
  ConstructProxyAttributes();
  GlobalStats::GetInstance()->Add(this);
}

unsigned int NamedNodeMap::length() const {
  return static_cast<unsigned int>(attribute_names_.size());
}

scoped_refptr<Attr> NamedNodeMap::Item(unsigned int item) {
  if (item < attribute_names_.size()) {
    return GetNamedItem(attribute_names_[item]);
  }
  return NULL;
}

scoped_refptr<Attr> NamedNodeMap::GetNamedItem(const std::string& name) const {
  if (!element_->HasAttribute(name)) {
    // Not need to throw an exception, fail gracefully.
    return NULL;
  }
  return GetOrCreateAttr(name);
}

scoped_refptr<Attr> NamedNodeMap::SetNamedItem(
    const scoped_refptr<Attr>& attribute) {
  if (!attribute) {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }

  if (attribute->container()) {
    if (attribute->container() == this) {
      // Already attached to this node map, don't do anything.
      return attribute;
    }
    // Attribute is already attached to a different NamedNodeMap.
    // TODO: Throw JS InUseAttributeError.
    return NULL;
  }

  const std::string& name = attribute->name();
  scoped_refptr<Attr> previous_attribute;

  if (element_->HasAttribute(name)) {
    // Get the previous attribute that was associated with 'name' and detach it
    // from NamedNodeMap.
    previous_attribute = GetOrCreateAttr(name);
    previous_attribute->set_container(NULL);
  }

  // Insert the new attribute and attach it to the NamedNodeMap.
  attribute->set_container(this);
  proxy_attributes_[name] = attribute->AsWeakPtr();

  // Inform the element about the new attribute. This should trigger a call to
  // NamedNodeMap::SetAttributeInternal and continue the update there.
  element_->SetAttribute(name, attribute->value());
  return previous_attribute;
}

scoped_refptr<Attr> NamedNodeMap::RemoveNamedItem(const std::string& name) {
  ProxyAttributeMap::iterator iter = proxy_attributes_.find(name);
  if (iter == proxy_attributes_.end()) {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }

  scoped_refptr<Attr> previous_attribute;
  if (element_->HasAttribute(name)) {
    // Get the previous attribute that was associated with 'name' and detach it
    // from NamedNodeMap.
    previous_attribute = GetOrCreateAttr(name);
    previous_attribute->set_container(NULL);
  }

  // Inform the element about the removed attribute. This should trigger a call
  // to NamedNodeMap::RemoveAttributeInternal and continue the update there.
  element_->RemoveAttribute(name);

  return previous_attribute;
}

bool NamedNodeMap::CanQueryNamedProperty(const std::string& name) const {
  return element_->HasAttribute(name);
}

void NamedNodeMap::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) const {
  for (size_t i = 0; i < attribute_names_.size(); ++i) {
    enumerator->AddProperty(attribute_names_[i]);
  }
}

void NamedNodeMap::SetAttributeInternal(const std::string& name,
                                        const std::string& value) {
  // If this a new name, add to it to the name vector.
  AttributeNameVector::const_iterator name_iter =
      std::find(attribute_names_.begin(), attribute_names_.end(), name);
  if (name_iter == attribute_names_.end()) {
    attribute_names_.push_back(name);
  }

  // Update the associated Attr object.
  ProxyAttributeMap::iterator attribute_iter = proxy_attributes_.find(name);
  if (attribute_iter != proxy_attributes_.end() && attribute_iter->second) {
    attribute_iter->second->SetValueInternal(value);
  }
}

void NamedNodeMap::RemoveAttributeInternal(const std::string& name) {
  attribute_names_.erase(
      std::remove(attribute_names_.begin(), attribute_names_.end(), name),
      attribute_names_.end());

  // Make sure to detach the proxy attribute from NamedNodeMap.
  ProxyAttributeMap::iterator iter = proxy_attributes_.find(name);
  if (iter != proxy_attributes_.end()) {
    if (iter->second) {
      iter->second->set_container(NULL);
    }
    proxy_attributes_.erase(iter);
  }
}

scoped_refptr<Element> NamedNodeMap::element() const { return element_; }

NamedNodeMap::~NamedNodeMap() { GlobalStats::GetInstance()->Remove(this); }

void NamedNodeMap::ConstructProxyAttributes() {
  // Construct the attribute name vector.
  attribute_names_.clear();
  const Element::AttributeMap& attribute_map = element_->attribute_map();
  for (Element::AttributeMap::const_iterator iter = attribute_map.begin();
       iter != attribute_map.end(); ++iter) {
    attribute_names_.push_back(iter->first);
  }

  // There's no need to create the ProxyAttributeMap since it'll be created
  // on demand when accessed by the user.
}

scoped_refptr<Attr> NamedNodeMap::GetOrCreateAttr(
    const std::string& name) const {
  ProxyAttributeMap::iterator iter = proxy_attributes_.find(name);
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
