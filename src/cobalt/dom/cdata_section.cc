// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/cdata_section.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace dom {

CDATASection::CDATASection(script::EnvironmentSettings* env_settings,
                           const base::StringPiece& cdata_section)
    : Text(env_settings, cdata_section) {}

CDATASection::CDATASection(Document* document,
                           const base::StringPiece& cdata_section)
    : Text(document, cdata_section) {}

base::Token CDATASection::node_name() const {
  return base::Tokens::cdata_section_node_name();
}

void CDATASection::Accept(NodeVisitor* visitor) { visitor->Visit(this); }

void CDATASection::Accept(ConstNodeVisitor* visitor) const {
  visitor->Visit(this);
}

scoped_refptr<Node> CDATASection::Duplicate() const {
  return new CDATASection(node_document(), data());
}

}  // namespace dom
}  // namespace cobalt
