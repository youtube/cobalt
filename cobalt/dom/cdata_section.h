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

#ifndef COBALT_DOM_CDATA_SECTION_H_
#define COBALT_DOM_CDATA_SECTION_H_

#include <string>

#include "base/string_piece.h"
#include "cobalt/dom/text.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

class Document;

// CDATA sections are used to escape blocks of text containing characters that
// would otherwise be regarded as markup.
//   https://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html#ID-667469212
class CDATASection : public Text {
 public:
  CDATASection(script::EnvironmentSettings* env_settings,
               const base::StringPiece& cdata_section);
  CDATASection(Document* document, const base::StringPiece& cdata_section);

  // Web API: Node
  //
  base::Token node_name() const override;
  NodeType node_type() const override { return Node::kCdataSectionNode; }

  // Custom, not in any spec: Node.
  //
  CDATASection* AsCDATASection() override { return this; }

  void Accept(NodeVisitor* visitor) override;
  void Accept(ConstNodeVisitor* visitor) const override;

  scoped_refptr<Node> Duplicate() const override;

  DEFINE_WRAPPABLE_TYPE(CDATASection);

 private:
  ~CDATASection() override {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CDATA_SECTION_H_
