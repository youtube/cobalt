// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_history_embeddings_service.h"

#include "components/passage_embeddings/mock_embedder.h"

namespace history_embeddings {

MockHistoryEmbeddingsService::MockHistoryEmbeddingsService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    history::HistoryService* history_service)
    : HistoryEmbeddingsService(
          os_crypt_async,
          history_service,
          /*page_content_annotations_service=*/nullptr,
          /*optimization_guide_decider=*/nullptr,
          /*service_controller=*/nullptr,
          std::make_unique<passage_embeddings::MockEmbedder>(),
          /*answerer=*/nullptr,
          /*intent_classifier=*/nullptr) {}

MockHistoryEmbeddingsService::~MockHistoryEmbeddingsService() = default;

}  // namespace history_embeddings
