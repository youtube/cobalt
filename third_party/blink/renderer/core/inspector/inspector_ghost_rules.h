// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSNestedDeclarationsRule;
class CSSStyleRule;
class CSSStyleSheet;
class ExecutionContext;

// Ghost Rules are style rules that exist only during getMatchedStylesForNode
// in order to surface the CSSNestedDeclaration rules that *could* be there,
// if the author takes some action, or another user-agent was used.
//
// For example:
//
//  .a {
//    .b {}
//    /* border: 1px solid black */
//    .c {}
//    -webkit-thing: auto;
//  }
//
// Here, no CSSNestedDeclaration rules are actually emitted, because there are
// no valid declarations. Yet, it's useful for the author to see the
// CSSNestedDeclaration rules that would be there, had "border:1px solid black"
// been uncommented, or "-webkit-thing" supported.
//
// Therefore, a CSSNestedDeclarations rule is temporarily inserted into the page
// stylesheet at any location where such a rule *could* appear.
//
// Note also that when parsing stylesheets for the inspector,
// CSSNestedDeclarations are always emitted, see
// CSSParserImpl::EmitNestedDeclarationsRuleIfNeeded. However, this does not
// affect the page stylesheet (the basis for what getMatchedStylesForNode
// returns), hence this class.
class CORE_EXPORT InspectorGhostRules {
  STACK_ALLOCATED();

 public:
  // Insert CSSNestedDeclaration rules in any place where they can occur.
  // Any inserted rules will automatically be removed by the destructor.
  void Populate(CSSStyleSheet&);
  ~InspectorGhostRules();

  bool Contains(CSSStyleRule* rule) const {
    return inner_rules_.Contains(rule);
  }

 private:
  void PopulateSheet(const ExecutionContext&, CSSStyleSheet&);
  void DepopulateSheet(CSSStyleSheet&);

  HeapHashSet<Member<CSSStyleSheet>> affected_stylesheets_;
  HeapHashSet<Member<CSSNestedDeclarationsRule>> inserted_rules_;
  // The inner CSSStyleRule for each item in `inserted_rules_`.
  HeapHashSet<Member<CSSStyleRule>> inner_rules_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_GHOST_RULES_H_
