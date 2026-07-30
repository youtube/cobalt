// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

class PaymentMethodsTaskInfo : public TaskInfo {
 public:
  PaymentMethodsTaskInfo() = default;
  ~PaymentMethodsTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kPaymentMethods; }
  std::string GetTitle() const override { return "Manage payment methods"; }
  std::string GetTaskDescription() const override {
    return "Add new payment methods or edit saved ones to check out faster";
  }
  std::string GetIconSymbolName() const override {
    return base::SysNSStringToUTF8(kCreditCardSymbol);
  }
  bool IsCustomSymbol() const override { return false; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override {
    return "AutofillCreditCardsViewed";
  }
  base::RepeatingClosure GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreatePaymentMethodsTaskInfo() {
  return std::make_unique<PaymentMethodsTaskInfo>();
}
