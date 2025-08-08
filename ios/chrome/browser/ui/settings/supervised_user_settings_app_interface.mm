// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/supervised_user_settings_app_interface.h"

#import "base/memory/ptr_util.h"
#import "base/memory/singleton.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/kids_chrome_management_client.h"
#import "components/supervised_user/core/browser/permission_request_creator.h"
#import "components/supervised_user/core/browser/permission_request_creator_mock.h"
#import "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_settings_service.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"

namespace {

// Helper class that holds a instance of TestUrlLoaderFactory.
// It allows the callers of this class to keep an alive instance
// of a TestUrlLoaderFactory for the duration of a test.
class TestUrlLoaderFactoryHelper {
 public:
  static TestUrlLoaderFactoryHelper* SharedInstance() {
    return base::Singleton<TestUrlLoaderFactoryHelper>::get();
  }

  void SetUp() {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
  }

  void TearDown() {
    shared_url_loader_factory_.reset();
    test_url_loader_factory_.reset();
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return std::ref(shared_url_loader_factory_);
  }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

void setUrlFilteringForUrl(const GURL& url, bool isAllowed) {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  const base::Value::Dict& local_settings =
      settings_service->LocalSettingsForTest();
  base::Value::Dict dict_to_insert;

  const base::Value::Dict* dict_value =
      local_settings.FindDict(supervised_user::kContentPackManualBehaviorHosts);
  if (dict_value) {
    dict_to_insert = dict_value->Clone();
  }
  dict_to_insert.Set(url.host(), isAllowed);
  settings_service->SetLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts,
      std::move(dict_to_insert));
}

bool isShowingInterstitialForState(web::WebState* web_state) {
  CHECK(web_state);
  auto* blocking_tab_helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state);

  CHECK(blocking_tab_helper);
  security_interstitials::IOSSecurityInterstitialPage* blocking_page =
      blocking_tab_helper->GetCurrentBlockingPage();
  return blocking_page && blocking_page->GetInterstitialType() ==
                              kSupervisedUserInterstitialType;
}

}  // namespace

@implementation SupervisedUserSettingsAppInterface : NSObject

+ (void)setSupervisedUserURLFilterBehavior:
    (supervised_user::SupervisedUserURLFilter::FilteringBehavior)behavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->SetLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior,
      base::Value(behavior));
  if (behavior ==
      supervised_user::SupervisedUserURLFilter::FilteringBehavior::ALLOW) {
    settings_service->SetLocalSetting(supervised_user::kSafeSitesEnabled,
                                      base::Value(true));
  }
}

+ (void)resetSupervisedUserURLFilterBehavior {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackDefaultFilteringBehavior);
}

+ (void)resetManualUrlFiltering {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RemoveLocalSetting(
      supervised_user::kContentPackManualBehaviorHosts);
}

+ (void)setFakePermissionCreator {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  CHECK(settings_service);
  std::unique_ptr<supervised_user::PermissionRequestCreator> creator =
      std::make_unique<supervised_user::PermissionRequestCreatorMock>(
          *settings_service);
  supervised_user::PermissionRequestCreatorMock* mocked_creator =
      static_cast<supervised_user::PermissionRequestCreatorMock*>(
          creator.get());
  mocked_creator->SetEnabled();

  supervised_user::SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  CHECK(service);
  service->remote_web_approvals_manager().ClearApprovalRequestsCreators();
  service->remote_web_approvals_manager().AddApprovalRequestCreator(
      std::move(creator));
}

+ (void)approveWebsiteDomain:(NSURL*)url {
  supervised_user::SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  settings_service->RecordLocalWebsiteApproval(net::GURLWithNSURL(url).host());
}

+ (void)setFilteringToAllowAllSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::
                                               SupervisedUserURLFilter::ALLOW];
}

+ (void)setFilteringToAllowApprovedSites {
  [self setSupervisedUserURLFilterBehavior:supervised_user::
                                               SupervisedUserURLFilter::BLOCK];
}

+ (void)addWebsiteToAllowList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), true);
}

+ (void)addWebsiteToBlockList:(NSURL*)host {
  setUrlFilteringForUrl(net::GURLWithNSURL(host), false);
}

+ (void)resetFirstTimeBanner {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromBrowserState(
      chrome_test_util::GetOriginalBrowserState());
  PrefService* user_prefs = browser_state->GetPrefs();
  CHECK(user_prefs);
  user_prefs->SetInteger(
      prefs::kFirstTimeInterstitialBannerState,
      static_cast<int>(
          supervised_user::FirstTimeInterstitialBannerState::kNeedToShow));
}

+ (void)setDefaultClassifyURLNavigationIsAllowed:(BOOL)is_allowed {
  // Fake the ClassifyUrl responses.
  kids_chrome_management::ClassifyUrlResponse response;
  auto url_classification =
      is_allowed ? kids_chrome_management::ClassifyUrlResponse::ALLOWED
                 : kids_chrome_management::ClassifyUrlResponse::RESTRICTED;
  response.set_display_classification(url_classification);
  std::string classify_url_service_url =
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
      "me:classifyUrl?alt=proto";

  auto* test_url_loader_factory =
      TestUrlLoaderFactoryHelper::SharedInstance()->test_url_loader_factory();
  CHECK(test_url_loader_factory);
  auto shared_url_loader_factory =
      TestUrlLoaderFactoryHelper::SharedInstance()->shared_url_loader_factory();
  CHECK(shared_url_loader_factory);

  test_url_loader_factory->AddResponse(classify_url_service_url,
                                       response.SerializeAsString());

  // Set up the KidsChromeManagementClient that provides fake safe search
  // responses.
  ChromeBrowserState* browser_state = ChromeBrowserState::FromBrowserState(
      chrome_test_util::GetOriginalBrowserState());
  auto* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  CHECK(identity_manager);
  auto kids_chrome_management_client =
      std::make_unique<KidsChromeManagementClient>(shared_url_loader_factory,
                                                   identity_manager);

  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserState(browser_state);
  supervised_user_service->GetURLFilter()->InitAsyncURLChecker(
      kids_chrome_management_client.get());
}

+ (void)setUpTestUrlLoaderFactoryHelper {
  TestUrlLoaderFactoryHelper::SharedInstance()->SetUp();
}

+ (void)tearDownTestUrlLoaderFactoryHelper {
  TestUrlLoaderFactoryHelper::SharedInstance()->TearDown();
}

+ (NSInteger)countSupervisedUserIntersitialsForExistingWebStates {
  int count = 0;
  int tab_count = chrome_test_util::GetMainTabCount();
  for (int i = 0; i < tab_count; i++) {
    if (isShowingInterstitialForState(
            chrome_test_util::GetWebStateAtIndexInCurrentMode(i))) {
      count++;
    }
  }
  return count;
}

@end
