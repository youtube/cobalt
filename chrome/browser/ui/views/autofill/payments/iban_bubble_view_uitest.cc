// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/manage_saved_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_iban_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {
const char kIbanForm[] = "/autofill_iban_form.html";
constexpr char kIbanValue[] = "DE91 1000 0000 0123 4567 89";
constexpr char kIbanValueWithoutWhitespaces[] = "DE91100000000123456789";
}  // namespace

namespace autofill {

class IbanBubbleViewFullFormBrowserTest
    : public SyncTest,
      public IBANSaveManager::ObserverForTest,
      public IbanBubbleControllerImpl::ObserverForTest {
 protected:
  IbanBubbleViewFullFormBrowserTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::kAutofillFillIbanFields,
                              autofill::features::kAutofillParseIBANFields},
        /*disabled_features=*/{});
  }

 public:
  IbanBubbleViewFullFormBrowserTest(const IbanBubbleViewFullFormBrowserTest&) =
      delete;
  IbanBubbleViewFullFormBrowserTest& operator=(
      const IbanBubbleViewFullFormBrowserTest&) = delete;
  ~IbanBubbleViewFullFormBrowserTest() override = default;

 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    TestAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}

    testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  // Various events that can be waited on by the DialogEventWaiter.
  enum DialogEvent : int {
    OFFERED_LOCAL_SAVE,
    ACCEPT_SAVE_IBAN_COMPLETE,
    DECLINE_SAVE_IBAN_COMPLETE,
    BUBBLE_SHOWN,
    ICON_SHOWN
  };

  // SyncTest::SetUpOnMainThread:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    // Set up the HTTPS server (uses the embedded_test_server).
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "components/test/data/autofill");
    embedded_test_server()->StartAcceptingConnections();

    ASSERT_TRUE(SetupClients());

    // It's important to use the blank tab here and not some arbitrary page.
    // This causes the RenderFrameHost to stay the same when navigating to the
    // HTML pages in tests. Since ContentAutofillDriver is per RFH, the driver
    // that this method starts observing will also be the one to notify later.
    AddBlankTabAndShow(GetBrowser(0));

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(GetProfile(0));

    // Set up this class as the ObserverForTest implementation.
    iban_save_manager_ = autofill_manager()
                             ->client()
                             ->GetFormDataImporter()
                             ->iban_save_manager_for_testing();
    iban_save_manager_->SetEventObserverForTesting(this);
    AddEventObserverToController();
  }

  void TearDownOnMainThread() override {
    // Explicitly set to null to avoid that it becomes dangling.
    iban_save_manager_ = nullptr;

    SyncTest::TearDownOnMainThread();
  }

  // The primary main frame's AutofillManager.
  TestAutofillManager* autofill_manager() {
    return autofill_manager_injector_[GetActiveWebContents()];
  }

  // IBANSaveManager::ObserverForTest:
  void OnOfferLocalSave() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::OFFERED_LOCAL_SAVE);
    }
  }

  // IBANSaveManager::ObserverForTest:
  void OnAcceptSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE);
    }
  }

  // IBANSaveManager::ObserverForTest:
  void OnDeclineSaveIbanComplete() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::DECLINE_SAVE_IBAN_COMPLETE);
    }
  }

  // IbanBubbleControllerImpl::ObserverForTest:
  void OnBubbleShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::BUBBLE_SHOWN);
    }
  }

  void OnIconShown() override {
    if (event_waiter_) {
      event_waiter_->OnEvent(DialogEvent::ICON_SHOWN);
    }
  }

  void NavigateToAndWaitForForm(const std::string& file_path) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        GetBrowser(0), embedded_test_server()->GetURL(file_path)));
    ASSERT_TRUE(autofill_manager()->WaitForFormsSeen(1));
  }

  void SubmitFormAndWaitForIbanLocalSaveBubble() {
    ResetEventWaiterForSequence(
        {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::BUBBLE_SHOWN});
    SubmitForm();
    ASSERT_TRUE(WaitForObservedEvent());
    EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                    ->GetVisible());
  }

  void SubmitForm() {
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_submit_button_js =
        "(function() { document.getElementById('submit').click(); })();";
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_submit_button_js));
    nav_observer.Wait();
  }

  // Should be called for autofill_iban_form.html.
  void FillForm(absl::optional<std::string> iban_value = absl::nullopt) {
    NavigateToAndWaitForForm(kIbanForm);
    content::WebContents* web_contents = GetActiveWebContents();
    const std::string click_fill_button_js =
        "(function() { document.getElementById('fill_form').click(); })();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, click_fill_button_js));

    if (iban_value.has_value()) {
      std::string set_value_js =
          "(function() { document.getElementById('iban').value ='" +
          iban_value.value() + "';})();";
      ASSERT_TRUE(content::ExecuteScript(web_contents, set_value_js));
    }
  }

  views::View* FindViewInBubbleById(DialogViewId view_id) {
    LocationBarBubbleDelegateView* iban_bubble_view =
        GetIbanBubbleDelegateView();
    views::View* specified_view =
        iban_bubble_view->GetViewByID(static_cast<int>(view_id));
    if (!specified_view) {
      // Many of the save IBAN bubble's inner views are not child views but
      // rather contained by the dialog. If we didn't find what we were
      // looking for, check there as well.
      specified_view =
          iban_bubble_view->GetWidget()->GetRootView()->GetViewByID(
              static_cast<int>(view_id));
    }
    return specified_view;
  }

  void ClickOnSaveButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        FindViewInBubbleById(DialogViewId::OK_BUTTON));
  }

  void ClickOnCancelButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        FindViewInBubbleById(DialogViewId::CANCEL_BUTTON));
  }

  void ClickOnCloseButton() {
    SaveIbanBubbleView* save_iban_bubble_views = GetSaveIbanBubbleView();
    CHECK(save_iban_bubble_views);
    ClickOnDialogViewAndWaitForWidgetDestruction(
        save_iban_bubble_views->GetBubbleFrameView()
            ->GetCloseButtonForTesting());
    CHECK(!GetSaveIbanBubbleView());
  }

  void ClickOnIbanValueToggleButton() {
    ClickOnDialogView(
        FindViewInBubbleById(DialogViewId::TOGGLE_IBAN_VALUE_MASKING_BUTTON));
  }

  std::u16string GetDisplayedIbanValue() {
    AutofillBubbleBase* iban_bubble_views = GetIbanBubbleView();
    CHECK(iban_bubble_views);
    views::Label* iban_value = static_cast<views::Label*>(
        FindViewInBubbleById(DialogViewId::IBAN_VALUE_LABEL));
    CHECK(iban_value);
    std::u16string iban_value_label = iban_value->GetText();
    // To simplify the expectations in tests, replaces the returned IBAN value
    // ellipsis ('\u2006') with a whitespace and oneDot ('\u2022') with '*'.
    base::ReplaceChars(iban_value_label, u"\u2022", u"*", &iban_value_label);
    base::ReplaceChars(iban_value_label, u"\u2006", u" ", &iban_value_label);
    return iban_value_label;
  }

  SaveIbanBubbleView* GetSaveIbanBubbleView() {
    AutofillBubbleBase* iban_bubble_view = GetIbanBubbleView();
    if (!iban_bubble_view) {
      return nullptr;
    }
    return static_cast<SaveIbanBubbleView*>(iban_bubble_view);
  }

  ManageSavedIbanBubbleView* GetManageSavedIbanBubbleView() {
    AutofillBubbleBase* iban_bubble_view = GetIbanBubbleView();
    if (!iban_bubble_view) {
      return nullptr;
    }
    return static_cast<ManageSavedIbanBubbleView*>(iban_bubble_view);
  }

  IbanBubbleType GetBubbleType() {
    IbanBubbleController* iban_bubble_controller =
        IbanBubbleController::GetOrCreate(GetActiveWebContents());
    if (!iban_bubble_controller) {
      return IbanBubbleType::kInactive;
    }
    return iban_bubble_controller->GetBubbleType();
  }

  SavePaymentIconView* GetSaveIbanIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(GetBrowser(0));
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kSaveIban);
    CHECK(browser_view->GetLocationBarView()->Contains(icon));
    return static_cast<SavePaymentIconView*>(icon);
  }

  content::WebContents* GetActiveWebContents() {
    return GetBrowser(0)->tab_strip_model()->GetActiveWebContents();
  }

  void AddEventObserverToController() {
    IbanBubbleControllerImpl* save_iban_bubble_controller_impl =
        static_cast<IbanBubbleControllerImpl*>(
            IbanBubbleController::GetOrCreate(GetActiveWebContents()));
    CHECK(save_iban_bubble_controller_impl);
    save_iban_bubble_controller_impl->SetEventObserverForTesting(this);
  }

  void ResetEventWaiterForSequence(std::list<DialogEvent> event_sequence) {
    event_waiter_ =
        std::make_unique<EventWaiter<DialogEvent>>(std::move(event_sequence));
  }

  void ClickOnView(views::View* view) {
    CHECK(view);
    ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnDialogView(views::View* view) {
    GetIbanBubbleDelegateView()->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(GetIbanBubbleDelegateView()
                                                 ->GetWidget()
                                                 ->non_client_view()
                                                 ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
  }

  void ClickOnDialogViewAndWaitForWidgetDestruction(views::View* view) {
    EXPECT_TRUE(GetIbanBubbleView());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetSaveIbanBubbleView()->GetWidget());
    ClickOnDialogView(view);
    destroyed_waiter.Wait();
    EXPECT_FALSE(GetIbanBubbleView());
  }

  views::Textfield* nickname_input() {
    return static_cast<views::Textfield*>(
        FindViewInBubbleById(DialogViewId::NICKNAME_TEXTFIELD));
  }

  [[nodiscard]] testing::AssertionResult WaitForObservedEvent() {
    return event_waiter_->Wait();
  }

  raw_ptr<IBANSaveManager> iban_save_manager_ = nullptr;

 private:
  LocationBarBubbleDelegateView* GetIbanBubbleDelegateView() {
    LocationBarBubbleDelegateView* iban_bubble_view = nullptr;
    switch (GetBubbleType()) {
      case IbanBubbleType::kLocalSave: {
        iban_bubble_view = GetSaveIbanBubbleView();
        CHECK(iban_bubble_view);
        break;
      }
      case IbanBubbleType::kManageSavedIban: {
        iban_bubble_view = GetManageSavedIbanBubbleView();
        CHECK(iban_bubble_view);
        break;
      }
      case IbanBubbleType::kInactive:
        NOTREACHED_NORETURN();
    }
    return iban_bubble_view;
  }

  AutofillBubbleBase* GetIbanBubbleView() {
    IbanBubbleController* iban_bubble_controller =
        IbanBubbleController::GetOrCreate(GetActiveWebContents());
    if (!iban_bubble_controller) {
      return nullptr;
    }
    return iban_bubble_controller->GetPaymentBubbleView();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<autofill::EventWaiter<DialogEvent>> event_waiter_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
};

// Tests the local save bubble. Ensures that clicking the 'No thanks' button
// successfully causes the bubble to go away, and causes a strike to be added.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingNoThanksClosesBubble) {
  base::HistogramTester histogram_tester;
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  // Clicking 'No thanks' should cancel and close it.
  ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  EXPECT_EQ(
      1, iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()->GetStrikes(
             IBANSaveManager::GetPartialIbanHashString(
                 kIbanValueWithoutWhitespaces)));
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kCancelled, 1);
}

// Tests overall StrikeDatabase interaction with the local save bubble. Runs an
// example of declining the prompt max times and ensuring that the
// offer-to-save bubble does not appear on the next try. Then, ensures that no
// strikes are added if the IBAN already has max strikes.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       StrikeDatabase_Local_FullFlowTest) {
  base::HistogramTester histogram_tester;
  // Show and ignore the bubble enough times in order to accrue maximum strikes.
  for (int i = 0; i < iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
                          ->GetMaxStrikesLimit();
       ++i) {
    FillForm(kIbanValue);
    SubmitFormAndWaitForIbanLocalSaveBubble();

    ResetEventWaiterForSequence({DialogEvent::DECLINE_SAVE_IBAN_COMPLETE});
    ClickOnCancelButton();
    ASSERT_TRUE(WaitForObservedEvent());
  }
  EXPECT_EQ(
      iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()->GetStrikes(
          IBANSaveManager::GetPartialIbanHashString(
              kIbanValueWithoutWhitespaces)),
      iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
          ->GetMaxStrikesLimit());
  // Submit the form a fourth time. Since the IBAN now has maximum strikes,
  // the bubble should not be shown.
  FillForm(kIbanValue);
  ResetEventWaiterForSequence(
      {DialogEvent::OFFERED_LOCAL_SAVE, DialogEvent::ICON_SHOWN});
  SubmitForm();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_TRUE(
      iban_save_manager_->GetIBANSaveStrikeDatabaseForTesting()
          ->ShouldBlockFeature(IBANSaveManager::GetPartialIbanHashString(
              kIbanValueWithoutWhitespaces)));

  EXPECT_TRUE(GetSaveIbanIconView()->GetVisible());
  EXPECT_FALSE(GetSaveIbanBubbleView());

  // Click the icon to show the bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());
  EXPECT_TRUE(FindViewInBubbleById(DialogViewId::MAIN_CONTENT_VIEW_LOCAL)
                  ->GetVisible());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.Reshows",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);

  ClickOnCancelButton();
  ASSERT_TRUE(WaitForObservedEvent());
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kNotShownMaxStrikesReached, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 3);
}

// Tests the local save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", false, 1);
}

// Tests the local save bubble. Ensures that clicking the 'Save' button
// successfully causes the bubble to go away.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingSaveClosesBubble_WithNickname) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();
  nickname_input()->SetText(u"My doctor's IBAN");

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kAccepted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname", true, 1);
}

// Tests the local save bubble. Ensures that clicking the [X] button will still
// see the omnibox icon.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingClosesBubbleStillShowOmnibox) {
  base::HistogramTester histogram_tester;
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ClickOnCloseButton();
  EXPECT_TRUE(GetSaveIbanIconView()->GetVisible());
  EXPECT_FALSE(GetSaveIbanBubbleView());
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptOffer.Local.FirstShow",
      autofill_metrics::SaveIbanPromptOffer::kShown, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SaveIbanPromptResult.Local.FirstShow",
      autofill_metrics::SaveIbanBubbleResult::kClosed, 1);
}

// Tests the local save bubble. Ensures that clicking the eye icon button
// successfully causes the IBAN value to be hidden or shown.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingHideOrShowIbanValueEyeIcon) {
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 1000 0000 0123 4567 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 **** **** **** **67 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 1000 0000 0123 4567 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 **** **** **** **67 89");
}

// Tests the local save bubble. Ensures that clicking the omnibox icon opens
// manage saved IBAN bubble with IBAN nickname.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_SavedIbanHasNickname) {
  const std::u16string kNickname = u"My doctor's IBAN";
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();
  nickname_input()->SetText(kNickname);

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Open up manage IBANs bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  const views::Label* nickname_label = static_cast<views::Label*>(
      FindViewInBubbleById(DialogViewId::NICKNAME_LABEL));
  EXPECT_TRUE(nickname_label);
  EXPECT_EQ(nickname_label->GetText(), kNickname);
  // Verify the bubble type is manage saved IBAN.
  ASSERT_EQ(GetBubbleType(), IbanBubbleType::kManageSavedIban);
}

// Tests the local save bubble. Ensures that clicking the omnibox icon opens
// manage saved IBAN bubble without IBAN nickname.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_SavedIbanNoNickname) {
  FillForm();
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Open up manage IBANs bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  EXPECT_FALSE(FindViewInBubbleById(DialogViewId::NICKNAME_LABEL));
  // Verify the bubble type is manage saved IBAN.
  ASSERT_EQ(GetBubbleType(), IbanBubbleType::kManageSavedIban);
}

// Tests the manage saved bubble. Ensures that clicking the eye icon button
// successfully causes the IBAN value to be masked or unmasked.
IN_PROC_BROWSER_TEST_F(IbanBubbleViewFullFormBrowserTest,
                       Local_ClickingHideOrShowIbanValueManageView) {
  FillForm(kIbanValue);
  SubmitFormAndWaitForIbanLocalSaveBubble();

  ResetEventWaiterForSequence({DialogEvent::ACCEPT_SAVE_IBAN_COMPLETE});
  ClickOnSaveButton();
  ASSERT_TRUE(WaitForObservedEvent());

  // Open up manage IBANs bubble.
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  ClickOnView(GetSaveIbanIconView());
  ASSERT_TRUE(WaitForObservedEvent());

  // Verify the bubble type is manage saved IBAN.
  ASSERT_EQ(GetBubbleType(), IbanBubbleType::kManageSavedIban);
  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 1000 0000 0123 4567 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 **** **** **** **67 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 1000 0000 0123 4567 89");

  ClickOnIbanValueToggleButton();
  EXPECT_EQ(GetDisplayedIbanValue(), u"DE91 **** **** **** **67 89");
}

}  // namespace autofill
