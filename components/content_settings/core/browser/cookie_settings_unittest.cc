// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"

#include <cstddef>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/buildflags/buildflags.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_IOS)
#include "components/content_settings/core/common/features.h"
#else
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
constexpr char kAllowedRequestsHistogram[] =
    "API.StorageAccess.AllowedRequests2";
}
#endif

namespace content_settings {

namespace {

class CookieSettingsObserver : public CookieSettings::Observer {
 public:
  explicit CookieSettingsObserver(CookieSettings* settings)
      : settings_(settings) {
    scoped_observation_.Observe(settings);
  }

  CookieSettingsObserver(const CookieSettingsObserver&) = delete;
  CookieSettingsObserver& operator=(const CookieSettingsObserver&) = delete;

  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override {
    ASSERT_EQ(block_third_party_cookies,
              settings_->ShouldBlockThirdPartyCookies());
    last_value_ = block_third_party_cookies;
  }

  bool last_value() { return last_value_; }

 private:
  raw_ptr<CookieSettings> settings_;
  bool last_value_ = false;
  base::ScopedObservation<CookieSettings, CookieSettings::Observer>
      scoped_observation_{this};
};

struct TestCase {
  std::string test_name;
  bool storage_access_grant_eligible;
  bool top_level_storage_access_grant_eligible;
  bool force_allow_third_party_cookies;
};

class CookieSettingsTest : public testing::TestWithParam<TestCase> {
 public:
  CookieSettingsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kChromeURL("chrome://foo"),
        kExtensionURL("chrome-extension://deadbeef"),
        kDomain("example.com"),
        kDotDomain(".example.com"),
        kSubDomain("www.example.com"),
        kOtherDomain("www.not-example.com"),
        kDomainWildcardPattern("[*.]example.com"),
        kHttpSite("http://example.com"),
        kHttpsSite("https://example.com"),
        kHttpsSubdomainSite("https://www.example.com"),
        kHttpsSite8080("https://example.com:8080"),
        kBlockedSiteForCookies(net::SiteForCookies::FromUrl(kBlockedSite)),
        kAllowedSiteForCookies(net::SiteForCookies::FromUrl(kAllowedSite)),
        kFirstPartySiteForCookies(
            net::SiteForCookies::FromUrl(kFirstPartySite)),
        kChromeSiteForCookies(net::SiteForCookies::FromUrl(kChromeURL)),
        kExtensionSiteForCookies(net::SiteForCookies::FromUrl(kExtensionURL)),
        kHttpSiteForCookies(net::SiteForCookies::FromUrl(kHttpSite)),
        kHttpsSiteForCookies(net::SiteForCookies::FromUrl(kHttpsSite)),
        kAllHttpsSitesPattern(ContentSettingsPattern::FromString("https://*")) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_IOS)
    enabled_features.push_back(kImprovedCookieControls);
#else
    if (IsStorageAccessGrantEligible()) {
      enabled_features.push_back(blink::features::kStorageAccessAPI);
    } else {
      disabled_features.push_back(blink::features::kStorageAccessAPI);
    }
#endif
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~CookieSettingsTest() override { settings_map_->ShutdownOnUIThread(); }

  void SetUp() override {
#if !BUILDFLAG(IS_IOS)
    is_privacy_sandbox_v4_enabled_ =
        base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings4);
#endif
    ContentSettingsRegistry::GetInstance()->ResetForTest();
    CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new CookieSettings(settings_map_.get(), &prefs_, false,
                                          "chrome-extension");
    cookie_settings_incognito_ = new CookieSettings(
        settings_map_.get(), &prefs_, true, "chrome-extension");
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  bool IsStorageAccessGrantEligible() const {
    return GetParam().storage_access_grant_eligible;
  }

  bool IsTopLevelStorageAccessGrantEligible() const {
    return GetParam().top_level_storage_access_grant_eligible;
  }

  bool IsForceAllowThirdPartyCookies() const {
    return GetParam().force_allow_third_party_cookies;
  }

  net::CookieSettingOverrides GetCookieSettingOverrides() const {
    net::CookieSettingOverrides overrides;
    if (IsStorageAccessGrantEligible()) {
      overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
    }
    if (IsTopLevelStorageAccessGrantEligible()) {
      overrides.Put(
          net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
    }
    if (IsForceAllowThirdPartyCookies()) {
      overrides.Put(net::CookieSettingOverride::kForceThirdPartyByUser);
    }
    return overrides;
  }

  // Assumes that cookie access would be blocked if not for a Storage Access API
  // grant or force allow.
  ContentSetting SettingWithEitherOverride() const {
    return IsStorageAccessGrantEligible() || IsForceAllowThirdPartyCookies()
               ? CONTENT_SETTING_ALLOW
               : CONTENT_SETTING_BLOCK;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant, and user force allow.
  ContentSetting SettingWithEitherOverrideForTopLevel() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    return (IsStorageAccessGrantEligible() &&
            IsTopLevelStorageAccessGrantEligible()) ||
                   IsForceAllowThirdPartyCookies()
               ? CONTENT_SETTING_ALLOW
               : CONTENT_SETTING_BLOCK;
  }

  ContentSetting SettingWithForceAllowThirdPartyCookies() const {
    return IsForceAllowThirdPartyCookies() ? CONTENT_SETTING_ALLOW
                                           : CONTENT_SETTING_BLOCK;
  }

  // The cookie access result would be blocked if not for a Storage Access API
  // grant or force allow.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithEitherOverride() const {
    if (IsStorageAccessGrantEligible()) {
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_STORAGE_ACCESS_GRANT;
    }
    if (IsForceAllowThirdPartyCookies()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

  // A version of above that considers Top-Level Storage Access API grant
  // instead of Storage Access API grant, and user force allow to allow cookie
  // access.
  net::cookie_util::StorageAccessResult
  BlockedStorageAccessResultWithTopLevelOverride() const {
    // TODO(crbug.com/1385156): Check TopLevelStorageAccessAPI instead after
    // separating the feature flag.
    if (IsStorageAccessGrantEligible() &&
        IsTopLevelStorageAccessGrantEligible()) {
      // TODO(crbug.com/1385156): Separate metrics between StorageAccessAPI
      // and the page-level variant.
      return net::cookie_util::StorageAccessResult::
          ACCESS_ALLOWED_TOP_LEVEL_STORAGE_ACCESS_GRANT;
    }
    if (IsForceAllowThirdPartyCookies()) {
      return net::cookie_util::StorageAccessResult::ACCESS_ALLOWED_FORCED;
    }
    return net::cookie_util::StorageAccessResult::ACCESS_BLOCKED;
  }

 protected:
  bool ShouldDeleteCookieOnExit(const std::string& domain, bool is_https) {
    return cookie_settings_->ShouldDeleteCookieOnExit(
        cookie_settings_->GetCookieSettings(), domain, is_https);
  }

  // There must be a valid SingleThreadTaskRunner::CurrentDefaultHandle in
  // HostContentSettingsMap's scope.
  base::test::SingleThreadTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;
  scoped_refptr<CookieSettings> cookie_settings_incognito_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kChromeURL;
  const GURL kExtensionURL;
  const std::string kDomain;
  const std::string kDotDomain;
  const std::string kSubDomain;
  const std::string kOtherDomain;
  const std::string kDomainWildcardPattern;
  const GURL kHttpSite;
  const GURL kHttpsSite;
  const GURL kHttpsSubdomainSite;
  const GURL kHttpsSite8080;
  const net::SiteForCookies kBlockedSiteForCookies;
  const net::SiteForCookies kAllowedSiteForCookies;
  const net::SiteForCookies kFirstPartySiteForCookies;
  const net::SiteForCookies kChromeSiteForCookies;
  const net::SiteForCookies kExtensionSiteForCookies;
  const net::SiteForCookies kHttpSiteForCookies;
  const net::SiteForCookies kHttpsSiteForCookies;
  ContentSettingsPattern kAllHttpsSitesPattern;
  bool is_privacy_sandbox_v4_enabled_ = false;

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if !BUILDFLAG(IS_IOS)
TEST(CookieSettings, TestDefaultStorageAccessSetting) {
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI));
}
#endif

TEST_P(CookieSettingsTest, UserBypass) {
  // Bypass shouldn't be enabled.
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  cookie_settings_->SetCookieSettingForUserBypass(kFirstPartySite);

  // Bypass should only be enabled for |kFirstPartySite| with non-bypassed
  // site(s) unaffected.
  EXPECT_TRUE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));

  FastForwardTime(cookie_settings_->kUserBypassEntriesTTL + base::Seconds(1));

  // Passing the expiry of the user bypass entries should disable user bypass
  // for |kFirstPartySite| leaving non-bypassed site(s) unaffected.
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kFirstPartySite));
  EXPECT_FALSE(
      cookie_settings_->IsStoragePartitioningBypassEnabled(kBlockedSite));
}

TEST_P(CookieSettingsTest, TestAllowlistedScheme) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpSite, kChromeSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kChromeSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kChromeURL, kHttpSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
#else
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
#endif
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kHttpSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesBlockSingle) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kBlockedSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesBlockThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  // Cookie is allowed only when block is overridden.

  // A(B) context. Inner frame is cross-origin from top-level frame.
  EXPECT_EQ(
      IsForceAllowThirdPartyCookies(),
      cookie_settings_->IsFullCookieAccessAllowed(
          kBlockedSite, kFirstPartySiteForCookies,
          /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_EQ(IsForceAllowThirdPartyCookies(),
            cookie_settings_->IsFullCookieAccessAllowed(
                kBlockedSite, net::SiteForCookies(),
                /*top_frame_origin=*/url::Origin::Create(kFirstPartySite),
                GetCookieSettingOverrides()));
  EXPECT_EQ(
      IsForceAllowThirdPartyCookies(),
      cookie_settings_->IsFullCookieAccessAllowed(
          kBlockedSite, net::SiteForCookies(),
          /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  // A(B(subA)) context. The inner frame is same-site with the top-level frame,
  // but there's an intermediate cross-site frame.
  EXPECT_EQ(IsForceAllowThirdPartyCookies() || IsStorageAccessGrantEligible(),
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSubdomainSite, net::SiteForCookies(),
                /*top_frame_origin=*/url::Origin::Create(kHttpsSite),
                GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesControlsDefault) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_EQ(
      cookie_settings_incognito_->IsFullCookieAccessAllowed(
          kBlockedSite, kFirstPartySiteForCookies,
          /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()),
      IsForceAllowThirdPartyCookies());
}

TEST_P(CookieSettingsTest, CookiesControlsEnabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  auto cookie_setting_overrides = GetCookieSettingOverrides();
  // Cookie is allowed only when block is overridden.
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_->IsFullCookieAccessAllowed(
                kBlockedSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_incognito_->IsFullCookieAccessAllowed(
                kBlockedSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
}

TEST_P(CookieSettingsTest, CookiesControlsDisabled) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesControlsEnabledForIncognito) {
  auto cookie_setting_overrides = GetCookieSettingOverrides();
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kIncognitoOnly));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_incognito_->IsFullCookieAccessAllowed(
                kBlockedSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
}

#if BUILDFLAG(IS_IOS)
// Test fixture with ImprovedCookieControls disabled.
class ImprovedCookieControlsDisabledCookieSettingsTest
    : public CookieSettingsTest {
 public:
  ImprovedCookieControlsDisabledCookieSettingsTest() : CookieSettingsTest() {
    feature_list_.InitAndDisableFeature(kImprovedCookieControls);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ImprovedCookieControlsDisabledCookieSettingsTest,
       CookiesControlsEnabledButFeatureDisabled) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_incognito_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ImprovedCookieControlsDisabledCookieSettingsTest,
    // Note that since Chrome's implementation of Storage Access API is not
    // supported on iOS (and therefore neither is the Top-Level Storage Access
    // API), we don't have to test those cases here, as this fixture only exists
    // on iOS.
    testing::ValuesIn<TestCase>({
        {"disable_all", false, false, false},
        {"force_3PCs", false, false, true},
    }),
    [](const testing::TestParamInfo<CookieSettingsTest::ParamType>& info) {
      return info.param.test_name;
    });
#endif

TEST_P(CookieSettingsTest, CookiesAllowThirdParty) {
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_P(CookieSettingsTest, CookiesExplicitBlockSingleThirdParty) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesExplicitSessionOnly) {
  cookie_settings_->SetCookieSetting(kBlockedSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}

TEST_P(CookieSettingsTest, ThirdPartyExceptionSessionOnly) {
  cookie_settings_->SetThirdPartyCookieSetting(kBlockedSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(cookie_settings_->IsCookieSessionOnly(kBlockedSite),
            !is_privacy_sandbox_v4_enabled_);
}

#if !BUILDFLAG(IS_IOS)
class CookieSettingsTestSandboxV4Enabled : public CookieSettingsTest {
 public:
  CookieSettingsTestSandboxV4Enabled() {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kPrivacySandboxSettings4);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CookieSettingsTestSandboxV4Enabled, ThirdPartyExceptionSessionOnly) {
  cookie_settings_->SetThirdPartyCookieSetting(kBlockedSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
}
#endif

TEST_P(CookieSettingsTest, KeepBlocked) {
  // Keep blocked cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_P(CookieSettingsTest, DeleteSessionOnly) {
  // Keep session_only http cookies if https is allowed.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Delete cookies if site is session only.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http blocked, https allowed - keep secure and non secure cookies.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Http and https session only, all is deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpSite, CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_P(CookieSettingsTest, DeleteSessionOnlyWithThirdPartyBlocking) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
}

#if !BUILDFLAG(IS_IOS)
TEST_P(CookieSettingsTestSandboxV4Enabled,
       DeleteSessionOnlyWithThirdPartyBlocking) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kBlockedSite));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
}
#endif

TEST_P(CookieSettingsTest, DeletionWithDifferentPorts) {
  // Keep cookies for site with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSite8080, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));

  // Delete cookies with special port.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSite8080,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
}

TEST_P(CookieSettingsTest, DeletionWithSubDomains) {
  // Cookies accessible by subdomains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies that have a session_only subdomain but are accessible by allowed
  // domains are kept.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));

  // Cookies created by session_only subdomains are deleted.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kHttpsSubdomainSite,
                                     CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kDotDomain, true));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, false));
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, false));
  EXPECT_TRUE(ShouldDeleteCookieOnExit(kSubDomain, true));
}

TEST_P(CookieSettingsTest, DeleteCookiesWithThirdPartyException) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetThirdPartyCookieSetting(kHttpsSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(ShouldDeleteCookieOnExit(kDomain, true),
            !is_privacy_sandbox_v4_enabled_);
}

#if !BUILDFLAG(IS_IOS)
TEST_P(CookieSettingsTestSandboxV4Enabled,
       DeleteCookiesWithThirdPartyException) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  cookie_settings_->SetThirdPartyCookieSetting(kHttpsSite,
                                               CONTENT_SETTING_SESSION_ONLY);
  EXPECT_FALSE(ShouldDeleteCookieOnExit(kDomain, true));
}
#endif

TEST_P(CookieSettingsTest, CookiesThirdPartyBlockedExplicitAllow) {
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // Extensions should always be allowed to use cookies.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesThirdPartyBlockedAllSitesAllowed) {
  auto cookie_setting_overrides = GetCookieSettingOverrides();

  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  // As an example for a url that matches all hosts but not all origins,
  // match all HTTPS sites.
  settings_map_->SetContentSettingCustomScope(
      kAllHttpsSitesPattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);

  // |kAllowedSite| should be allowed.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kBlockedSiteForCookies, /*top_frame_origin*/ absl::nullopt,
      cookie_setting_overrides));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTPS sites should be allowed in a first-party context.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      cookie_setting_overrides));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));

  // HTTP sites should be allowed.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
  EXPECT_TRUE(cookie_settings_->IsCookieSessionOnly(kFirstPartySite));

  // Third-party cookies should be blocked.
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_->IsFullCookieAccessAllowed(
                kFirstPartySite, kBlockedSiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kBlockedSiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));
}

TEST_P(CookieSettingsTest, CookiesBlockEverything) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin*/ absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, CookiesBlockEverythingExceptAllowed) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  cookie_settings_->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kFirstPartySiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kAllowedSiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
  EXPECT_FALSE(cookie_settings_->IsCookieSessionOnly(kAllowedSite));
}

#if !BUILDFLAG(IS_IOS)
TEST_P(CookieSettingsTest, GetCookieSettingAllowedTelemetry) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kOff));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_ALLOW);
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(net::cookie_util::StorageAccessResult::ACCESS_ALLOWED),
      1);
}

// The behaviour of the Storage Access API should be gated behind
// |kStorageAccessAPI|. The setting also affects which buckets are used by
// metrics.
TEST_P(CookieSettingsTest, GetCookieSettingSAA) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);
  const GURL third_url = GURL(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithEitherOverride()), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, third_url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

// A top-level storage access grant should behave similarly to standard SAA
// grants. TODO(crbug.com/1385156): as requirements for the two APIs solidify,
// this will likely not continue to be true.
TEST_P(CookieSettingsTest, GetCookieSettingTopLevelStorageAccess) {
  const GURL top_level_url(kFirstPartySite);
  const GURL url(kAllowedSite);
  const GURL third_url(kBlockedSite);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 0);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverrideForTopLevel());
  histogram_tester.ExpectTotalCount(kAllowedRequestsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kAllowedRequestsHistogram,
      static_cast<int>(BlockedStorageAccessResultWithTopLevelOverride()), 1);

  // Invalid pair the |top_level_url| granting access to |url| is now
  // being loaded under |url| as the top level url.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                top_level_url, url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());

  // Invalid pairs where a |third_url| is used.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, third_url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                third_url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

// Subdomains of the granted resource url should not gain access if a valid
// grant exists; the grant should also not apply on different schemes.
TEST_P(CookieSettingsTest, GetCookieSettingSAAResourceWildcards) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kHttpsSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                GURL(kHttpsSubdomainSite), top_level_url,
                GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(GURL(kHttpSite), top_level_url,
                                         GetCookieSettingOverrides(), nullptr),
      SettingWithForceAllowThirdPartyCookies());
}

// Subdomains of the granted top level url should not grant access if a valid
// grant exists; the grant should also not apply on different schemes.
TEST_P(CookieSettingsTest, GetCookieSettingSAATopLevelWildcards) {
  const GURL top_level_url = GURL(kHttpsSite);
  const GURL url = GURL(kFirstPartySite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride());
  EXPECT_EQ(
      cookie_settings_->GetCookieSetting(url, GURL(kHttpsSubdomainSite),
                                         GetCookieSettingOverrides(), nullptr),
      SettingWithForceAllowThirdPartyCookies());
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, GURL(kHttpSite), GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}

// Explicit settings should be respected regardless of whether Storage Access
// API is enabled and/or has grants.
TEST_P(CookieSettingsTest, GetCookieSettingRespectsExplicitSettings) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            CONTENT_SETTING_BLOCK);
}

// Once a grant expires access should no longer be given.
TEST_P(CookieSettingsTest, GetCookieSettingSAAExpiredGrant) {
  const GURL top_level_url = GURL(kFirstPartySite);
  const GURL url = GURL(kAllowedSite);

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURLNoWildcard(url),
      ContentSettingsPattern::FromURLNoWildcard(top_level_url),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW,
      {content_settings::GetConstraintExpiration(base::Seconds(100)),
       SessionModel::UserSession});

  // When requesting our setting for the url/top-level combination our grant is
  // for access should be allowed iff SAA is enabled. For any other domain pairs
  // access should still be blocked.
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithEitherOverride());

  // If we fastforward past the expiration of our grant the result should be
  // CONTENT_SETTING_BLOCK now.
  FastForwardTime(base::Seconds(101));
  EXPECT_EQ(cookie_settings_->GetCookieSetting(
                url, top_level_url, GetCookieSettingOverrides(), nullptr),
            SettingWithForceAllowThirdPartyCookies());
}
#endif

TEST_P(CookieSettingsTest, ExtensionsRegularSettings) {
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // Regular cookie settings also apply to extensions.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, ExtensionsOwnCookies) {
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions can always use cookies (and site data) in their own origin.
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
#else
  // Except if extensions are disabled. Then the extension-specific checks do
  // not exist and the default setting is to block.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kExtensionURL, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
#endif
}

TEST_P(CookieSettingsTest, ExtensionsThirdParty) {
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));

  // XHRs stemming from extensions are exempt from third-party cookie blocking
  // rules (as the first party is always the extension's security origin).
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kExtensionSiteForCookies,
      /*top_frame_origin=*/absl::nullopt, GetCookieSettingOverrides()));
}

TEST_P(CookieSettingsTest, ThirdPartyException) {
  GURL first_party_url = kFirstPartySiteForCookies.RepresentativeUrl();
  auto cookie_setting_overrides = GetCookieSettingOverrides();

  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/absl::nullopt,
      cookie_setting_overrides));

  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));

  cookie_settings_->SetThirdPartyCookieSetting(first_party_url,
                                               CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/absl::nullopt,
      cookie_setting_overrides));

  cookie_settings_->ResetThirdPartyCookieSetting(first_party_url);
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(first_party_url, nullptr));
  EXPECT_EQ(cookie_setting_overrides.Has(
                net::CookieSettingOverride::kForceThirdPartyByUser),
            cookie_settings_->IsFullCookieAccessAllowed(
                kHttpsSite, kFirstPartySiteForCookies,
                /*top_frame_origin=*/absl::nullopt, cookie_setting_overrides));

  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, nullptr));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/absl::nullopt,
      cookie_setting_overrides));
}

TEST_P(CookieSettingsTest, ManagedThirdPartyException) {
  SettingSource source;
  EXPECT_TRUE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &source));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
  EXPECT_EQ(source, SettingSource::SETTING_SOURCE_USER);

  prefs_.SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(
      cookie_settings_->IsThirdPartyAccessAllowed(kFirstPartySite, &source));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kFirstPartySiteForCookies, /*top_frame_origin=*/absl::nullopt,
      GetCookieSettingOverrides()));
  EXPECT_EQ(source, SettingSource::SETTING_SOURCE_POLICY);
}

TEST_P(CookieSettingsTest, ThirdPartySettingObserver) {
  CookieSettingsObserver observer(cookie_settings_.get());
  EXPECT_FALSE(observer.last_value());
  prefs_.SetInteger(prefs::kCookieControlsMode,
                    static_cast<int>(CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(observer.last_value());
}

TEST_P(CookieSettingsTest, LegacyCookieAccessAllowAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_P(CookieSettingsTest, LegacyCookieAccessBlockAll) {
  settings_map_->SetDefaultContentSetting(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDomain));
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            cookie_settings_->GetCookieAccessSemanticsForDomain(kDotDomain));
}

TEST_P(CookieSettingsTest, LegacyCookieAccessAllowDomainPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomain),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These two test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // These two test cases default into NONLEGACY.
      // Subdomain does not match pattern.
      {net::CookieAccessSemantics::NONLEGACY, kSubDomain},
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

TEST_P(CookieSettingsTest, LegacyCookieAccessAllowDomainWildcardPattern) {
  // Override the policy provider for this test, since the legacy cookie access
  // setting can only be set by policy.
  TestUtils::OverrideProvider(
      settings_map_.get(), std::make_unique<MockProvider>(),
      HostContentSettingsMap::ProviderType::POLICY_PROVIDER);
  settings_map_->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(kDomainWildcardPattern),
      ContentSettingsPattern::Wildcard(),
      ContentSettingsType::LEGACY_COOKIE_ACCESS, CONTENT_SETTING_ALLOW);
  const struct {
    net::CookieAccessSemantics status;
    std::string cookie_domain;
  } kTestCases[] = {
      // These three test cases are LEGACY because they match the setting.
      {net::CookieAccessSemantics::LEGACY, kDomain},
      {net::CookieAccessSemantics::LEGACY, kDotDomain},
      // Subdomain matches pattern.
      {net::CookieAccessSemantics::LEGACY, kSubDomain},
      // This test case defaults into NONLEGACY.
      {net::CookieAccessSemantics::NONLEGACY, kOtherDomain}};
  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.status, cookie_settings_->GetCookieAccessSemanticsForDomain(
                               test.cookie_domain));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTest,
    testing::ValuesIn<TestCase>({
      {"disable_all", false, false, false},
          {"disable_SAA_disable_TopLevel_force_3PCs", false, false, true},
          {"disable_SAA_enable_TopLevel", false, true, false},
          {"disable_SAA_enable_TopLevel_force_3PCs", false, true, true},
#if !BUILDFLAG(IS_IOS)
          {"enable_SAA_disable_TopLevel", true, false, false},
          {"enable_SAA_disable_TopLevel_force_3PCs", true, false, true},
          {"enable_SAA_enable_TopLevel", true, true, false},
          {"enable_all", true, true, true},
#endif
    }),
    [](const testing::TestParamInfo<CookieSettingsTest::ParamType>& info) {
      return info.param.test_name;
    });

#if !BUILDFLAG(IS_IOS)
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieSettingsTestSandboxV4Enabled,
    testing::ValuesIn<TestCase>({
        {"disable_all", false, false, false},
        {"disable_SAA_disable_TopLevel_force_3PCs", false, false, true},
        {"disable_SAA_enable_TopLevel", false, true, false},
        {"disable_SAA_enable_TopLevel_force_3PCs", false, true, true},
        {"enable_SAA_disable_TopLevel", true, false, false},
        {"enable_SAA_disable_TopLevel_force_3PCs", true, false, true},
        {"enable_SAA_enable_TopLevel", true, true, false},
        {"enable_all", true, true, true},
    }),
    [](const testing::TestParamInfo<CookieSettingsTest::ParamType>& info) {
      return info.param.test_name;
    });
#endif
}  // namespace

}  // namespace content_settings
