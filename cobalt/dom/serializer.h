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

#ifndef COBALT_DOM_SERIALIZER_H_
#define COBALT_DOM_SERIALIZER_H_

#include <iosfwd>

#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// This class is responsible for serializing nodes into the provided output
// stream. Note that in the serialization result, the attributes are sorted
// alphabetically, each tag has both opening and closing tag.
class Serializer : public ConstNodeVisitor {
 public:
  explicit Serializer(std::ostream* out_stream);

  // Serializes the provided node and its descendants.
  void Serialize(const scoped_refptr<const Node>& node);
  // Serializes the provided node, excluding the descendants.
  void SerializeSelfOnly(const scoped_refptr<const Node>& node);
  // Serializes the descendants of the provided node, excluding the node itself.
  void SerializeDescendantsOnly(const scoped_refptr<const Node>& node);

 private:
  // From ConstNodeVisitor.
  void Visit(const CDATASection* cdata_section) override;
  void Visit(const Comment* comment) override;
  void Visit(const Document* document) override;
  void Visit(const DocumentType* document_type) override;
  void Visit(const Element* element) override;
  void Visit(const Text* text) override;

  // Pointer to the output stream provided by the user.
  std::ostream* out_stream_;
  // Used by the Visit methods. True when Visit is called when entering a Node,
  // false when leaving.
  bool entering_node_;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SERIALIZER_H_
