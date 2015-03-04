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
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// The Text interface represents the textual content of Element or Attr.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-text
class Text : public Node {
 public:
  static scoped_refptr<Text> Create(const base::StringPiece& text);

  // Web API: Node
  //
  const std::string& node_name() const OVERRIDE;
  NodeType node_type() const OVERRIDE { return Node::kTextNode; }

  std::string text_content() const OVERRIDE { return text_; }
  void set_text_content(const std::string& text) OVERRIDE { text_ = text; }

  // Custom, not in any spec.
  //
  bool IsText() const OVERRIDE { return true; }

  scoped_refptr<Text> AsText() OVERRIDE { return this; }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  void Accept(ConstNodeVisitor* visitor) const OVERRIDE;

  // Unlike text_content() which returns a copy, returns a reference
  // to the underlying sequence of characters. This can save copying in layout
  // engine where text is broken into words. Should be used carefully
  // to avoid dangling string iterators.
  const std::string& text() const { return text_; }

 private:
  explicit Text(const base::StringPiece& text);
  ~Text() OVERRIDE {}

  bool CheckAcceptAsChild(const scoped_refptr<Node>& child) const OVERRIDE;

  std::string text_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_TEXT_H_
