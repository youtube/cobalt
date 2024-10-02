// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_CANDIDATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_CANDIDATE_H_

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"

namespace blink {

class HTMLAnchorElement;
class KURL;
struct Referrer;
class SpeculationRuleSet;

// See documentation for "SpeculationCandidate" in
// third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.
// Largely equivalent to the mojom type, but stores some extra fields that
// are used by DevTools.
class SpeculationCandidate : public GarbageCollected<SpeculationCandidate> {
 public:
  SpeculationCandidate(const KURL& url,
                       mojom::blink::SpeculationAction action,
                       const Referrer& referrer,
                       bool requires_anonymous_client_ip_when_cross_origin,
                       mojom::blink::SpeculationTargetHint target_hint,
                       mojom::blink::SpeculationEagerness eagerness,
                       network::mojom::blink::NoVarySearchPtr no_vary_search,
                       mojom::blink::SpeculationInjectionWorld injection_world,
                       SpeculationRuleSet* rule_set,
                       HTMLAnchorElement* anchor);
  virtual ~SpeculationCandidate() = default;

  void Trace(Visitor* visitor) const;

  mojom::blink::SpeculationCandidatePtr ToMojom() const;

  const KURL& url() const { return url_; }
  mojom::blink::SpeculationAction action() const { return action_; }
  mojom::blink::SpeculationTargetHint target_hint() const {
    return target_hint_;
  }
  mojom::blink::SpeculationEagerness eagerness() const { return eagerness_; }
  SpeculationRuleSet* rule_set() const { return rule_set_; }
  // Only set for candidates derived from a document rule (is null for
  // candidates derived from list rules).
  HTMLAnchorElement* anchor() const { return anchor_; }

 private:
  const KURL url_;
  const mojom::blink::SpeculationAction action_;
  const Referrer referrer_;
  const bool requires_anonymous_client_ip_when_cross_origin_;
  const mojom::blink::SpeculationTargetHint target_hint_;
  const mojom::blink::SpeculationEagerness eagerness_;
  const network::mojom::blink::NoVarySearchPtr no_vary_search_;
  const mojom::blink::SpeculationInjectionWorld injection_world_;
  const Member<SpeculationRuleSet> rule_set_;
  const Member<HTMLAnchorElement> anchor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_CANDIDATE_H_
