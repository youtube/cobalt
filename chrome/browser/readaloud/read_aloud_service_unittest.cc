// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/readaloud/read_aloud_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

namespace {

class MockDomDistillerService
    : public dom_distiller::DomDistillerContextKeyedService {
 public:
  MockDomDistillerService()
      : DomDistillerContextKeyedService(nullptr,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        {}) {}
  MOCK_METHOD(std::unique_ptr<dom_distiller::ViewerHandle>,
              ViewUrlIgnoreCache,
              (dom_distiller::ViewRequestDelegate*,
               std::unique_ptr<dom_distiller::DistillerPage>,
               const GURL&),
              (override));
  MOCK_METHOD(std::unique_ptr<dom_distiller::DistillerPage>,
              CreateDefaultDistillerPageWithHandle,
              (std::unique_ptr<dom_distiller::SourcePageHandle>),
              (override));
};

std::unique_ptr<KeyedService> BuildMockDomDistillerService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockDomDistillerService>>();
}

}  // namespace

class ReadAloudServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kReadAloudNative);

    dom_distiller::DomDistillerServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockDomDistillerService));
  }

  MockDomDistillerService* mock_distiller_service() {
    return static_cast<MockDomDistillerService*>(
        dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
            profile()));
  }

  ReadAloudService* service() {
    return ReadAloudServiceFactory::GetForProfile(profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAloudServiceTest, DistillNullWebContents) {
  // Should be a completely safe no-op.
  service()->DistillPage(nullptr);
  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
}

TEST_F(ReadAloudServiceTest, DistillPageAndArticleReady) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  dom_distiller::ViewRequestDelegate* delegate_ptr = nullptr;
  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce([&](dom_distiller::ViewRequestDelegate* delegate,
                    std::unique_ptr<dom_distiller::DistillerPage> page,
                    const GURL& url) {
        delegate_ptr = delegate;
        return std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing());
      });

  service()->DistillPage(web_contents());

  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());
  ASSERT_NE(nullptr, delegate_ptr);

  // Simulate DomDistiller finishing distillation and triggering OnArticleReady.
  dom_distiller::DistilledArticleProto proto;
  delegate_ptr->OnArticleReady(&proto);

  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
}

TEST_F(ReadAloudServiceTest, OnArticleUpdated) {
  dom_distiller::ArticleDistillationUpdate update({}, false, false);
  // Should be a completely safe no-op.
  service()->OnArticleUpdated(update);
}

TEST_F(ReadAloudServiceTest, ShutdownClearsHandle) {
  NavigateAndCommit(GURL("https://www.example.com/article"));

  EXPECT_CALL(*mock_distiller_service(),
              CreateDefaultDistillerPageWithHandle(testing::_))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::test::MockDistillerPage>())));

  EXPECT_CALL(*mock_distiller_service(),
              ViewUrlIgnoreCache(service(), testing::_,
                                 GURL("https://www.example.com/article")))
      .WillOnce(testing::Return(testing::ByMove(
          std::make_unique<dom_distiller::ViewerHandle>(base::DoNothing()))));

  service()->DistillPage(web_contents());
  EXPECT_NE(nullptr, service()->GetViewerHandleForTesting());

  service()->Shutdown();
  EXPECT_EQ(nullptr, service()->GetViewerHandleForTesting());
}

}  // namespace readaloud
