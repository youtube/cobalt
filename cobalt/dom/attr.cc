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

#include "cobalt/dom/attr.h"

#include "cobalt/dom/element.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/stats.h"

namespace cobalt {
namespace dom {

Attr::Attr(const std::string& name, const std::string& value,
           const scoped_refptr<const NamedNodeMap>& container)
    : name_(name), value_(value), container_(container) {
  Stats::GetInstance()->Add(this);
}

void Attr::set_value(const std::string& value) {
  value_ = value;
  if (container_) {
    container_->element()->SetAttribute(name_, value_);
  }
}

Attr::~Attr() { Stats::GetInstance()->Remove(this); }

scoped_refptr<const NamedNodeMap> Attr::container() const { return container_; }

void Attr::set_container(const scoped_refptr<const NamedNodeMap>& value) {
  container_ = value;
}

}  // namespace dom
}  // namespace cobalt
