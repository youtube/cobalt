// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/non_ui_syncable_service_based_model_type_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// ModelTypeSyncBridge implementation for test-only code-path :(
// This is required to allow calling
// ModelTypeController::ClearMetadataWhileStopped() in browser tests.
// TODO(crbug.com/1418351): Remove test-only code-path.
class FakeSyncableServiceBasedBridge : public ModelTypeSyncBridge {
 public:
  explicit FakeSyncableServiceBasedBridge(
      std::unique_ptr<ModelTypeChangeProcessor> change_processor)
      : ModelTypeSyncBridge(std::move(change_processor)) {
    CHECK_IS_TEST();
  }

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override {
    NOTREACHED();
    return nullptr;
  }
  absl::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> /*metadata_change_list*/,
      EntityChangeList /*entity_data*/) override {
    NOTREACHED();
    return {};
  }
  absl::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> /*metadata_change_list*/,
      EntityChangeList /*entity_changes*/) override {
    NOTREACHED();
    return {};
  }
  void GetData(StorageKeyList /*storage_keys*/,
               DataCallback /*callback*/) override {
    NOTREACHED();
  }
  void GetAllDataForDebugging(DataCallback /*callback*/) override {
    NOTREACHED();
  }
  std::string GetClientTag(const EntityData& /*entity_data*/) override {
    NOTREACHED();
    return {};
  }
  std::string GetStorageKey(const EntityData& /*entity_data*/) override {
    NOTREACHED();
    return {};
  }
};

// Helper object that allows constructing and destructing the
// SyncableServiceBasedBridge on the model thread. Gets constructed on the UI
// thread, but all other operations including destruction happen on the model
// thread.
class BridgeBuilder {
 public:
  BridgeBuilder(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner) {
    DCHECK(store_factory);
    DCHECK(syncable_service_provider);

    // Unretained is safe because destruction also happens on |task_runner_| and
    // can't overtake this task.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BridgeBuilder::InitOnModelThread,
                       base::Unretained(this), type, std::move(store_factory),
                       std::move(syncable_service_provider), dump_stack));
  }

  BridgeBuilder(const BridgeBuilder&) = delete;
  BridgeBuilder& operator=(const BridgeBuilder&) = delete;

  ~BridgeBuilder() { DCHECK(task_runner_->RunsTasksInCurrentSequence()); }

  // Indirectly called for each operation by ProxyModelTypeControllerDelegate.
  base::WeakPtr<ModelTypeControllerDelegate> GetBridgeDelegate() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate();
  }

 private:
  void InitOnModelThread(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!bridge_);

    base::WeakPtr<SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    auto processor =
        std::make_unique<ClientTagBasedModelTypeProcessor>(type, dump_stack);

    // |syncable_service| can be null in tests.
    // TODO(crbug.com/1418351): Remove test-only code-path.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type, std::move(store_factory), std::move(processor),
          syncable_service.get());
    } else {
      bridge_ = std::make_unique<FakeSyncableServiceBasedBridge>(
          std::move(processor));
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;
};

// This is a slightly adapted version of base::OnTaskRunnerDeleter: The one
// difference is that if the destruction request already happens on the target
// sequence, then this avoids posting a task, and instead deletes the given
// object immediately. See https://crbug.com/970354#c19.
struct CustomOnTaskRunnerDeleter {
  explicit CustomOnTaskRunnerDeleter(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~CustomOnTaskRunnerDeleter() = default;

  CustomOnTaskRunnerDeleter(CustomOnTaskRunnerDeleter&&) = default;

  // For compatibility with std:: deleters.
  template <typename T>
  void operator()(const T* ptr) {
    if (!ptr) {
      return;
    }

    if (task_runner_->RunsTasksInCurrentSequence()) {
      delete ptr;
    } else {
      task_runner_->DeleteSoon(FROM_HERE, ptr);
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

ProxyModelTypeControllerDelegate::DelegateProvider BuildDelegateProvider(
    ModelType type,
    OnceModelTypeStoreFactory store_factory,
    NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
        syncable_service_provider,
    const base::RepeatingClosure& dump_stack,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // Can't use std::make_unique or base::WrapUnique because of custom deleter.
  auto bridge_builder =
      std::unique_ptr<BridgeBuilder, CustomOnTaskRunnerDeleter>(
          new BridgeBuilder(type, std::move(store_factory),
                            std::move(syncable_service_provider), dump_stack,
                            task_runner),
          CustomOnTaskRunnerDeleter(task_runner));
  // Note that the binding owns the BridgeBuilder instance.
  return base::BindRepeating(&BridgeBuilder::GetBridgeDelegate,
                             std::move(bridge_builder));
}

}  // namespace

NonUiSyncableServiceBasedModelTypeController::
    NonUiSyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        SyncableServiceProvider syncable_service_provider,
        const base::RepeatingClosure& dump_stack,
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        DelegateMode delegate_mode)
    : ModelTypeController(type) {
  auto full_sync_mode_delegate =
      std::make_unique<ProxyModelTypeControllerDelegate>(
          task_runner,
          BuildDelegateProvider(type, std::move(store_factory),
                                std::move(syncable_service_provider),
                                dump_stack, task_runner));
  // In transport mode we want the same behavior as full sync mode, so we use
  // the same thread-proxying delegate, which shares the BridgeBuilder, which
  // shares the underlying ModelTypeSyncBridge.
  auto transport_mode_delegate =
      delegate_mode == DelegateMode::kTransportModeWithSingleModel
          ? std::make_unique<ForwardingModelTypeControllerDelegate>(
                full_sync_mode_delegate.get())
          : nullptr;
  InitModelTypeController(std::move(full_sync_mode_delegate),
                          std::move(transport_mode_delegate));
}

NonUiSyncableServiceBasedModelTypeController::
    ~NonUiSyncableServiceBasedModelTypeController() = default;

}  // namespace syncer
