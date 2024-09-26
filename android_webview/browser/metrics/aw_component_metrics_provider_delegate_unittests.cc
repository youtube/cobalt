// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_component_metrics_provider_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/lifecycle/webview_app_state_observer.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/component_updater/android/components_info_holder.h"
#include "components/component_updater/component_updater_service.h"
#include "components/metrics/component_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using component_updater::ComponentInfo;

namespace android_webview {

namespace {

class AwMetricsServiceClientTestDelegate
    : public AwMetricsServiceClient::Delegate {
  void RegisterAdditionalMetricsProviders(
      metrics::MetricsService* service) override {}
  void AddWebViewAppStateObserver(WebViewAppStateObserver* observer) override {}
  bool HasAwContentsEverCreated() const override { return false; }
};

class AwComponentMetricsProviderDelegateTest : public testing::Test {
 protected:
  AwComponentMetricsProviderDelegateTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        prefs_(std::make_unique<TestingPrefServiceSimple>()),
        client_(std::make_unique<AwMetricsServiceClient>(
            std::make_unique<AwMetricsServiceClientTestDelegate>())) {
    // Required by MetricsService.
    base::SetRecordActionTaskRunner(task_runner_);
    AwMetricsServiceClient::RegisterMetricsPrefs(prefs_->registry());

    client_->Initialize(prefs_.get());
  }

  AwMetricsServiceClient* GetClient() { return client_.get(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<AwMetricsServiceClient> client_;
};

}  // namespace

TEST_F(AwComponentMetricsProviderDelegateTest,
       TestAppsPackageNamesComponent_NotLoaded) {
  metrics::ComponentMetricsProvider provider(
      std::make_unique<AwComponentMetricsProviderDelegate>(GetClient()));

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_TRUE(system_profile.chrome_component().empty());
}

TEST_F(AwComponentMetricsProviderDelegateTest,
       TestAppsPackageNamesComponent_Loaded) {
  metrics::ComponentMetricsProvider provider(
      std::make_unique<AwComponentMetricsProviderDelegate>(GetClient()));

  std::string allowlist_version = "123.456.78.9";
  GetClient()->SetAppPackageNameLoggingRule(AppPackageNameLoggingRule(
      base::Version(allowlist_version), base::Time::Now()));

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  ASSERT_EQ(1, system_profile.chrome_component().size());
  metrics::SystemProfileProto::ChromeComponent allowlist_component =
      system_profile.chrome_component()[0];
  EXPECT_EQ(
      metrics::
          SystemProfileProto_ComponentId_WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST,
      allowlist_component.component_id());
  EXPECT_EQ(allowlist_version, allowlist_component.version());
}

TEST_F(AwComponentMetricsProviderDelegateTest,
       TestAppsPackageNamesComponent_CachedIsDifferentFromLoaded) {
  metrics::ComponentMetricsProvider provider(
      std::make_unique<AwComponentMetricsProviderDelegate>(GetClient()));

  std::string allowlist_version = "123.456.78.9";
  GetClient()->SetAppPackageNameLoggingRule(AppPackageNameLoggingRule(
      base::Version(allowlist_version), base::Time::Now()));

  component_updater::ComponentsInfoHolder::GetInstance()->AddComponent(
      kWebViewAppsPackageNamesAllowlistComponentId,
      base::Version("222.333.444.555"));

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  ASSERT_EQ(1, system_profile.chrome_component().size());
  metrics::SystemProfileProto::ChromeComponent allowlist_component =
      system_profile.chrome_component()[0];
  EXPECT_EQ(
      metrics::
          SystemProfileProto_ComponentId_WEBVIEW_APPS_PACKAGE_NAMES_ALLOWLIST,
      allowlist_component.component_id());
  EXPECT_EQ(allowlist_version, allowlist_component.version());
}

TEST_F(AwComponentMetricsProviderDelegateTest, TestMultipleComponents) {
  AwComponentMetricsProviderDelegate delegate(GetClient());

  std::string fake_component_id = "abcdefgh";
  base::Version fake_component_version("123.456.78.9");
  component_updater::ComponentsInfoHolder::GetInstance()->AddComponent(
      fake_component_id, fake_component_version);
  component_updater::ComponentsInfoHolder::GetInstance()->AddComponent(
      kWebViewAppsPackageNamesAllowlistComponentId,
      base::Version("222.333.444.555"));

  std::vector<ComponentInfo> components = delegate.GetComponents();
  ASSERT_EQ(1u, components.size());
  EXPECT_EQ(fake_component_id, components[0].id);
  EXPECT_EQ(fake_component_version, components[0].version);
}

}  // namespace android_webview