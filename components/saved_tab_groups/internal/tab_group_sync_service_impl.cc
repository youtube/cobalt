// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/saved_tab_groups/delegate/tab_group_sync_delegate.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/saved_tab_group_sync_bridge.h"
#include "components/saved_tab_groups/internal/shared_tab_group_data_sync_bridge.h"
#include "components/saved_tab_groups/internal/stats.h"
#include "components/saved_tab_groups/internal/sync_data_type_configuration.h"
#include "components/saved_tab_groups/internal/tab_group_sync_bridge_mediator.h"
#include "components/saved_tab_groups/internal/tab_group_sync_coordinator_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_metrics_logger.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace tab_groups {
namespace {
constexpr base::TimeDelta kDelayBeforeMetricsLogged = base::Seconds(10);

constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
constexpr bool is_ios = !!BUILDFLAG(IS_IOS);
constexpr bool is_desktop = !(is_android || is_ios);

bool IsSanitizationRequired(const SavedTabGroup& tab_group, const GURL url) {
  return tab_group.is_shared_tab_group() && url.SchemeIsHTTPOrHTTPS();
}

void OnCanApplyOptimizationCompleted(
    TabGroupSyncService::UrlRestrictionCallback callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::optional<proto::UrlRestriction> url_restriction;
  if (metadata.any_metadata().has_value()) {
    url_restriction =
        optimization_guide::ParsedAnyMetadata<proto::UrlRestriction>(
            metadata.any_metadata().value());
  }

  std::move(callback).Run(std::move(url_restriction));
}

void OnPageEntitiesResponseReceived(
    const GURL& url,
    base::OnceCallback<void(const std::u16string&)> callback,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  if (decision != optimization_guide::OptimizationGuideDecision::kTrue) {
    std::move(callback).Run(std::u16string());
    return;
  }

  if (metadata.any_metadata().has_value()) {
    std::optional<optimization_guide::proto::PageEntitiesMetadata>
        page_entities_metadata = metadata.ParsedMetadata<
            optimization_guide::proto::PageEntitiesMetadata>();
    if (page_entities_metadata.has_value() &&
        !page_entities_metadata->alternative_title().empty()) {
      std::move(callback).Run(
          base::ASCIIToUTF16(page_entities_metadata->alternative_title()));
      return;
    }
  }

  std::move(callback).Run(std::u16string());
}

}  // namespace

TabGroupSyncServiceImpl::TabGroupSyncServiceImpl(
    std::unique_ptr<SavedTabGroupModel> model,
    std::unique_ptr<SyncDataTypeConfiguration> saved_tab_group_configuration,
    std::unique_ptr<SyncDataTypeConfiguration> shared_tab_group_configuration,
    PrefService* pref_service,
    std::unique_ptr<TabGroupSyncMetricsLogger> metrics_logger,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    signin::IdentityManager* identity_manager)
    : model_(std::move(model)),
      sync_bridge_mediator_(std::make_unique<TabGroupSyncBridgeMediator>(
          model_.get(),
          pref_service,
          std::move(saved_tab_group_configuration),
          std::move(shared_tab_group_configuration))),
      metrics_logger_(std::move(metrics_logger)),
      pref_service_(pref_service),
      opt_guide_(optimization_guide_decider) {
  model_->AddObserver(this);
  if (opt_guide_) {
    opt_guide_->RegisterOptimizationTypes(
        {optimization_guide::proto::PAGE_ENTITIES,
         optimization_guide::proto::SAVED_TAB_GROUP});
  }
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

TabGroupSyncServiceImpl::~TabGroupSyncServiceImpl() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed();
  }
}

void TabGroupSyncServiceImpl::SetCoordinator(
    std::unique_ptr<TabGroupSyncCoordinator> coordinator) {
  CHECK(!coordinator_);
  coordinator_ = std::move(coordinator);
  if (IsTabGroupSyncCoordinatorEnabled()) {
    AddObserver(coordinator_.get());
  }
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncServiceImpl::CreateScopedLocalObserverPauser() {
  CHECK(coordinator_);
  return coordinator_->CreateScopedLocalObserverPauser();
}

void TabGroupSyncServiceImpl::GetURLRestriction(
    const GURL& url,
    TabGroupSyncService::UrlRestrictionCallback callback) {
  if (!opt_guide_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::SAVED_TAB_GROUP,
      base::BindOnce(&OnCanApplyOptimizationCompleted, std::move(callback)));
}

std::unique_ptr<std::vector<SavedTabGroup>>
TabGroupSyncServiceImpl::TakeSharedTabGroupsAvailableAtStartupForMessaging() {
  return std::move(shared_tab_groups_available_at_startup_for_messaging_);
}

void TabGroupSyncServiceImpl::AddObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.AddObserver(observer);

  // If the observer is added late and missed the init signal, send the signal
  // now.
  if (is_initialized_) {
    observer->OnInitialized();
  }
}

void TabGroupSyncServiceImpl::RemoveObserver(
    TabGroupSyncService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TabGroupSyncServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  for (signin::ConsentLevel consent_level :
       {signin::ConsentLevel::kSignin, signin::ConsentLevel::kSync}) {
    // Only record metrics when setting the primary account.
    switch (event_details.GetEventTypeFor(consent_level)) {
      case signin::PrimaryAccountChangeEvent::Type::kNone:
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        break;
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        if (metrics_logger_) {
          metrics_logger_->RecordMetricsOnSignin(model_->saved_tab_groups(),
                                                 consent_level);
        }
    }
  }
}

void TabGroupSyncServiceImpl::SetIsInitializedForTesting(bool initialized) {
  is_initialized_ = initialized;
}

void TabGroupSyncServiceImpl::Shutdown() {
  metrics_logger_.reset();
  identity_manager_observation_.Reset();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSavedTabGroupControllerDelegate() {
  return sync_bridge_mediator_->GetSavedTabGroupControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabGroupSyncServiceImpl::GetSharedTabGroupControllerDelegate() {
  return sync_bridge_mediator_->GetSharedTabGroupControllerDelegate();
}

void TabGroupSyncServiceImpl::SetTabGroupSyncDelegate(
    std::unique_ptr<TabGroupSyncDelegate> delegate) {
  auto coordinator =
      std::make_unique<TabGroupSyncCoordinatorImpl>(std::move(delegate), this);
  SetCoordinator(std::move(coordinator));
}

void TabGroupSyncServiceImpl::AddGroup(SavedTabGroup group) {
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(
        base::BindOnce(&TabGroupSyncServiceImpl::AddGroup,
                       weak_ptr_factory_.GetWeakPtr(), std::move(group)));
    return;
  }

  VLOG(2) << __func__;
  base::Uuid group_id = group.saved_guid();
  group.SetCreatedBeforeSyncingTabGroups(
      !sync_bridge_mediator_->IsSavedBridgeSyncing());
  group.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());

  std::optional<LocalTabGroupID> local_group_id = group.local_group_id();

  model_->AddedLocally(std::move(group));

  // Local group id can be null for tests.
  if (local_group_id.has_value()) {
    LogEvent(TabGroupEvent::kTabGroupCreated, local_group_id.value());
  }
}

void TabGroupSyncServiceImpl::RemoveGroup(const LocalTabGroupID& local_id) {
  VLOG(2) << __func__;

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  LogEvent(TabGroupEvent::kTabGroupRemoved, local_id);
  model_->RemovedLocally(local_id);
}

void TabGroupSyncServiceImpl::RemoveGroup(const base::Uuid& sync_id) {
  VLOG(2) << __func__;
  // TODO(shaktisahu): Provide LogEvent API to work with sync ID.
  model_->RemovedLocally(sync_id);
}

void TabGroupSyncServiceImpl::UpdateVisualData(
    const LocalTabGroupID local_group_id,
    const tab_groups::TabGroupVisualData* visual_data) {
  VLOG(2) << __func__;
  model_->UpdateVisualDataLocally(local_group_id, visual_data);
  UpdateAttributions(local_group_id);
  LogEvent(TabGroupEvent::kTabGroupVisualsChanged, local_group_id,
           std::nullopt);
  stats::RecordTabGroupVisualsMetrics(visual_data);
}

void TabGroupSyncServiceImpl::UpdateGroupPosition(
    const base::Uuid& sync_id,
    std::optional<bool> is_pinned,
    std::optional<int> new_index) {
  VLOG(2) << __func__;

  std::optional<SavedTabGroup> group = GetGroup(sync_id);
  if (!group.has_value()) {
    return;
  }

  if (is_pinned.has_value() && group->is_pinned() != is_pinned) {
    model_->TogglePinState(sync_id);
  }

  if (new_index.has_value()) {
    model_->ReorderGroupLocally(sync_id, new_index.value());
  }
}

void TabGroupSyncServiceImpl::AddTab(const LocalTabGroupID& group_id,
                                     const LocalTabID& tab_id,
                                     const std::u16string& title,
                                     const GURL& url,
                                     std::optional<size_t> position) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (tab) {
    DVLOG(1) << __func__ << " Called for a tab that already exists";
    return;
  }

  SavedTabGroupTab new_tab(url, title, group->saved_guid(), position,
                           /*saved_tab_guid=*/std::nullopt, tab_id);
  new_tab.SetCreatorCacheGuid(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge());
  new_tab.SetIsPendingSanitization(IsSanitizationRequired(*group, url));

  UpdateAttributions(group_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->AddTabToGroupLocally(group->saved_guid(), std::move(new_tab));
  LogEvent(TabGroupEvent::kTabAdded, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::NavigateTab(const LocalTabGroupID& group_id,
                                          const LocalTabID& tab_id,
                                          const GURL& url,
                                          const std::u16string& title) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // Update attributions for the tab first.
  UpdateAttributions(group_id, tab_id);

  // Use the builder to create the updated tab.
  bool will_update_url = url.SchemeIsHTTPOrHTTPS() && url != tab->url();
  bool is_pending_sanitization = IsSanitizationRequired(*group, url);

  SavedTabGroupTab updated_tab(*tab);
  updated_tab.SetURL(url);
  updated_tab.SetTitle(title);
  updated_tab.SetIsPendingSanitization(is_pending_sanitization);

  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->UpdateTabInGroup(group->saved_guid(), std::move(updated_tab),
                           /*notify_observers=*/will_update_url);
  LogEvent(TabGroupEvent::kTabNavigated, group_id, tab_id);
}

void TabGroupSyncServiceImpl::UpdateTabProperties(
    const LocalTabGroupID& group_id,
    const LocalTabID& tab_id,
    const SavedTabGroupTabBuilder& tab_builder) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  // Use the builder to create the updated tab.
  SavedTabGroupTabBuilder new_builder = tab_builder;
  SavedTabGroupTab updated_tab = new_builder.Build(*tab);
  model_->UpdateTabInGroup(group->saved_guid(), std::move(updated_tab),
                           /*notify_observers=*/false);
}

void TabGroupSyncServiceImpl::RemoveTab(const LocalTabGroupID& group_id,
                                        const LocalTabID& tab_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  base::Uuid sync_id = group->saved_guid();
  UpdateAttributions(group_id);
  LogEvent(TabGroupEvent::kTabRemoved, group_id, tab_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  model_->RemoveTabFromGroupLocally(sync_id, tab->saved_tab_guid());
}

void TabGroupSyncServiceImpl::MoveTab(const LocalTabGroupID& group_id,
                                      const LocalTabID& tab_id,
                                      int new_group_index) {
  VLOG(2) << __func__;
  auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  auto* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  UpdateAttributions(group_id);
  model_->MoveTabInGroupTo(group->saved_guid(), tab->saved_tab_guid(),
                           new_group_index);
  LogEvent(TabGroupEvent::kTabGroupTabsReordered, group_id, std::nullopt);
}

void TabGroupSyncServiceImpl::OnTabSelected(const LocalTabGroupID& group_id,
                                            const LocalTabID& tab_id) {
  VLOG(2) << __func__;
  const SavedTabGroup* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const SavedTabGroupTab* tab = group->GetTab(tab_id);
  if (!tab) {
    DVLOG(1) << __func__ << " Called for a tab that doesn't exist";
    return;
  }

  UpdateAttributions(group_id);
  model_->UpdateLastUserInteractionTimeLocally(group_id);
  LogEvent(TabGroupEvent::kTabSelected, group_id, tab_id);
}

void TabGroupSyncServiceImpl::SaveGroup(SavedTabGroup group) {
  CHECK(is_desktop);
  const base::Uuid sync_id = group.saved_guid();
  const LocalTabGroupID local_id = group.local_group_id().value();
  AddGroup(std::move(group));
  ConnectLocalTabGroup(sync_id, local_id, OpeningSource::kOpenedFromRevisitUi);
}

void TabGroupSyncServiceImpl::UnsaveGroup(const LocalTabGroupID& local_id) {
  CHECK(is_desktop);
  std::optional<SavedTabGroup> group = GetGroup(local_id);
  CHECK(group);
  coordinator_->DisconnectLocalTabGroup(local_id);
  RemoveGroup(group->saved_guid());
}

void TabGroupSyncServiceImpl::MakeTabGroupShared(
    const LocalTabGroupID& local_group_id,
    std::string_view collaboration_id) {
  const SavedTabGroup* saved_group = model_->Get(local_group_id);
  CHECK(saved_group);
  CHECK(!saved_group->is_shared_tab_group());

  // Make a deep copy of the group without fields which are not used in shared
  // tab groups, and without migration of local IDs.
  SavedTabGroup shared_group =
      saved_group->CloneAsSharedTabGroup(std::string(collaboration_id));
  for (auto& tab : shared_group.saved_tabs()) {
    tab.SetIsPendingSanitization(
        IsSanitizationRequired(shared_group, tab.url()));
  }

  // Make a copy before moving the group.
  base::Uuid shared_group_id = shared_group.saved_guid();
  // Clear the pointer before adding the new group as it might invalidate the
  // pointer.
  saved_group = nullptr;
  // TODO(crbug.com/370745855): remove the originating saved tab group from the
  // model afterwards.
  model_->AddedLocally(std::move(shared_group));
}

void TabGroupSyncServiceImpl::MakeTabGroupSharedForTesting(
    const LocalTabGroupID& local_group_id,
    std::string_view collaboration_id) {
  model_->MakeTabGroupSharedForTesting(local_group_id,
                                       std::string(collaboration_id));
}

std::vector<SavedTabGroup> TabGroupSyncServiceImpl::GetAllGroups() const {
  VLOG(2) << __func__;
  std::unordered_set<base::Uuid, base::UuidHash> tab_groups_to_skip;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (group.saved_tabs().empty()) {
      tab_groups_to_skip.insert(group.saved_guid());
    }
    if (group.originating_saved_tab_group_guid().has_value()) {
      // Exclude saved tabs which were used to create a shared tab group.
      tab_groups_to_skip.insert(
          group.originating_saved_tab_group_guid().value());
    }
  }

  std::vector<SavedTabGroup> tab_groups;
  for (const SavedTabGroup& group : model_->saved_tab_groups()) {
    if (tab_groups_to_skip.contains(group.saved_guid())) {
      continue;
    }
    tab_groups.push_back(group);
  }

  return tab_groups;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const base::Uuid& guid) const {
  VLOG(2) << __func__;
  const SavedTabGroup* tab_group = model_->Get(guid);
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::optional<SavedTabGroup> TabGroupSyncServiceImpl::GetGroup(
    const LocalTabGroupID& local_id) const {
  const SavedTabGroup* tab_group = model_->Get(local_id);
  VLOG(2) << __func__;
  return tab_group ? std::make_optional<SavedTabGroup>(*tab_group)
                   : std::nullopt;
}

std::vector<LocalTabGroupID> TabGroupSyncServiceImpl::GetDeletedGroupIds()
    const {
  return GetDeletedGroupIdsFromPref();
}

void TabGroupSyncServiceImpl::OpenTabGroup(
    const base::Uuid& sync_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  CHECK(coordinator_);
  VLOG(2) << __func__;
  coordinator_->HandleOpenTabGroupRequest(sync_group_id, std::move(context));
}

void TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(base::BindOnce(
        &TabGroupSyncServiceImpl::UpdateLocalTabGroupMapping,
        weak_ptr_factory_.GetWeakPtr(), sync_id, local_id, opening_source));
    return;
  }

  VLOG(2) << __func__;

  // If the group was marked as locally-closed in prefs, clear that entry - the
  // group has been reopened.
  RemoveLocallyClosedGroupIdFromPref(sync_id);

  model_->OnGroupOpenedInTabStrip(sync_id, local_id);

  // Record metrics.
  EventDetails event_details(TabGroupEvent::kTabGroupOpened);
  event_details.local_tab_group_id = local_id;
  event_details.opening_source = opening_source;

  const SavedTabGroup* group = model_->Get(local_id);
  metrics_logger_->LogEvent(event_details, group, nullptr);
}

void TabGroupSyncServiceImpl::RemoveLocalTabGroupMapping(
    const LocalTabGroupID& local_id,
    ClosingSource closing_source) {
  VLOG(2) << __func__;
  RemoveDeletedGroupIdFromPref(local_id);

  auto* group = model_->Get(local_id);
  if (!group) {
    return;
  }

  // Record the group's guid as locally-closed in prefs.
  AddLocallyClosedGroupIdToPref(group->saved_guid());

  model_->OnGroupClosedInTabStrip(local_id);

  // Record metrics.
  EventDetails event_details(TabGroupEvent::kTabGroupClosed);
  event_details.local_tab_group_id = local_id;
  event_details.closing_source = closing_source;
  metrics_logger_->LogEvent(event_details, group, nullptr);
}

void TabGroupSyncServiceImpl::UpdateLocalTabId(
    const LocalTabGroupID& local_group_id,
    const base::Uuid& sync_tab_id,
    const LocalTabID& local_tab_id) {
  VLOG(2) << __func__;
  auto* group = model_->Get(local_group_id);
  CHECK(group);

  const auto* tab = group->GetTab(sync_tab_id);
  CHECK(tab);

  model_->UpdateLocalTabId(group->saved_guid(), *tab, local_tab_id);
}

void TabGroupSyncServiceImpl::ConnectLocalTabGroup(
    const base::Uuid& sync_id,
    const LocalTabGroupID& local_id,
    OpeningSource opening_source) {
  CHECK(coordinator_);
  if (!is_initialized_) {
    VLOG(2) << __func__ << " Invoked before init";
    pending_actions_.emplace_back(base::BindOnce(
        &TabGroupSyncServiceImpl::ConnectLocalTabGroup,
        weak_ptr_factory_.GetWeakPtr(), sync_id, local_id, opening_source));
    return;
  }

  VLOG(2) << __func__;
  UpdateLocalTabGroupMapping(sync_id, local_id, opening_source);
  coordinator_->ConnectLocalTabGroup(sync_id, local_id);
}

bool TabGroupSyncServiceImpl::IsRemoteDevice(
    const std::optional<std::string>& cache_guid) const {
  std::optional<std::string> local_cache_guid =
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge();
  if (!local_cache_guid || !cache_guid) {
    return false;
  }

  return local_cache_guid.value() != cache_guid.value();
}

bool TabGroupSyncServiceImpl::WasTabGroupClosedLocally(
    const base::Uuid& sync_tab_group_id) const {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (account_id) {
    return syncer::GetAccountKeyedPrefDictEntry(
        pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
        signin::GaiaIdHash::FromGaiaId(*account_id),
        sync_tab_group_id.AsLowercaseString().c_str());
  }
  return false;
}

void TabGroupSyncServiceImpl::RecordTabGroupEvent(
    const EventDetails& event_details) {
  // Find the group from the passed sync or local ID.
  const SavedTabGroup* group = nullptr;
  if (event_details.local_tab_group_id) {
    group = model_->Get(event_details.local_tab_group_id.value());
  }

  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const SavedTabGroupTab* tab = nullptr;
  if (event_details.local_tab_id) {
    tab = group->GetTab(event_details.local_tab_id.value());
  }

  metrics_logger_->LogEvent(event_details, group, tab);
}

TabGroupSyncMetricsLogger*
TabGroupSyncServiceImpl::GetTabGroupSyncMetricsLogger() {
  return metrics_logger_.get();
}

void TabGroupSyncServiceImpl::HandleTabGroupsReordered(TriggerSource source) {
  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupsReordered(source);
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupReorderedLocally() {
  HandleTabGroupsReordered(TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupReorderedFromSync() {
  HandleTabGroupsReordered(TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedFromSync(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupAddedLocally(
    const base::Uuid& guid) {
  HandleTabGroupAdded(guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::REMOTE);
}

void TabGroupSyncServiceImpl::SavedTabGroupUpdatedLocally(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid) {
  HandleTabGroupUpdated(group_guid, tab_guid, TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedFromSync(
    const SavedTabGroup& removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group.saved_guid();
  id_pair.second = removed_group.local_group_id();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::HandleTabGroupRemoved,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(id_pair), TriggerSource::REMOTE));
}

void TabGroupSyncServiceImpl::SavedTabGroupRemovedLocally(
    const SavedTabGroup& removed_group) {
  std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair;
  id_pair.first = removed_group.saved_guid();
  id_pair.second = removed_group.local_group_id();
  HandleTabGroupRemoved(std::move(id_pair), TriggerSource::LOCAL);
}

void TabGroupSyncServiceImpl::HandleTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  if (!is_initialized_) {
    return;
  }

  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group) {
    return;
  }

  if (saved_tab_group->saved_tabs().empty()) {
    empty_groups_.emplace(guid);
    // Wait for another sync update with tabs before notifying the UI.
    return;
  }

  // Post task is used here to avoid reentrancy. See crbug.com/373500807 for
  // details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TabGroupSyncServiceImpl::NotifyTabGroupAdded,
                                weak_ptr_factory_.GetWeakPtr(), guid, source));
}

void TabGroupSyncServiceImpl::HandleTabGroupUpdated(
    const base::Uuid& group_guid,
    const std::optional<base::Uuid>& tab_guid,
    TriggerSource source) {
  if (!is_initialized_) {
    return;
  }

  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  if (base::Contains(empty_groups_, group_guid)) {
    empty_groups_.erase(group_guid);
    // This is the first time we are notifying the observers about the group as
    // it was empty before.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TabGroupSyncServiceImpl::NotifyTabGroupAdded,
                       weak_ptr_factory_.GetWeakPtr(), group_guid, source));
    return;
  }

  // Post task is used here to avoid reentrancy. See crbug.com/373500807 for
  // details.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::NotifyTabGroupUpdated,
                     weak_ptr_factory_.GetWeakPtr(), group_guid, source));
}

void TabGroupSyncServiceImpl::NotifyTabGroupAdded(const base::Uuid& guid,
                                                  TriggerSource source) {
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  // Saved tab group should be transitions to shared before notifying observers
  // because the new group may be opened automatically on some platforms.
  TransitionSavedToSharedTabGroupIfNeeded(*saved_tab_group);

  for (TabGroupSyncService::Observer& observer : observers_) {
    observer.OnTabGroupAdded(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::NotifyTabGroupUpdated(const base::Uuid& guid,
                                                    TriggerSource source) {
  const SavedTabGroup* saved_tab_group = model_->Get(guid);
  if (!saved_tab_group || saved_tab_group->saved_tabs().empty()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupUpdated(*saved_tab_group, source);
  }
}

void TabGroupSyncServiceImpl::HandleTabGroupRemoved(
    std::pair<base::Uuid, std::optional<LocalTabGroupID>> id_pair,
    TriggerSource source) {
  VLOG(2) << __func__;

  // When a group is deleted, there's no more need to keep any "was locally
  // closed" pref entry around.
  // TODO(crbug.com/363927991): This also gets called during signout, when all
  // groups that belong to the account get closed. In that case, the pref
  // entries should *not* get cleared. Currently this only works because the
  // account_id has already been cleared here, which is fragile. Ideally,
  // HandleTabGroupRemoved() would receive a "reason" param, where one of the
  // possible values would be "signout".
  RemoveLocallyClosedGroupIdFromPref(id_pair.first);

  if (is_initialized_) {
    for (auto& observer : observers_) {
      observer.OnTabGroupRemoved(id_pair.first, source);
    }
  }

  auto local_id = id_pair.second;
  if (!local_id.has_value()) {
    return;
  }

  // For sync initiated deletions, cache the local ID in prefs until the group
  // is closed in the UI.
  if (source == TriggerSource::REMOTE) {
    AddDeletedGroupIdToPref(local_id.value(), id_pair.first);
  }

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupRemoved(local_id.value(), source);
  }
}

std::vector<LocalTabGroupID>
TabGroupSyncServiceImpl::GetDeletedGroupIdsFromPref() const {
  std::vector<LocalTabGroupID> deleted_ids;

  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  base::Value::Dict& pref_data = update.Get();

  for (const auto [serialized_local_id, serialized_sync_id] : pref_data) {
    auto local_id = LocalTabGroupIDFromString(serialized_local_id);
    DCHECK(local_id.has_value());
    if (!local_id.has_value()) {
      continue;
    }

    deleted_ids.emplace_back(local_id.value());
  }

  return deleted_ids;
}

void TabGroupSyncServiceImpl::AddDeletedGroupIdToPref(
    const LocalTabGroupID& local_id,
    const base::Uuid& sync_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Set(LocalTabGroupIDToString(local_id), sync_id.AsLowercaseString());
}

void TabGroupSyncServiceImpl::RemoveDeletedGroupIdFromPref(
    const LocalTabGroupID& local_id) {
  ScopedDictPrefUpdate update(pref_service_, prefs::kDeletedTabGroupIds);
  update->Remove(LocalTabGroupIDToString(local_id));
}

void TabGroupSyncServiceImpl::AddLocallyClosedGroupIdToPref(
    const base::Uuid& sync_id) {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (!account_id) {
    // If there's no signed-in account, nothing to do.
    return;
  }
  syncer::SetAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*account_id),
      sync_id.AsLowercaseString().c_str(), base::Value());
}

void TabGroupSyncServiceImpl::RemoveLocallyClosedGroupIdFromPref(
    const base::Uuid& sync_id) {
  std::optional<std::string> account_id =
      sync_bridge_mediator_->GetAccountIdForSavedBridge();
  if (!account_id) {
    // If there's no signed-in account, nothing to do. Most notably, this
    // happens right after sign-out, when all tab groups associated to the
    // account get closed.
    return;
  }
  syncer::RemoveAccountKeyedPrefDictEntry(
      pref_service_, prefs::kLocallyClosedRemoteTabGroupIds,
      signin::GaiaIdHash::FromGaiaId(*account_id),
      sync_id.AsLowercaseString().c_str());
}

void TabGroupSyncServiceImpl::SavedTabGroupLocalIdChanged(
    const base::Uuid& group_guid) {
  VLOG(2) << __func__;
  const SavedTabGroup* saved_tab_group = model_->Get(group_guid);
  CHECK(saved_tab_group);

  if (!is_initialized_) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTabGroupLocalIdChanged(group_guid,
                                      saved_tab_group->local_group_id());
  }
}

void TabGroupSyncServiceImpl::SavedTabGroupModelLoaded() {
  VLOG(2) << __func__;

  // Store a snapshot of shared tab groups before notifying anyone else that
  // the service is initialized.
  // It is not safe to use observers to listen for Observer::OnInitialized and
  // query for the model at that point for a few reasons:
  // (1) The observer might be added too late, which means some calls could
  //     already be lost.
  // (2) There is a PostTask between the model being finished and observers
  //     being informed. This means that the state could have changed before we
  //     can retrieve it.
  StoreSharedTabGroupsAvailableAtStartupForMessaging();

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::NotifyServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabGroupSyncServiceImpl::NotifyServiceInitialized() {
  VLOG(2) << __func__;

  is_initialized_ = true;

  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    std::move(callback).Run();
  }

  for (auto& observer : observers_) {
    observer.OnInitialized();
  }

  ForceRemoveClosedTabGroupsOnStartup();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabGroupSyncServiceImpl::RecordMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelayBeforeMetricsLogged);
}

void TabGroupSyncServiceImpl::UpdateAttributions(
    const LocalTabGroupID& group_id,
    const std::optional<LocalTabID>& tab_id) {
  model_->UpdateLastUpdaterCacheGuidForGroup(
      sync_bridge_mediator_->GetLocalCacheGuidForSavedBridge(), group_id,
      tab_id);
}

void TabGroupSyncServiceImpl::
    StoreSharedTabGroupsAvailableAtStartupForMessaging() {
  shared_tab_groups_available_at_startup_for_messaging_ =
      std::make_unique<std::vector<SavedTabGroup>>();
  for (const SavedTabGroup* group : model_->GetSharedTabGroupsOnly()) {
    CHECK(group);

    // Dereference to create a safe copy.
    shared_tab_groups_available_at_startup_for_messaging_->push_back(*group);
  }
}

void TabGroupSyncServiceImpl::RecordMetrics() {
  auto saved_tab_groups = model_->saved_tab_groups();
  std::vector<bool> is_remote(saved_tab_groups.size());

  for (size_t i = 0; i < saved_tab_groups.size(); ++i) {
    is_remote[i] = IsRemoteDevice(saved_tab_groups[i].creator_cache_guid());
  }

  metrics_logger_->RecordMetricsOnStartup(saved_tab_groups, is_remote);
}

void TabGroupSyncServiceImpl::ForceRemoveClosedTabGroupsOnStartup() {
  if (!ShouldForceRemoveClosedTabGroupsOnStartup()) {
    return;
  }

  std::vector<base::Uuid> group_ids;
  for (const auto& group : model_->saved_tab_groups()) {
    if (group.local_group_id()) {
      continue;
    }
    group_ids.push_back(group.saved_guid());
  }

  VLOG(0) << __func__
          << " Cleaning up groups on startup, groups# = " << group_ids.size();

  for (const auto& group_id : group_ids) {
    model_->RemovedLocally(group_id);
  }

  metrics_logger_->RecordTabGroupDeletionsOnStartup(group_ids.size());
}

void TabGroupSyncServiceImpl::LogEvent(
    TabGroupEvent event,
    LocalTabGroupID group_id,
    const std::optional<LocalTabID>& tab_id) {
  if (!metrics_logger_) {
    DVLOG(1) << __func__ << " Metrics logger doesn't exist";
    return;
  }

  const auto* group = model_->Get(group_id);
  if (!group) {
    DVLOG(1) << __func__ << " Called for a group that doesn't exist";
    return;
  }

  const auto* tab =
      tab_id.has_value() ? group->GetTab(tab_id.value()) : nullptr;

  EventDetails event_details(event);
  event_details.local_tab_group_id = group_id;
  event_details.local_tab_id = tab_id;
  metrics_logger_->LogEvent(event_details, group, tab);
}

void TabGroupSyncServiceImpl::GetPageTitle(const GURL& url,
                                           GetTitleCallback callback) {
  if (!opt_guide_) {
    std::move(callback).Run(std::u16string());
    return;
  }

  opt_guide_->CanApplyOptimization(
      url, optimization_guide::proto::PAGE_ENTITIES,
      base::BindOnce(&OnPageEntitiesResponseReceived, url,
                     std::move(callback)));
}

bool TabGroupSyncServiceImpl::TransitionSavedToSharedTabGroupIfNeeded(
    const SavedTabGroup& shared_group) {
  if (!shared_group.originating_saved_tab_group_guid().has_value()) {
    return false;
  }

  const SavedTabGroup* originating_saved_group =
      model_->Get(shared_group.originating_saved_tab_group_guid().value());
  if (!originating_saved_group ||
      !originating_saved_group->local_group_id().has_value()) {
    // Originating group doesn't exist in the model or it's not open in the tab
    // strip model. The group may not exist if it was deleted from the current
    // device before the remote shared tab group was downloaded.
    return false;
  }

  // Make a copy because both groups will be updated.
  LocalTabGroupID local_group_id =
      originating_saved_group->local_group_id().value();

  // First, remove the local tab group mapping and then disconnect the local tab
  // group. Note that on some platforms the coordinator may call
  // RemoveLocalTabGroupMapping() but it should be a no-op.
  RemoveLocalTabGroupMapping(local_group_id,
                             ClosingSource::kDisconnectOnGroupShared);
  coordinator_->DisconnectLocalTabGroup(local_group_id);

  // Connect the shared tab group to the local group: update the local tab
  // group mapping on all platforms, and update the mapping for session
  // restore.
  ConnectLocalTabGroup(shared_group.saved_guid(), local_group_id,
                       OpeningSource::kConnectOnGroupShare);

  return true;
}

}  // namespace tab_groups
