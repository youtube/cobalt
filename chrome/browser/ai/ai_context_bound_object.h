// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
#define CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ref.h"
#include "base/supports_user_data.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

class AIContextBoundObjectSet;

// Base class for storing an object that shall be deleted when the
// document is gone such as `AILanguageModel` and `AISummarizer`.

// The ownership chain of the relevant class is:
// `BucketContext` (via `SupportsUserData` or `DocumentAssociatedData`) -owns->
// `AIContextBoundObjectSet` -owns->
// `AIContextBoundObject` (implements some blink::mojom interface) -owns->
// `mojo::Receiver<blink::mojom::SomeInterface>`
class AIContextBoundObject {
 public:
  explicit AIContextBoundObject(
      AIContextBoundObjectSet& context_bound_object_set);
  virtual ~AIContextBoundObject();

  void RemoveFromSet();

 private:
  // The `AIContextBoundObject` will be owned by the `AIContextBoundObjectSet`,
  // so we can store the `raw_ref` here.
  base::raw_ref<AIContextBoundObjectSet> context_bound_object_set_;
};

#endif  // CHROME_BROWSER_AI_AI_CONTEXT_BOUND_OBJECT_H_
