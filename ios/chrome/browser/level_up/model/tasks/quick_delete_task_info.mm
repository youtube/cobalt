// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

class QuickDeleteTaskInfo : public TaskInfo {
 public:
  QuickDeleteTaskInfo() = default;
  ~QuickDeleteTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kQuickDelete; }
  std::string GetTitle() const override { return "Quick delete"; }
  std::string GetTaskDescription() const override {
    return "Manage your history, cookies and more to protect your privacy";
  }
  std::string GetIconSymbolName() const override {
    return base::SysNSStringToUTF8(kTrashSymbol);
  }
  bool IsCustomSymbol() const override { return false; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSafety;
  }
  std::string GetTriggerUserAction() const override { return ""; }
  base::RepeatingClosure GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateQuickDeleteTaskInfo() {
  return std::make_unique<QuickDeleteTaskInfo>();
}
