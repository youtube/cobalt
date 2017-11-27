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

#ifndef COBALT_DOM_DOCUMENT_TYPE_H_
#define COBALT_DOM_DOCUMENT_TYPE_H_

#include <string>

#include "cobalt/dom/document.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// The DocumentType interface represents a Node containing a doctype.
//   https://www.w3.org/TR/2015/WD-dom-20150618/#documenttype
class DocumentType : public Node {
 public:
  DocumentType(Document* document, base::Token name)
      : Node(document), name_(name) {}
  DocumentType(Document* document, base::Token name,
               const std::string& public_id, const std::string& system_id)
      : Node(document),
        name_(name),
        public_id_(public_id),
        system_id_(system_id) {}

  // WebAPI: Node
  NodeType node_type() const override { return Node::kDocumentTypeNode; }
  base::Token node_name() const override { return name_; }

  // WebAPI: DocumentType
  base::Token name() const { return name_; }
  const std::string& public_id() const { return public_id_; }
  const std::string& system_id() const { return system_id_; }

  // Custom, not in any spec: Node.
  //
  DocumentType* AsDocumentType() override { return this; }

  void Accept(NodeVisitor* visitor) override;
  void Accept(ConstNodeVisitor* visitor) const override;

  scoped_refptr<Node> Duplicate() const override {
    return new DocumentType(node_document(), name_, public_id_, system_id_);
  }

  DEFINE_WRAPPABLE_TYPE(DocumentType);

 private:
  ~DocumentType() override {}

  base::Token name_;
  std::string public_id_;
  std::string system_id_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOCUMENT_TYPE_H_
