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

#ifndef COBALT_DOM_NODE_H_
#define COBALT_DOM_NODE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/mutation_observer.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/dom/registered_observer_list.h"

namespace cobalt {
namespace dom {

class CDATASection;
class Comment;
class Document;
class DocumentType;
class Element;
class HTMLCollection;
class NodeList;
class Text;

// Used to implement type-safe visiting via double-dispatch on a class
// hierarchy of Node objects.
class NodeVisitor {
 public:
  virtual void Visit(CDATASection* cdata_section) = 0;
  virtual void Visit(Comment* comment) = 0;
  virtual void Visit(Document* document) = 0;
  virtual void Visit(DocumentType* document_type) = 0;
  virtual void Visit(Element* element) = 0;
  virtual void Visit(Text* text) = 0;

 protected:
  ~NodeVisitor() {}
};

class ConstNodeVisitor {
 public:
  virtual void Visit(const CDATASection* cdata_section) = 0;
  virtual void Visit(const Comment* comment) = 0;
  virtual void Visit(const Document* document) = 0;
  virtual void Visit(const DocumentType* document_type) = 0;
  virtual void Visit(const Element* element) = 0;
  virtual void Visit(const Text* text) = 0;

 protected:
  ~ConstNodeVisitor() {}
};

// A Node is an interface from which a number of DOM types inherit, and allows
// these various types to be treated (or tested) similarly.
//   https://www.w3.org/TR/dom/#interface-node
//
// Memory management:
// ------------------
// All Node types are reference counted and support weak pointers.
// While this should be enough to prevent memory leaks in most situations,
// there are certain conditions where a memory leak can occur. A notable
// example is a circular reference between DOM and JavaScript:
//   http://www.javascriptkit.com/javatutors/closuresleak/index3.shtml
//
// Using strong shared references, each node owns its first child and next
// sibling. The references to parent and previous sibling are weak. In addition,
// a JavaScript wrapper, which is a JavaScript reference for a node, will also
// have a strong shared reference to the node, and the bindings layer ensures
// that a JavaScript wrapper exists for all ancestors of the node.
// As a result, any JavaScript reference to a node will keep the whole DOM tree
// that the node belongs to alive, which is a conforming behavior.
//
class Node : public EventTarget {
 public:
  // Web API: Node
  // NodeType values as defined by Web API Node.nodeType.
  // Work around lack of strongly-typed enums in C++03.
  typedef uint16 NodeType;
  // A name that is given only to pacify the compiler. Use |NodeType| instead.
  enum NodeTypeInternal : uint16 {
    kElementNode = 1,
    kTextNode = 3,
    kCdataSectionNode = 4,
    kCommentNode = 8,
    kDocumentNode = 9,
    kDocumentTypeNode = 10,
  };

  // Custom, not in any spec.
  // Node generation constants.
  enum NodeGeneration {
    kInvalidNodeGeneration = 0,
    kInitialNodeGeneration = 1,
  };

  // Web API: EventTarget
  //
  bool DispatchEvent(const scoped_refptr<Event>& event) override;

  // Web API: Node
  //
  virtual NodeType node_type() const = 0;
  virtual base::Token node_name() const = 0;

  Document* owner_document() const;
  Node* parent_node() const { return parent_; }
  Element* parent_element() const;
  bool HasChildNodes() const;
  scoped_refptr<NodeList> child_nodes() const;
  Node* first_child() const { return first_child_; }
  Node* last_child() const { return last_child_; }
  Node* next_sibling() const { return next_sibling_; }
  Node* previous_sibling() const { return previous_sibling_; }

  virtual base::optional<std::string> node_value() const {
    return base::nullopt;
  }
  virtual void set_node_value(
      const base::optional<std::string>& /* node_value */) {}

  virtual base::optional<std::string> text_content() const {
    return base::nullopt;
  }
  virtual void set_text_content(
      const base::optional<std::string>& /* text_content */) {}

  scoped_refptr<Node> CloneNode(bool deep) const;

  bool Contains(const scoped_refptr<Node>& other_name) const;

  scoped_refptr<Node> InsertBefore(const scoped_refptr<Node>& new_child,
                                   const scoped_refptr<Node>& reference_child);
  scoped_refptr<Node> AppendChild(const scoped_refptr<Node>& new_child);
  scoped_refptr<Node> ReplaceChild(const scoped_refptr<Node>& node,
                                   const scoped_refptr<Node>& child);
  scoped_refptr<Node> RemoveChild(const scoped_refptr<Node>& node);

  // Web API: ParentNode (implements)
  // The ParentNode interface contains methods that are particular to Node
  // objects that can have children.
  //   https://www.w3.org/TR/dom/#parentnode
  //
  scoped_refptr<HTMLCollection> children();
  Element* first_element_child() const;
  Element* last_element_child() const;
  unsigned int child_element_count() const;

  scoped_refptr<Element> QuerySelector(const std::string& selectors);
  scoped_refptr<NodeList> QuerySelectorAll(const std::string& selectors);

  // Web API: NonDocumentTypeChildNode (implements)
  // The NonDocumentTypeChildNode interface contains methods that are particular
  // to Node objects that can have a parent.
  //   https://www.w3.org/TR/2014/WD-dom-20140710/#interface-nondocumenttypechildnode
  Element* previous_element_sibling() const;
  Element* next_element_sibling() const;

  // From the spec: Node.
  //
  // A node's node document can be changed by the adopt algorithm.
  //   https://www.w3.org/TR/dom/#concept-node-adopt
  void AdoptIntoDocument(Document* document);

  // Each node has an associated node document, set upon creation, that is a
  // document.
  //   https://www.w3.org/TR/dom/#concept-node-document
  // Returns the node document if it still exists, NULL if not.
  Document* node_document() const { return node_document_.get(); }

  // Custom, not in any spec.
  //
  virtual bool HasAttributes() const { return false; }

  // Returns the root Node of the tree this node belongs to. If this node is the
  // root, it will return this Node.
  Node* GetRootNode();

  bool IsCDATASection() const { return node_type() == kCdataSectionNode; }
  bool IsComment() const { return node_type() == kCommentNode; }
  bool IsDocument() const { return node_type() == kDocumentNode; }
  bool IsDocumentType() const { return node_type() == kDocumentTypeNode; }
  bool IsElement() const { return node_type() == kElementNode; }
  bool IsText() const { return node_type() == kTextNode; }

  // Safe type conversion methods that will downcast to the required type if
  // possible or return NULL otherwise.
  virtual CDATASection* AsCDATASection();
  virtual Comment* AsComment();
  virtual Document* AsDocument();
  virtual DocumentType* AsDocumentType();
  virtual Element* AsElement();
  virtual Text* AsText();

  // Node generation counter that will be modified for every content change
  // that affects the topology of the subtree defined by this node.
  // The returned node generation will be never equal to kInvalidNodeGeneration.
  uint32_t node_generation() const { return node_generation_; }

  // Children classes implement this method to support type-safe visiting via
  // double dispatch.
  virtual void Accept(NodeVisitor* visitor) = 0;
  virtual void Accept(ConstNodeVisitor* visitor) const = 0;

  virtual scoped_refptr<Node> Duplicate() const = 0;

  // Purges all cached resource reference from the current node and all
  // descendents.
  void PurgeCachedResourceReferencesRecursively();

  bool RegisterMutationObserver(const scoped_refptr<MutationObserver>& observer,
                                const MutationObserverInit& options) {
    return registered_observers_.AddMutationObserver(observer, options);
  }
  void UnregisterMutationObserver(
      const scoped_refptr<MutationObserver>& observer) {
    registered_observers_.RemoveMutationObserver(observer);
  }

  DEFINE_WRAPPABLE_TYPE(Node);
  void TraceMembers(script::Tracer* tracer) override;

 protected:
  explicit Node(Document* document);
  virtual ~Node();

  // Called to notify that the node along with all its descendants has been
  // inserted to its owner document.
  virtual void OnInsertedIntoDocument();
  // Called to notify that the node along with all its descendants has been
  // removed from to its owner document.
  virtual void OnRemovedFromDocument();

  virtual bool IsInDocument() const { return inserted_into_document_; }

  virtual void MarkDisplayNoneOnNodeAndDescendants();
  virtual void PurgeCachedBackgroundImagesOfNodeAndDescendants();
  virtual void InvalidateComputedStylesOfNodeAndDescendants();
  virtual void InvalidateLayoutBoxesOfNodeAndAncestors();
  virtual void InvalidateLayoutBoxesOfNodeAndDescendants();

  virtual void InvalidateLayoutBoxSizes() {}
  virtual void InvalidateLayoutBoxCrossReferences() {}
  virtual void InvalidateLayoutBoxRenderTreeNodes() {}

  void MarkDisplayNoneOnDescendants();
  void PurgeCachedBackgroundImagesOfDescendants();
  void InvalidateComputedStylesOfDescendants();
  void InvalidateLayoutBoxesOfAncestors();
  void InvalidateLayoutBoxesOfDescendants();

  // Triggers a generation update in this node and all its ancestor nodes.
  void UpdateGenerationForNodeAndAncestors();

  // Gather a list of RegisteredObservers on this node and its ancestors.
  typedef std::vector<RegisteredObserver> RegisteredObserverVector;
  scoped_ptr<RegisteredObserverVector> GatherInclusiveAncestorsObservers();

  void ReplaceAll(const scoped_refptr<Node>& node);

 private:
  // From EventTarget.
  std::string GetDebugName() override { return node_name().c_str(); }

  // From the spec: Node.
  // Mutation algorithms.
  //   https://www.w3.org/TR/dom/#mutation-algorithms
  //
  bool EnsurePreInsertionValidity(const scoped_refptr<Node>& node,
                                  const scoped_refptr<Node>& child);

  scoped_refptr<Node> PreInsert(const scoped_refptr<Node>& node,
                                const scoped_refptr<Node>& child);
  void Insert(const scoped_refptr<Node>& node, const scoped_refptr<Node>& child,
              bool suppress_observers);

  scoped_refptr<Node> PreRemove(const scoped_refptr<Node>& child);
  void Remove(const scoped_refptr<Node>& node, bool suppress_observers);

  // Called everytime mutation happens, i.e. when a child is inserted or removed
  // from this node.
  virtual void OnMutation() {}

  // Weak reference to the node document.
  base::WeakPtr<Document> node_document_;
  // Weak references to parent, previous sibling and last child.
  Node* parent_;
  Node* previous_sibling_;
  Node* last_child_;
  // Whether the node has been inserted into its node document.
  bool inserted_into_document_;
  // Node generation counter.
  uint32_t node_generation_;

  // Strong references to first child and next sibling.
  scoped_refptr<Node> first_child_;
  scoped_refptr<Node> next_sibling_;

  RegisteredObserverList registered_observers_;

  scoped_refptr<HTMLCollection> children_collection_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NODE_H_
