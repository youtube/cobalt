// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_H_
#define CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/ai/ai_semantic_embedder.mojom.h"

class AIContextBoundObjectSet;

class AISemanticEmbedder : public AIContextBoundObject,
                           public blink::mojom::AISemanticEmbedder {
 public:
  AISemanticEmbedder(
      AIContextBoundObjectSet& context_bound_object_set,
      mojo::PendingReceiver<blink::mojom::AISemanticEmbedder> receiver);
  AISemanticEmbedder(const AISemanticEmbedder&) = delete;
  AISemanticEmbedder& operator=(const AISemanticEmbedder&) = delete;
  ~AISemanticEmbedder() override;

  // blink::mojom::AISemanticEmbedder:
  void Embed(const std::vector<std::string>& inputs,
             blink::mojom::AISemanticEmbedderEmbedOptionsPtr options,
             EmbedCallback callback) override;

 private:
  void OnEmbedComplete(EmbedCallback callback,
                       std::vector<std::string> passages,
                       std::vector<passage_embeddings::Embedding> embeddings,
                       uint64_t job_id,
                       passage_embeddings::ComputeEmbeddingsStatus status);

  mojo::Receiver<blink::mojom::AISemanticEmbedder> receiver_;
  std::vector<passage_embeddings::Embedder::Job> pending_jobs_;

  base::WeakPtrFactory<AISemanticEmbedder> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_H_
