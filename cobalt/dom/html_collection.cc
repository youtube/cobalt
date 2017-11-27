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

#include "cobalt/dom/html_collection.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "cobalt/dom/dom_token_list.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/node_children_iterator.h"
#include "cobalt/dom/node_descendants_iterator.h"

namespace cobalt {
namespace dom {

/////////////////////////////////////////////////////////////////////////////
// NodeCollection
/////////////////////////////////////////////////////////////////////////////

// Predicate callback used by NodeCollection to match relevant Nodes.
typedef base::Callback<bool(Node*)> Predicate;

// NodeCollection is used to implement different types of HTMLCollections
// by customizing its functionality though different NodeIterator and
// Predicate objects.
//
// The underlying Element collection is cached until a change in the generation
// of the Node that was used to create the collection is detected.
//
template <typename NodeIterator>
class NodeCollection : public HTMLCollection {
 public:
  NodeCollection(const scoped_refptr<const Node>& base,
                 const Predicate& predicate);
  ~NodeCollection() override {}

  uint32 length() const override;

  scoped_refptr<Element> Item(uint32 item) const override;
  scoped_refptr<Element> NamedItem(const std::string& name) const override;

  bool CanQueryNamedProperty(const std::string& name) const override;
  void EnumerateNamedProperties(
      script::PropertyEnumerator* enumerator) const override;

  void TraceMembers(script::Tracer* tracer) override;

 private:
  // Checks if the collection cache is up to date and refreshes it if it's not.
  void MaybeRefreshCollection() const;

  // Base node that was used to generate the collection.
  const base::WeakPtr<Node> base_;
  // Predicate callback.
  const Predicate predicate_;
  // Generation of the base node that was used to create the cache.
  mutable uint32_t base_node_generation_;
  // Cached collection elements.
  mutable std::vector<scoped_refptr<Element> > cached_collection_;
};

template <typename NodeIterator>
NodeCollection<NodeIterator>::NodeCollection(
    const scoped_refptr<const Node>& base, const Predicate& predicate)
    : base_(base::AsWeakPtr(const_cast<Node*>(base.get()))),
      predicate_(predicate),
      base_node_generation_(Node::kInvalidNodeGeneration) {}

template <typename NodeIterator>
void NodeCollection<NodeIterator>::TraceMembers(
    script::Tracer* tracer) {
  tracer->Trace(base_);
  for (auto& element : cached_collection_) {
    tracer->Trace(element);
  }
}

template <typename NodeIterator>
void NodeCollection<NodeIterator>::MaybeRefreshCollection() const {
  scoped_refptr<const Node> base(base_);
  if (!base) {
    return;
  }

  if (base_node_generation_ != base->node_generation()) {
    NodeIterator iterator(base);

    cached_collection_.clear();
    Node* child = iterator.First();
    while (child) {
      if (predicate_.Run(child)) {
        cached_collection_.push_back(child->AsElement());
      }
      child = iterator.Next();
    }
    base_node_generation_ = base->node_generation();
  }
}

template <typename NodeIterator>
uint32 NodeCollection<NodeIterator>::length() const {
  MaybeRefreshCollection();
  return static_cast<uint32>(cached_collection_.size());
}

template <typename NodeIterator>
scoped_refptr<Element> NodeCollection<NodeIterator>::Item(uint32 item) const {
  MaybeRefreshCollection();
  if (item < cached_collection_.size()) {
    return cached_collection_[item];
  }
  return NULL;
}

template <typename NodeIterator>
scoped_refptr<Element> NodeCollection<NodeIterator>::NamedItem(
    const std::string& name) const {
  MaybeRefreshCollection();
  for (size_t i = 0; i < cached_collection_.size(); ++i) {
    scoped_refptr<Element> element = cached_collection_[i];
    if (element && element->id() == name) {
      return element;
    }
  }
  return NULL;
}

template <typename NodeIterator>
bool NodeCollection<NodeIterator>::CanQueryNamedProperty(
    const std::string& name) const {
  return NamedItem(name) != NULL;
}

template <typename NodeIterator>
void NodeCollection<NodeIterator>::EnumerateNamedProperties(
    script::PropertyEnumerator* enumerator) const {
  MaybeRefreshCollection();
  for (size_t i = 0; i < cached_collection_.size(); ++i) {
    scoped_refptr<Element> element = cached_collection_[i];
    if (element) {
      enumerator->AddProperty(element->id().c_str());
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
// Predicates
/////////////////////////////////////////////////////////////////////////////

namespace {

// Used to implement HTMLCollection::kChildElements.
bool IsElement(Node* node) { return node->IsElement(); }

// Used to implement HTMLCollection::kElementsByClassName.
bool IsElementWithClassName(const std::string& class_name, Node* node) {
  return node->IsElement() &&
         node->AsElement()->class_list()->Contains(class_name);
}

// Used to implement HTMLCollection::kElementsByLocalName.
bool IsElementWithLocalName(const std::string& local_name, Node* node) {
  return node->IsElement() &&
         (local_name == "*" || node->AsElement()->local_name() == local_name);
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////
// HTMLCollection
/////////////////////////////////////////////////////////////////////////////

// static
scoped_refptr<HTMLCollection> HTMLCollection::CreateWithChildElements(
    const scoped_refptr<const Node>& base) {
  if (!base) {
    return NULL;
  }
  return new NodeCollection<NodeChildrenIterator>(base, base::Bind(&IsElement));
}

// static
scoped_refptr<HTMLCollection> HTMLCollection::CreateWithElementsByClassName(
    const scoped_refptr<const Node>& base, const std::string& name) {
  if (!base) {
    return NULL;
  }
  return new NodeCollection<NodeDescendantsIterator>(
      base, base::Bind(&IsElementWithClassName, name));
}

// static
scoped_refptr<HTMLCollection> HTMLCollection::CreateWithElementsByLocalName(
    const scoped_refptr<const Node>& base, const std::string& name) {
  if (!base) {
    return NULL;
  }
  return new NodeCollection<NodeDescendantsIterator>(
      base, base::Bind(&IsElementWithLocalName, name));
}

HTMLCollection::HTMLCollection() { GlobalStats::GetInstance()->Add(this); }

HTMLCollection::~HTMLCollection() { GlobalStats::GetInstance()->Remove(this); }

}  // namespace dom
}  // namespace cobalt
