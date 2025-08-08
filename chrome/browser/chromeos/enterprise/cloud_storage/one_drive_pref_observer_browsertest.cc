// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/one_drive_pref_observer.h"

#include <string>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/dependency_graph.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

using extensions::api::odfs_config_private::Mount;
using testing::ElementsAreArray;

namespace {
constexpr char kMicrosoft365PWAStartUrl[] =
    "https://www.microsoft365.com/?from=Homescreen";
}  // namespace

namespace chromeos::cloud_storage {

class OneDrivePrefObserverBrowserTest : public policy::PolicyTest {
 public:
  OneDrivePrefObserverBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kUploadOfficeToCloud,
         chromeos::features::kMicrosoftOneDriveIntegrationForEnterprise},
        {});
  }
  ~OneDrivePrefObserverBrowserTest() override = default;

 protected:
  void SetOneDriveMount(const std::string& mount) {
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kMicrosoftOneDriveMount, base::Value(mount));
    provider_.UpdateChromePolicy(policies);
  }

  void SetOneDriveAccountRestrictions(std::vector<std::string> restrictions) {
    base::Value::List restrictions_list;
    for (auto& restriction : restrictions) {
      restrictions_list.Append(std::move(restriction));
    }
    policy::PolicyMap policies;
    policy::PolicyTest::SetPolicy(
        &policies, policy::key::kMicrosoftOneDriveAccountRestrictions,
        base::Value(std::move(restrictions_list)));
    provider_.UpdateChromePolicy(policies);
  }

  bool OneDrivePrefObserverServiceExists() {
    std::vector<raw_ptr<DependencyNode, VectorExperimental>> nodes;
    const bool success = BrowserContextDependencyManager::GetInstance()
                             ->GetDependencyGraphForTesting()
                             .GetConstructionOrder(&nodes);
    EXPECT_TRUE(success);
    return base::Contains(
        nodes, "OneDrivePrefObserverFactory",
        [](const DependencyNode* node) -> std::string_view {
          return static_cast<const KeyedServiceBaseFactory*>(node)->name();
        });
  }

  void InstallMicrosoft365() {
    auto m365_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(kMicrosoft365PWAStartUrl));
    const webapps::AppId m365_app_id =
        web_app::test::InstallWebApp(profile(), std::move(m365_app_info));
    EXPECT_EQ(ash::kMicrosoft365AppId, m365_app_id);
  }

  void CheckMountChangedEvent(const extensions::Event& event,
                              const std::string& expected_mode) {
    EXPECT_EQ(event.event_name,
              extensions::api::odfs_config_private::OnMountChanged::kEventName);
    ASSERT_EQ(1u, event.event_args.size());
    const base::Value::Dict* event_dict = event.event_args.front().GetIfDict();
    ASSERT_TRUE(event_dict);
    const std::string* mode = event_dict->FindString("mode");
    ASSERT_TRUE(mode);
    EXPECT_THAT(*mode, expected_mode);
  }

  void CheckRestrictionChangedEvent(
      const extensions::Event& event,
      const std::vector<std::string>& expected_restrictions) {
    EXPECT_EQ(event.event_name, extensions::api::odfs_config_private::
                                    OnAccountRestrictionsChanged::kEventName);
    ASSERT_EQ(1u, event.event_args.size());
    const base::Value::Dict* event_dict = event.event_args.front().GetIfDict();
    ASSERT_TRUE(event_dict);
    const base::Value::List* restrictions =
        event_dict->FindList("restrictions");
    ASSERT_TRUE(restrictions);
    EXPECT_THAT(*restrictions, ElementsAreArray(expected_restrictions));
  }

  void CheckM365SupportedLinkDefaultPrefSet(bool value) {
    PrefService* pref_service = profile()->GetPrefs();
    ASSERT_TRUE(pref_service);
    EXPECT_EQ(value,
              pref_service->GetBoolean(prefs::kM365SupportedLinkDefaultSet));
  }

  Profile* profile() { return browser()->profile(); }

  const extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(profile());
  }

  extensions::ExtensionService* extension_service() {
    return extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return browser()->profile()->GetProfilePolicyConnector();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       KeyedServiceRegistered) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  ASSERT_TRUE(OneDrivePrefObserverServiceExists());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_NE(crosapi::browser_util::IsLacrosEnabled(),
            OneDrivePrefObserverServiceExists());
#else
  NOTREACHED();
#endif
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       OneDriveModeChangedEventTriggered) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  SetOneDriveMount(ToString(Mount::kAutomated));
  ASSERT_EQ(1u, observer.events().size());
  CheckMountChangedEvent(*observer.events().begin()->second, "automated");
  observer.ClearEvents();

  SetOneDriveMount(ToString(Mount::kAutomated));
  ASSERT_EQ(0u, observer.events().size());

  SetOneDriveMount(ToString(Mount::kAllowed));
  ASSERT_EQ(1u, observer.events().size());
  CheckMountChangedEvent(*observer.events().begin()->second, "allowed");
  observer.ClearEvents();

  SetOneDriveMount(ToString(Mount::kDisallowed));
  ASSERT_EQ(1u, observer.events().size());
  CheckMountChangedEvent(*observer.events().begin()->second, "disallowed");
  observer.ClearEvents();

  SetOneDriveMount(ToString(Mount::kNone));
  ASSERT_EQ(1u, observer.events().size());
  CheckMountChangedEvent(*observer.events().begin()->second, "");
  observer.ClearEvents();
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       OneDriveAccountRestrictionsChangedEventTriggered) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  extensions::TestEventRouterObserver observer(event_router);

  SetOneDriveAccountRestrictions({"https://www.google.com"});
  ASSERT_EQ(1u, observer.events().size());
  CheckRestrictionChangedEvent(*observer.events().begin()->second,
                               {"https://www.google.com"});
  observer.ClearEvents();

  SetOneDriveAccountRestrictions({"https://www.google.com"});
  ASSERT_EQ(0u, observer.events().size());

  SetOneDriveAccountRestrictions(
      {"https://www.google.com", "https://chromium.org"});
  ASSERT_EQ(1u, observer.events().size());
  CheckRestrictionChangedEvent(
      *observer.events().begin()->second,
      {"https://www.google.com", "https://chromium.org"});
  observer.ClearEvents();

  SetOneDriveAccountRestrictions({"common"});
  ASSERT_EQ(1u, observer.events().size());
  CheckRestrictionChangedEvent(*observer.events().begin()->second, {"common"});
  observer.ClearEvents();

  SetOneDriveAccountRestrictions({"abcd1234-1234-1234-1234-1234abcd1234"});
  ASSERT_EQ(1u, observer.events().size());
  CheckRestrictionChangedEvent(*observer.events().begin()->second,
                               {"abcd1234-1234-1234-1234-1234abcd1234"});
  observer.ClearEvents();
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       OdfsExtensionUninstalledOnDisallowed) {
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  SetOneDriveMount(ToString(Mount::kAutomated));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Odfs extension")
          .SetID(extension_misc::kODFSExtensionId)
          .Build();
  extension_service()->AddExtension(extension.get());
  ASSERT_TRUE(extension_registry()->enabled_extensions().Contains(
      extension_misc::kODFSExtensionId));

  SetOneDriveMount(ToString(Mount::kDisallowed));
  ASSERT_FALSE(extension_registry()->GetExtensionById(
      extension_misc::kODFSExtensionId,
      extensions::ExtensionRegistry::IncludeFlag::EVERYTHING));
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       UnmanagedOdfsExtensionNotUninstalledOnDisallowed) {
  profile_policy_connector()->OverrideIsManagedForTesting(false);
  SetOneDriveMount(ToString(Mount::kAutomated));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Odfs extension")
          .SetID(extension_misc::kODFSExtensionId)
          .Build();
  extension_service()->AddExtension(extension.get());
  ASSERT_TRUE(extension_registry()->enabled_extensions().Contains(
      extension_misc::kODFSExtensionId));

  SetOneDriveMount(ToString(Mount::kDisallowed));
  ASSERT_TRUE(extension_registry()->GetExtensionById(
      extension_misc::kODFSExtensionId,
      extensions::ExtensionRegistry::IncludeFlag::EVERYTHING));
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       SetSupportedLinksPreferenceForM365SuccessOnInstall) {
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  SetOneDriveMount(ToString(Mount::kAutomated));
  InstallMicrosoft365();

  ASSERT_TRUE(apps::AppServiceProxyFactory::GetForProfile(profile())
                  ->PreferredAppsList()
                  .IsPreferredAppForSupportedLinks(ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(true);
}

IN_PROC_BROWSER_TEST_F(
    OneDrivePrefObserverBrowserTest,
    PRE_SetSupportedLinksPreferenceForM365SuccessOnAppReadiness) {
  // Install Microsoft365 while Clippy is deactivated, so that the supported
  // link preference is not set.
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  SetOneDriveMount(ToString(Mount::kDisallowed));
  InstallMicrosoft365();

  ASSERT_FALSE(apps::AppServiceProxyFactory::GetForProfile(profile())
                   ->PreferredAppsList()
                   .IsPreferredAppForSupportedLinks(ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(false);

  // Let the main test start with Clippy set to automated.
  SetOneDriveMount(ToString(Mount::kAutomated));
}

// The Microsoft 365 PWA was installed in the pre-test without properly setting
// the supported link preference to simulate the scenario of an existing user
// with the Microsoft 365 already installed before enabling Clippy.
IN_PROC_BROWSER_TEST_F(
    OneDrivePrefObserverBrowserTest,
    SetSupportedLinksPreferenceForM365SuccessOnAppReadiness) {
  ASSERT_TRUE(apps::AppServiceProxyFactory::GetForProfile(profile())
                  ->PreferredAppsList()
                  .IsPreferredAppForSupportedLinks(ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(true);
}

IN_PROC_BROWSER_TEST_F(
    OneDrivePrefObserverBrowserTest,
    PRE_SetSupportedLinksPreferenceForM365DontResetUserActions) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());

  // The supported link preference for M365 is set on the first install
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  SetOneDriveMount(ToString(Mount::kAutomated));
  InstallMicrosoft365();

  ASSERT_TRUE(proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(
      ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(true);

  // Reset the link preference for M365 before simulating a user logout.
  proxy->RemoveSupportedLinksPreference(ash::kMicrosoft365AppId);
}

// Since the supported links preference was already set once by default in the
// pre test, it should not be set again on session start.
IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       SetSupportedLinksPreferenceForM365DontResetUserActions) {
  ASSERT_FALSE(apps::AppServiceProxyFactory::GetForProfile(profile())
                   ->PreferredAppsList()
                   .IsPreferredAppForSupportedLinks(ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(true);
}

IN_PROC_BROWSER_TEST_F(OneDrivePrefObserverBrowserTest,
                       SetSupportedLinksPreferenceForM365NoDefaultOnAllowed) {
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  SetOneDriveMount(ToString(Mount::kAllowed));
  InstallMicrosoft365();

  ASSERT_FALSE(apps::AppServiceProxyFactory::GetForProfile(profile())
                   ->PreferredAppsList()
                   .IsPreferredAppForSupportedLinks(ash::kMicrosoft365AppId));
  CheckM365SupportedLinkDefaultPrefSet(false);
}

}  // namespace chromeos::cloud_storage
