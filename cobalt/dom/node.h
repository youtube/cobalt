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

#ifndef DOM_NODE_H_
#define DOM_NODE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/event_target.h"

namespace cobalt {
namespace dom {

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
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-node
//
// Memory management:
// ------------------
// All Node types are reference counted and support weak pointers.
// While this should be enough to prevent memory leaks in most situations,
// there are certain conditions where a memory leak can occur. A notable
// example is a circular reference between DOM and JavaScript:
//   http://www.javascriptkit.com/javatutors/closuresleak/index3.shtml
//
// DOM requires each Node to store references to its parent, first and last
// children, and previous and next siblings. To conserve memory, these
// references are also used to enforce the life-time of Node objects.
// Using strong references, each parent owns its first sibling, and each
// sibling owns its next sibling. All other references are weak.
//
// Under certain rare conditions, this may cause a behavior that differs from
// a normal browser behavior. For example:
//   <body>
//     <div id="a"><div id="b"></div></div>
//     <script>
//       (function() {
//         var b = document.querySelector('#b');
//         document.body.removeChild(document.body.firstElementChild);
//         window.setTimeout(function() {
//           // Chrome: <div id="a"/>
//           // Cobalt: null
//           console.log(b.parentNode);
//           // Both: null
//           console.log(b.parentNode.parentNode);
//         }, 5000);
//       })();
//     </script>
//   </body>
//
// If required, these limitation can be fixed by using a garbage collection
// scheme similar to Oilpan:
//   http://www.chromium.org/blink/blink-gc
class Node : public EventTarget {
 public:
  // Web API: Node
  // NodeType values as defined by Web API Node.nodeType.
  enum NodeType {
    kElementNode = 1,
    kTextNode = 3,
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
  bool DispatchEvent(const scoped_refptr<Event>& event) OVERRIDE;

  // Web API: Node
  //
  virtual NodeType node_type() const = 0;
  virtual std::string node_name() const = 0;

  scoped_refptr<Document> owner_document() const;
  scoped_refptr<Node> parent_node() const { return parent_.get(); }
  scoped_refptr<Element> parent_element() const;
  bool HasChildNodes() const;
  scoped_refptr<NodeList> child_nodes() const;
  scoped_refptr<Node> first_child() const { return first_child_; }
  scoped_refptr<Node> last_child() const { return last_child_.get(); }
  scoped_refptr<Node> next_sibling() const { return next_sibling_; }
  scoped_refptr<Node> previous_sibling() const {
    return previous_sibling_.get();
  }

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

  virtual scoped_refptr<Node> InsertBefore(
      const scoped_refptr<Node>& new_child,
      const scoped_refptr<Node>& reference_child);
  scoped_refptr<Node> AppendChild(const scoped_refptr<Node>& new_child);
  scoped_refptr<Node> ReplaceChild(const scoped_refptr<Node>& node,
                                   const scoped_refptr<Node>& child);
  scoped_refptr<Node> RemoveChild(const scoped_refptr<Node>& node);

  // Web API: ParentNode (implements)
  // The ParentNode interface contains methods that are particular to Node
  // objects that can have children.
  //   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-parentnode
  scoped_refptr<HTMLCollection> children();
  scoped_refptr<Element> first_element_child();
  scoped_refptr<Element> last_element_child();
  unsigned int child_element_count();

  // Web API: NonDocumentTypeChildNode (implements)
  // The NonDocumentTypeChildNode interface contains methods that are particular
  // to Node objects that can have a parent.
  //   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-nondocumenttypechildnode
  scoped_refptr<Element> previous_element_sibling();
  scoped_refptr<Element> next_element_sibling();

  // Custom, not in any spec.
  //
  virtual bool HasAttributes() const { return false; }

  virtual bool IsComment() const { return false; }
  virtual bool IsDocument() const { return false; }
  virtual bool IsDocumentType() const { return false; }
  virtual bool IsElement() const { return false; }
  virtual bool IsText() const { return false; }

  // Safe type conversion methods that will downcast to the required type if
  // possible or return NULL otherwise.
  virtual scoped_refptr<Comment> AsComment();
  virtual scoped_refptr<Document> AsDocument();
  virtual scoped_refptr<DocumentType> AsDocumentType();
  virtual scoped_refptr<Element> AsElement();
  virtual scoped_refptr<Text> AsText();

  // Node generation counter that will be modified for every content change
  // that affects the topology of the subtree defined by this node.
  // The returned node generation will be never equal to kInvalidNodeGeneration.
  uint32_t node_generation() const { return node_generation_; }

  // Children classes implement this method to support type-safe visiting via
  // double dispatch.
  virtual void Accept(NodeVisitor* visitor) = 0;
  virtual void Accept(ConstNodeVisitor* visitor) const = 0;

  virtual scoped_refptr<Node> Duplicate() const = 0;

  DEFINE_WRAPPABLE_TYPE(Node);

 protected:
  explicit Node(Document* document);
  virtual ~Node();

  // Called to notify that the node along with all its descendants has been
  // inserted to its owner document.
  virtual void OnInsertedIntoDocument();
  // Called to notify that the node along with all its descendants has been
  // removed from to its owner document.
  virtual void OnRemovedFromDocument();

  // Derived class can override this method to prevent certain children types
  // from being appended.
  virtual bool CheckAcceptAsChild(const scoped_refptr<Node>& child) const;

  // Triggers a generation update in this node and all its ancestor nodes.
  void UpdateNodeGeneration();

  // Implementations of QuerySelector and QuerySelectorAll on Document and
  // Element.
  scoped_refptr<Element> QuerySelectorInternal(const std::string& selectors,
                                               cssom::CSSParser* css_parser);
  scoped_refptr<NodeList> QuerySelectorAllInternal(
      const std::string& selectors, cssom::CSSParser* css_parser);

 private:
  // Weak reference to the parent node.
  base::WeakPtr<Node> parent_;
  // Strong reference that owns the next sibling node.
  scoped_refptr<Node> next_sibling_;
  // Weak reference to the previous sibling node.
  base::WeakPtr<Node> previous_sibling_;
  // Strong reference that owns the first child in the subtree defined by
  // this node.
  scoped_refptr<Node> first_child_;
  // Weak reference to the last child in the subtree defined by this node.
  base::WeakPtr<Node> last_child_;
  // Weak reference to the containing document.
  base::WeakPtr<Document> owner_document_;
  // Its value is true if the node is currently inserted into its owner
  // document.  Note that a node always has an owner document after it is
  // created but it may not be attached to the document until functions like
  // AppendChild() is called.
  bool inserted_into_document_;
  // Node generation counter.
  uint32_t node_generation_;

  friend class Document;
  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_NODE_H_
