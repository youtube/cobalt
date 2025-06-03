// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "ash/api/tasks/tasks_types.h"
#include "ash/shell.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/list_model.h"

namespace ash {
namespace {

const char kLastSelectedTaskListIdPref[] =
    "ash.glanceables.tasks.last_selected_task_list_id";
const char kLastSelectedTaskListTimePref[] =
    "ash.glanceables.tasks.last_selected_task_list_time";

}  // namespace

TasksComboboxModel::TasksComboboxModel(ui::ListModel<api::TaskList>* task_lists)
    : task_lists_(task_lists) {}

TasksComboboxModel::~TasksComboboxModel() = default;

// static
void TasksComboboxModel::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kLastSelectedTaskListIdPref, "");
  registry->RegisterTimePref(kLastSelectedTaskListTimePref, base::Time());
}

// static
void TasksComboboxModel::ClearUserStatePrefs(PrefService* pref_service) {
  pref_service->ClearPref(kLastSelectedTaskListIdPref);
  pref_service->ClearPref(kLastSelectedTaskListTimePref);
}

size_t TasksComboboxModel::GetItemCount() const {
  return task_lists_->item_count();
}

std::u16string TasksComboboxModel::GetItemAt(size_t index) const {
  return base::UTF8ToUTF16(task_lists_->GetItemAt(index)->title);
}

absl::optional<size_t> TasksComboboxModel::GetDefaultIndex() const {
  const auto most_recently_updated_task_list_iter = std::max_element(
      task_lists_->begin(), task_lists_->end(),
      [](const auto& x1, const auto& x2) { return x1->updated < x2->updated; });
  const size_t most_recently_updated_task_list_index =
      most_recently_updated_task_list_iter - task_lists_->begin();

  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  const auto& last_selected_task_list_id =
      pref_service->GetString(kLastSelectedTaskListIdPref);
  const auto& last_selected_task_list_time =
      pref_service->GetTime(kLastSelectedTaskListTimePref);

  if (!last_selected_task_list_id.empty() &&
      last_selected_task_list_time >
          most_recently_updated_task_list_iter->get()->updated) {
    const auto last_selected_task_list_iter =
        std::find_if(task_lists_->begin(), task_lists_->end(),
                     [&last_selected_task_list_id](const auto& x) {
                       return x->id == last_selected_task_list_id;
                     });
    if (last_selected_task_list_iter != task_lists_->end()) {
      return last_selected_task_list_iter - task_lists_->begin();
    }
  }

  if (!last_selected_task_list_id.empty()) {
    pref_service->ClearPref(kLastSelectedTaskListIdPref);
    pref_service->ClearPref(kLastSelectedTaskListTimePref);
  }

  return most_recently_updated_task_list_index;
}

api::TaskList* TasksComboboxModel::GetTaskListAt(size_t index) const {
  return task_lists_->GetItemAt(index);
}

void TasksComboboxModel::SaveLastSelectedTaskList(
    const std::string& task_list_id) {
  CHECK(!task_list_id.empty());

  auto* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetString(kLastSelectedTaskListIdPref, task_list_id);
  pref_service->SetTime(kLastSelectedTaskListTimePref, base::Time::Now());
}

}  // namespace ash
