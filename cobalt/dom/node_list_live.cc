// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/node_list_live.h"

#include "cobalt/dom/node.h"
#include "cobalt/dom/node_children_iterator.h"

namespace cobalt {
namespace dom {

// static
scoped_refptr<NodeListLive> NodeListLive::CreateWithChildren(
    const scoped_refptr<const Node>& base) {
  DCHECK(base.get());
  return new NodeListLive(base);
}

unsigned int NodeListLive::length() {
  MaybeRefreshCollection();
  return NodeList::length();
}

scoped_refptr<Node> NodeListLive::Item(unsigned int item) {
  MaybeRefreshCollection();
  return NodeList::Item(item);
}

void NodeListLive::MaybeRefreshCollection() {
  if (base_node_generation_ != base_->node_generation()) {
    Clear();

    // The allocations caused by |AppendNode| below show up as hot in
    // profiles.  In order to mitigate this, we do an initial traversal to
    // figure out how many nodes we plan on appending, reserve, and then
    // append.
    NodeChildrenIterator iterator(base_);
    Node* child = iterator.First();
    int pending_append_count = 0;
    while (child) {
      pending_append_count++;
      child = iterator.Next();
    }
    ReserveForInternalCollection(pending_append_count);
    child = iterator.First();
    while (child) {
      AppendNode(child);
      child = iterator.Next();
    }

    base_node_generation_ = base_->node_generation();
  }
}

NodeListLive::NodeListLive(const scoped_refptr<const Node>& base)
    : base_(base), base_node_generation_(Node::kInvalidNodeGeneration) {}

}  // namespace dom
}  // namespace cobalt
