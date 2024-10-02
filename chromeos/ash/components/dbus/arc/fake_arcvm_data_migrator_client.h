// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Fake implementation of ArcVmDataMigratorClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcVmDataMigratorClient
    : public ArcVmDataMigratorClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcVmDataMigratorClient* Get();

  // ArcVmDataMigratorClient overrides:
  void HasDataToMigrate(
      const arc::data_migrator::HasDataToMigrateRequest& request,
      chromeos::DBusMethodCallback<bool> callback) override;
  void GetAndroidDataSize(
      const arc::data_migrator::GetAndroidDataSizeRequest& request,
      chromeos::DBusMethodCallback<int64_t> callback) override;
  void StartMigration(const arc::data_migrator::StartMigrationRequest& request,
                      chromeos::VoidDBusMethodCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  FakeArcVmDataMigratorClient(const FakeArcVmDataMigratorClient&) = delete;
  FakeArcVmDataMigratorClient& operator=(const FakeArcVmDataMigratorClient&) =
      delete;

  void SendDataMigrationProgress(
      const arc::data_migrator::DataMigrationProgress& progress);

  void set_has_data_to_migrate(absl::optional<bool> has_data_to_migrate) {
    has_data_to_migrate_ = has_data_to_migrate;
  }

  void set_android_data_size(absl::optional<int64_t> android_data_size) {
    android_data_size_ = android_data_size;
  }

 protected:
  friend class ArcVmDataMigratorClient;

  FakeArcVmDataMigratorClient();
  ~FakeArcVmDataMigratorClient() override;

 private:
  base::ObserverList<Observer> observers_;

  absl::optional<bool> has_data_to_migrate_ = true;
  absl::optional<int64_t> android_data_size_ = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_
