// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/condition_variable.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/geolocation/system_geolocation_source_lacros.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/geolocation.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"

namespace {

using SystemGeolocationSourceLacrosTests = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests, PrefChange) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  // As we are adding the crosapi change to ash in the same commit, we may be
  // missing the Pref when run with older versions of ash. Hence we'll skip this
  // test when the preference is not available.
  const int min_version = 8;
  const int version = chromeos::LacrosService::Get()
                          ->GetInterfaceVersion<crosapi::mojom::Prefs>();
  if (min_version > version) {
    GTEST_SKIP() << "Skipping as the geolocation pref is not available in the"
                    "current version of crosapi ("
                 << version << ")";
  }

  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

  // By default, the the geolocation is allowed in ash.
  base::test::TestFuture<absl::optional<::base::Value>> future;
  prefs->GetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 future.GetCallback());

  auto out_value = future.Take();
  EXPECT_TRUE(out_value.has_value());
  EXPECT_TRUE(out_value.value().GetBool());

  // Set up the system source to save the pref changes into a future object
  SystemGeolocationSourceLacros source;
  base::test::RepeatingTestFuture<device::LocationSystemPermissionStatus>
      status;

  source.RegisterPermissionUpdateCallback(status.GetCallback());
  // Wait for status to be asynchronously updated.

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());

  // Change the value in ash.
  base::test::TestFuture<void> set_future;
  prefs->SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 ::base::Value(false), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());
  set_future.Clear();

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied, status.Take());

  // Change the value in ash.
  prefs->SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 ::base::Value(true), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed, status.Take());
}

// TODO(b/293398125): re-enable
IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests,
                       DISABLED_IntegrationToBrowser) {
  class Observer : public device::GeolocationManager::PermissionObserver {
   public:
    // device::GeolocationManager::PermissionObserver:
    void OnSystemPermissionUpdated(
        device::LocationSystemPermissionStatus status) override {
      status_.AddValue(std::move(status));
    }
    base::test::RepeatingTestFuture<device::LocationSystemPermissionStatus>
        status_;
  };

  device::GeolocationManager* manager =
      device::GeolocationManager::GetInstance();
  ASSERT_TRUE(manager);

  Observer observer;
  manager->AddObserver(&observer);

  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::Prefs>());

  base::test::TestFuture<absl::optional<::base::Value>> future;
  auto& prefs =
      chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();

  // By default, the the geolocation is allowed in ash.
  prefs->GetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 future.GetCallback());

  // As we are adding the crosapi change to ash in the same commit, we may be
  // missing the Pref when run with older versions of ash. Hence we'll skip this
  // test when the preference is not available.
  auto out_value = future.Take();
  if (!out_value.has_value()) {
    GTEST_SKIP() << "Skipping as the geolocation pref is not available in the "
                    "current version of Ash";
  }

  ASSERT_TRUE(out_value.has_value());
  ASSERT_TRUE(out_value.value().GetBool());

  // Initial value should be to allow.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            manager->GetSystemPermission());

  // Change the value in ash.
  base::test::TestFuture<void> set_future;
  prefs->SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 ::base::Value(false), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kDenied,
            observer.status_.Take());

  // Change the value in ash.
  prefs->SetPref(crosapi::mojom::PrefPath::kGeolocationAllowed,
                 ::base::Value(true), set_future.GetCallback());
  EXPECT_TRUE(set_future.Wait());

  // Check that the change in pref was registered.
  EXPECT_EQ(device::LocationSystemPermissionStatus::kAllowed,
            observer.status_.Take());
}

IN_PROC_BROWSER_TEST_F(SystemGeolocationSourceLacrosTests, ActivityTracking) {
  // This tests that the activity tracking calls on the GeolocationManager are
  // routed via the crosapi to the appropriate methods in the mock
  // implementation.

  auto* lacros_service = chromeos::LacrosService::Get();
  EXPECT_TRUE(lacros_service);
  EXPECT_TRUE(
      lacros_service->IsRegistered<crosapi::mojom::GeolocationService>());
  EXPECT_TRUE(
      lacros_service->IsAvailable<crosapi::mojom::GeolocationService>());

  // As we are adding the crosapi change to ash in the same commit, we may be
  // appropriate method for activity tracking in Ash. Hence we'll skip this
  // test in such a case.
  const int min_version = static_cast<int>(
      crosapi::mojom::GeolocationService::kTrackGeolocationAttemptedMinVersion);
  const int version =
      lacros_service->GetInterfaceVersion<crosapi::mojom::GeolocationService>();
  if (min_version > version) {
    GTEST_SKIP()
        << "Skipping as the GeolocationService on the Ash side is too old ("
        << version << "<" << min_version << ")";
  }

  class MockGeolocationService : public crosapi::mojom::GeolocationService {
   public:
    MockGeolocationService() : receiver_(this) {}
    MockGeolocationService(const MockGeolocationService&) = delete;
    MockGeolocationService& operator=(const MockGeolocationService&) = delete;
    ~MockGeolocationService() override = default;

    // void BindReceiver(mojo::PendingReceiver<mojom::GeolocationService>
    // receiver);
    mojo::PendingRemote<crosapi::mojom::GeolocationService>
    BindNewPipeAndPassRemote() {
      return receiver_.BindNewPipeAndPassRemote();
    }

    // crosapi::mojom::GeolocationService:
    void GetWifiAccessPoints(GetWifiAccessPointsCallback callback) override {}
    void TrackGeolocationAttempted(const std::string& name) override {
      ++usage_cnt_;
    }
    void TrackGeolocationRelinquished(const std::string& name) override {
      --usage_cnt_;
    }

    int usage_cnt_ = 0;
    mojo::Receiver<crosapi::mojom::GeolocationService> receiver_;
  };

  // Instantiate the mock service and inject it to be used by crosapi as
  // endpoint
  MockGeolocationService mock;
  lacros_service->InjectRemoteForTesting(mock.BindNewPipeAndPassRemote());

  EXPECT_EQ(0, mock.usage_cnt_);

  device::GeolocationManager* manager =
      device::GeolocationManager::GetInstance();
  EXPECT_TRUE(manager);

  manager->TrackGeolocationAttempted();
  mock.receiver_.WaitForIncomingCall();
  EXPECT_EQ(1, mock.usage_cnt_);

  manager->TrackGeolocationAttempted();
  manager->TrackGeolocationAttempted();
  mock.receiver_.FlushForTesting();
  EXPECT_EQ(3, mock.usage_cnt_);

  manager->TrackGeolocationRelinquished();
  mock.receiver_.WaitForIncomingCall();
  EXPECT_EQ(2, mock.usage_cnt_);

  manager->TrackGeolocationRelinquished();
  manager->TrackGeolocationRelinquished();
  mock.receiver_.FlushForTesting();
  EXPECT_EQ(0, mock.usage_cnt_);
}

}  // namespace
