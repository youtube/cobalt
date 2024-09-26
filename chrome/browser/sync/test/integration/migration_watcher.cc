// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/migration_watcher.h"

#include "chrome/browser/sync/test/integration/migration_waiter.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "components/sync/driver/sync_service_impl.h"

MigrationWatcher::MigrationWatcher(SyncServiceImplHarness* harness)
    : harness_(harness), migration_waiter_(nullptr) {
  syncer::BackendMigrator* migrator =
      harness_->service()->GetBackendMigratorForTest();
  // PSS must have a migrator after sync is setup and initial data type
  // configuration is complete.
  migrator->AddMigrationObserver(this);
}

MigrationWatcher::~MigrationWatcher() {
  DCHECK(!migration_waiter_);
}

bool MigrationWatcher::HasPendingBackendMigration() const {
  syncer::BackendMigrator* migrator =
      harness_->service()->GetBackendMigratorForTest();
  return migrator && migrator->state() != syncer::BackendMigrator::IDLE;
}

syncer::ModelTypeSet MigrationWatcher::GetMigratedTypes() const {
  return migrated_types_;
}

void MigrationWatcher::OnMigrationStateChange() {
  if (HasPendingBackendMigration()) {
    // A new bunch of data types are in the process of being migrated. Merge
    // them into |pending_types_|.
    pending_types_.PutAll(harness_->service()
                              ->GetBackendMigratorForTest()
                              ->GetPendingMigrationTypesForTest());
    DVLOG(1) << harness_->profile_debug_name()
             << ": new pending migration types "
             << syncer::ModelTypeSetToDebugString(pending_types_);
  } else {
    // Migration just finished for a bunch of data types. Merge them into
    // |migrated_types_|.
    migrated_types_.PutAll(pending_types_);
    pending_types_.Clear();
    DVLOG(1) << harness_->profile_debug_name() << ": new migrated types "
             << syncer::ModelTypeSetToDebugString(migrated_types_);
  }

  // Manually trigger a check of the exit condition.
  if (migration_waiter_) {
    migration_waiter_->OnMigrationStateChange();
  }
}

void MigrationWatcher::set_migration_waiter(MigrationWaiter* waiter) {
  DCHECK(!migration_waiter_);
  migration_waiter_ = waiter;
}

void MigrationWatcher::clear_migration_waiter() {
  DCHECK(migration_waiter_);
  migration_waiter_ = nullptr;
}
