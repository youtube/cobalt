// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/arc_net_host_impl.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"

namespace arc {
namespace {

class ArcNetHostImplTest : public testing::Test {
 public:
  ArcNetHostImplTest(const ArcNetHostImplTest&) = delete;
  ArcNetHostImplTest& operator=(const ArcNetHostImplTest&) = delete;

 protected:
  ArcNetHostImplTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        context_(std::make_unique<user_prefs::TestBrowserContextWithPrefs>()),
        service_(
            ArcNetHostImpl::GetForBrowserContextForTesting(context_.get())) {
    arc::prefs::RegisterProfilePrefs(pref_service()->registry());
    service()->SetPrefService(pref_service());
  }

  ~ArcNetHostImplTest() override { service_->Shutdown(); }

  void SetUp() override { ash::PatchPanelClient::InitializeFake(); }

  void TearDown() override { ash::PatchPanelClient::Shutdown(); }

  ArcNetHostImpl* service() { return service_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<user_prefs::TestBrowserContextWithPrefs> context_;
  const raw_ptr<ArcNetHostImpl, ExperimentalAsh> service_;
};

TEST_F(ArcNetHostImplTest, SetAlwaysOnVpn_SetPackage) {
  EXPECT_EQ(false, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ("", pref_service()->GetString(prefs::kAlwaysOnVpnPackage));

  const std::string vpn_package = "com.android.vpn";
  const bool lockdown = true;

  service()->SetAlwaysOnVpn(vpn_package, lockdown);

  EXPECT_EQ(lockdown, pref_service()->GetBoolean(prefs::kAlwaysOnVpnLockdown));
  EXPECT_EQ(vpn_package, pref_service()->GetString(prefs::kAlwaysOnVpnPackage));
}

TEST_F(ArcNetHostImplTest, NotifyAndroidWifiMulticastLockChange) {
  int cnt1 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();
  service()->NotifyAndroidWifiMulticastLockChange(true);
  int cnt2 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();

  service()->NotifyAndroidWifiMulticastLockChange(false);
  int cnt3 = ash::FakePatchPanelClient::Get()
                 ->GetAndroidWifiMulticastLockChangeNotifyCount();

  EXPECT_EQ(1, cnt2 - cnt1);
  EXPECT_EQ(1, cnt3 - cnt2);
}

}  // namespace
}  // namespace arc
