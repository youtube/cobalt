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

#ifndef DOM_COMMENT_H_
#define DOM_COMMENT_H_

#include "base/string_piece.h"
#include "cobalt/dom/node.h"

namespace cobalt {
namespace dom {

// The Comment interface represents textual notations within markup; although
// it is generally not visually shown, such comments can be still retrieved
// from the document.
//   http://www.w3.org/TR/2014/WD-dom-20140710/#interface-comment
class Comment : public Node {
 public:
  static scoped_refptr<Comment> Create(const base::StringPiece& comment);

  // Web API: Node
  //
  const std::string& node_name() const OVERRIDE;
  NodeType node_type() const OVERRIDE { return Node::kCommentNode; }

  std::string text_content() const OVERRIDE { return comment_; }
  void set_text_content(const std::string& value) OVERRIDE { comment_ = value; }

  // Custom, not in any spec.
  //
  bool IsComment() const OVERRIDE { return true; }

  scoped_refptr<Comment> AsComment() OVERRIDE { return this; }

  void Accept(NodeVisitor* visitor) OVERRIDE;
  void Accept(ConstNodeVisitor* visitor) const OVERRIDE;

 private:
  explicit Comment(const base::StringPiece& comment);
  ~Comment() OVERRIDE {}

  bool CheckAcceptAsChild(const scoped_refptr<Node>& child) const OVERRIDE;

  std::string comment_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_COMMENT_H_
