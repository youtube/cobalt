// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_UTILS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CascadeLayerMap;

using FunctionRuleMap = HeapHashMap<AtomicString, Member<StyleRuleFunction>>;

// Certain at-rules define names that can be referenced later by their
// corresponding properties, for example @keyframes/animation-name. When
// multiple at-rules want to define the same name, the "winning" rule is
// determined by the relative strength of the originating layer [1], and
// order of appearance.
//
// This helper function takes a list of name-defining rules, and outputs
// the winner while taking cascade layers into account.
//
// Note that one set of winners exists per tree-scope. See css-scoping-1 [2]
// for an explanation of how tree-scoped names work.
//
// [1] https://drafts.csswg.org/css-cascade-5/#layering
// [2] https://drafts.csswg.org/css-scoping/#shadow-names
template <typename T>
void AddNameDefiningRules(const HeapVector<Member<T>>& input_rules,
                          const CascadeLayerMap* cascade_layer_map,
                          HeapHashMap<AtomicString, Member<T>>& out) {
  for (T* rule : input_rules) {
    auto result = out.insert(rule->Name(), rule);
    if (result.is_new_entry) {
      continue;
    }
    Member<T>& stored_rule = result.stored_value->value;
    const bool should_override =
        !cascade_layer_map ||
        cascade_layer_map->CompareLayerOrder(stored_rule->GetCascadeLayer(),
                                             rule->GetCascadeLayer()) <= 0;
    if (should_override) {
      stored_rule = rule;
    }
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_RESOLVER_UTILS_H_
