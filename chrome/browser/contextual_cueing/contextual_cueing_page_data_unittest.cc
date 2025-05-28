// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"

namespace contextual_cueing {

class ContextualCueingPageDataTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    web_contents_ = CreateTestWebContents();
  }

  void TearDown() override {
    web_contents_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  void InvokePdfPageCountReceived(size_t page_count) {
    auto* page_data =
        ContextualCueingPageData::GetForPage(web_contents_->GetPrimaryPage());
    page_data->OnPdfPageCountReceived(
        pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess, {}, page_count);
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(ContextualCueingPageDataTest, Basic) {
  base::test::TestFuture<std::string> future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("basic label", future.Get());
}

TEST_F(ContextualCueingPageDataTest, NonPdfPageFails) {
  base::test::TestFuture<std::string> future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ContextualCueingPageDataTest, PdfPageCountFails) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<std::string> future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  InvokePdfPageCountReceived(1);

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(ContextualCueingPageDataTest, PdfPageCountPasses) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<std::string> future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  InvokePdfPageCountReceived(4);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("pdf label", future.Get());
}

TEST_F(ContextualCueingPageDataTest, BasicAndPdfPageCountCondition) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  base::test::TestFuture<std::string> future;
  optimization_guide::proto::GlicContextualCueingMetadata metadata;
  auto* config = metadata.add_cueing_configurations();
  config->set_cue_label("pdf label");
  auto* pdf_condition = config->add_conditions();
  pdf_condition->set_signal(optimization_guide::proto::
                                CONTEXTUAL_CUEING_CLIENT_SIGNAL_PDF_PAGE_COUNT);
  pdf_condition->set_cueing_operator(
      optimization_guide::proto::ContextualCueingOperator::
          CONTEXTUAL_CUEING_OPERATOR_GREATER_THAN_OR_EQUAL_TO);
  pdf_condition->set_int64_threshold(2);

  // Second basic condition should get picked.
  config = metadata.add_cueing_configurations();
  config->set_cue_label("basic label");

  ContextualCueingPageData::CreateForPage(web_contents_->GetPrimaryPage(),
                                          std::move(metadata),
                                          future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("basic label", future.Get());
}

}  // namespace contextual_cueing
