// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {

class SyncableService;

// Controller responsible for integrating legacy data type implementations
// (SyncableService) within the new sync architecture (USS), for types living on
// the UI thread.
class SyncableServiceBasedModelTypeController : public ModelTypeController {
 public:
  enum class DelegateMode {
    // The data type runs only in full-sync mode. This is deprecated; new data
    // types should not use this!
    kLegacyFullSyncModeOnly,
    // The data type runs in both full-sync mode and transport mode, and it
    // shares a single data model across both modes (i.e. the data type does not
    // distinguish between syncing users and signed-in non-syncing users).
    kTransportModeWithSingleModel
  };

  // `syncable_service` may be null in tests.
  // `delegate_mode` determines whether only a single delegate (for full-sync
  // mode) will be created, or two separate delegates for both full-sync and
  // transport mode.
  SyncableServiceBasedModelTypeController(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      DelegateMode delegate_mode);

  SyncableServiceBasedModelTypeController(
      const SyncableServiceBasedModelTypeController&) = delete;
  SyncableServiceBasedModelTypeController& operator=(
      const SyncableServiceBasedModelTypeController&) = delete;

  ~SyncableServiceBasedModelTypeController() override;

 private:
  // Delegate owned by this instance; delegate instances passed to the base
  // class forward their calls to `delegate_`.
  std::unique_ptr<ModelTypeControllerDelegate> delegate_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNCABLE_SERVICE_BASED_MODEL_TYPE_CONTROLLER_H_
