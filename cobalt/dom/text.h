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

#ifndef COBALT_DOM_TEXT_H_
#define COBALT_DOM_TEXT_H_

#include <string>

#include "base/string_piece.h"
#include "cobalt/dom/character_data.h"
#include "cobalt/dom/document.h"
#include "cobalt/script/environment_settings.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace dom {

// The Text interface represents the textual content of Element or Attr.
//   https://www.w3.org/TR/2014/WD-dom-20140710/#interface-text
class Text : public CharacterData {
 public:
  Text(script::EnvironmentSettings* env_settings,
       const base::StringPiece& comment);
  Text(Document* document, const base::StringPiece& text);

  // Web API: Node
  //
  base::Token node_name() const override;
  NodeType node_type() const override { return Node::kTextNode; }

  // Custom, not in any spec: Node.
  //
  Text* AsText() override { return this; }

  void Accept(NodeVisitor* visitor) override;
  void Accept(ConstNodeVisitor* visitor) const override;

  scoped_refptr<Node> Duplicate() const override {
    TRACK_MEMORY_SCOPE("DOM");
    return new Text(node_document(), data());
  }

  DEFINE_WRAPPABLE_TYPE(Text);

 protected:
  ~Text() override {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TEXT_H_
