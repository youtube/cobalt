// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/comment.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

Comment::Comment(script::EnvironmentSettings* env_settings,
                 const base::StringPiece& comment)
    : CharacterData(base::polymorphic_downcast<DOMSettings*>(env_settings)
                        ->window()
                        ->document()
                        .get(),
                    comment) {}

Comment::Comment(Document* document, const base::StringPiece& comment)
    : CharacterData(document, comment) {}

base::Token Comment::node_name() const {
  return base::Tokens::comment_node_name();
}

void Comment::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Comment::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

scoped_refptr<Node> Comment::Duplicate() const {
  return new Comment(node_document(), data());
}

}  // namespace dom
}  // namespace cobalt
