/*
 * Copyright (C) 2007 Alexey Proskuryakov (ap@nypop.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/xml/native_xpath_ns_resolver.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

void CountNonElementResolver(const Node* node) {
  if (!node || node->IsElementNode()) {
    return;
  }
  if (const auto* attr = DynamicTo<Attr>(node)) {
    if (attr->ownerElement())
      return;
  } else if (const auto* char_data = DynamicTo<CharacterData>(node)) {
    if (char_data->parentElement())
      return;
  }
  node->GetDocument().CountUse(WebFeature::kCreateNSResolverWithNonElements2);
}

}  // namespace

NativeXPathNSResolver::NativeXPathNSResolver(Node* node) : node_(node) {}

AtomicString NativeXPathNSResolver::lookupNamespaceURI(const String& prefix) {
  // This is not done by Node::lookupNamespaceURI as per the DOM3 Core spec,
  // but the XPath spec says that we should do it for XPathNSResolver.
  if (prefix == "xml") {
    CountNonElementResolver(node_);
    return xml_names::kNamespaceURI;
  }

  return node_ ? node_->lookupNamespaceURI(prefix) : g_null_atom;
}

void NativeXPathNSResolver::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
