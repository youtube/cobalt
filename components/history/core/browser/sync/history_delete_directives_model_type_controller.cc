// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_delete_directives_model_type_controller.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace history {

namespace {

base::WeakPtr<syncer::SyncableService> GetSyncableServiceFromHistoryService(
    HistoryService* history_service) {
  if (history_service) {
    return history_service->GetDeleteDirectivesSyncableService();
  }
  return nullptr;
}

using DelegateMode =
    syncer::SyncableServiceBasedModelTypeController::DelegateMode;

DelegateMode GetDelegateMode() {
  // Transport mode is only supported if if `kReplaceSyncPromosWithSignInPromos`
  // is enabled.
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return DelegateMode::kTransportModeWithSingleModel;
  }
  return DelegateMode::kLegacyFullSyncModeOnly;
}

}  // namespace

HistoryDeleteDirectivesModelTypeController::
    HistoryDeleteDirectivesModelTypeController(
        const base::RepeatingClosure& dump_stack,
        syncer::SyncService* sync_service,
        syncer::ModelTypeStoreService* model_type_store_service,
        HistoryService* history_service,
        PrefService* pref_service)
    : SyncableServiceBasedModelTypeController(
          syncer::HISTORY_DELETE_DIRECTIVES,
          model_type_store_service->GetStoreFactory(),
          GetSyncableServiceFromHistoryService(history_service),
          dump_stack,
          GetDelegateMode()),
      helper_(syncer::HISTORY_DELETE_DIRECTIVES, sync_service, pref_service) {}

HistoryDeleteDirectivesModelTypeController::
    ~HistoryDeleteDirectivesModelTypeController() = default;

syncer::DataTypeController::PreconditionState
HistoryDeleteDirectivesModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  if (helper_.sync_service()->GetUserSettings()->IsEncryptEverythingEnabled()) {
    return PreconditionState::kMustStopAndClearData;
  }
  return helper_.GetPreconditionState();
}

void HistoryDeleteDirectivesModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(NOT_RUNNING, state());

  sync_service_observation_.Observe(helper_.sync_service());
  SyncableServiceBasedModelTypeController::LoadModels(configure_context,
                                                      model_load_callback);
}

void HistoryDeleteDirectivesModelTypeController::Stop(
    syncer::SyncStopMetadataFate fate,
    StopCallback callback) {
  DCHECK(CalledOnValidThread());

  sync_service_observation_.Reset();

  SyncableServiceBasedModelTypeController::Stop(fate, std::move(callback));
}

void HistoryDeleteDirectivesModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  // Most of these calls will be no-ops but SyncService handles that just fine.
  helper_.sync_service()->DataTypePreconditionChanged(type());
}

}  // namespace history
