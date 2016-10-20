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

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "cobalt/base/user_log.h"
#include "cobalt/cssom/css_rule_visitor.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/dom/cdata_section.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_type.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/html_collection.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/node_descendants_iterator.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/node_list_live.h"
#include "cobalt/dom/rule_matching.h"
#include "cobalt/dom/text.h"
#if defined(OS_STARBOARD)
#include "starboard/configuration.h"
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#define HANDLE_CORE_DUMP
#include "starboard/ps4/core_dump_handler.h"
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#endif  // defined(OS_STARBOARD)

namespace cobalt {
namespace dom {

namespace {

// This struct manages the user log information for Node count.
struct NodeCountLog {
 public:
  NodeCountLog() : count(0) {
    base::UserLog::Register(base::UserLog::kNodeCountIndex, "NodeCnt", &count,
                            sizeof(count));
#if defined(HANDLE_CORE_DUMP)
    SbCoreDumpRegisterHandler(CoreDumpHandler, this);
#endif
  }

  ~NodeCountLog() {
#if defined(HANDLE_CORE_DUMP)
    SbCoreDumpUnregisterHandler(CoreDumpHandler, this);
#endif
    base::UserLog::Deregister(base::UserLog::kNodeCountIndex);
  }

#if defined(HANDLE_CORE_DUMP)
  static void CoreDumpHandler(void* context) {
    SbCoreDumpLogInteger(
        "Total number of nodes",
        static_cast<NodeCountLog*>(context)->count);
  }
#endif

  int count;

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeCountLog);
};

base::LazyInstance<NodeCountLog> node_count_log = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Algorithm for DispatchEvent:
//   https://www.w3.org/TR/dom/#dispatching-events
bool Node::DispatchEvent(const scoped_refptr<Event>& event) {
  DCHECK(event);
  DCHECK(!event->IsBeingDispatched());
  DCHECK(event->initialized_flag());

  TRACE_EVENT1("cobalt::dom", "Node::DispatchEvent", "event",
               event->type().c_str());

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

// Algorithm for owner_document:
//   https://www.w3.org/TR/2015/WD-dom-20150618/#dom-node-ownerdocument
scoped_refptr<Document> Node::owner_document() const {
  // 1. If the context object is a document, return null.
  if (IsDocument()) {
    return NULL;
  }
  // 2. Return the node document.
  return node_document();
}

scoped_refptr<Element> Node::parent_element() const {
  return parent_ ? parent_->AsElement() : NULL;
}

bool Node::HasChildNodes() const { return first_child_ != NULL; }

scoped_refptr<NodeList> Node::child_nodes() const {
  return NodeListLive::CreateWithChildren(this);
}

// Algorithm for CloneNode:
//   https://www.w3.org/TR/2015/WD-dom-20150618/#dom-node-clonenode
scoped_refptr<Node> Node::CloneNode(bool deep) const {
  scoped_refptr<Node> new_node = Duplicate();
  DCHECK(new_node);
  if (deep) {
    scoped_refptr<Node> child = first_child_;
    while (child) {
      scoped_refptr<Node> new_child = child->CloneNode(true);
      DCHECK(new_child);
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

// Algorithm for InsertBefore:
//   https://www.w3.org/TR/dom/#dom-node-insertbefore
scoped_refptr<Node> Node::InsertBefore(
    const scoped_refptr<Node>& new_child,
    const scoped_refptr<Node>& reference_child) {
  // The insertBefore(node, child) method must return the result of
  // pre-inserting node into the context object before child.
  return PreInsert(new_child, reference_child);
}

// Algorithm for AppendChild:
//   https://www.w3.org/TR/dom/#dom-node-appendchild
scoped_refptr<Node> Node::AppendChild(const scoped_refptr<Node>& new_child) {
  // The appendChild(node) method must return the result of appending node to
  // the context object.
  // To append a node to a parent, pre-insert node into parent before null.
  return PreInsert(new_child, NULL);
}

// Algorithm for ReplaceChild:
//   https://www.w3.org/TR/dom/#dom-node-replacechild
scoped_refptr<Node> Node::ReplaceChild(const scoped_refptr<Node>& node,
                                       const scoped_refptr<Node>& child) {
  // The replaceChild(node, child) method must return the result of replacing
  // child with node within the context object.
  // To replace a child with node within a parent, run these steps:
  //   https://www.w3.org/TR/dom/#concept-node-replace

  // Custom, not in any spec.
  if (!node || !child) {
    // TODO: Throw JS ReferenceError.
    return NULL;
  }
  if (child == node) {
    return node;
  }

  // 1. If parent is not a Document, DocumentFragment, or Element node, throw a
  // "HierarchyRequestError".
  if (!IsDocument() && !IsElement()) {
    // TODO: Throw JS HierarchyRequestError.
    return NULL;
  }

  // 2. If node is a host-including inclusive ancestor of parent, throw a
  // "HierarchyRequestError".
  Node* ancestor = this;
  while (ancestor) {
    if (node == ancestor) {
      // TODO: Throw JS HierarchyRequestError.
      return NULL;
    }
    ancestor = ancestor->parent_;
  }

  // 3. If child's parent is not parent, throw a "NotFoundError" exception.
  if (child->parent_ != this) {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }

  // 4. If node is not a DocumentFragment, DocumentType, Element, Text,
  // ProcessingInstruction, or Comment node, throw a "HierarchyRequestError".
  // Note: Since we support CDATASection, it is also included here, so the only
  // type that is excluded is document.
  if (node->IsDocument()) {
    // TODO: Throw JS HierarchyRequestError.
    return NULL;
  }

  // 5. If either node is a Text node and parent is a document, or node is a
  // doctype and parent is not a document, throw a "HierarchyRequestError".
  if ((node->IsText() && IsDocument()) ||
      (node->IsDocumentType() && !IsDocument())) {
    // TODO: Throw JS HierarchyRequestError.
    return NULL;
  }

  // 6. Not needed by Cobalt.

  // 7. Let reference child be child's next sibling.
  scoped_refptr<Node> reference_child = child->next_sibling_;

  // 8. If reference child is node, set it to node's next sibling.
  if (reference_child == node) {
    reference_child = node->next_sibling_;
  }

  // 9. Adopt node into parent's node document.
  node->AdoptIntoDocument(node_document_);

  // 10. Remove child from its parent with the suppress observers flag set.
  Remove(child);

  // 11. Insert node into parent before reference child with the suppress
  // observers flag set.
  Insert(node, reference_child);

  return child;
}

// Algorithm for RemoveChild:
//   https://www.w3.org/TR/dom/#dom-node-removechild
scoped_refptr<Node> Node::RemoveChild(const scoped_refptr<Node>& node) {
  // The removeChild(child) method must return the result of pre-removing child
  // from the context object.
  return PreRemove(node);
}

scoped_refptr<HTMLCollection> Node::children() const {
  return HTMLCollection::CreateWithChildElements(this);
}

scoped_refptr<Element> Node::first_element_child() const {
  Node* child = first_child();
  while (child) {
    if (child->IsElement()) {
      return child->AsElement();
    }
    child = child->next_sibling();
  }
  return NULL;
}

scoped_refptr<Element> Node::last_element_child() const {
  Node* child = last_child();
  while (child) {
    if (child->IsElement()) {
      return child->AsElement();
    }
    child = child->previous_sibling();
  }
  return NULL;
}

unsigned int Node::child_element_count() const {
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

scoped_refptr<Element> Node::QuerySelector(const std::string& selectors) {
  return dom::QuerySelector(
      this, selectors, node_document_->html_element_context()->css_parser());
}

scoped_refptr<NodeList> Node::QuerySelectorAll(const std::string& selectors) {
  return dom::QuerySelectorAll(
      this, selectors, node_document_->html_element_context()->css_parser());
}

scoped_refptr<Element> Node::previous_element_sibling() const {
  Node* sibling = previous_sibling();
  while (sibling) {
    if (sibling->IsElement()) {
      return sibling->AsElement();
    }
    sibling = sibling->previous_sibling();
  }
  return NULL;
}

scoped_refptr<Element> Node::next_element_sibling() const {
  Node* sibling = next_sibling();
  while (sibling) {
    if (sibling->IsElement()) {
      return sibling->AsElement();
    }
    sibling = sibling->next_sibling();
  }
  return NULL;
}

// Algorithm for AdoptIntoDocument:
//   https://www.w3.org/TR/dom/#concept-node-adopt
void Node::AdoptIntoDocument(Document* document) {
  DCHECK(!IsDocument());
  if (!document) {
    return;
  }

  // 1, Not needed by Cobalt.

  // 2. If node's parent is not null, remove node from its parent.
  if (parent_) {
    parent_->RemoveChild(this);
  }

  // 3. Set node's inclusive descendants's node document to document.
  node_document_ = base::AsWeakPtr(document);
  NodeDescendantsIterator it(this);
  Node* descendant = it.First();
  while (descendant) {
    descendant->node_document_ = base::AsWeakPtr(document);
    descendant = it.Next();
  }

  // 4. Not needed by Cobalt.
}

scoped_refptr<Node> Node::GetRootNode() {
  Node* root = this;
  while (root->parent_node()) {
    root = root->parent_node();
  }
  return make_scoped_refptr(root);
}

scoped_refptr<CDATASection> Node::AsCDATASection() { return NULL; }

scoped_refptr<Comment> Node::AsComment() { return NULL; }

scoped_refptr<Document> Node::AsDocument() { return NULL; }

scoped_refptr<DocumentType> Node::AsDocumentType() { return NULL; }

scoped_refptr<Element> Node::AsElement() { return NULL; }

scoped_refptr<Text> Node::AsText() { return NULL; }

Node::Node(Document* document)
    : node_document_(base::AsWeakPtr(document)),
      parent_(NULL),
      previous_sibling_(NULL),
      last_child_(NULL),
      inserted_into_document_(false),
      node_generation_(kInitialNodeGeneration) {
  DCHECK(node_document_);
  ++(node_count_log.Get().count);
  GlobalStats::GetInstance()->Add(this);
}

Node::~Node() {
  Node* node = last_child_;
  while (node) {
    node->parent_ = NULL;
    node->next_sibling_ = NULL;
    node = node->previous_sibling_;
  }
  --(node_count_log.Get().count);
  GlobalStats::GetInstance()->Remove(this);
}

void Node::OnInsertedIntoDocument() {
  DCHECK(node_document_);
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

void Node::InvalidateComputedStylesRecursively() {
  Node* child = first_child_;
  while (child) {
    child->InvalidateComputedStylesRecursively();
    child = child->next_sibling_;
  }
}

void Node::InvalidateLayoutBoxesFromNodeAndAncestors() {
  if (parent_) {
    parent_->InvalidateLayoutBoxesFromNodeAndAncestors();
  }
}

void Node::InvalidateLayoutBoxesFromNodeAndDescendants() {
  Node* child = first_child_;
  while (child) {
    child->InvalidateLayoutBoxesFromNodeAndDescendants();
    child = child->next_sibling_;
  }
}

void Node::UpdateGenerationForNodeAndAncestors() {
  if (++node_generation_ == kInvalidNodeGeneration) {
    node_generation_ = kInitialNodeGeneration;
  }
  if (parent_) {
    parent_->UpdateGenerationForNodeAndAncestors();
  }
}

// Algorithm for EnsurePreInsertionValidity:
//   https://www.w3.org/TR/dom/#concept-node-ensure-pre-insertion-validity
bool Node::EnsurePreInsertionValidity(const scoped_refptr<Node>& node,
                                      const scoped_refptr<Node>& child) {
  if (!node) {
    return false;
  }

  // 1. If parent is not a Document, DocumentFragment, or Element node, throw a
  // "HierarchyRequestError".
  if (!IsDocument() && !IsElement()) {
    // TODO: Throw JS HierarchyRequestError.
    return false;
  }

  // 2. If node is a host-including inclusive ancestor of parent, throw a
  // "HierarchyRequestError".
  Node* ancestor = this;
  while (ancestor) {
    if (node == ancestor) {
      // TODO: Throw JS HierarchyRequestError.
      return false;
    }
    ancestor = ancestor->parent_;
  }

  // 3. If child is not null and its parent is not parent, throw a
  // "NotFoundError" exception.
  if (child && child->parent_ != this) {
    // TODO: Throw JS NotFoundError.
    return false;
  }

  // 4. If node is not a DocumentFragment, DocumentType, Element, Text,
  // ProcessingInstruction, or Comment node, throw a "HierarchyRequestError".
  // Note: Since we support CDATASection, it is also included here, so the only
  // type that is excluded is document.
  if (node->IsDocument()) {
    // TODO: Throw JS HierarchyRequestError.
    return false;
  }

  // 5. If either node is a Text node and parent is a document, or node is a
  // doctype and parent is not a document, throw a "HierarchyRequestError".
  if ((node->IsText() && IsDocument()) ||
      (node->IsDocumentType() && !IsDocument())) {
    // TODO: Throw JS HierarchyRequestError.
    return false;
  }

  // 6. Not needed by Cobalt.

  return true;
}

// Algorithm for PreInsert:
//   https://www.w3.org/TR/dom/#concept-node-pre-insert
scoped_refptr<Node> Node::PreInsert(const scoped_refptr<Node>& node,
                                    const scoped_refptr<Node>& child) {
  // 1. Ensure pre-insertion validity of node into parent before child.
  if (!EnsurePreInsertionValidity(node, child)) {
    return NULL;
  }

  // 2. Let reference child be child.
  // 3. If reference child is node, set it to node's next sibling.
  // 4. Adopt node into parent's node document.
  // 5. Insert node into parent before reference child.
  node->AdoptIntoDocument(node_document_);
  Insert(node, child == node ? child->next_sibling_ : child);

  // 6. Return node.
  return node;
}

// Algorithm for Insert:
//   https://www.w3.org/TR/dom/#concept-node-insert
void Node::Insert(const scoped_refptr<Node>& node,
                  const scoped_refptr<Node>& child) {
  // 1. 2. Not needed by Cobalt.
  // 3. Let nodes be node's children if node is a DocumentFragment node, and a
  // list containing solely node otherwise.
  // 4. ~ 6. Not needed by Cobalt.
  // 7. For each newNode in nodes, in tree order, run these substeps:
  //   1. Insert newNode into parent before child or at the end of parent if
  //   child is null.
  //   2. Run the insertion steps with newNode.

  node->parent_ = this;

  scoped_refptr<Node> next_sibling = child;
  Node* previous_sibling =
      next_sibling ? next_sibling->previous_sibling_ : last_child_;

  if (previous_sibling) {
    previous_sibling->next_sibling_ = node;
  } else {
    first_child_ = node;
  }
  node->previous_sibling_ = previous_sibling;

  if (next_sibling) {
    next_sibling->previous_sibling_ = node;
  } else {
    last_child_ = node;
  }
  node->next_sibling_ = next_sibling;

  // Custom, not in any spec.

  OnMutation();
  node->UpdateGenerationForNodeAndAncestors();

  // Invalidate the layout boxes of the new parent as a result of its children
  // being changed.
  // NOTE: The added node does not have any invalidations done, because they
  // occur on the remove and are guaranteed to not be needed at this point.
  InvalidateLayoutBoxesFromNodeAndAncestors();

  if (inserted_into_document_) {
    node->OnInsertedIntoDocument();
    Document* document = node_document();
    if (document) {
      document->OnDOMMutation();
    }
  }
}

// Algorithm for PreRemove:
//   https://www.w3.org/TR/dom/#concept-node-pre-remove
scoped_refptr<Node> Node::PreRemove(const scoped_refptr<Node>& child) {
  // 1. If child's parent is not parent, throw a "NotFoundError" exception.
  if (!child || child->parent_ != this) {
    // TODO: Throw JS NotFoundError.
    return NULL;
  }

  // 2. Remove child from parent.
  Remove(child);

  // 3. Return child.
  return child;
}

// Algorithm for Remove:
//   https://www.w3.org/TR/dom/#concept-node-remove
void Node::Remove(const scoped_refptr<Node>& node) {
  DCHECK(node);

  OnMutation();
  node->UpdateGenerationForNodeAndAncestors();

  // Invalidate the layout boxes of the previous parent as a result of its
  // children being changed.
  InvalidateLayoutBoxesFromNodeAndAncestors();
  // Invalidate the styles and layout boxes of the node being removed from
  // the tree. These are no longer valid as a result of the child and its
  // descendants losing their inherited styles.
  node->InvalidateComputedStylesRecursively();
  node->InvalidateLayoutBoxesFromNodeAndDescendants();

  bool was_inserted_to_document = node->inserted_into_document_;
  if (was_inserted_to_document) {
    node->OnRemovedFromDocument();
  }

  // 1. ~ 8. Not needed by Cobalt.
  // 9. Remove node from its parent.
  // 10. Run the removing steps with node, parent, and oldPreviousSibling.

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
  node->parent_ = NULL;
  node->previous_sibling_ = NULL;
  node->next_sibling_ = NULL;

  // Custom, not in any spec.

  if (was_inserted_to_document) {
    scoped_refptr<Document> document = node->owner_document();
    if (document) {
      document->OnDOMMutation();
    }
  }
}

}  // namespace dom
}  // namespace cobalt
