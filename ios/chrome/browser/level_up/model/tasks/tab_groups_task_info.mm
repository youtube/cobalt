// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/tasks/task_factories.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

class TabGroupsTaskInfo : public TaskInfo {
 public:
  TabGroupsTaskInfo() = default;
  ~TabGroupsTaskInfo() override = default;

  // TaskInfo implementation.
  TaskType GetTaskType() const override { return TaskType::kTabGroups; }
  std::string GetTitle() const override {
    return l10n_util::GetStringUTF8(IDS_IOS_LEVEL_UP_FEATURE_TAB_GROUPS);
  }
  std::string GetTaskDescription() const override {
    return "Stay organized with tab groups";
  }
  std::string GetIconSymbolName() const override {
    return base::SysNSStringToUTF8(kTabsSymbol);
  }
  bool IsCustomSymbol() const override { return false; }
  LevelUpTaskCategory GetCategory() const override {
    return LevelUpTaskCategory::kProductivity;
  }
  std::string GetTriggerUserAction() const override {
    return "MobileTabGroupUserCreatedNewGroup";
  }
  base::RepeatingClosure GetNavigationAction() const override {
    return base::DoNothing();
  }
};

std::unique_ptr<TaskInfo> CreateTabGroupsTaskInfo() {
  return std::make_unique<TabGroupsTaskInfo>();
}
