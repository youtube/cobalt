// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/mock_at_memory_query_service.h"

#include <memory>

#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/at_memory_query_service_delegate.h"

namespace accessibility_annotator {

namespace {

class StubAtMemoryQueryServiceDelegate
    : public accessibility_annotator::AtMemoryQueryServiceDelegate {
 public:
  void RetrieveLiveTabContext(
      accessibility_annotator::LiveTabContextQuery query,
      base::OnceCallback<void(accessibility_annotator::LiveTabContextResponse)>
          callback) override {
    std::move(callback).Run({});
  }
};

}  // namespace

MockAtMemoryQueryService::MockAtMemoryQueryService()
    : accessibility_annotator::AtMemoryQueryService(
          std::make_unique<StubAtMemoryQueryServiceDelegate>(),
          /*data_provider=*/nullptr,
          /*personal_context_resolver=*/nullptr,
          /*remote_model_executor=*/nullptr) {}

MockAtMemoryQueryService::~MockAtMemoryQueryService() = default;

}  // namespace accessibility_annotator
