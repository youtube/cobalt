// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_controller.h"

#include "ash/api/tasks/tasks_client.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/system/unified/classroom_bubble_student_view.h"
#include "ash/system/unified/tasks_combobox_model.h"
#include "base/check.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

GlanceablesController::GlanceablesController() {
  DCHECK(SessionController::Get());
  SessionController::Get()->AddObserver(this);
}

GlanceablesController::~GlanceablesController() {
  DCHECK(SessionController::Get());
  SessionController::Get()->RemoveObserver(this);
}

// static
void GlanceablesController::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlanceablesEnabled, true);
  ClassroomBubbleStudentView::RegisterUserProfilePrefs(registry);
  TasksComboboxModel::RegisterUserProfilePrefs(registry);
}

// static
void GlanceablesController::ClearUserStatePrefs(PrefService* prefs) {
  ClassroomBubbleStudentView::ClearUserStatePrefs(prefs);
  TasksComboboxModel::ClearUserStatePrefs(prefs);
}

void GlanceablesController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_account_id_ = account_id;
  bubble_shown_count_ = 0;
  login_time_ = base::Time::Now();
}

bool GlanceablesController::AreGlanceablesAvailable() const {
  return GetClassroomClient() != nullptr || GetTasksClient() != nullptr;
}

void GlanceablesController::UpdateClientsRegistration(
    const AccountId& account_id,
    const ClientsRegistration& registration) {
  clients_registry_.insert_or_assign(account_id, registration);
}

GlanceablesClassroomClient* GlanceablesController::GetClassroomClient() const {
  const auto iter = clients_registry_.find(active_account_id_);
  return iter != clients_registry_.end() ? iter->second.classroom_client.get()
                                         : nullptr;
}

api::TasksClient* GlanceablesController::GetTasksClient() const {
  const auto iter = clients_registry_.find(active_account_id_);
  return iter != clients_registry_.end() ? iter->second.tasks_client.get()
                                         : nullptr;
}

void GlanceablesController::NotifyGlanceablesBubbleClosed() {
  for (auto& clients : clients_registry_) {
    if (clients.second.classroom_client) {
      clients.second.classroom_client->OnGlanceablesBubbleClosed();
    }
    if (clients.second.tasks_client) {
      clients.second.tasks_client->OnGlanceablesBubbleClosed();
    }
  }

  RecordTotalShowTime(base::TimeTicks::Now() - last_bubble_show_time_);
}

void GlanceablesController::RecordGlanceablesBubbleShowTime(
    base::TimeTicks bubble_show_timestamp) {
  last_bubble_show_time_ = base::TimeTicks::Now();

  if (bubble_shown_count_ == 0) {
    RecordLoginToShowTime(base::Time::Now() - login_time_);
  }

  bubble_shown_count_++;
}

}  // namespace ash
