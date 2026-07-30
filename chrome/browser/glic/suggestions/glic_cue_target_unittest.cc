// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/pdf/common/constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using ::page_content_annotations::Category;
using ::page_content_annotations::CategoryType;
using ::page_content_annotations::PageContentAnnotationsResult;

class GlicCueTargetTest : public testing::Test {
 public:
  explicit GlicCueTargetTest(bool discard_shopping_pdfs = true) {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2,
             {{"ContextualCueingV2EduClassifierThreshold", "0.7"},
              {"ContextualCueingV2ShoppingClassifierThreshold", "0.6"},
              {"ContextualCueingV2DiscardShoppingPdfs",
               discard_shopping_pdfs ? "true" : "false"}}},
        },
        {});
  }

  void SetUp() override {
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();

    profile_ = testing_profile_manager->CreateTestingProfile(
        "TestProfile", std::move(testing_factories));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);

    // We don't strictly need these to be fully functional because
    // IsPageEligible doesn't use them, but the constructor requires them.
    mock_glic_keyed_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, IdentityManagerFactory::GetForProfile(profile_),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);

    mock_browser_window_interface_ =
        std::make_unique<MockBrowserWindowInterface>();
    EXPECT_CALL(*mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile_));

    target_ = std::make_unique<GlicCueTarget>(
        *mock_glic_keyed_service_,
        /*optimization_guide_keyed_service=*/nullptr,
        *mock_browser_window_interface_);
  }

  void TearDown() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    target_.reset();
    mock_browser_window_interface_.reset();
    mock_glic_keyed_service_.reset();
    web_contents_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  PageContentAnnotationsResult CreateAnnotationResult(CategoryType type,
                                                      int score_percent) {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{type, static_cast<float>(score_percent) / 100.0f}});
  }

  PageContentAnnotationsResult CreateMultiAnnotationResult(CategoryType type1,
                                                           int score1_percent,
                                                           CategoryType type2,
                                                           int score2_percent) {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{type1, static_cast<float>(score1_percent) / 100.0f},
         {type2, static_cast<float>(score2_percent) / 100.0f}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<TestingProfile> profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  GlicProfileManager glic_profile_manager_;

  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_keyed_service_;

  std::unique_ptr<GlicCueTarget> target_;
};

TEST_F(GlicCueTargetTest, IsPageEligible_LowScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 60);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_LowScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 50);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 70);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreShopping_PdfDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  // Although it's high score shopping, we discard shopping PDFs.
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEdu_PdfNotDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  // High score edu PDFs are still eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEduAndShopping_PdfDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is true, having shopping present disqualifies it.
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

class GlicCueTargetDoNotDiscardShoppingPdfsTest : public GlicCueTargetTest {
 public:
  GlicCueTargetDoNotDiscardShoppingPdfsTest()
      : GlicCueTargetTest(/*discard_shopping_pdfs=*/false) {}
};

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEdu_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreShopping_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is false, shopping PDFs are eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEduAndShopping_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is false, having both high scores is eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

}  // namespace
}  // namespace glic
