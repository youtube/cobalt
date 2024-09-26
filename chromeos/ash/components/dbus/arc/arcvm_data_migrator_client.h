// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARCVM_DATA_MIGRATOR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARCVM_DATA_MIGRATOR_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "dbus/bus.h"
#include "dbus/message.h"

namespace ash {

// ArcVmDataMigratorClient is used to communicate with arcvm-data-migrator.
class COMPONENT_EXPORT(ASH_DBUS_ARC) ArcVmDataMigratorClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called upon receiving a progress signal from arcvm-data-migrator.
    virtual void OnDataMigrationProgress(
        const arc::data_migrator::DataMigrationProgress& progress) = 0;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ArcVmDataMigratorClient* Get();

  // Checks whether the host's /home/root/<hash>/android-data/data has data that
  // needs to be migrated.
  virtual void HasDataToMigrate(
      const arc::data_migrator::HasDataToMigrateRequest& request,
      chromeos::DBusMethodCallback<bool> callback) = 0;

  // Obtains the total size of files under the host's
  // /home/root/<hash>/android-data/data in bytes.
  virtual void GetAndroidDataSize(
      const arc::data_migrator::GetAndroidDataSizeRequest& request,
      chromeos::DBusMethodCallback<int64_t> callback) = 0;

  // Starts the migration.
  virtual void StartMigration(
      const arc::data_migrator::StartMigrationRequest& request,
      chromeos::VoidDBusMethodCallback callback) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  ArcVmDataMigratorClient(const ArcVmDataMigratorClient&) = delete;
  ArcVmDataMigratorClient& operator=(const ArcVmDataMigratorClient&) = delete;

 protected:
  // Initialize() should be used instead.
  ArcVmDataMigratorClient();
  virtual ~ArcVmDataMigratorClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_ARCVM_DATA_MIGRATOR_CLIENT_H_
