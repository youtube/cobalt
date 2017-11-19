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

#include "cobalt/dom/attr.h"

#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/named_node_map.h"

namespace cobalt {
namespace dom {

Attr::Attr(const std::string& name, const std::string& value,
           const scoped_refptr<const NamedNodeMap>& container)
    : name_(name), value_(value), container_(container) {
  GlobalStats::GetInstance()->Add(this);
}

void Attr::set_value(const std::string& value) {
  value_ = value;
  if (container_) {
    container_->element()->SetAttribute(name_, value_);
  }
}

const std::string& Attr::node_value() const {
#ifdef __LB_SHELL__FORCE_LOGGING__
  // TODO: All warnings logged from Web APIs should contain JavaScript call
  //       stack and should be deduplicated by JavaScript location.
  static bool duplicate_warning = false;
  if (!duplicate_warning) {
    duplicate_warning = true;
    LOG(WARNING) << "Use of deprecated Web API: Attr.nodeValue.";
  }
#endif  // __LB_SHELL__FORCE_LOGGING__

  return value_;
}

void Attr::TraceMembers(script::Tracer* tracer) {
  // This const cast is safe, as the tracer will only be using it as a
  // |Wrappable|, which is threadsafe, as JavaScript is single threaded.
  tracer->Trace(const_cast<NamedNodeMap*>(container_.get()));
}

Attr::~Attr() { GlobalStats::GetInstance()->Remove(this); }

scoped_refptr<const NamedNodeMap> Attr::container() const { return container_; }

void Attr::set_container(const scoped_refptr<const NamedNodeMap>& value) {
  container_ = value;
}

}  // namespace dom
}  // namespace cobalt
