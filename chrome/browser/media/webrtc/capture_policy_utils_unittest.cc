// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
constexpr char kTestSite1[] = "https://foo.test.org";
constexpr char kTestSite1Pattern[] = "foo.test.org";
constexpr char kTestSite1NonMatchingPattern[] = "foo.org";
}  // namespace

class CapturePolicyUtilsTest : public testing::Test {
 public:
  CapturePolicyUtilsTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kScreenCaptureAllowed, true);
    prefs_.registry()->RegisterListPref(prefs::kScreenCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(prefs::kWindowCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(prefs::kTabCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(
        prefs::kSameOriginTabCaptureAllowedByOrigins);
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

// Test that the default policy allows all capture.
TEST_F(CapturePolicyUtilsTest, SimpleAllowTest) {
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that setting |kScreenCaptureAllowed| to false, denies all capture.
TEST_F(CapturePolicyUtilsTest, SimpleDenyTest) {
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  EXPECT_EQ(AllowedScreenCaptureLevel::kDisallowed,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the FullCapture allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideUnrestricted) {
  base::Value::List matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Window/Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideWindowTabs) {
  base::Value::List matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kWindowCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kWindow,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideTabs) {
  base::Value::List matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Same Origin Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideSameOriginTabs) {
  base::Value::List matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kSameOrigin,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that an item that doesn't match any list still respects the default.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideNoMatches) {
  base::Value::List matchlist;
  matchlist.Append(kTestSite1NonMatchingPattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kDisallowed,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Ensure that a full wildcard policy is accepted.
TEST_F(CapturePolicyUtilsTest, TestWildcard) {
  base::Value::List matchlist;
  matchlist.Append("*");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://a.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://b.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://c.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://d.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://e.com"), *prefs()));
}

// Ensure that if a URL appears in multiple lists that it returns the most
// restrictive list that it is included in.
TEST_F(CapturePolicyUtilsTest, TestOverrideMoreRestrictive) {
  base::Value::List full_capture_list;
  full_capture_list.Append("a.com");
  full_capture_list.Append("b.com");
  full_capture_list.Append("c.com");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins,
                   std::move(full_capture_list));

  base::Value::List window_tab_list;
  window_tab_list.Append("b.com");
  prefs()->SetList(prefs::kWindowCaptureAllowedByOrigins,
                   std::move(window_tab_list));

  base::Value::List tab_list;
  tab_list.Append("c.com");
  tab_list.Append("d.com");
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(tab_list));

  base::Value::List same_origin_tab_list;
  same_origin_tab_list.Append("d.com");
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(same_origin_tab_list));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kUnrestricted,
      capture_policy::GetAllowedCaptureLevel(GURL("https://a.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kWindow,
      capture_policy::GetAllowedCaptureLevel(GURL("https://b.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://c.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kSameOrigin,
      capture_policy::GetAllowedCaptureLevel(GURL("https://d.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kDisallowed,
      capture_policy::GetAllowedCaptureLevel(GURL("https://e.com"), *prefs()));
}

// Ensure that if a URL appears in multiple lists that it returns the most
// restrictive list that it is included in.
TEST_F(CapturePolicyUtilsTest, TestSubdomainOverrides) {
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);

  base::Value::List same_origin_tab_list;
  same_origin_tab_list.Append("github.io");
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(same_origin_tab_list));

  base::Value::List tab_list;
  tab_list.Append("foo.github.io");
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(tab_list));

  base::Value::List full_capture_list;
  full_capture_list.Append("[*.]github.io");
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins,
                   std::move(full_capture_list));

  EXPECT_EQ(AllowedScreenCaptureLevel::kSameOrigin,
            capture_policy::GetAllowedCaptureLevel(GURL("https://github.io"),
                                                   *prefs()));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(
                GURL("https://foo.github.io"), *prefs()));
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(
                GURL("https://bar.github.io"), *prefs()));
}

// Returns a std::vector<DesktopMediaList::Type> containing all values.
std::vector<DesktopMediaList::Type> GetFullMediaList() {
  return {DesktopMediaList::Type::kCurrentTab,
          DesktopMediaList::Type::kWebContents, DesktopMediaList::Type::kWindow,
          DesktopMediaList::Type::kScreen};
}

// Test FilterMediaList with the different values of AllowedScreenCaptureLevel
// and ensure that values are filtered out and remain in sorted out.
TEST_F(CapturePolicyUtilsTest, FilterMediaListUnrestricted) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab, DesktopMediaList::Type::kWebContents,
      DesktopMediaList::Type::kWindow, DesktopMediaList::Type::kScreen};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kUnrestricted;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedWindow) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab, DesktopMediaList::Type::kWebContents,
      DesktopMediaList::Type::kWindow};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kWindow;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedTab) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab,
      DesktopMediaList::Type::kWebContents};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kTab;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

// We don't do the SameOrigin filter at the MediaTypes level, so this should be
// the same.
TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedSameOrigin) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab,
      DesktopMediaList::Type::kWebContents};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kSameOrigin;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

class MultiCaptureTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<bool, std::vector<std::string>, std::string>> {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(IsMainProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    for (const std::string& url : AllowedOrigins()) {
      content_settings->SetContentSettingDefaultScope(
          GURL(url), GURL(url), ContentSettingsType::ALL_SCREEN_CAPTURE,
          ContentSetting::CONTENT_SETTING_ALLOW);
    }
  }

  void TearDown() override { profile_.reset(); }

  Profile* profile() { return profile_.get(); }
  bool IsMainProfile() const { return std::get<0>(GetParam()); }
  std::vector<std::string> AllowedOrigins() const {
    return std::get<1>(GetParam());
  }
  std::string CurrentOrigin() const { return std::get<2>(GetParam()); }

 protected:
  bool ExpectedIsMultiCaptureAllowed() {
    std::vector<std::string> allowed_urls = AllowedOrigins();
    return
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        IsMainProfile() &&
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
        base::Contains(allowed_urls, CurrentOrigin());
  }

  bool ExpectedIsMultiCaptureAllowedForAnyUrl() {
    return
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        IsMainProfile() &&
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
        !AllowedOrigins().empty();
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(MultiCaptureTest, IsMultiCaptureAllowedBasedOnPolicy) {
  EXPECT_EQ(ExpectedIsMultiCaptureAllowed(),
            capture_policy::IsGetAllScreensMediaAllowed(profile(),
                                                        GURL(CurrentOrigin())));
}

TEST_P(MultiCaptureTest, IsMultiCaptureAllowedForAnyUrl) {
  EXPECT_EQ(ExpectedIsMultiCaptureAllowedForAnyUrl(),
            capture_policy::IsGetAllScreensMediaAllowedForAnySite(profile()));
}

INSTANTIATE_TEST_SUITE_P(
    MultiCaptureTestCases,
    MultiCaptureTest,
    ::testing::Combine(
        // Is main profile?
        ::testing::Bool(),
        // Allowed origins
        ::testing::ValuesIn({std::vector<std::string>{},
                             std::vector<std::string>{
                                 "https://www.google.com"}}),
        // Origin to test
        ::testing::ValuesIn({std::string("https://www.google.com"),
                             std::string("https://www.notallowed.com")})));

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
