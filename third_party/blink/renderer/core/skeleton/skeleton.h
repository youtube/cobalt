// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class Document;

// Represents a skeleton being rendered
class Skeleton : public GarbageCollected<Skeleton> {
 public:
  class Observer : public GarbageCollectedMixin {
   public:
    // Invoked when a Render resulted in a Document.
    // Used to signal the hosting document that it can start rendering the
    // skeleton.
    virtual void DocumentReady(Skeleton& skeleton) = 0;
  };

  Skeleton(Observer& observer, Document& owner_document);

  // Do a HEAD request to get the skeleton url for 'url'
  void FetchSkeletonURL(KURL url);

  // Render the skeleton in the owner document
  void Render();

  Document& GetSkeletonDocument() {
    CHECK(skeleton_document_);
    return *skeleton_document_;
  }

  Document& GetOwnerDocument() {
    CHECK(owner_document_);
    return *owner_document_;
  }

  void Trace(Visitor* visitor) const;

 private:
  class HTMLFetcher;
  class LinkFetcher;

  void StartHTMLFetch(const KURL& skeleton_url);
  void HTMLFetchFinished(const String& html, bool success);
  void ParseSkeletonHTML(const String& html);

  Member<Observer> observer_;
  Member<Document> owner_document_;
  Member<Document> skeleton_document_;
  Member<HTMLFetcher> html_fetcher_;
  Member<LinkFetcher> link_fetcher_;

  String fetched_html_;

  // Set to true if Render() has been called, in which case DocumentReady()
  // should be invoked when the skeleton document is ready.
  bool render_requested_ = false;

  // Set to true when the skeleton fetch has finished
  bool html_fetch_completed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SKELETON_SKELETON_H_
