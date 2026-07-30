// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton_loader.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"

namespace blink {

// static
const char SkeletonLoader::kSupplementName[] = "SkeletonLoader";

SkeletonLoader::SkeletonLoader(Document& owner_document)
    : Supplement<Document>(owner_document) {}

// static
SkeletonLoader* SkeletonLoader::Get(Document& document) {
  return Supplement<Document>::From<SkeletonLoader>(document);
}

// static
SkeletonLoader& SkeletonLoader::Ensure(Document& document) {
  SkeletonLoader* loader = Get(document);
  if (!loader) {
    loader = MakeGarbageCollected<SkeletonLoader>(document);
    Supplement<Document>::ProvideTo<SkeletonLoader>(document, loader);
  }
  return *loader;
}

void SkeletonLoader::AddSkeletonPrefetchLink(KURL url) {
  auto result = skeletons_.insert(url, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<Skeleton>(*this, GetDocument());
    result.stored_value->value->FetchSkeletonURL(url);
  }
}

void SkeletonLoader::NavigateTo(KURL url) {
  CHECK(!current_skeleton_);
  auto it = skeletons_.find(url);
  if (it == skeletons_.end()) {
    return;
  }
  current_skeleton_ = it->value.Get();
  current_skeleton_->Render();
}

void SkeletonLoader::CancelNavigation() {
  RemoveSkeletonTree();
  current_skeleton_ = nullptr;
}

void SkeletonLoader::RestoringFromBFCache() {
  RemoveSkeletonTree();
  current_skeleton_ = nullptr;
}

void SkeletonLoader::DocumentReady(Skeleton& skeleton) {
  if (&skeleton == current_skeleton_.Get()) {
    InsertSkeletonTree(skeleton.GetSkeletonDocument());
  }
}

void SkeletonLoader::RemoveSkeletonTree() {
  if (Element* root = GetDocument().documentElement()) {
    root->ClearSkeletonPseudo();
  }
}

void SkeletonLoader::InsertSkeletonTree(Document& skeleton_document) {
  if (Element* root = GetDocument().documentElement()) {
    PseudoElement& skeleton_pseudo = root->EnsureSkeletonPseudo();
    ShadowRoot& shadow_root = skeleton_pseudo.EnsureUserAgentShadowRoot();
    CHECK_EQ(shadow_root.firstChild(), nullptr);
    if (Element* skeleton_root = skeleton_document.documentElement()) {
      shadow_root.AppendChild(skeleton_root);
    }
  }
}

void SkeletonLoader::Trace(Visitor* visitor) const {
  visitor->Trace(current_skeleton_);
  visitor->Trace(skeletons_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
