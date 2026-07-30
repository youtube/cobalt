// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_

#include "base/component_export.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace ml {

// TODO: crbug.com/519276089 - rename to a generic name since the
// text-safety/bert-safety split no longer exists. Also migrate out of `ml`
// since it no longer depends on `ml` functions.
// TsHolder holds a single TextSafetyModel. Its operations may block.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_ML) TsHolder final {
 public:
  TsHolder();
  ~TsHolder();

  static base::SequenceBound<TsHolder> Create();

  void Reset(
      on_device_model::mojom::TextSafetyModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::TextSafetyModel> model);

 private:
  // A connected model, once we've received assets.
  mojo::UniqueReceiverSet<on_device_model::mojom::TextSafetyModel> model_;
};

}  // namespace ml

#endif  // SERVICES_ON_DEVICE_MODEL_ML_TS_MODEL_H_
