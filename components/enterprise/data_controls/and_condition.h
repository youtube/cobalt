// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_AND_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_AND_CONDITION_H_

#include <memory>
#include <vector>

#include "components/enterprise/data_controls/condition.h"

namespace data_controls {

// Implementation of an abstract "and" condition, which evaluates to true if all
// of its sub-conditions are true.
class AndCondition : public Condition {
 public:
  // Returns nullptr if the passed vector is empty.
  static std::unique_ptr<Condition> Create(
      std::vector<std::unique_ptr<Condition>> conditions);

  ~AndCondition() override;

  // Condition:
  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit AndCondition(std::vector<std::unique_ptr<Condition>> conditions);

  const std::vector<std::unique_ptr<Condition>> conditions_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_AND_CONDITION_H_
