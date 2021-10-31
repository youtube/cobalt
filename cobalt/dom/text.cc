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

#include "cobalt/dom/text.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

Text::Text(script::EnvironmentSettings* env_settings,
           const base::StringPiece& comment)
    : CharacterData(base::polymorphic_downcast<DOMSettings*>(env_settings)
                        ->window()
                        ->document()
                        .get(),
                    comment) {}

Text::Text(Document* document, const base::StringPiece& text)
    : CharacterData(document, text) {}

base::Token Text::node_name() const { return base::Tokens::text_node_name(); }

void Text::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void Text::Accept(ConstNodeVisitor* visitor) const { visitor->Visit(this); }

}  // namespace dom
}  // namespace cobalt
