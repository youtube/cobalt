// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/field_trial_service_ash.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {
crosapi::mojom::FieldTrialGroupInfoPtr CreateFieldTrialGroupInfo(
    const std::string& trial_name,
    const std::string& group_name) {
  auto info = crosapi::mojom::FieldTrialGroupInfo::New();
  info->trial_name = trial_name;
  info->group_name = group_name;
  return info;
}

static crosapi::FieldTrialServiceAsh* g_field_trial_service_ash = nullptr;

void OnFieldTrialGroupFinalizedCallback(const std::string& trial_name,
                                        const std::string& group_name) {
  if (g_field_trial_service_ash)
    g_field_trial_service_ash->OnFieldTrialGroupFinalized(trial_name,
                                                          group_name);
}

}  // namespace

namespace crosapi {

FieldTrialServiceAsh::FieldTrialServiceAsh() {
  DCHECK(g_field_trial_service_ash == nullptr);
  g_field_trial_service_ash = this;
  base::FieldTrialList::AddObserver(this);
}

FieldTrialServiceAsh::~FieldTrialServiceAsh() {
  base::FieldTrialList::RemoveObserver(this);
  g_field_trial_service_ash = nullptr;
}

void FieldTrialServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FieldTrialService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FieldTrialServiceAsh::AddFieldTrialObserver(
    mojo::PendingRemote<mojom::FieldTrialObserver> observer) {
  mojo::Remote<mojom::FieldTrialObserver> remote(std::move(observer));

  // Send active field trial groups when the observer is added
  // before subscribing field trial group updates.
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  std::vector<mojom::FieldTrialGroupInfoPtr> infos;
  for (const auto& group : active_groups) {
    infos.push_back(
        CreateFieldTrialGroupInfo(group.trial_name, group.group_name));
  }
  remote->OnFieldTrialGroupActivated(std::move(infos));

  observers_.Add(std::move(remote));
}

void FieldTrialServiceAsh::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&OnFieldTrialGroupFinalizedCallback,
                                  trial_name, group_name));
    return;
  }

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observers_) {
    std::vector<mojom::FieldTrialGroupInfoPtr> infos;
    infos.push_back(CreateFieldTrialGroupInfo(trial_name, group_name));
    observer->OnFieldTrialGroupActivated(std::move(infos));
  }
}

}  // namespace crosapi
