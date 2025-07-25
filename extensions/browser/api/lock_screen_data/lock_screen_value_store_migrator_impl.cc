// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/value_store/value_store.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"

namespace extensions {
namespace lock_screen_data {

LockScreenValueStoreMigratorImpl::LockScreenValueStoreMigratorImpl(
    content::BrowserContext* context,
    ValueStoreCache* source_store_cache,
    ValueStoreCache* target_store_cache,
    base::SequencedTaskRunner* task_runner,
    const std::string& crypto_key)
    : context_(context),
      source_store_cache_(source_store_cache),
      target_store_cache_(target_store_cache),
      task_runner_(task_runner),
      crypto_key_(crypto_key) {}

LockScreenValueStoreMigratorImpl::~LockScreenValueStoreMigratorImpl() = default;

void LockScreenValueStoreMigratorImpl::Run(
    const std::set<ExtensionId>& extensions_to_migrate,
    ExtensionMigratedCallback callback) {
  DCHECK(extensions_to_migrate_.empty());
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);
  extensions_to_migrate_ = extensions_to_migrate;

  for (const auto& extension_id : extensions_to_migrate_)
    StartMigrationForExtension(extension_id);
}

bool LockScreenValueStoreMigratorImpl::IsMigratingExtensionData(
    const ExtensionId& extension_id) const {
  return extensions_to_migrate_.count(extension_id) > 0;
}

void LockScreenValueStoreMigratorImpl::ClearDataForExtension(
    const ExtensionId& extension_id,
    base::OnceClosure callback) {
  ClearMigrationData(extension_id);

  DataItem::DeleteAllItemsForExtension(
      context_, target_store_cache_, task_runner_, extension_id,
      base::BindOnce(
          &LockScreenValueStoreMigratorImpl::DeleteItemsFromSourceStore,
          weak_ptr_factory_.GetWeakPtr(), extension_id, std::move(callback)));
}

LockScreenValueStoreMigratorImpl::MigrationData::MigrationData() = default;
LockScreenValueStoreMigratorImpl::MigrationData::~MigrationData() = default;

void LockScreenValueStoreMigratorImpl::StartMigrationForExtension(
    const ExtensionId& extension_id) {
  DataItem::GetRegisteredValuesForExtension(
      context_, source_store_cache_, task_runner_, extension_id,
      base::BindOnce(&LockScreenValueStoreMigratorImpl::OnGotItemsForExtension,
                     weak_ptr_factory_.GetWeakPtr(), extension_id));
}

void LockScreenValueStoreMigratorImpl::OnGotItemsForExtension(
    const ExtensionId& extension_id,
    OperationResult result,
    base::Value::Dict items) {
  if (!IsMigratingExtensionData(extension_id))
    return;

  if (result != OperationResult::kSuccess) {
    ClearMigrationData(extension_id);
    callback_.Run(extension_id);
    return;
  }

  for (const auto item : items) {
    migration_items_[extension_id].pending.emplace_back(
        std::make_unique<DataItem>(item.first, extension_id, context_,
                                   source_store_cache_, task_runner_,
                                   crypto_key_));
  }

  MigrateNextForExtension(extension_id);
}

void LockScreenValueStoreMigratorImpl::MigrateNextForExtension(
    const ExtensionId& extension_id) {
  if (!IsMigratingExtensionData(extension_id))
    return;

  // If nothing is left to migrate, run the migration callback for the
  // extension.
  if (migration_items_[extension_id].pending.empty()) {
    ClearMigrationData(extension_id);
    callback_.Run(extension_id);
    return;
  }

  migration_items_[extension_id].current_source =
      std::move(migration_items_[extension_id].pending.back());
  migration_items_[extension_id].pending.pop_back();

  migration_items_[extension_id].current_target = std::make_unique<DataItem>(
      migration_items_[extension_id].current_source->id(), extension_id,
      context_, target_store_cache_, task_runner_, crypto_key_);

  migration_items_[extension_id].current_source->Read(
      base::BindOnce(&LockScreenValueStoreMigratorImpl::OnCurrentItemRead,
                     weak_ptr_factory_.GetWeakPtr(), extension_id));
}

void LockScreenValueStoreMigratorImpl::OnCurrentItemRead(
    const ExtensionId& extension_id,
    OperationResult result,
    std::unique_ptr<std::vector<char>> data) {
  if (!IsMigratingExtensionData(extension_id))
    return;

  // Skip items encrypted with a different key - they belong to different
  // context.
  if (result == OperationResult::kWrongKey) {
    MigrateNextForExtension(extension_id);
    return;
  }

  if (result == OperationResult::kSuccess) {
    migration_items_[extension_id].current_target->Register(base::BindOnce(
        &LockScreenValueStoreMigratorImpl::OnTargetItemRegistered,
        weak_ptr_factory_.GetWeakPtr(), extension_id, std::move(data)));
  } else {
    OnTargetItemWritten(extension_id, OperationResult::kFailed);
  }
}

void LockScreenValueStoreMigratorImpl::OnTargetItemRegistered(
    const ExtensionId& extension_id,
    std::unique_ptr<std::vector<char>> data,
    OperationResult result) {
  if (!IsMigratingExtensionData(extension_id))
    return;

  // Ignore already registered error - the item might have been registered
  // during previous, unfinished migration attempt.
  if (result == OperationResult::kSuccess ||
      result == OperationResult::kAlreadyRegistered) {
    migration_items_[extension_id].current_target->Write(
        *data,
        base::BindOnce(&LockScreenValueStoreMigratorImpl::OnTargetItemWritten,
                       weak_ptr_factory_.GetWeakPtr(), extension_id));
  } else {
    OnTargetItemWritten(extension_id, OperationResult::kFailed);
  }
}

void LockScreenValueStoreMigratorImpl::OnTargetItemWritten(
    const ExtensionId& extension_id,
    OperationResult result) {
  if (!IsMigratingExtensionData(extension_id))
    return;

  // Make best effort attempt to delete new item if the item migration failed.
  if (result != OperationResult::kSuccess)
    migration_items_[extension_id].current_target->Delete(base::DoNothing());

  migration_items_[extension_id].current_source->Delete(
      base::BindOnce(&LockScreenValueStoreMigratorImpl::OnCurrentItemMigrated,
                     weak_ptr_factory_.GetWeakPtr(), extension_id));
}

void LockScreenValueStoreMigratorImpl::OnCurrentItemMigrated(
    const ExtensionId& extension_id,
    OperationResult result) {
  MigrateNextForExtension(extension_id);
}

void LockScreenValueStoreMigratorImpl::DeleteItemsFromSourceStore(
    const ExtensionId& extension_id,
    base::OnceClosure callback) {
  DataItem::DeleteAllItemsForExtension(
      context_, source_store_cache_, task_runner_, extension_id,
      base::BindOnce(
          &LockScreenValueStoreMigratorImpl::RunClearDataForExtensionCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LockScreenValueStoreMigratorImpl::RunClearDataForExtensionCallback(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void LockScreenValueStoreMigratorImpl::ClearMigrationData(
    const ExtensionId& extension_id) {
  extensions_to_migrate_.erase(extension_id);

  migration_items_[extension_id].pending.clear();
  migration_items_[extension_id].current_source.reset();
  migration_items_[extension_id].current_target.reset();
}

}  // namespace lock_screen_data
}  // namespace extensions
