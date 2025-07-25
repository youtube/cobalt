// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_FOR_SERVICE_WORKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_FOR_SERVICE_WORKER_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Document;
class HTMLAnchorElement;
class IntersectionObserver;
class IntersectionObserverEntry;

class CORE_EXPORT AnchorElementObserverForServiceWorker
    : public GarbageCollected<AnchorElementObserverForServiceWorker>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static AnchorElementObserverForServiceWorker* From(Document&);
  explicit AnchorElementObserverForServiceWorker(
      base::PassKey<AnchorElementObserverForServiceWorker>,
      Document& document);
  AnchorElementObserverForServiceWorker(
      AnchorElementObserverForServiceWorker&&) = delete;
  AnchorElementObserverForServiceWorker& operator=(
      AnchorElementObserverForServiceWorker&&) = delete;
  AnchorElementObserverForServiceWorker(
      const AnchorElementObserverForServiceWorker&) = delete;
  AnchorElementObserverForServiceWorker& operator=(
      const AnchorElementObserverForServiceWorker&) = delete;
  virtual ~AnchorElementObserverForServiceWorker() = default;
  using Links = HeapVector<Member<HTMLAnchorElement>>;

  void MaybeSendNavigationTargetLinks(const Links& candidate_links);
  void MaybeSendPendingWarmUpRequests();
  void ObserveAnchorElementVisibility(HTMLAnchorElement& element);
  void Trace(Visitor* visitor) const override;

 private:
  void UpdateVisibleAnchors(
      const HeapVector<Member<IntersectionObserverEntry>>& entries);
  void SendPendingWarmUpRequests(TimerBase*);
  Document& GetDocument() { return *GetSupplementable(); }

  Member<IntersectionObserver> intersection_observer_;

  // Remember already handled links to prevent excessive duplicate
  // warm-up requests.
  HeapHashSet<WeakMember<HTMLAnchorElement>> already_handled_links_;

  // The following `pending_warm_up_links_` keeps the pending warm-up requests
  // until the document is loaded to prioritize loading the document.
  HeapDeque<Member<HTMLAnchorElement>> pending_warm_up_links_;

  // Sent URL count to browser process.
  int total_request_count_ = 0;

  // This timer is used to avoid frequent warm-up mojo calls. Before actually
  // sending URL candidates, we wait for a while to accept more requests, and
  // then send the accumulated URL candidates in one batch.
  HeapTaskRunnerTimer<AnchorElementObserverForServiceWorker> batch_timer_;

  // `true` indicates the timer hasn't used.
  bool is_first_batch_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_OBSERVER_FOR_SERVICE_WORKER_H_
