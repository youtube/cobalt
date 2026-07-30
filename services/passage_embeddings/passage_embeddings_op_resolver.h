// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_
#define SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace passage_embeddings {

class HistoryOpResolver : public optimization_guide::TFLiteOpResolver {
 public:
  explicit HistoryOpResolver(bool allow_gpu_execution);
  HistoryOpResolver(const HistoryOpResolver&) = delete;
  HistoryOpResolver& operator=(const HistoryOpResolver&) = delete;

  ~HistoryOpResolver() override = default;
};

class GemmaOpResolver : public optimization_guide::TFLiteOpResolver {
 public:
  GemmaOpResolver();
  GemmaOpResolver(const GemmaOpResolver&) = delete;
  GemmaOpResolver& operator=(const GemmaOpResolver&) = delete;

  ~GemmaOpResolver() override = default;
};

}  // namespace passage_embeddings

#endif  // SERVICES_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_OP_RESOLVER_H_
