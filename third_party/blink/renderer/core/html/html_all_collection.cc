/*
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/html_all_collection.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_htmlcollection.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

HTMLAllCollection::HTMLAllCollection(ContainerNode& node)
    : HTMLCollection(node, kDocAll, kDoesNotOverrideItemAfter) {}

HTMLAllCollection::HTMLAllCollection(ContainerNode& node, CollectionType type)
    : HTMLAllCollection(node) {
  DCHECK_EQ(type, kDocAll);
}

HTMLAllCollection::~HTMLAllCollection() = default;

Element* HTMLAllCollection::AnonymousIndexedGetter(unsigned index) {
  return HTMLCollection::item(index);
}

V8UnionElementOrHTMLCollection* HTMLAllCollection::NamedGetter(
    const AtomicString& name) {
  HTMLCollection* items = GetDocument().DocumentAllNamedItems(name);

  if (!items->length())
    return nullptr;

  if (items->length() == 1) {
    return MakeGarbageCollected<V8UnionElementOrHTMLCollection>(items->item(0));
  }

  return MakeGarbageCollected<V8UnionElementOrHTMLCollection>(items);
}

}  // namespace blink
