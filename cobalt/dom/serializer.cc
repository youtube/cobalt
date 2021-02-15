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

#include "cobalt/dom/serializer.h"

#include <map>
#include <string>

#include "base/strings/stringprintf.h"
#include "cobalt/dom/attr.h"
#include "cobalt/dom/cdata_section.h"
#include "cobalt/dom/comment.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/document_type.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/named_node_map.h"
#include "cobalt/dom/text.h"

namespace cobalt {
namespace dom {
namespace {

const char kStyleAttributeName[] = "style";

void WriteAttributes(const scoped_refptr<const Element>& element,
                     std::ostream* out_stream) {
  const Element::AttributeMap& attributes = element->attribute_map();
  typedef std::map<std::string, std::string> SortedAttributeMap;
  SortedAttributeMap sorted_attribute_map;
  sorted_attribute_map.insert(attributes.begin(), attributes.end());

  {
    // The "style" attribute is handled specially because HTMLElements store
    // it explicitly as a cssom::CSSDeclaredStyleDeclaration structure instead
    // of as an attribute string, so we add it (or replace it) explicitly in the
    // attribute map.
    base::Optional<std::string> style_attribute = element->GetStyleAttribute();
    if (style_attribute && !style_attribute->empty()) {
      sorted_attribute_map[kStyleAttributeName] = std::move(*style_attribute);
    }
  }

  for (SortedAttributeMap::const_iterator iter = sorted_attribute_map.begin();
       iter != sorted_attribute_map.end(); ++iter) {
    const std::string& name = iter->first;
    const std::string& value = iter->second;

    *out_stream << " " << name;
    if (!value.empty()) {
      *out_stream << "="
                  << "\"" << value << "\"";
    }
  }
}

}  // namespace

Serializer::Serializer(std::ostream* out_stream)
    : out_stream_(out_stream), entering_node_(true) {}

void Serializer::Serialize(const scoped_refptr<const Node>& node) {
  entering_node_ = true;
  node->Accept(this);

  SerializeDescendantsOnly(node);

  entering_node_ = false;
  node->Accept(this);
}

void Serializer::SerializeSelfOnly(const scoped_refptr<const Node>& node) {
  entering_node_ = true;
  node->Accept(this);

  if (node->first_child()) {
    *out_stream_ << "...";
  }

  entering_node_ = false;
  node->Accept(this);
}

void Serializer::SerializeDescendantsOnly(
    const scoped_refptr<const Node>& node) {
  const Node* child = node->first_child();
  while (child) {
    Serialize(child);
    child = child->next_sibling();
  }
}

void Serializer::Visit(const CDATASection* cdata_section) {
  if (entering_node_) {
    *out_stream_ << "<![CDATA[" << cdata_section->data() << "]]>";
  }
}

void Serializer::Visit(const Comment* comment) {
  if (entering_node_) {
    *out_stream_ << "<!--" << comment->data() << "-->";
  }
}

void Serializer::Visit(const Document* document) {}

void Serializer::Visit(const DocumentType* document_type) {
  if (entering_node_) {
    *out_stream_ << "<!DOCTYPE " << document_type->name() << ">";
  }
}

void Serializer::Visit(const Element* element) {
  if (entering_node_) {
    *out_stream_ << "<" << element->local_name();
    WriteAttributes(element, out_stream_);
    *out_stream_ << ">";
  } else {
    *out_stream_ << "</" << element->local_name() << ">";
  }
}

void Serializer::Visit(const Text* text) {
  if (entering_node_) {
    *out_stream_ << text->data();
  }
}

}  // namespace dom
}  // namespace cobalt
