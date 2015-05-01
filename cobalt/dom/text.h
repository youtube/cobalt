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

#ifndef DOM_TEXT_H_
#define DOM_TEXT_H_

#include "base/string_piece.h"
#include "cobalt/dom/character_data.h"

namespace cobalt {
namespace dom {

// The Text interface represents the textual content of Element or Attr.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-text
class Text : public CharacterData {
 public:
  explicit Text(const base::StringPiece& text);

  // Web API: Node
  //
  const std::string& node_name() const OVERRIDE;
  NodeType node_type() const OVERRIDE { return Node::kTextNode; }

  // Custom, not in any spec.
  //
  bool IsText() const OVERRIDE { return true; }

  scoped_refptr<Text> AsText() OVERRIDE { return this; }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  void Accept(ConstNodeVisitor* visitor) const OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(Text);

 private:
  ~Text() OVERRIDE {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_TEXT_H_
