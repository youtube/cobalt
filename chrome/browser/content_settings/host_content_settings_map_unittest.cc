// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/client_hints/common/client_hints.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;

namespace {

bool MatchPrimaryPattern(const ContentSettingsPattern& expected_primary,
                         const ContentSettingsPattern& primary_pattern,
                         const ContentSettingsPattern& secondary_pattern) {
  return expected_primary == primary_pattern;
}

base::Time GetSettingLastModifiedDate(HostContentSettingsMap* map,
                                      GURL primary_url,
                                      GURL secondary_url,
                                      ContentSettingsType type) {
  content_settings::SettingInfo info;
  map->GetWebsiteSetting(primary_url, secondary_url, type, &info);
  return info.metadata.last_modified;
}
}  // namespace

class MockUserModifiableProvider
    : public content_settings::UserModifiableProvider {
 public:
  ~MockUserModifiableProvider() override = default;
  MOCK_CONST_METHOD2(
      GetRuleIterator,
      std::unique_ptr<content_settings::RuleIterator>(ContentSettingsType,
                                                      bool));

  MOCK_METHOD5(SetWebsiteSetting,
               bool(const ContentSettingsPattern&,
                    const ContentSettingsPattern&,
                    ContentSettingsType,
                    base::Value&&,
                    const content_settings::ContentSettingConstraints&));

  MOCK_METHOD1(ClearAllContentSettingsRules, void(ContentSettingsType));

  MOCK_METHOD0(ShutdownOnUIThread, void());

  MOCK_METHOD3(UpdateLastVisitTime,
               bool(const ContentSettingsPattern& primary_pattern,
                    const ContentSettingsPattern& secondary_pattern,
                    ContentSettingsType content_type));
  MOCK_METHOD3(ResetLastVisitTime,
               bool(const ContentSettingsPattern& primary_pattern,
                    const ContentSettingsPattern& secondary_pattern,
                    ContentSettingsType content_type));

  MOCK_METHOD1(SetClockForTesting, void(base::Clock*));
};

class HostContentSettingsMapTest : public testing::Test {
 public:
  HostContentSettingsMapTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  const std::string& GetPrefName(ContentSettingsType type) {
    return content_settings::WebsiteSettingsRegistry::GetInstance()
        ->Get(type)
        ->pref_name();
  }

  content::BrowserTaskEnvironment task_environment_;
};

// Wrapper to TestingProfile to reduce test boilerplates, by keeping a fixed
// |content_type| so caller only need to specify it once.
class TesterForType {
 public:
  TesterForType(TestingProfile *profile, ContentSettingsType content_type)
      : prefs_(profile->GetTestingPrefService()),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(profile)),
        content_type_(content_type) {
    switch (content_type_) {
      case ContentSettingsType::COOKIES:
        policy_default_setting_ = prefs::kManagedDefaultCookiesSetting;
        break;
      case ContentSettingsType::POPUPS:
        policy_default_setting_ = prefs::kManagedDefaultPopupsSetting;
        break;
      case ContentSettingsType::ADS:
        policy_default_setting_ = prefs::kManagedDefaultAdsSetting;
        break;
      default:
        // Add support as needed.
        NOTREACHED();
    }
  }

  TesterForType(const TesterForType&) = delete;
  TesterForType& operator=(const TesterForType&) = delete;

  void ClearPolicyDefault() {
    prefs_->RemoveManagedPref(policy_default_setting_);
  }

  void SetPolicyDefault(ContentSetting setting) {
    prefs_->SetManagedPref(policy_default_setting_,
                           std::make_unique<base::Value>(setting));
  }

  void AddUserException(std::string exception,
                        ContentSetting content_settings) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(exception);
    host_content_settings_map_->SetContentSettingCustomScope(
        pattern, ContentSettingsPattern::Wildcard(), content_type_,
        content_settings);
  }

  // Wrapper to query GetWebsiteSetting(), and only return the source.
  content_settings::SettingSource GetSettingSourceForURL(
      const std::string& url_str) {
    GURL url(url_str);
    content_settings::SettingInfo setting_info;
    base::Value result = host_content_settings_map_->GetWebsiteSetting(
        url, url, content_type_, &setting_info);
    return setting_info.source;
  }

 private:
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  ContentSettingsType content_type_;
  const char* policy_default_setting_;
};

TEST_F(HostContentSettingsMapTest, DefaultValues) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check setting defaults.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, host_content_settings_map->GetContentSetting(
                                       GURL(chrome::kChromeUINewTabURL),
                                       GURL(chrome::kChromeUINewTabURL),
                                       ContentSettingsType::JAVASCRIPT));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::POPUPS, nullptr));
}

TEST_F(HostContentSettingsMapTest, IndividualSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check returning individual settings.
  GURL host("http://example.com/");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  // Check returning all settings for a host.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::NOTIFICATIONS));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::AUTOPLAY, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::AUTOPLAY));

  // Check returning all hosts for a setting.
  GURL host2("http://example.org/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::JAVASCRIPT, &host_settings);
  // |host_settings| contains the default setting and 2 exception.
  EXPECT_EQ(3U, host_settings.size());
  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::POPUPS,
                                                   &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, GetWebsiteSettingsForOneType) {
  TestingProfile profile;
  GURL hosts[] = {GURL("https://example1.com/"), GURL("https://example2.com/")};
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(0U, client_hints_settings.size());

  // Add setting for hosts[0].
  base::Value client_hint_value(42);

  base::Value::Dict client_hints_dictionary;
  client_hints_dictionary.Set(client_hints::kClientHintsSettingKey,
                              {std::move(client_hint_value)});
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), ContentSettingsType::CLIENT_HINTS,
      base::Value(client_hints_dictionary.Clone()));

  // Reading the settings should now return one setting.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(client_hints_dictionary,
              client_hints_settings.at(i).setting_value);
  }

  // Add setting for hosts[1].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[1], GURL(), ContentSettingsType::CLIENT_HINTS,
      base::Value(client_hints_dictionary.Clone()));

  // Reading the settings should now return two settings.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(client_hints_dictionary,
              client_hints_settings.at(i).setting_value);
  }

  // Add settings again for hosts[0].
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      hosts[0], GURL(), ContentSettingsType::CLIENT_HINTS,
      base::Value(client_hints_dictionary.Clone()));

  // Reading the settings should still return two settings.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(2U, client_hints_settings.size());
  for (size_t i = 0; i < client_hints_settings.size(); ++i) {
    EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(hosts[i]),
              client_hints_settings.at(i).primary_pattern);
    EXPECT_EQ(ContentSettingsPattern::Wildcard(),
              client_hints_settings.at(i).secondary_pattern);
    EXPECT_EQ(client_hints_dictionary,
              client_hints_settings.at(i).setting_value);
  }
}

TEST_F(HostContentSettingsMapTest, Clear) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Check clearing one type.
  GURL host("http://example.org/");
  GURL host2("http://example.net/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingDefaultScope(
      host2, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  ContentSettingsForOneType host_settings;
  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::COOKIES,
                                                   &host_settings);
  // |host_settings| contains only the default setting.
  EXPECT_EQ(1U, host_settings.size());
}

TEST_F(HostContentSettingsMapTest, Patterns) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("example.org");
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES));
}

// Changing a setting for one origin doesn't affect subdomains.
TEST_F(HostContentSettingsMapTest, Origins) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://www.example.com/");
  GURL host3("http://example.org/");
  GURL host4("http://example.com:8080");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(host1);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host4, host4, ContentSettingsType::COOKIES));
}

TEST_F(HostContentSettingsMapTest, Observer) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  MockSettingsObserver observer(host_content_settings_map);

  GURL host("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::Wildcard();
  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, primary_pattern,
                                                 secondary_pattern, false));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnContentSettingsChanged(host_content_settings_map,
                                                 ContentSettingsType::COOKIES,
                                                 false, _, _, true));
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
}

TEST_F(HostContentSettingsMapTest, ObserveDefaultPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();
  GURL host("http://example.com");

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  const content_settings::WebsiteSettingsInfo* info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          ContentSettingsType::COOKIES);
  // Clearing the backing pref should also clear the internal cache.
  prefs->ClearPref(info->default_value_pref_name());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  // Resetting the pref to its previous value should update the cache.
  prefs->SetInteger(info->default_value_pref_name(), CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
}

TEST_F(HostContentSettingsMapTest, ObserveExceptionPref) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  PrefService* prefs = profile.GetPrefs();

  // Make a copy of the default pref value so we can reset it later.
  base::Value default_value =
      prefs->FindPreference(GetPrefName(ContentSettingsType::COOKIES))
          ->GetValue()
          ->Clone();

  GURL host("http://example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  // Make a copy of the pref's new value so we can reset it later.
  base::Value new_value =
      prefs->FindPreference(GetPrefName(ContentSettingsType::COOKIES))
          ->GetValue()
          ->Clone();

  // Clearing the backing pref should also clear the internal cache.
  prefs->Set(GetPrefName(ContentSettingsType::COOKIES), default_value);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  // Resetting the pref to its previous value should update the cache.
  prefs->Set(GetPrefName(ContentSettingsType::COOKIES), new_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
}

TEST_F(HostContentSettingsMapTest, HostTrimEndingDotCheck) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  GURL host_ending_with_dot("http://example.com./");
  url::Origin origin = url::Origin::Create(host_ending_with_dot);
  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromOrigin(origin);

  EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
      host_ending_with_dot, site_for_cookies, origin,
      net::CookieSettingOverrides()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_DEFAULT);
  EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
      host_ending_with_dot, site_for_cookies, origin,
      net::CookieSettingOverrides()));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(cookie_settings->IsFullCookieAccessAllowed(
      host_ending_with_dot, site_for_cookies, origin,
      net::CookieSettingOverrides()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::JAVASCRIPT,
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::JAVASCRIPT,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::JAVASCRIPT));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::POPUPS,
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::POPUPS,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::POPUPS));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::AUTOPLAY,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY));
  host_content_settings_map->SetContentSettingDefaultScope(
      host_ending_with_dot, GURL(), ContentSettingsType::AUTOPLAY,
      CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host_ending_with_dot, host_ending_with_dot,
                ContentSettingsType::AUTOPLAY));
}

TEST_F(HostContentSettingsMapTest, NestedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host1("http://example.com/");
  GURL host2("http://b.example.com/");
  GURL host3("http://a.b.example.com/");
  GURL host4("http://a.example.com/");
  GURL host5("http://b.b.example.com/");
  ContentSettingsPattern pattern1 =
      ContentSettingsPattern::FromString("[*.]example.com");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]b.example.com");
  ContentSettingsPattern pattern3 =
      ContentSettingsPattern::FromString("a.b.example.com");

  // Test nested patterns for one type.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));
  host_content_settings_map->SetContentSettingCustomScope(
      pattern1, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern3, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host1, host1, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host4, host4, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host5, host5, ContentSettingsType::COOKIES));

  host_content_settings_map->ClearSettingsForOneType(
      ContentSettingsType::COOKIES);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  GURL https_host1("https://b.example.com/");
  GURL https_host2("https://a.b.example.com/");
  ContentSettingsPattern pattern4 =
      ContentSettingsPattern::FromString("b.example.com");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern4, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  // Pattern "b.example.com" will affect (http|https)://b.example.com
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host2, host2, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                https_host1, https_host1, ContentSettingsType::COOKIES));
  // Pattern "b.example.com" will not affect (http|https)://a.b.example.com
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host3, host3, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                https_host2, https_host2, ContentSettingsType::COOKIES));
}

TEST_F(HostContentSettingsMapTest, TypeIsolatedSettings) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION));
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritInitialAllow) {
  // The cookie setting has an initial value of ALLOW, so all changes should be
  // inherited from regular to incognito mode.
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES));

  // The inherited setting should be included in ContentSettingsForOneType
  // table as well.
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::COOKIES, &otr_settings);
    ASSERT_EQ(1u, otr_settings.size());
    EXPECT_FALSE(otr_settings[0].incognito);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, content_settings::ValueToContentSetting(
                                         otr_settings[0].setting_value));
  }

  // Changing content settings on the main map should also affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_EQ(CONTENT_SETTING_SESSION_ONLY,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(
      CONTENT_SETTING_SESSION_ONLY,
      otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES));

  // Inherited setting + default in ContentSettingsForOneType
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::COOKIES, &otr_settings);
    ASSERT_EQ(2u, otr_settings.size());
    EXPECT_FALSE(otr_settings[0].incognito);
    EXPECT_EQ(
        CONTENT_SETTING_SESSION_ONLY,
        content_settings::ValueToContentSetting(otr_settings[0].setting_value));
    EXPECT_FALSE(otr_settings[1].incognito);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, content_settings::ValueToContentSetting(
                                         otr_settings[1].setting_value));
  }

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  otr_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      otr_map->GetContentSetting(host, host, ContentSettingsType::COOKIES));

  // The GetSettingsForOneType includes incognito setting first, but also
  // inherited ones.
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::COOKIES, &otr_settings);
    ASSERT_EQ(3u, otr_settings.size());
    EXPECT_TRUE(otr_settings[0].incognito);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, content_settings::ValueToContentSetting(
                                         otr_settings[0].setting_value));
    EXPECT_FALSE(otr_settings[1].incognito);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, content_settings::ValueToContentSetting(
                                         otr_settings[1].setting_value));
    EXPECT_FALSE(otr_settings[2].incognito);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, content_settings::ValueToContentSetting(
                                         otr_settings[2].setting_value));
  }
}

TEST_F(HostContentSettingsMapTest, IncognitoInheritPopups) {
  // The popup setting has an initial value of BLOCK, but it is allowed
  // to inherit ALLOW settings because it doesn't provide access to user data.
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS));

  // Changing content settings on the main map should affect the
  // incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS));

  // Changing content settings on the incognito map should NOT affect the
  // main map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::POPUPS, CONTENT_SETTING_BLOCK);
  otr_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::POPUPS));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      otr_map->GetContentSetting(host, host, ContentSettingsType::POPUPS));
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritPref) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetContentSetting(host, host,
                                       ContentSettingsType::MEDIASTREAM_MIC));

  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::MEDIASTREAM_MIC,
                                   &otr_settings);
    ASSERT_EQ(1u, otr_settings.size());
    EXPECT_FALSE(otr_settings[0].incognito);
    EXPECT_EQ(CONTENT_SETTING_ASK, content_settings::ValueToContentSetting(
                                       otr_settings[0].setting_value));
  }

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetContentSetting(host, host,
                                       ContentSettingsType::MEDIASTREAM_MIC));

  // GetSettingsForOneType should return preference followed by default, both inherited.
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::MEDIASTREAM_MIC,
                                   &otr_settings);
    ASSERT_EQ(2u, otr_settings.size());
    EXPECT_FALSE(otr_settings[0].incognito);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, content_settings::ValueToContentSetting(
                                         otr_settings[0].setting_value));

    EXPECT_FALSE(otr_settings[1].incognito);
    EXPECT_EQ(CONTENT_SETTING_ASK, content_settings::ValueToContentSetting(
                                       otr_settings[1].setting_value));
  }

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::MEDIASTREAM_MIC,
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetContentSetting(host, host,
                                       ContentSettingsType::MEDIASTREAM_MIC));

  // The inherited ALLOW gets turned back into ASK in GetSettingsForOneType,
  // mirroring the reverting to ASK behavior above.
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::MEDIASTREAM_MIC,
                                   &otr_settings);
    ASSERT_EQ(2u, otr_settings.size());
    EXPECT_FALSE(otr_settings[0].incognito);
    EXPECT_EQ(CONTENT_SETTING_ASK, content_settings::ValueToContentSetting(
                                       otr_settings[0].setting_value));

    EXPECT_FALSE(otr_settings[1].incognito);
    EXPECT_EQ(CONTENT_SETTING_ASK, content_settings::ValueToContentSetting(
                                       otr_settings[1].setting_value));
  }
}

TEST_F(HostContentSettingsMapTest, IncognitoPartialInheritDefault) {
  // Permissions marked INHERIT_IF_LESS_PERMISSIVE in
  // ContentSettingsRegistry only inherit BLOCK and ASK settings from regular
  // to incognito if the initial value is ASK.
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                              nullptr));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(host, host, ContentSettingsType::GEOLOCATION));

  // BLOCK should be inherited from the main map to the incognito map.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            otr_map->GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                              nullptr));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      otr_map->GetContentSetting(host, host, ContentSettingsType::GEOLOCATION));

  // ALLOW should not be inherited from the main map to the incognito map (but
  // it still overwrites the BLOCK, hence incognito reverts to ASK).
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::GEOLOCATION, nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            otr_map->GetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                              nullptr));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      otr_map->GetContentSetting(host, host, ContentSettingsType::GEOLOCATION));
}

TEST_F(HostContentSettingsMapTest, IncognitoDontInheritSetting) {
  // Website settings marked DONT_INHERIT_IN_INCOGNITO in
  // WebsiteSettingsRegistry (e.g. usb chooser data) don't inherit any values
  // from from regular to incognito.
  TestingProfile profile;
  Profile* otr_profile =
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  HostContentSettingsMap* otr_map =
      HostContentSettingsMapFactory::GetForProfile(otr_profile);

  GURL host("http://example.com/");

  // USB chooser data defaults to |nullptr|.
  EXPECT_EQ(base::Value(),
            host_content_settings_map->GetWebsiteSetting(
                host, host, ContentSettingsType::USB_CHOOSER_DATA, nullptr));
  EXPECT_EQ(base::Value(),
            otr_map->GetWebsiteSetting(
                host, host, ContentSettingsType::USB_CHOOSER_DATA, nullptr));

  base::Value::Dict test_dict;
  test_dict.Set("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      host, host, ContentSettingsType::USB_CHOOSER_DATA,
      base::Value(test_dict.Clone()));

  // The setting is not inherted by |otr_map|.
  base::Value stored_value = host_content_settings_map->GetWebsiteSetting(
      host, host, ContentSettingsType::USB_CHOOSER_DATA, nullptr);
  EXPECT_EQ(stored_value, test_dict);
  EXPECT_EQ(base::Value(),
            otr_map->GetWebsiteSetting(
                host, host, ContentSettingsType::USB_CHOOSER_DATA, nullptr));
  {
    ContentSettingsForOneType otr_settings;
    otr_map->GetSettingsForOneType(ContentSettingsType::USB_CHOOSER_DATA,
                                   &otr_settings);
    // Nothing gets inherited here, and there is no default.
    ASSERT_EQ(0u, otr_settings.size());
  }
}

TEST_F(HostContentSettingsMapTest, PrefExceptionsOperation) {
  using content_settings::SETTING_SOURCE_POLICY;
  using content_settings::SETTING_SOURCE_USER;

  const char kUrl1[] = "http://user_exception_allow.com";
  const char kUrl2[] = "http://user_exception_block.com";
  const char kUrl3[] = "http://non_exception.com";

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  TesterForType tester(&profile, ContentSettingsType::COOKIES);

  // Add |kUrl1| and |kUrl2| only.
  tester.AddUserException(kUrl1, CONTENT_SETTING_ALLOW);
  tester.AddUserException(kUrl2, CONTENT_SETTING_BLOCK);

  // No policy setting: follow users settings.
  tester.ClearPolicyDefault();
  // User exceptions.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl2));
  // User default.
  EXPECT_EQ(SETTING_SOURCE_USER, tester.GetSettingSourceForURL(kUrl3));

  // Policy overrides users always.
  tester.SetPolicyDefault(CONTENT_SETTING_ALLOW);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
  tester.SetPolicyDefault(CONTENT_SETTING_BLOCK);
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl1));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl2));
  EXPECT_EQ(SETTING_SOURCE_POLICY, tester.GetSettingSourceForURL(kUrl3));
}

TEST_F(HostContentSettingsMapTest, GetUserModifiableContentSetting) {
  GURL url("http://user_exception_allow.com");

  TestingProfile profile;
  // Arbitrarily using cookies as content type to test.
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultCookiesSetting,
      std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_ALLOW, map->GetUserModifiableContentSetting(
                                       url, url, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            map->GetContentSetting(url, url, ContentSettingsType::COOKIES));
}

// For a single Unicode encoded pattern, check if it gets converted to punycode
// and old pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeOnly) {
  TestingProfile profile;
  PrefService* prefs = profile.GetPrefs();

  // Set utf-8 data.
  {
    ScopedDictPrefUpdate update(prefs,
                                GetPrefName(ContentSettingsType::COOKIES));
    base::Value::Dict& all_settings_dictionary = update.Get();

    base::Value::Dict dummy_payload;
    dummy_payload.Set("setting", CONTENT_SETTING_ALLOW);
    all_settings_dictionary.Set("[*.]\xC4\x87ira.com,*",
                                std::move(dummy_payload));
  }

  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::Value::Dict& all_settings_dictionary =
      prefs->GetDict(GetPrefName(ContentSettingsType::COOKIES));
  EXPECT_FALSE(all_settings_dictionary.FindDict("[*.]\xC4\x87ira.com,*"));
  EXPECT_TRUE(all_settings_dictionary.FindDict("[*.]xn--ira-ppa.com,*"));
}

// If both Unicode and its punycode pattern exist, make sure we don't touch the
// settings for the punycode, and that Unicode pattern gets deleted.
TEST_F(HostContentSettingsMapTest, CanonicalizeExceptionsUnicodeAndPunycode) {
  TestingProfile profile;

  base::Value value =
      base::JSONReader::Read("{\"[*.]\\xC4\\x87ira.com,*\":{\"setting\":1}}")
          .value();
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::COOKIES), value);

  // Set punycode equivalent, with different setting.
  base::Value puny_value =
      base::JSONReader::Read("{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}")
          .value();
  profile.GetPrefs()->Set(GetPrefName(ContentSettingsType::COOKIES),
                          puny_value);

  // Initialize the content map.
  HostContentSettingsMapFactory::GetForProfile(&profile);

  const base::Value& content_setting_prefs =
      profile.GetPrefs()->GetValue(GetPrefName(ContentSettingsType::COOKIES));
  std::string prefs_as_json;
  base::JSONWriter::Write(content_setting_prefs, &prefs_as_json);
  EXPECT_STREQ("{\"[*.]xn--ira-ppa.com,*\":{\"setting\":2}}",
               prefs_as_json.c_str());
}

// If a default-content-setting is managed, the managed value should be used
// instead of the default value.
TEST_F(HostContentSettingsMapTest, ManagedDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, nullptr));

  // Set managed-default-content-setting through the coresponding preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultAdsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, nullptr));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultAdsSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::ADS, nullptr));
}

TEST_F(HostContentSettingsMapTest,
       GetNonDefaultContentSettingsIfTypeManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set url for JavaScript setting.
  GURL host("http://example.com/");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));

  // Set managed-default-content-setting for content-settings-type JavaScript.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));
}

// Managed default content setting should have higher priority
// than user defined patterns.
TEST_F(HostContentSettingsMapTest,
       ManagedDefaultContentSettingIgnoreUserPattern) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Block all JavaScript.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);

  // Set an exception to allow "[*.]example.com"
  GURL host("http://example.com/");

  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::JAVASCRIPT, nullptr));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));

  // Set managed-default-content-settings-preferences.
  prefs->SetManagedPref(prefs::kManagedDefaultJavaScriptSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));

  // Remove managed-default-content-settings-preferences.
  prefs->RemoveManagedPref(prefs::kManagedDefaultJavaScriptSetting);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::JAVASCRIPT));
}

// If a default-content-setting is set to managed setting, the user defined
// setting should be preserved.
TEST_F(HostContentSettingsMapTest, OverwrittenDefaultContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  // Set user defined default-content-setting for Cookies.
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  // Set preference to manage the default-content-setting for Cookies.
  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  // Remove the preference to manage the default-content-setting for Cookies.
  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));
}

// If a setting for a default-content-setting-type is set while the type is
// managed, then the new setting should be preserved and used after the
// default-content-setting-type is not managed anymore.
TEST_F(HostContentSettingsMapTest, SettingDefaultContentSettingsWhenManaged) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile.GetTestingPrefService();

  prefs->SetManagedPref(prefs::kManagedDefaultCookiesSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));

  prefs->RemoveManagedPref(prefs::kManagedDefaultCookiesSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetDefaultContentSetting(
                ContentSettingsType::COOKIES, nullptr));
}

TEST_F(HostContentSettingsMapTest, GetContentSetting) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL host("http://example.com/");
  GURL embedder("chrome://foo");
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                embedder, host, ContentSettingsType::COOKIES));
}

TEST_F(HostContentSettingsMapTest, AddContentSettingsObserver) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::MockObserver mock_observer;

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  EXPECT_CALL(mock_observer, OnContentSettingChanged(
                                 pattern, ContentSettingsPattern::Wildcard(),
                                 ContentSettingsType::COOKIES));

  host_content_settings_map->AddObserver(&mock_observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_DEFAULT);
}

// Guest profiles do not exist on Android, so don't run these tests there.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(HostContentSettingsMapTest, GuestProfile) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetGuestSession();
  std::unique_ptr<Profile> profile = profile_builder.Build();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile.get());

  GURL host("http://example.com/");
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.com");

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  // Changing content settings should not result in any prefs being stored
  // however the value should be set in memory for OTR-Guest profiles.
  host_content_settings_map->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  const base::Value::Dict& all_settings_dictionary =
      profile->GetPrefs()->GetDict(GetPrefName(ContentSettingsType::COOKIES));
  EXPECT_TRUE(all_settings_dictionary.empty());
}

// Default settings should not be modifiable for Guest profile (there is no UI
// to do this).
TEST_F(HostContentSettingsMapTest, GuestProfileDefaultSetting) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetGuestSession();
  std::unique_ptr<Profile> profile = profile_builder.Build();
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile.get());

  GURL host("http://example.com/");

  // There are no custom rules, so this should be the default.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            host_content_settings_map->GetContentSetting(
                host, host, ContentSettingsType::COOKIES));

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);

    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              host_content_settings_map->GetContentSetting(
                  host, host, ContentSettingsType::COOKIES));
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(HostContentSettingsMapTest, InvalidPattern) {
  // This is a regression test for crbug.com/618529, which fixed a memory leak
  // when a website setting was set under a URL that mapped to an invalid
  // pattern.
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  GURL unsupported_url = GURL("view-source:http://www.google.com");
  base::Value::Dict test_dict;
  test_dict.Set("test", "value");
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      unsupported_url, unsupported_url, ContentSettingsType::APP_BANNER,
      base::Value(std::move(test_dict)));
  EXPECT_EQ(base::Value(), host_content_settings_map->GetWebsiteSetting(
                               unsupported_url, unsupported_url,
                               ContentSettingsType::APP_BANNER, nullptr));
}

TEST_F(HostContentSettingsMapTest, ClearSettingsForOneTypeWithPredicate) {
  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  ContentSettingsForOneType host_settings;

  // Patterns with wildcards.
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]example.org");
  ContentSettingsPattern pattern2 =
      ContentSettingsPattern::FromString("[*.]example.net");

  // Patterns without wildcards.
  GURL url1("https://www.google.com/");
  GURL url2("https://www.google.com/maps");
  GURL url3("http://www.google.com/maps");
  GURL url3_origin_only("http://www.google.com/");

  host_content_settings_map->SetContentSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK);
  host_content_settings_map->SetWebsiteSettingCustomScope(
      pattern2, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::APP_BANNER, base::Value(base::Value::Type::DICT));

  // First, test that we clear only COOKIES (not APP_BANNER), and pattern2.
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::COOKIES, base::Time(), base::Time::Max(),
      base::BindRepeating(&MatchPrimaryPattern, pattern2));
  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::COOKIES,
                                                   &host_settings);
  // |host_settings| contains default & block.
  EXPECT_EQ(2U, host_settings.size());
  EXPECT_EQ(pattern, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].secondary_pattern.ToString());

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::APP_BANNER, &host_settings);
  // |host_settings| still contains the value for APP_BANNER.
  EXPECT_EQ(1U, host_settings.size());
  EXPECT_EQ(pattern2, host_settings[0].primary_pattern);
  EXPECT_EQ("*", host_settings[0].secondary_pattern.ToString());

  // Next, test that we do correct pattern matching w/ an origin policy item.
  // We verify that we have no settings stored.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  // Add settings.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url1, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICT));
  // This setting should override the one above, as it's the same origin.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url2, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICT));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      url3, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(base::Value::Type::DICT));
  // Verify we only have two.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, &host_settings);
  EXPECT_EQ(2u, host_settings.size());

  // Clear the http one, which we should be able to do w/ the origin only, as
  // the scope of ContentSettingsType::SITE_ENGAGEMENT is
  // REQUESTING_ORIGIN_ONLY_SCOPE.
  ContentSettingsPattern http_pattern =
      ContentSettingsPattern::FromURLNoWildcard(url3_origin_only);
  host_content_settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::SITE_ENGAGEMENT, base::Time(), base::Time::Max(),
      base::BindRepeating(&MatchPrimaryPattern, http_pattern));
  // Verify we only have one, and it's url1.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::SITE_ENGAGEMENT, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::FromURLNoWildcard(url1),
            host_settings[0].primary_pattern);
}

TEST_F(HostContentSettingsMapTest, ClearSettingsWithTimePredicate) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  base::Time now = base::Time::Now();
  base::Time back_1_hour = now - base::Hours(1);
  base::Time back_30_days = now - base::Days(30);
  base::Time back_31_days = now - base::Days(31);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(now);
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url1("https://www.google.com/");
  GURL url2("https://maps.google.com/");
  GURL url3("https://photos.google.com");

  // Add setting for url1.
  map->SetContentSettingDefaultScope(url1, GURL(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_BLOCK);

  // Add setting for url2.
  test_clock.SetNow(back_1_hour);
  map->SetContentSettingDefaultScope(url2, GURL(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_BLOCK);

  // Add setting for url3 with the timestamp of 31 days old.
  test_clock.SetNow(back_31_days);
  map->SetContentSettingDefaultScope(url3, GURL(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_BLOCK);

  // Verify we have three pattern and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, &host_settings);
  EXPECT_EQ(4u, host_settings.size());

  // Clear all settings since |now|.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, now, base::Time::Max(),
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we have two pattern (url2, url3) and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, &host_settings);
  EXPECT_EQ(3u, host_settings.size());
  EXPECT_EQ("https://maps.google.com:443",
            host_settings[0].primary_pattern.ToString());
  EXPECT_EQ("https://photos.google.com:443",
            host_settings[1].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[2].primary_pattern.ToString());

  // Clear all settings since the beginning of time to 30 days old.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, base::Time(), back_30_days,
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have one pattern (url2) and the default.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, &host_settings);
  EXPECT_EQ(2u, host_settings.size());
  EXPECT_EQ("https://maps.google.com:443",
            host_settings[0].primary_pattern.ToString());
  EXPECT_EQ("*", host_settings[1].primary_pattern.ToString());

  // Clear all settings since the beginning of time.
  map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::POPUPS, base::Time(), base::Time::Max(),
      HostContentSettingsMap::PatternSourcePredicate());

  // Verify we only have the default setting.
  map->GetSettingsForOneType(ContentSettingsType::POPUPS, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ("*", host_settings[0].primary_pattern.ToString());
}

TEST_F(HostContentSettingsMapTest, GetSettingLastModified) {
  TestingProfile profile;
  auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  map->SetClockForTesting(&test_clock);

  ContentSettingsForOneType host_settings;

  GURL url("https://www.google.com/");

  // Last modified date for non existent settings should be base::Time().
  base::Time t =
      GetSettingLastModifiedDate(map, url, url, ContentSettingsType::POPUPS);
  EXPECT_EQ(base::Time(), t);

  // Add setting for url.
  map->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_BLOCK);
  t = GetSettingLastModifiedDate(map, url, url, ContentSettingsType::POPUPS);
  EXPECT_EQ(t, test_clock.Now());

  test_clock.Advance(base::Seconds(1));
  // Modify setting.
  map->SetContentSettingDefaultScope(url, GURL(), ContentSettingsType::POPUPS,
                                     CONTENT_SETTING_ALLOW);

  t = GetSettingLastModifiedDate(map, url, url, ContentSettingsType::POPUPS);
  EXPECT_EQ(t, test_clock.Now());
}

TEST_F(HostContentSettingsMapTest, IsRestrictedToSecureOrigins) {
  TestingProfile profile;
  const auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);
  EXPECT_TRUE(
      map->IsRestrictedToSecureOrigins(ContentSettingsType::GEOLOCATION));

  EXPECT_FALSE(
      map->IsRestrictedToSecureOrigins(ContentSettingsType::JAVASCRIPT));
}

TEST_F(HostContentSettingsMapTest, CanSetNarrowestSetting) {
  TestingProfile profile;
  const auto* map = HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL valid_url("http://google.com");
  EXPECT_TRUE(map->CanSetNarrowestContentSetting(valid_url, valid_url,
                                                 ContentSettingsType::POPUPS));

  GURL invalid_url("about:blank");
  EXPECT_FALSE(map->CanSetNarrowestContentSetting(invalid_url, invalid_url,
                                                  ContentSettingsType::POPUPS));
}

TEST_F(HostContentSettingsMapTest, MigrateRequestingAndTopLevelOriginSettings) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL requesting_origin("https://requester.com");
  GURL embedding_origin("https://embedder.com");

  ContentSettingsPattern requesting_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin);
  ContentSettingsPattern embedding_pattern =
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin);

  map->AllowInvalidSecondaryPatternForTesting(true);

  // Set content settings for 2 types that use requesting and top level
  // origin as well as one for a type that doesn't.
  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::MIDI_SYSEX,
                                    CONTENT_SETTING_ALLOW);

  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::COOKIES,
                                    CONTENT_SETTING_ALLOW);

  map->MigrateSettingsPrecedingPermissionDelegationActivation();

  map->AllowInvalidSecondaryPatternForTesting(false);

  ContentSettingsForOneType host_settings;
  // Verify that all the settings are deleted except the cookies setting.
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);

  map->GetSettingsForOneType(ContentSettingsType::MIDI_SYSEX, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);

  map->GetSettingsForOneType(ContentSettingsType::COOKIES, &host_settings);
  EXPECT_EQ(2u, host_settings.size());
  EXPECT_EQ(requesting_pattern, host_settings[0].primary_pattern);
  EXPECT_EQ(embedding_pattern, host_settings[0].secondary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[1].secondary_pattern);
}

TEST_F(HostContentSettingsMapTest,
       MigrateRequestingAndTopLevelOriginSettingsResetsEmbeddedSetting) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  GURL requesting_origin("https://requester.com");
  GURL embedding_origin("https://embedder.com");

  ContentSettingsPattern requesting_pattern =
      ContentSettingsPattern::FromURLNoWildcard(requesting_origin);
  ContentSettingsPattern embedding_pattern =
      ContentSettingsPattern::FromURLNoWildcard(embedding_origin);

  map->AllowInvalidSecondaryPatternForTesting(true);

  map->SetContentSettingCustomScope(requesting_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    CONTENT_SETTING_BLOCK);
  map->SetContentSettingCustomScope(embedding_pattern, embedding_pattern,
                                    ContentSettingsType::GEOLOCATION,
                                    CONTENT_SETTING_ALLOW);

  map->MigrateSettingsPrecedingPermissionDelegationActivation();

  map->AllowInvalidSecondaryPatternForTesting(false);

  ContentSettingsForOneType host_settings;
  // Verify that all settings for the embedding origin are deleted. This is
  // important so that a user is repropmted if a permission request from an
  // embedded site they had previously blocked makes a new request.
  map->GetSettingsForOneType(ContentSettingsType::GEOLOCATION, &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(),
            host_settings[0].secondary_pattern);
}

// Creates new instance of PrefProvider and overrides it in
// |host_content_settings_map|.
void ReloadProviders(PrefService* pref_service,
                     HostContentSettingsMap* host_content_settings_map) {
  auto pref_provider = std::make_unique<content_settings::PrefProvider>(
      pref_service, false, true, false);
  content_settings::TestUtils::OverrideProvider(
      host_content_settings_map, std::move(pref_provider),
      HostContentSettingsMap::PREF_PROVIDER);
}

TEST_F(HostContentSettingsMapTest, GetPatternsFromScopingType) {
  const GURL primary_url("http://a.b.example1.com:8080");
  const GURL secondary_url("http://a.b.example2.com:8080");

  TestingProfile profile;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  // Testing case:
  //   WebsiteSettingsInfo::REQUESTING_ORIGIN_WITH_TOP_ORIGIN_EXCEPTIONS_SCOPE.
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::COOKIES,
      CONTENT_SETTING_ALLOW);

  ContentSettingsForOneType settings;

  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::COOKIES,
                                                   &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURL(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern, ContentSettingsPattern::Wildcard());

  // Testing cases:
  //   WebsiteSettingsInfo::REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE,
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS, &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            content_settings::URLToSchemefulSitePattern(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern,
            content_settings::URLToSchemefulSitePattern(secondary_url));

  // Testing cases:
  //   WebsiteSettingsInfo::REQUESTING_AND_TOP_ORIGIN_SCOPE,
  host_content_settings_map->SetContentSettingDefaultScope(
      primary_url, secondary_url, ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW);

  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, &settings);

  EXPECT_EQ(settings[0].primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(primary_url));
  EXPECT_EQ(settings[0].secondary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(secondary_url));

  // Testing cases:
  //   WebsiteSettingsInfo::TOP_ORIGIN_WITH_RESOURCE_EXCEPTIONS_SCOPE,
  //   WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
  //   WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
  //   WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE.
  for (const auto& kContentSetting :
       {ContentSettingsType::JAVASCRIPT, ContentSettingsType::NOTIFICATIONS,
        ContentSettingsType::GEOLOCATION,
        ContentSettingsType::FEDERATED_IDENTITY_API}) {
    host_content_settings_map->SetContentSettingDefaultScope(
        primary_url, secondary_url, kContentSetting, CONTENT_SETTING_ALLOW);

    host_content_settings_map->GetSettingsForOneType(kContentSetting,
                                                     &settings);

    EXPECT_EQ(settings[0].primary_pattern,
              ContentSettingsPattern::FromURLNoWildcard(primary_url));
    EXPECT_EQ(settings[0].secondary_pattern,
              ContentSettingsPattern::Wildcard());
  }
}

// Tests if changing a settings in incognito mode does not affects the regular
// mode.
TEST_F(HostContentSettingsMapTest, IncognitoChangesDoNotPersist) {
  TestingProfile profile;
  auto* regular_map = HostContentSettingsMapFactory::GetForProfile(&profile);
  auto* incognito_map = HostContentSettingsMapFactory::GetForProfile(
      profile.GetPrimaryOTRProfile(/*create_if_needed=*/true));
  auto* registry = content_settings::WebsiteSettingsRegistry::GetInstance();
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();

  GURL url("https://example.com");
  ContentSettingsPattern pattern = ContentSettingsPattern::FromURL(url);
  content_settings::SettingInfo setting_info;

  for (const content_settings::WebsiteSettingsInfo* info : *registry) {
    SCOPED_TRACE(info->name());

    // Get regular profile default value.
    base::Value original_value =
        regular_map->GetWebsiteSetting(url, url, info->type(), &setting_info);
    // Get a different valid value for incognito mode.
    base::Value new_value;
    if (content_setting_registry->Get(info->type())) {
      // If no original value is available, the settings does not have any valid
      // values and no more steps are required.
      if (!original_value.is_int())
        continue;

      for (int another_value = 0;
           another_value < ContentSetting::CONTENT_SETTING_NUM_SETTINGS;
           another_value++) {
        if (another_value != original_value.GetInt() &&
            content_setting_registry->Get(info->type())
                ->IsSettingValid(static_cast<ContentSetting>(another_value))) {
          new_value = base::Value(another_value);
          break;
        }
      }
      ASSERT_NE(new_value.type(), base::Value::Type::NONE)
          << "Every content setting should have at least two values.";
    } else {
      base::Value::Dict dict;
      dict.SetByDottedPath("foo.bar", 0);
      new_value = base::Value(std::move(dict));
    }
    // Ensure a different value is received.
    ASSERT_NE(original_value, new_value);

    // Set the different value in incognito mode.
    base::Value incognito_value = new_value.Clone();
    incognito_map->SetWebsiteSettingCustomScope(
        pattern, ContentSettingsPattern::Wildcard(), info->type(),
        std::move(new_value));

    // Ensure incognito mode value is changed.
    EXPECT_EQ(incognito_value, incognito_map->GetWebsiteSetting(
                                   url, url, info->type(), &setting_info));

    // Ensure regular mode value is not changed.
    base::Value regular_mode_value =
        regular_map->GetWebsiteSetting(url, url, info->type(), &setting_info);
    EXPECT_EQ(original_value, regular_mode_value);
  }
}

// Validate that a content setting that uses a different scope/constraint can
// co-exist with another setting
TEST_F(HostContentSettingsMapTest, MixedScopeSettings) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
  ReloadProviders(profile.GetPrefs(), map);

  // The following type is used as a sample of the persistent permission type.
  // It can be replaced with any other type if required.
  const ContentSettingsType persistent_type =
      ContentSettingsType::STORAGE_ACCESS;

  const GURL example_url1("https://example.com");
  const GURL example_url2("https://other_site.example");

  // Set default permission to ASK and expect it for a website.
  map->SetDefaultContentSetting(persistent_type, CONTENT_SETTING_ASK);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url1, example_url1, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url2, example_url2, persistent_type));

  // Set permission and expect to retrieve it correctly. This will default to a
  // never expiring Durable permission.
  map->SetContentSettingDefaultScope(example_url1, example_url1,
                                     persistent_type, CONTENT_SETTING_BLOCK);
  // Set a Session only permission for our second url and we expect it should
  // co-exist with the other permission just fine.
  map->SetContentSettingDefaultScope(
      example_url2, example_url2, persistent_type, CONTENT_SETTING_ALLOW,
      {base::Time(), content_settings::SessionModel::UserSession});

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(example_url1, example_url1, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      map->GetContentSetting(example_url2, example_url2, persistent_type));

  // Restart and expect reset of Session scoped permission to ASK, while keeping
  // the permission of Durable scope.
  ReloadProviders(profile.GetPrefs(), map);

  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(example_url1, example_url1, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url2, example_url2, persistent_type));
}

// Validate that GetSettingsForOneType works with a SessionModel specified.
// We should act like no preference is specified if the value is
// SessionModel::None; otherwise, only the preferences from the specified
// scope should be returned (if any).
TEST_F(HostContentSettingsMapTest, GetSettingsForOneTypeWithSessionModel) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
  ReloadProviders(profile.GetPrefs(), map);

  // The following type is used as a sample of the persistent permission type.
  // It can be replaced with any other type if required.
  const ContentSettingsType persistent_type =
      ContentSettingsType::STORAGE_ACCESS;

  const GURL example_url1("https://example.com");
  const GURL example_url2("https://other_site.example");

  // Set default permission to ASK and expect it for a website.
  map->SetDefaultContentSetting(persistent_type, CONTENT_SETTING_ASK);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url1, example_url1, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url2, example_url2, persistent_type));

  // Set permissions in two different scopes.
  map->SetContentSettingDefaultScope(
      example_url1, example_url1, persistent_type, CONTENT_SETTING_BLOCK,
      {base::Time(), content_settings::SessionModel::Durable});
  map->SetContentSettingDefaultScope(
      example_url2, example_url2, persistent_type, CONTENT_SETTING_ALLOW,
      {base::Time(), content_settings::SessionModel::UserSession});

  // Validate that if we retrieve all our settings we should see both settings
  // and the default values returned.
  ContentSettingsForOneType settings;
  map->GetSettingsForOneType(persistent_type, &settings);
  ASSERT_EQ(3u, settings.size());

  // Validate that using no SessionModel functions the exact same way.
  map->GetSettingsForOneType(persistent_type, &settings, absl::nullopt);
  ASSERT_EQ(3u, settings.size());

  // Each one/type of settings we set should be retrievable by specifying the
  // specific scope without getting any of the other results. For Durable we
  // should see our set value and the default value.
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::Durable);
  ASSERT_EQ(2u, settings.size());

  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(1u, settings.size());

  // Reload to clear our session settings.
  ReloadProviders(profile.GetPrefs(), map);

  // If a scope is specified that has no settings, we should get an empty set
  // returned.
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(0u, settings.size());
}

// Validate that the settings array retrieved correctly carries the expiry data
// for settings and they can detect if and when they expire.
// GetSettingsForOneType should also omit any settings that are already expired.
TEST_F(HostContentSettingsMapTest, GetSettingsForOneTypeWithExpiry) {
  TestingProfile profile;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);

  content_settings::ContentSettingsRegistry::GetInstance()->ResetForTest();
  ReloadProviders(profile.GetPrefs(), map);

  // The following type is used as a sample of the persistent permission type.
  // It can be replaced with any other type if required.
  const ContentSettingsType persistent_type =
      ContentSettingsType::STORAGE_ACCESS;

  const GURL example_url1("https://example.com");
  const GURL example_url2("https://other_site.example");
  const GURL example_url3("https://third_site.example");

  // Set default permission to ASK and expect it for a website.
  map->SetDefaultContentSetting(persistent_type, CONTENT_SETTING_ASK);
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url1, example_url1, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url2, example_url2, persistent_type));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      map->GetContentSetting(example_url3, example_url3, persistent_type));

  // Set permissions with our first two urls with different expiry times and our
  // third with no expiration.
  map->SetContentSettingDefaultScope(
      example_url1, example_url1, persistent_type, CONTENT_SETTING_BLOCK,
      {content_settings::GetConstraintExpiration(base::Seconds(100)),
       content_settings::SessionModel::UserSession});
  map->SetContentSettingDefaultScope(
      example_url2, example_url2, persistent_type, CONTENT_SETTING_ALLOW,
      {content_settings::GetConstraintExpiration(base::Seconds(200)),
       content_settings::SessionModel::UserSession});
  map->SetContentSettingDefaultScope(
      example_url3, example_url3, persistent_type, CONTENT_SETTING_ALLOW,
      {base::Time(), content_settings::SessionModel::UserSession});

  // Validate that we can retrieve all our settings and none of them are
  // expired.
  ContentSettingsForOneType settings;
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(3u, settings.size());

  // None of our current settings should be expired, but we'll keep each one to
  // check the validity later on.
  ContentSettingPatternSource url1_setting;
  ContentSettingPatternSource url2_setting;
  ContentSettingPatternSource url3_setting;
  for (const auto& entry : settings) {
    ASSERT_FALSE(entry.IsExpired());
    // Store the relevant settings to check on later.
    if (entry.primary_pattern.Matches(example_url1)) {
      url1_setting = entry;
    } else if (entry.primary_pattern.Matches(example_url2)) {
      url2_setting = entry;
    } else if (entry.primary_pattern.Matches(example_url3)) {
      url3_setting = entry;
    }
  }

  // If we Fastforward by 101 seconds we should see only our first setting is
  // expired, we now retrieve 1 less setting and the rest are okay.
  FastForwardTime(base::Seconds(101));
  ASSERT_TRUE(url1_setting.IsExpired());
  ASSERT_FALSE(url2_setting.IsExpired());
  ASSERT_FALSE(url3_setting.IsExpired());
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(2u, settings.size());

  // If we fast forward again we should expire our second setting and drop if
  // from our retrieval list now.
  FastForwardTime(base::Seconds(101));
  ASSERT_TRUE(url1_setting.IsExpired());
  ASSERT_TRUE(url2_setting.IsExpired());
  ASSERT_FALSE(url3_setting.IsExpired());
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(1u, settings.size());

  // If we fast forwarding much further it shouldn't make a difference as our
  // last setting and the default setting should never expire.
  FastForwardTime(base::Minutes(100));
  ASSERT_TRUE(url1_setting.IsExpired());
  ASSERT_TRUE(url2_setting.IsExpired());
  ASSERT_FALSE(url3_setting.IsExpired());
  map->GetSettingsForOneType(persistent_type, &settings,
                             content_settings::SessionModel::UserSession);
  ASSERT_EQ(1u, settings.size());
}
