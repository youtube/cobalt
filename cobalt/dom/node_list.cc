/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/node_list.h"

#include "cobalt/dom/node.h"
#include "cobalt/dom/node_children_iterator.h"
#include "cobalt/dom/stats.h"

namespace cobalt {
namespace dom {

// static
scoped_refptr<NodeList> NodeList::CreateWithChildren(
    const scoped_refptr<const Node>& base) {
  if (!base) {
    return NULL;
  }
  return new NodeList(base);
}

unsigned int NodeList::length() {
  MaybeRefreshCollection();
  return cached_collection_.size();
}

scoped_refptr<Node> NodeList::Item(unsigned int item) {
  MaybeRefreshCollection();
  if (item < cached_collection_.size()) {
    return cached_collection_[item];
  }
  return NULL;
}

void NodeList::MaybeRefreshCollection() {
  if (base_node_generation_ != base_->node_generation()) {
    NodeChildrenIterator iterator(base_);

    cached_collection_.clear();
    Node* child = iterator.First();
    while (child) {
      cached_collection_.push_back(child);
      child = iterator.Next();
    }
    base_node_generation_ = base_->node_generation();
  }
}

NodeList::NodeList(const scoped_refptr<const Node>& base)
    : base_(base), base_node_generation_(Node::kInvalidNodeGeneration) {
  Stats::GetInstance()->Add(this);
}

NodeList::~NodeList() { Stats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
