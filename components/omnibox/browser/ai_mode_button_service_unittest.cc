// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/ai_mode_button_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/ai_mode_button_config.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class AiModeButtonServiceTest : public testing::Test {
 public:
  void SetUp() override {
    template_url_service()->Load();

    AiModeButtonService::GoogleStrings google_strings;
    google_strings.entrypoint_label = u"Google AI";
    google_strings.context_menu_label = u"Show Google AI";
    service_ = std::make_unique<AiModeButtonService>(template_url_service(),
                                                     google_strings);
  }

  TemplateURLService* template_url_service() {
    return search_engines_test_environment_.template_url_service();
  }

  TemplateURL* FindTurl(const std::u16string& keyword) {
    for (const auto& turl : template_url_service()->GetTemplateURLs()) {
      if (turl->keyword() == keyword) {
        return turl.get();
      }
    }
    NOTREACHED();
  }

  base::test::TaskEnvironment task_environment_;
  const std::vector<TemplateURLService::Initializer> test_engines_ = {
      {"google", "https://google.com/search?q={searchTerms}", "Google"},
      {"google2", "https://google.co.uk/search?q={searchTerms}", "Google 2"},
      {"nongoogle", "https://nongoogle.com/search?q={searchTerms}",
       "Non Google"},
      {"nongoogle2", "https://nongoogle.com/search?q={searchTerms}",
       "Non Google 2"},
  };
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_{
      {.template_url_service_initializer = test_engines_}};
  std::unique_ptr<AiModeButtonService> service_;
};

TEST_F(AiModeButtonServiceTest, ConfigWithGoogleDse) {
  // Expect google config when DSE is google.
  const auto* config = service_->GetCurrentConfig();
  ASSERT_TRUE(config);
  EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
  EXPECT_EQ(config->text, u"Google AI");
}

TEST_F(AiModeButtonServiceTest, NoConfigWithNonGoogleDse) {
  // Expect no config when DSE is not google.
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle"));
  const auto* config = service_->GetCurrentConfig();
  EXPECT_FALSE(config);
}

TEST_F(AiModeButtonServiceTest, CallbackCalledWhenDseChange) {
  base::MockRepeatingCallback<void(
      const ai_mode_button_config::AiModeButtonConfig*)>
      callback;

  // Expect immediate notification on registration.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_TRUE(config);
    EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
    EXPECT_EQ(config, service_->GetCurrentConfig());
  });
  auto subscription = service_->RegisterOnConfigChanged(callback.Get());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect call on DSE change; null config for non-google DSE.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_FALSE(config);
    EXPECT_FALSE(service_->GetCurrentConfig());
  });
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle"));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect no call on DSE change when config is unaffected.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"nongoogle2"));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Expect call on DSE change back; google config for google DSE.
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([this](const auto* config) {
    EXPECT_TRUE(config);
    EXPECT_EQ(config->id, SearchEngineType::SEARCH_ENGINE_GOOGLE);
    EXPECT_EQ(config, service_->GetCurrentConfig());
  });
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"google"));

  // Expect no call on DSE change when config is unaffected.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  template_url_service()->SetUserSelectedDefaultSearchProvider(
      FindTurl(u"google2"));
}
