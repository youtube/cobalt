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

#include "cobalt/dom/node.h"

#include <vector>

#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_type.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/rule_matching.h"
#include "cobalt/dom/stats.h"
#include "cobalt/dom/text.h"

namespace cobalt {
namespace dom {

const Node::NodeType Node::kElementNode;
const Node::NodeType Node::kTextNode;
const Node::NodeType Node::kCommentNode;
const Node::NodeType Node::kDocumentNode;
const Node::NodeType Node::kDocumentTypeNode;

// Algorithm for DispatchEvent:
//   http://www.w3.org/TR/dom/#dispatching-events
bool Node::DispatchEvent(const scoped_refptr<Event>& event) {
  DCHECK(event);
  DCHECK(!event->IsBeingDispatched());
  DCHECK(event->initialized_flag());

  if (!event || event->IsBeingDispatched() || !event->initialized_flag()) {
    return false;
  }

  typedef std::vector<scoped_refptr<Node> > Ancestors;
  Ancestors ancestors;
  for (scoped_refptr<Node> current = this->parent_node(); current != NULL;
       current = current->parent_node()) {
    ancestors.push_back(current);
  }

  event->set_target(this);

  // The capture phase
  if (!ancestors.empty()) {
    event->set_event_phase(Event::kCapturingPhase);
    for (Ancestors::reverse_iterator iter = ancestors.rbegin();
         iter != ancestors.rend() && !event->propagation_stopped(); ++iter) {
      (*iter)->FireEventOnListeners(event);
    }
  }

  // The at target phase
  if (!event->propagation_stopped()) {
    event->set_event_phase(Event::kAtTarget);
    FireEventOnListeners(event);
  }

  // The bubbling phase
  if (!event->propagation_stopped() && event->bubbles() && !ancestors.empty()) {
    event->set_event_phase(Event::kBubblingPhase);
    for (Ancestors::iterator iter = ancestors.begin();
         iter != ancestors.end() && !event->propagation_stopped(); ++iter) {
      (*iter)->FireEventOnListeners(event);
    }
  }

  event->set_event_phase(Event::kNone);

  return !event->default_prevented();
}

// Return NULL if the node itself is Document.
//   http://www.w3.org/TR/2015/WD-dom-20150618/#dom-node-ownerdocument
scoped_refptr<Document> Node::owner_document() const {
  if (owner_document_ == this) {
    return NULL;
  }
  return owner_document_.get();
}

scoped_refptr<Element> Node::parent_element() const {
  return parent_ ? parent_->AsElement() : NULL;
}

bool Node::HasChildNodes() const { return first_child_ != NULL; }

scoped_refptr<NodeList> Node::child_nodes() const {
  return NodeList::CreateWithChildren(this);
}

// Algorithm for CloneNode:
//   http://www.w3.org/TR/2015/WD-dom-20150618/#dom-node-clonenode
scoped_refptr<Node> Node::CloneNode(bool deep) const {
  scoped_refptr<Node> new_node = Duplicate();
  if (deep) {
    scoped_refptr<Node> child = first_child_;
    while (child) {
      scoped_refptr<Node> new_child = child->CloneNode(true);
      new_node->AppendChild(new_child);
      child = child->next_sibling_;
    }
  }
  return new_node;
}

bool Node::Contains(const scoped_refptr<Node>& other_node) const {
  const Node* child = first_child_;
  while (child) {
    if (child == other_node || child->Contains(other_node)) {
      return true;
    }
    child = child->next_sibling_;
  }
  return false;
}

scoped_refptr<Node> Node::InsertBefore(
    const scoped_refptr<Node>& new_child,
    const scoped_refptr<Node>& reference_child) {
  if (!new_child) {
    // TODO(***REMOVED***): Throw JS ReferenceError.
    return NULL;
  }
  // Check if this node can accept new_child as a child.
  if (!CheckAcceptAsChild(new_child)) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return NULL;
  }
  if (reference_child && reference_child->parent_ != this) {
    // TODO(***REMOVED***): Throw JS NotFoundError.
    return NULL;
  }
  // Inserting before itself doesn't change anything.
  if (reference_child == new_child) {
    return new_child;
  }

  if (new_child->parent_) {
    new_child->parent_->RemoveChild(new_child);
  }
  new_child->parent_ = StaticAsWeakPtr<Node>(this);

  scoped_refptr<Node> next_sibling = reference_child;
  base::WeakPtr<Node> previous_sibling;

  if (next_sibling) {
    previous_sibling = next_sibling->previous_sibling_;
  } else {
    previous_sibling = last_child_;
  }

  if (previous_sibling) {
    previous_sibling->next_sibling_ = new_child;
  } else {
    first_child_ = new_child;
  }
  new_child->previous_sibling_ = previous_sibling;

  if (next_sibling) {
    next_sibling->previous_sibling_ = StaticAsWeakPtr<Node>(new_child);
  } else {
    last_child_ = StaticAsWeakPtr<Node>(new_child);
  }
  new_child->next_sibling_ = next_sibling;

  new_child->UpdateNodeGeneration();

  if (inserted_into_document_) {
    new_child->OnInsertedIntoDocument();
    owner_document_->OnDOMMutation();
  }

  return new_child;
}

scoped_refptr<Node> Node::AppendChild(const scoped_refptr<Node>& new_child) {
  return InsertBefore(new_child, NULL);
}

// Algorithm for ReplaceChild:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#concept-node-replace
scoped_refptr<Node> Node::ReplaceChild(const scoped_refptr<Node>& node,
                                       const scoped_refptr<Node>& child) {
  // Custom, not in any spec.
  if (!node || !child) {
    // TODO(***REMOVED***): Throw JS ReferenceError.
    return NULL;
  }
  if (child == node) {
    return node;
  }

  // 1. If parent is not a Document, DocumentFragment, or Element node,
  //    throw a "HierarchyRequestError".
  if (!child->parent_->IsDocument() && !child->parent_->IsElement()) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return NULL;
  }

  // 3. If child's parent is not parent, throw a "NotFoundError" exception.
  if (child->parent_ != this) {
    // TODO(***REMOVED***): Throw JS NotFoundError.
    return NULL;
  }

  // 4. If node is not a DocumentFragment, DocumentType, Element, Text,
  //    ProcessingInstruction, or Comment node, throw a "HierarchyRequestError".
  if (!node->IsElement() && !node->IsText() && !node->IsComment()) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return NULL;
  }

  // 5. If either node is a Text node and parent is a document, or node is a
  //    doctype and parent is not a document, throw a "HierarchyRequestError".
  if (node->IsText() && node->parent_->IsDocument()) {
    // TODO(***REMOVED***): Throw JS HierarchyRequestError.
    return NULL;
  }

  // 6. Not needed by Cobalt.

  // 7. Let reference child be child's next sibling.
  scoped_refptr<Node> reference_child = child->next_sibling_;

  // 8. If reference child is node, set it to node's next sibling.
  if (reference_child == node) reference_child = node->next_sibling_;

  // 9. Not needed by Cobalt.

  // 10. Remove child from its parent with the suppress observers flag set.
  RemoveChild(child);

  // 11. Insert node into parent before reference child with the suppress
  //     observers flag set.
  InsertBefore(node, reference_child);

  return child;
}

// Algorithm for RemoveChild:
//   http://www.w3.org/TR/2014/WD-dom-20140710/#concept-node-remove
scoped_refptr<Node> Node::RemoveChild(const scoped_refptr<Node>& node) {
  // Custom, not in any spec.
  if (!node) {
    // TODO(***REMOVED***): Throw JS ReferenceError.
    return NULL;
  }

  // Pre-remove 1. If child's parent is not parent, throw a "NotFoundError"
  //               exception.
  if (node->parent_ != this) {
    // TODO(***REMOVED***): Throw JS NotFoundError.
    return NULL;
  }

  // Custom, not in any spec.
  bool was_inserted_to_document = node->inserted_into_document_;
  if (was_inserted_to_document) {
    node->OnRemovedFromDocument();
  }
  node->UpdateNodeGeneration();

  // 2. ~ 7. Not needed by Cobalt.

  // 8. Remove node from its parent.
  if (node->previous_sibling_) {
    node->previous_sibling_->next_sibling_ = node->next_sibling_;
  } else {
    first_child_ = node->next_sibling_;
  }
  if (node->next_sibling_) {
    node->next_sibling_->previous_sibling_ = node->previous_sibling_;
  } else {
    last_child_ = node->previous_sibling_;
  }
  node->parent_.reset();
  node->previous_sibling_.reset();
  node->next_sibling_ = NULL;

  // Custom, not in any spec.
  if (was_inserted_to_document) {
    node->owner_document()->OnDOMMutation();
  }

  return node;
}

scoped_refptr<HTMLCollection> Node::children() {
  return HTMLCollection::CreateWithChildElements(this);
}

scoped_refptr<Element> Node::first_element_child() {
  Node* child = first_child();
  while (child) {
    if (child->IsElement()) {
      return child->AsElement();
    }
    child = child->next_sibling();
  }
  return NULL;
}

scoped_refptr<Element> Node::last_element_child() {
  Node* child = last_child();
  while (child) {
    if (child->IsElement()) {
      return child->AsElement();
    }
    child = child->previous_sibling();
  }
  return NULL;
}

unsigned int Node::child_element_count() {
  unsigned int num_elements = 0;
  const Node* child = first_child();
  while (child) {
    if (child->IsElement()) {
      ++num_elements;
    }
    child = child->next_sibling();
  }
  return num_elements;
}

scoped_refptr<Element> Node::previous_element_sibling() {
  Node* sibling = previous_sibling();
  while (sibling) {
    if (sibling->IsElement()) {
      return sibling->AsElement();
    }
    sibling = sibling->previous_sibling();
  }
  return NULL;
}

scoped_refptr<Element> Node::next_element_sibling() {
  Node* sibling = next_sibling();
  while (sibling) {
    if (sibling->IsElement()) {
      return sibling->AsElement();
    }
    sibling = sibling->next_sibling();
  }
  return NULL;
}

scoped_refptr<Comment> Node::AsComment() { return NULL; }

scoped_refptr<Document> Node::AsDocument() { return NULL; }

scoped_refptr<DocumentType> Node::AsDocumentType() { return NULL; }

scoped_refptr<Element> Node::AsElement() { return NULL; }

scoped_refptr<Text> Node::AsText() { return NULL; }

Node::Node(Document* document)
    : owner_document_(base::AsWeakPtr(document)),
      inserted_into_document_(false),
      node_generation_(kInitialNodeGeneration) {
  DCHECK(owner_document_);

  Stats::GetInstance()->Add(this);
}

Node::~Node() { Stats::GetInstance()->Remove(this); }

void Node::OnInsertedIntoDocument() {
  DCHECK(owner_document_);
  DCHECK(!inserted_into_document_);
  inserted_into_document_ = true;

  Node* child = first_child_;
  while (child) {
    child->OnInsertedIntoDocument();
    child = child->next_sibling_;
  }
}

void Node::OnRemovedFromDocument() {
  DCHECK(inserted_into_document_);
  inserted_into_document_ = false;

  Node* child = first_child_;
  while (child) {
    child->OnRemovedFromDocument();
    child = child->next_sibling_;
  }
}

bool Node::CheckAcceptAsChild(const scoped_refptr<Node>& child) const {
  UNREFERENCED_PARAMETER(child);
  return true;
}

void Node::UpdateNodeGeneration() {
  if (++node_generation_ == kInvalidNodeGeneration) {
    node_generation_ = kInitialNodeGeneration;
  }
  if (parent_) {
    parent_->UpdateNodeGeneration();
  }
}

scoped_refptr<Element> Node::QuerySelectorInternal(
    const std::string& selectors, cssom::CSSParser* css_parser) {
  DCHECK(css_parser);

  // Generate a rule with the given selectors and no style.
  scoped_refptr<cssom::CSSStyleRule> rule = css_parser->ParseStyleRule(
      selectors + " {}", base::SourceLocation("[object Element]", 1, 1));

  // Iterate through the descendants of the node and find the first matching
  // element if any.
  NodeDescendantsIterator iterator(this);
  Node* child = iterator.First();
  while (child) {
    if (child->IsElement()) {
      scoped_refptr<Element> element = child->AsElement();
      if (MatchRuleAndElement(rule, element)) {
        return element;
      }
    }
    child = iterator.Next();
  }
  return NULL;
}

}  // namespace dom
}  // namespace cobalt
