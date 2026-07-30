// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/ml/ts_model.h"

#include <utility>

#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "services/on_device_model/safety/bert_safety_model.h"

namespace ml {

namespace mojom = ::on_device_model::mojom;

TsHolder::TsHolder() = default;
TsHolder::~TsHolder() = default;

// static
base::SequenceBound<TsHolder> TsHolder::Create() {
  return base::SequenceBound<TsHolder>(
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
}

void TsHolder::Reset(mojom::TextSafetyModelParamsPtr params,
                     mojo::PendingReceiver<mojom::TextSafetyModel> model) {
  model_.Clear();

  auto impl = on_device_model::BertSafetyModel::Create(std::move(params));
  if (impl) {
    model_.Add(std::move(impl), std::move(model));
  }
}

}  // namespace ml
