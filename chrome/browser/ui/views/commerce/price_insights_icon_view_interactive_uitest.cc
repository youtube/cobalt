// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/search/ntp_features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);

const char kShoppingURL[] = "/shopping.html";
const char kNonShoppingURL[] = "/non-shopping.html";
const char kProductClusterTitle[] = "Product Cluster Title";
int kIconExpandedMaxTimesLast28days = 3;

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("page content");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class PriceInsightsIconViewInteractiveTest : public InteractiveBrowserTest {
 public:
  PriceInsightsIconViewInteractiveTest() {
    test_features_.InitWithFeatures(
        {commerce::kCommerceAllowChipExpansion, commerce::kPriceInsights},
        {ntp_features::kNtpHistoryClustersModuleDiscounts});
  }

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();

    SetUpTabHelperAndShoppingService();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&PriceInsightsIconViewInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownInProcessBrowserTestFixture() override {
    is_browser_context_services_created = false;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    is_browser_context_services_created = true;
    commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context) {
          return commerce::MockShoppingService::Build();
        }));
  }

 protected:
  raw_ptr<commerce::MockShoppingService, AcrossTasksDanglingUntriaged>
      mock_shopping_service_;
  absl::optional<commerce::PriceInsightsInfo> price_insights_info_;
  absl::optional<commerce::ProductInfo> product_info_;
  base::CallbackListSubscription create_services_subscription_;
  bool is_browser_context_services_created{false};

 private:
  base::test::ScopedFeatureList test_features_;

  void SetUpTabHelperAndShoppingService() {
    EXPECT_TRUE(is_browser_context_services_created);
    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));

    price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
        true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);

    product_info_ = commerce::ProductInfo();
    product_info_->title = "Product";
    product_info_->product_cluster_title = "Product";
    product_info_->product_cluster_id = 12345L;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);

    EXPECT_CALL(*mock_shopping_service_, IsPriceInsightsEligible)
        .Times(testing::AnyNumber());

    mock_shopping_service_->SetIsPriceInsightsEligible(true);
    mock_shopping_service_->SetIsShoppingListEligible(false);
    mock_shopping_service_->SetIsDiscountEligibleToShowOnNavigation(false);

    MockGetProductInfoForUrlResponse();
    MockGetPriceInsightsInfoForUrlResponse();
  }

  void MockGetProductInfoForUrlResponse() {
    commerce::ProductInfo info;
    info.product_cluster_title = kProductClusterTitle;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(info);
  }

  void MockGetPriceInsightsInfoForUrlResponse() {
    absl::optional<commerce::PriceInsightsInfo> price_insights_info =
        commerce::CreateValidPriceInsightsInfo(
            true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info);
  }

  base::WeakPtrFactory<PriceInsightsIconViewInteractiveTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewInteractiveTest,
                       SidePanelShownOnPress) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl);
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(),
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the action chip to open the side panel
      PressButton(kPriceInsightsChipElementId),
      WaitForShow(kSidePanelElementId), FlushEvents(),
      // Click on the action chip again to close the side panel
      PressButton(kPriceInsightsChipElementId),
      WaitForHide(kSidePanelElementId), FlushEvents());
}

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewInteractiveTest,
                       IconIsNotHighlightedAfterClicking) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl);
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl);

  const bool expected_to_highlight = false;

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(), EnsurePresent(kPriceInsightsChipElementId),
      PressButton(kPriceInsightsChipElementId), FlushEvents(),
      CheckView(
          kPriceInsightsChipElementId,
          [](PriceInsightsIconView* icon) {
            return icon->IsIconHighlightedForTesting();
          },
          expected_to_highlight));
}

class PriceInsightsIconViewEngagementTest
    : public PriceInsightsIconViewInteractiveTest {
 public:
  PriceInsightsIconViewEngagementTest() {
    test_features_.InitAndEnableFeatures(
        {commerce::kPriceInsights,
         feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature},
        {});
    test_clock_.SetNow(base::Time::Now());
  }

  void SetUp() override { PriceInsightsIconViewInteractiveTest::SetUp(); }

  void SetUpOnMainThread() override {
    PriceInsightsIconViewInteractiveTest::SetUpOnMainThread();

    BrowserFeaturePromoController* const promo_controller =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetFeaturePromoController();
    EXPECT_TRUE(
        user_education::test::WaitForFeatureEngagementReady(promo_controller));
    EXPECT_TRUE(user_education::test::SetClock(promo_controller, test_clock_));
    RunTestSequence(InstrumentTab(kShoppingTab));
  }

  void NavigateToANonShoppingPage() {
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(absl::nullopt);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        absl::nullopt);
    RunTestSequence(
        NavigateWebContents(kShoppingTab,
                            embedded_test_server()->GetURL(kNonShoppingURL)),
        FlushEvents(), EnsureNotPresent(kPriceInsightsChipElementId));
  }

  void NavigateToAShoppingPage(bool expected_to_show_label) {
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(product_info_);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info_);
    RunTestSequence(
        NavigateWebContents(kShoppingTab,
                            embedded_test_server()->GetURL(kShoppingURL)),
        FlushEvents(), EnsurePresent(kPriceInsightsChipElementId),
        CheckViewProperty(kPriceInsightsChipElementId,
                          &PriceInsightsIconView::ShouldShowLabel,
                          expected_to_show_label));
  }

  void VerifyIconExpandedOncePerDay() {
    base::HistogramTester histogram_tester;
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      0);

    NavigateToANonShoppingPage();
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      0);

    NavigateToAShoppingPage(/*expected_to_show_label=*/true);
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 1, 1);

    NavigateToANonShoppingPage();
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 1, 1);

    NavigateToAShoppingPage(/*expected_to_show_label=*/false);
    histogram_tester.ExpectTotalCount("Commerce.PriceInsights.OmniboxIconShown",
                                      2);
    histogram_tester.ExpectBucketCount(
        "Commerce.PriceInsights.OmniboxIconShown", 0, 1);
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Commerce.PriceInsights.OmniboxIconShown"),
        BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1), base::Bucket(2, 0)));
  }

 protected:
  base::SimpleTestClock test_clock_;

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewEngagementTest,
                       ExpandedIconShownOncePerDayOnly) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());

  VerifyIconExpandedOncePerDay();
}

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewEngagementTest,
                       ExpandedIconShownMaxTimesLast28days) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl)
      .Times(testing::AnyNumber());
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl)
      .Times(testing::AnyNumber());
  while (kIconExpandedMaxTimesLast28days--) {
    VerifyIconExpandedOncePerDay();
    // Advance one day
    test_clock_.Advance(base::Days(1));
  }
  // Icon should not expanded after the max has reach.
  NavigateToANonShoppingPage();
  NavigateToAShoppingPage(/*expected_to_show_label=*/false);

  // Advance 28 days, icon should expand again.
  test_clock_.Advance(base::Days(28));
  NavigateToANonShoppingPage();
  NavigateToAShoppingPage(/*expected_to_show_label=*/true);
}
