// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_semantic_embedder.h"

#include <utility>

namespace content {

EchoAISemanticEmbedder::EchoAISemanticEmbedder() = default;

EchoAISemanticEmbedder::~EchoAISemanticEmbedder() = default;

void EchoAISemanticEmbedder::Embed(
    const std::vector<std::string>& inputs,
    blink::mojom::AISemanticEmbedderEmbedOptionsPtr options,
    EmbedCallback callback) {
  auto result = blink::mojom::SemanticEmbedderResult::New();
  result->embeddings.reserve(inputs.size());

  for (size_t i = 0; i < inputs.size(); ++i) {
    auto content_embedding = blink::mojom::ContentEmbedding::New();
    content_embedding->values = std::vector<float>(1, 0.0f);
    result->embeddings.push_back(std::move(content_embedding));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace content
