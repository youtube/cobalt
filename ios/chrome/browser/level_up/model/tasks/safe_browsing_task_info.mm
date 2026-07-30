// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

class SafeBrowsingTaskInfo : public TaskInfo {
 public:
  SafeBrowsingTaskInfo() = default;
  ~SafeBrowsingTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kSafeBrowsing; }
  std::string GetTitle() const override { return "Enhanced Safe Browsing"; }
  std::string GetTaskDescription() const override {
    return "Add an extra layer of protection against online threats";
  }
  std::string GetIconSymbolName() const override {
    return base::SysNSStringToUTF8(kShieldSymbol);
  }
  bool IsCustomSymbol() const override { return false; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kSafety;
  }
  std::string GetTriggerUserAction() const override {
    return "MobilePrivacySafeBrowsingSettingsClose";
  }
  TaskInfo::NavigationAction GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateSafeBrowsingTaskInfo() {
  return std::make_unique<SafeBrowsingTaskInfo>();
}
