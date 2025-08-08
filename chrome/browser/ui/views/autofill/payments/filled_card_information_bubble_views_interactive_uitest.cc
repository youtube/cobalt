// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/filled_card_information_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/filled_card_information_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card_test_api.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"

namespace autofill {

class ViewVisibilityWaiter : public views::ViewObserver {
 public:
  explicit ViewVisibilityWaiter(views::View* observed_view,
                                bool expected_visible)
      : view_(observed_view), expected_visible_(expected_visible) {
    observation_.Observe(view_.get());
  }
  ViewVisibilityWaiter(const ViewVisibilityWaiter&) = delete;
  ViewVisibilityWaiter& operator=(const ViewVisibilityWaiter&) = delete;

  ~ViewVisibilityWaiter() override = default;

  // Wait for changes to occur, or return immediately if view already has
  // expected visibility.
  void Wait() {
    if (expected_visible_ != view_->GetVisible()) {
      run_loop_.Run();
    }
  }

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    if (expected_visible_ == observed_view->GetVisible()) {
      run_loop_.Quit();
    }
  }

  raw_ptr<views::View> view_;
  const bool expected_visible_;
  base::RunLoop run_loop_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

class FilledCardInformationBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest,
      public FilledCardInformationBubbleControllerImpl::ObserverForTest {
 public:
  // Various events that can be waited on by the DialogEventWaiter.
  enum class BubbleEvent : int {
    BUBBLE_SHOWN,
  };

  FilledCardInformationBubbleViewsInteractiveUiTest() = default;
  ~FilledCardInformationBubbleViewsInteractiveUiTest() override = default;
  FilledCardInformationBubbleViewsInteractiveUiTest(
      const FilledCardInformationBubbleViewsInteractiveUiTest&) = delete;
  FilledCardInformationBubbleViewsInteractiveUiTest& operator=(
      const FilledCardInformationBubbleViewsInteractiveUiTest&) = delete;

  // FilledCardInformationBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(BubbleEvent::BUBBLE_SHOWN);
    }
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    FilledCardInformationBubbleControllerImpl* controller =
        static_cast<FilledCardInformationBubbleControllerImpl*>(
            FilledCardInformationBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    DCHECK(controller);
    controller->SetEventObserverForTesting(this);
  }

  void ShowBubble() {
    CreditCard card = test::GetVirtualCard();
    ShowBubble(&card, /*cvc=*/u"123");
  }

  void ShowBubble(const CreditCard* filled_card, const std::u16string& cvc) {
    ResetEventWaiterForSequence({BubbleEvent::BUBBLE_SHOWN});
    FilledCardInformationBubbleOptions options;
    options.masked_card_name =
        CreditCard::NetworkForDisplay(filled_card->network());
    options.masked_card_number_last_four =
        filled_card->ObfuscatedNumberWithVisibleLastFourDigits();
    options.filled_card = *filled_card;
    options.cvc = cvc;
    options.card_image = gfx::test::CreateImage(32, 20);
    GetController()->ShowBubble(options);
    ASSERT_TRUE(event_waiter_->Wait());
  }

  void ReshowBubble() {
    ResetEventWaiterForSequence({BubbleEvent::BUBBLE_SHOWN});
    GetController()->ReshowBubble();
    ASSERT_TRUE(event_waiter_->Wait());
  }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  std::u16string GetValueForField(FilledCardInformationBubbleField field) {
    return GetController()->GetValueForField(field);
  }

  void ClickOnField(FilledCardInformationBubbleField field) {
    GetController()->OnFieldClicked(field);
  }

  FilledCardInformationBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return FilledCardInformationBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  FilledCardInformationBubbleViews* GetBubbleViews() {
    FilledCardInformationBubbleControllerImpl* controller = GetController();
    if (!controller) {
      return nullptr;
    }

    return static_cast<FilledCardInformationBubbleViews*>(
        controller->GetBubble());
  }

  FilledCardInformationIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kFilledCardInformation);
    DCHECK(icon);
    return static_cast<FilledCardInformationIconView*>(icon);
  }

  void ResetEventWaiterForSequence(std::list<BubbleEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<BubbleEvent>>(std::move(event_sequence));
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  std::unique_ptr<EventWaiter<BubbleEvent>> event_waiter_;
};

// Invokes a bubble showing the complete information for the virtual card
// selected to fill the form.
IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       ShowBubble) {
  ShowBubble();
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());
}

// Invokes the bubble and verifies the bubble is dismissed upon page navigation.
// Flaky on macOS, Linux, and Win. crbug.com/1254101
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_DismissBubbleUponNavigation DISABLED_DismissBubbleUponNavigation
#else
#define MAYBE_DismissBubbleUponNavigation DismissBubbleUponNavigation
#endif
IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       MAYBE_DismissBubbleUponNavigation) {
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetBubbleViews());
  EXPECT_FALSE(GetIconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       CopyFieldValue) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  std::u16string clipboard_text;

  CreditCard card;
  test::SetCreditCardInfo(&card, "John Smith", "5454545454545454",
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  card.set_record_type(CreditCard::RecordType::kVirtualCard);
  card.set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  test_api(card).set_network_for_card(kMasterCard);
  ShowBubble(&card, u"345");

  // Verify the displayed text. We change the format of card number in the ui.
  EXPECT_EQ(GetValueForField(FilledCardInformationBubbleField::kCardNumber),
            u"5454 5454 5454 5454");
  EXPECT_EQ(
      GetValueForField(FilledCardInformationBubbleField::kExpirationMonth),
      base::ASCIIToUTF16(test::NextMonth().c_str()));
  EXPECT_EQ(GetValueForField(FilledCardInformationBubbleField::kExpirationYear),
            base::ASCIIToUTF16(test::NextYear().c_str()));
  EXPECT_EQ(GetValueForField(FilledCardInformationBubbleField::kCardholderName),
            u"John Smith");
  EXPECT_EQ(GetValueForField(FilledCardInformationBubbleField::kCvc), u"345");

  // Simulate clicking on each field in the bubble, ensuring that it was
  // copied to the clipboard and the selection was logged in UMA.
  base::HistogramTester histogram_tester;

  // Card number (also ensure it doesn't contain spaces):
  ClickOnField(FilledCardInformationBubbleField::kCardNumber);
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, u"5454545454545454");
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.FieldClicked",
      autofill_metrics::FilledCardInformationBubbleFieldClicked::kCardNumber,
      1);

  // Expiration month:
  ClickOnField(FilledCardInformationBubbleField::kExpirationMonth);
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, base::ASCIIToUTF16(test::NextMonth().c_str()));
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.FieldClicked",
      autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kExpirationMonth,
      1);

  // Expiration year:
  ClickOnField(FilledCardInformationBubbleField::kExpirationYear);
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, base::ASCIIToUTF16(test::NextYear().c_str()));
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.FieldClicked",
      autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kExpirationYear,
      1);

  // Cardholder name:
  ClickOnField(FilledCardInformationBubbleField::kCardholderName);
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, u"John Smith");
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.FieldClicked",
      autofill_metrics::FilledCardInformationBubbleFieldClicked::
          kCardholderName,
      1);

  // CVC:
  ClickOnField(FilledCardInformationBubbleField::kCvc);
  clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr,
                      &clipboard_text);
  EXPECT_EQ(clipboard_text, u"345");
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.FieldClicked",
      autofill_metrics::FilledCardInformationBubbleFieldClicked::kCVC, 1);
}

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       Metrics_BubbleShownAndClosedByUser) {
  base::HistogramTester histogram_tester;

  ShowBubble();
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.Shown", false, 1);

  // Mock deactivation due to clicking the close button.
  views::test::WidgetDestroyedWaiter destroyed_waiter1(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter1.Wait();

  // Confirm .FirstShow metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.FilledCardInformationBubble.Result.FirstShow",
      autofill_metrics::FilledCardInformationBubbleResult::kClosed, 1);

  // Bubble is reshown by the user.
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.Shown", true, 1);

  // Mock deactivation due to clicking the close button.
  views::test::WidgetDestroyedWaiter destroyed_waiter2(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter2.Wait();

  // Confirm .Reshows metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.FilledCardInformationBubble.Result.Reshows",
      autofill_metrics::FilledCardInformationBubbleResult::kClosed, 1);

  // Bubble is reshown by the user. Closing a reshown bubble makes the browser
  // inactive for some reason, so we must reactivate it first.
  browser()->window()->Activate();
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.Shown", true, 2);
}

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       Metrics_BubbleClosedByNotInteracted) {
  base::HistogramTester histogram_tester;

  // Show the bubble.
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Mock browser being closed.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  browser()->tab_strip_model()->CloseAllTabs();
  destroyed_waiter.Wait();

  // Confirm metrics.
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.Result.FirstShow",
      autofill_metrics::FilledCardInformationBubbleResult::kNotInteracted, 1);
}

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       TooltipAndAccessibleName) {
  ShowBubble();
  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  EXPECT_EQ(5U, GetBubbleViews()->fields_to_buttons_map_.size());
  std::u16string normal_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_BUTTON_TOOLTIP_NORMAL_VIRTUAL_CARD);
  std::u16string clicked_button_tooltip = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_BUBBLE_BUTTON_TOOLTIP_CLICKED_VIRTUAL_CARD);
  for (auto& pair : GetBubbleViews()->fields_to_buttons_map_) {
    EXPECT_EQ(normal_button_tooltip, pair.second->GetTooltipText());
    EXPECT_EQ(pair.second->GetText() + u" " + normal_button_tooltip,
              pair.second->GetViewAccessibility().GetCachedName());
  }

  auto& card_number_button =
      GetBubbleViews()->fields_to_buttons_map_
          [FilledCardInformationBubbleField::kCardNumber];
  auto& cardholder_name_button =
      GetBubbleViews()->fields_to_buttons_map_
          [FilledCardInformationBubbleField::kCardholderName];

  GetBubbleViews()->OnFieldClicked(
      FilledCardInformationBubbleField::kCardNumber);
  EXPECT_EQ(clicked_button_tooltip, card_number_button->GetTooltipText());
  EXPECT_EQ(u"5555 5555 5555 4444 " + clicked_button_tooltip,
            card_number_button->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(normal_button_tooltip, cardholder_name_button->GetTooltipText());
  EXPECT_EQ(u"Lorem Ipsum " + normal_button_tooltip,
            cardholder_name_button->GetViewAccessibility().GetCachedName());

  GetBubbleViews()->OnFieldClicked(
      FilledCardInformationBubbleField::kCardholderName);
  EXPECT_EQ(normal_button_tooltip, card_number_button->GetTooltipText());
  EXPECT_EQ(u"5555 5555 5555 4444 " + normal_button_tooltip,
            card_number_button->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(clicked_button_tooltip, cardholder_name_button->GetTooltipText());
  EXPECT_EQ(u"Lorem Ipsum " + clicked_button_tooltip,
            cardholder_name_button->GetViewAccessibility().GetCachedName());
}

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsInteractiveUiTest,
                       IconViewAccessibleName) {
  EXPECT_EQ(
      GetIconView()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILLED_CARD_INFORMATION_ICON_TOOLTIP_VIRTUAL_CARD));
  EXPECT_EQ(
      GetIconView()->GetTextForTooltipAndAccessibleName(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILLED_CARD_INFORMATION_ICON_TOOLTIP_VIRTUAL_CARD));
}

class FilledCardInformationBubbleViewsPrerenderTest
    : public FilledCardInformationBubbleViewsInteractiveUiTest {
 public:
  FilledCardInformationBubbleViewsPrerenderTest()
      : prerender_helper_(base::BindRepeating(
            &FilledCardInformationBubbleViewsPrerenderTest::web_contents,
            base::Unretained(this))) {}
  ~FilledCardInformationBubbleViewsPrerenderTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(FilledCardInformationBubbleViewsPrerenderTest,
                       KeepBubbleOnPrerenderNavigation) {
  base::HistogramTester histogram_tester;

  // Navigate the primary page with the initial url.
  const GURL& url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Show the bubble and wait until the icon visibility changes.
  {
    ShowBubble();
    ViewVisibilityWaiter(GetIconView(), true).Wait();
  }

  ASSERT_TRUE(GetBubbleViews());
  ASSERT_TRUE(IsIconVisible());

  // Start a prerender.
  prerender_helper_.AddPrerender(
      embedded_test_server()->GetURL("/title1.html"));

  // Ensure the bubble isn't closed by prerender navigation and isn't from the
  // prerendered page.
  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());
  histogram_tester.ExpectBucketCount(
      "Autofill.FilledCardInformationBubble.Shown", false, 1);

  // Activate a prerendered page and wait until the icon visibility changes.
  {
    prerender_helper_.NavigatePrimaryPage(url);
    ViewVisibilityWaiter(GetIconView(), false).Wait();
  }

  // Ensure the bubble hides after prerender Activation.
  EXPECT_FALSE(GetBubbleViews());
  EXPECT_FALSE(IsIconVisible());
}

}  // namespace autofill
