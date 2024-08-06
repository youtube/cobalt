// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_LOOKUP_TABLE_TRAINER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_LOOKUP_TABLE_TRAINER_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "third_party/chromium/media/learning/common/learning_task.h"
#include "third_party/chromium/media/learning/impl/training_algorithm.h"

namespace media_m96 {
namespace learning {

// Trains a lookup table model.
class COMPONENT_EXPORT(LEARNING_IMPL) LookupTableTrainer
    : public TrainingAlgorithm {
 public:
  LookupTableTrainer();

  LookupTableTrainer(const LookupTableTrainer&) = delete;
  LookupTableTrainer& operator=(const LookupTableTrainer&) = delete;

  ~LookupTableTrainer() override;

  void Train(const LearningTask& task,
             const TrainingData& training_data,
             TrainedModelCB model_cb) override;
};

}  // namespace learning
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_IMPL_LOOKUP_TABLE_TRAINER_H_
