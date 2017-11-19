// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/node_list.h"

#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

NodeList::NodeList() { GlobalStats::GetInstance()->Add(this); }

uint32 NodeList::length() { return static_cast<uint32>(collection_.size()); }

scoped_refptr<Node> NodeList::Item(uint32 item) {
  if (item < collection_.size()) {
    return collection_[item];
  }
  return NULL;
}

void NodeList::Clear() { collection_.clear(); }

void NodeList::AppendNode(const scoped_refptr<Node>& node) {
  collection_.push_back(node);
}

void NodeList::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(collection_);
}

NodeList::~NodeList() { GlobalStats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
