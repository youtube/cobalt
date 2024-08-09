// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "third_party/chromium/media/learning/common/labelled_example.h"
#include "third_party/chromium/media/learning/common/learning_task.h"

namespace media_m96 {
namespace learning {

class LearningTaskController;

// Interface to provide a Learner given the task name.
class COMPONENT_EXPORT(LEARNING_COMMON) LearningSession
    : public base::SupportsUserData::Data {
 public:
  LearningSession();

  LearningSession(const LearningSession&) = delete;
  LearningSession& operator=(const LearningSession&) = delete;

  ~LearningSession() override;

  // Return a LearningTaskController for the given task.
  virtual std::unique_ptr<LearningTaskController> GetController(
      const std::string& task_name) = 0;
};

}  // namespace learning
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_LEARNING_COMMON_LEARNING_SESSION_H_
