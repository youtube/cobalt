/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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
 *
 */

#include "third_party/blink/renderer/core/dom/element_data_cache.h"

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/dom/element_data.h"

namespace blink {

inline unsigned AttributeHash(
    const Vector<Attribute, kAttributePrealloc>& attributes) {
  return StringHasher::HashMemory(base::as_byte_span(attributes));
}

inline bool HasSameAttributes(
    const Vector<Attribute, kAttributePrealloc>& attributes,
    ShareableElementData& element_data) {
  return std::equal(attributes.begin(), attributes.end(),
                    element_data.attribute_array_,
                    UNSAFE_TODO(element_data.attribute_array_ +
                                element_data.Attributes().size()));
}

ShareableElementData*
ElementDataCache::CachedShareableElementDataWithAttributes(
    const StringImpl* tag_name,
    const Vector<Attribute, kAttributePrealloc>& attributes) {
  DCHECK(!attributes.empty());

  unsigned hash = WTF::HashInts(tag_name->GetHash(), AttributeHash(attributes));
  ShareableElementDataCache::ValueType* it =
      shareable_element_data_cache_.insert(hash, std::pair(nullptr, nullptr))
          .stored_value;

  // FIXME: This prevents sharing when there's a hash collision.
  if (it->value.second == nullptr || it->value.first != tag_name ||
      !HasSameAttributes(attributes, *it->value.second)) {
    it->value.first = tag_name;
    it->value.second = ShareableElementData::CreateWithAttributes(attributes);
  }

  return it->value.second.Get();
}

ElementDataCache::ElementDataCache() = default;

void ElementDataCache::Trace(Visitor* visitor) const {
  visitor->Trace(shareable_element_data_cache_);
}

}  // namespace blink
