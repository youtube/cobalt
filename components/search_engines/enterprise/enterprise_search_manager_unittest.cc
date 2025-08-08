// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/enterprise/enterprise_search_manager.h"

#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pointee;
using testing::Property;

double kTimestamp = static_cast<double>(
    base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

base::Value::Dict GenerateSearchPrefEntry(const std::string& keyword,
                                          bool featured) {
  base::Value::Dict entry;
  entry.Set(DefaultSearchManager::kShortName, keyword + "name");
  entry.Set(DefaultSearchManager::kKeyword, featured ? "@" + keyword : keyword);
  entry.Set(DefaultSearchManager::kURL,
            std::string("https://") + keyword + ".com/{searchTerms}");
  entry.Set(DefaultSearchManager::kEnforcedByPolicy, false);
  entry.Set(DefaultSearchManager::kFeaturedByPolicy, featured);
  entry.Set(DefaultSearchManager::kFaviconURL,
            std::string("https://") + keyword + ".com/favicon.ico");
  entry.Set(DefaultSearchManager::kSafeForAutoReplace, false);
  entry.Set(DefaultSearchManager::kDateCreated, kTimestamp);
  entry.Set(DefaultSearchManager::kLastModified, kTimestamp);
  return entry;
}

base::Value::Dict GenerateSiteSearchPrefEntry(const std::string& keyword) {
  base::Value::Dict entry =
      GenerateSearchPrefEntry(keyword, /*featured=*/false);
  entry.Set(DefaultSearchManager::kCreatedByPolicy,
            static_cast<int>(TemplateURLData::CreatedByPolicy::kSiteSearch));
  return entry;
}

base::Value::Dict GenerateSearchAggregatorPrefEntry(const std::string& keyword,
                                                    bool featured) {
  base::Value::Dict entry = GenerateSearchPrefEntry(keyword, featured);
  entry.Set(
      DefaultSearchManager::kCreatedByPolicy,
      static_cast<int>(TemplateURLData::CreatedByPolicy::kSearchAggregator));
  entry.Set(DefaultSearchManager::kSuggestionsURL,
            std::string("https://") + keyword + ".com/suggest");
  return entry;
}

class EnterpriseSearchManagerTest : public testing::Test {
 public:
  EnterpriseSearchManagerTest() = default;
  ~EnterpriseSearchManagerTest() override = default;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    EnterpriseSearchManager::RegisterProfilePrefs(pref_service_->registry());

    scoped_feature_list_.InitAndEnableFeature(
        omnibox::kEnableSearchAggregatorPolicy);
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EnterpriseSearchManagerTest, EmptyList) {
  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(IsEmpty())).Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      base::Value::List());
}

TEST_F(EnterpriseSearchManagerTest, SiteSearchOnly) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")),
                      Pointee(Property(&TemplateURLData::keyword, u"docs")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}

TEST_F(EnterpriseSearchManagerTest, SearchAggregatorsOnly) {
  base::Value::List pref_value;
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback,
              Run(ElementsAre(
                  Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
                  Pointee(Property(&TemplateURLData::keyword, u"aggregator")))))
      .Times(1);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(pref_value));
}

TEST_F(EnterpriseSearchManagerTest, SiteSearchAndSearchAggregators) {
  base::Value::List site_search_pref_value;
  site_search_pref_value.Append(GenerateSiteSearchPrefEntry("work"));

  base::Value::List aggregator_pref_value;
  aggregator_pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/true));
  aggregator_pref_value.Append(
      GenerateSearchAggregatorPrefEntry("aggregator", /*featured=*/false));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(
      callback,
      Run(ElementsAre(Pointee(Property(&TemplateURLData::keyword, u"work")))));
  EXPECT_CALL(
      callback,
      Run(ElementsAre(
          Pointee(Property(&TemplateURLData::keyword, u"work")),
          Pointee(Property(&TemplateURLData::keyword, u"@aggregator")),
          Pointee(Property(&TemplateURLData::keyword, u"aggregator")))));

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(site_search_pref_value));

  pref_service()->SetManagedPref(
      EnterpriseSearchManager::kEnterpriseSearchAggregatorSettingsPrefName,
      std::move(aggregator_pref_value));
}

TEST_F(EnterpriseSearchManagerTest, SiteSearch_NotCreatedByPolicy) {
  base::Value::List pref_value;
  pref_value.Append(GenerateSiteSearchPrefEntry("work"));
  pref_value.Append(GenerateSiteSearchPrefEntry("docs"));

  base::MockRepeatingCallback<void(
      EnterpriseSearchManager::OwnedTemplateURLDataVector&&)>
      callback;
  EXPECT_CALL(callback, Run(_)).Times(0);

  EnterpriseSearchManager manager(pref_service(), callback.Get());
  pref_service()->SetUserPref(
      EnterpriseSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}
