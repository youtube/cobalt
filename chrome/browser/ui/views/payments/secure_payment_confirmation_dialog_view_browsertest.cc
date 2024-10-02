// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_dialog_view.h"
#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"
#include "chrome/browser/ui/views/payments/test_secure_payment_confirmation_payment_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/payments/core/sizes.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/window/dialog_client_view.h"

namespace payments {
namespace {

const SkBitmap CreateInstrumentIcon(SkColor color, int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

const SkBitmap CreateMaxSizeInstrumentIcon(SkColor color) {
  return CreateInstrumentIcon(
      color, kSecurePaymentConfirmationInstrumentIconMaximumWidthPx,
      kSecurePaymentConfirmationInstrumentIconHeightPx);
}

}  // namespace

class SecurePaymentConfirmationDialogViewTest
    : public DialogBrowserTest,
      public SecurePaymentConfirmationDialogView::ObserverForTest {
 public:
  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents = GetActiveWebContents();
    CreateModel();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents->GetPrimaryMainFrame(), model_.GetWeakPtr(),
            GetWeakPtr());

    // TODO(crbug.com/1360854): Ideally, we'd expect the browser window to be
    // active here and could check that |IsBrowserWindowActivate()| returned
    // true, but on wayland, windows cannot be activated as they are on other
    // platforms.
    EXPECT_EQ(browser()->window()->IsActive(),
              test_delegate_->IsBrowserWindowActive());

    test_delegate_->ShowDialog(nullptr);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::WeakPtr<SecurePaymentConfirmationDialogViewTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void CreateModel() {
    model_.set_title(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

    model_.set_merchant_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));
    model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant"));
    model_.set_merchant_origin(
        absl::optional<std::u16string>(u"merchant1.com"));

    model_.set_instrument_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
    model_.set_instrument_value(u"Mastercard ****4444");
    instrument_icon_ =
        std::make_unique<SkBitmap>(CreateMaxSizeInstrumentIcon(SK_ColorBLUE));
    model_.set_instrument_icon(instrument_icon_.get());

    model_.set_total_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));
    model_.set_total_value(u"$20.00 USD");

    model_.set_verify_button_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));

    model_.set_opt_out_visible(false);
    model_.set_opt_out_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LABEL));
    model_.set_opt_out_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_LABEL));
    model_.set_relying_party_id(u"relyingparty.com");
  }

  void InvokeSecurePaymentConfirmationUI() {
    content::WebContents* web_contents = GetActiveWebContents();

    test_delegate_ =
        std::make_unique<TestSecurePaymentConfirmationPaymentRequestDelegate>(
            web_contents->GetPrimaryMainFrame(), model_.GetWeakPtr(),
            GetWeakPtr());

    test_delegate_->ShowDialog(nullptr);

    // The web-modal dialog should be open.
    web_modal::WebContentsModalDialogManager*
        web_contents_modal_dialog_manager =
            web_modal::WebContentsModalDialogManager::FromWebContents(
                web_contents);
    EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

    // By default, disable the input event protector for testing purposes.
    // However, see the AcceptButtonIgnoresAccidentalInputs test, which
    // explicitly checks that the protector is able to block unintended inputs.
    auto mock_input_protector =
        std::make_unique<views::MockInputEventActivationProtector>();
    EXPECT_CALL(*mock_input_protector, IsPossiblyUnintendedInteraction)
        .WillRepeatedly(testing::Return(false));
    test_delegate_->dialog_view()
        ->GetDialogClientView()
        ->SetInputProtectorForTesting(std::move(mock_input_protector));
  }

  void ExpectLabelText(
      const std::u16string& text,
      SecurePaymentConfirmationDialogView::DialogViewID view_id) {
    EXPECT_EQ(text, static_cast<views::Label*>(
                        test_delegate_->dialog_view()->GetViewByID(
                            static_cast<int>(view_id)))
                        ->GetText());
  }

  void ExpectOptOutText(views::View* view,
                        const std::u16string& relying_party_id,
                        const std::u16string& opt_out_link_label) {
    // To avoid overfitting, we check only that the opt-out label contains both
    // the relying party and the call-to-action text that is expected.
    std::string opt_out_text =
        base::UTF16ToUTF8(static_cast<views::StyledLabel*>(view)->GetText());
    EXPECT_THAT(opt_out_text,
                ::testing::HasSubstr(base::UTF16ToUTF8(relying_party_id)));
    EXPECT_THAT(opt_out_text,
                ::testing::HasSubstr(base::UTF16ToUTF8(opt_out_link_label)));
  }

  // Verify that the data displayed on the view matches what is expected given
  // the current `model_` state.
  void ExpectViewMatchesModel() {
    ASSERT_NE(test_delegate_->dialog_view(), nullptr);

    EXPECT_EQ(model_.verify_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_OK));

    EXPECT_EQ(model_.cancel_button_label(),
              test_delegate_->dialog_view()->GetDialogButtonLabel(
                  ui::DIALOG_BUTTON_CANCEL));

    EXPECT_TRUE(test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
        SecurePaymentConfirmationDialogView::DialogViewID::HEADER_ICON)));

    EXPECT_EQ(
        model_.progress_bar_visible(),
        test_delegate_->dialog_view()
            ->GetViewByID(static_cast<int>(SecurePaymentConfirmationDialogView::
                                               DialogViewID::PROGRESS_BAR))
            ->GetVisible());

    ExpectLabelText(model_.title(),
                    SecurePaymentConfirmationDialogView::DialogViewID::TITLE);

    ExpectLabelText(
        model_.merchant_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_LABEL);
    ExpectLabelText(
        FormatMerchantLabel(model_.merchant_name(), model_.merchant_origin()),
        SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

    ExpectLabelText(
        model_.instrument_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_LABEL);
    ExpectLabelText(
        model_.instrument_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_VALUE);

    ASSERT_EQ(instrument_icon_.get(), model_.instrument_icon());
    const SkBitmap* expected_icon =
        instrument_icon_->drawsNothing()
            ? gfx::CreateVectorIcon(
                  kCreditCardIcon,
                  kSecurePaymentConfirmationInstrumentIconDefaultWidthPx,
                  test_delegate_->dialog_view()->GetColorProvider()->GetColor(
                      ui::kColorDialogForeground))
                  .bitmap()
            : model_.instrument_icon();
    EXPECT_TRUE(cc::MatchesBitmap(
        *expected_icon,
        *(static_cast<views::ImageView*>(
              test_delegate_->dialog_view()->GetViewByID(
                  static_cast<int>(SecurePaymentConfirmationDialogView::
                                       DialogViewID::INSTRUMENT_ICON)))
              ->GetImage()
              .bitmap()),
        cc::ExactPixelComparator()));

    ExpectLabelText(
        model_.total_label(),
        SecurePaymentConfirmationDialogView::DialogViewID::TOTAL_LABEL);
    ExpectLabelText(
        model_.total_value(),
        SecurePaymentConfirmationDialogView::DialogViewID::TOTAL_VALUE);

    // The Opt Out link always exists, but should only be visible if requested.
    views::View* opt_out_view =
        test_delegate_->dialog_view()->GetFootnoteViewForTesting();
    EXPECT_NE(opt_out_view, nullptr);
    EXPECT_EQ(opt_out_view->GetVisible(), model_.opt_out_visible());
    ExpectOptOutText(opt_out_view, model_.relying_party_id(),
                     model_.opt_out_link_label());
  }

  void ClickButton(views::View* button) {
    gfx::Point center(button->width() / 2, button->height() / 2);
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, center, center,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
    button->OnMousePressed(event);
    button->OnMouseReleased(event);
  }

  // SecurePaymentConfirmationDialogView::ObserverForTest:
  void OnDialogClosed() override { dialog_closed_ = true; }
  void OnConfirmButtonPressed() override { confirm_pressed_ = true; }
  void OnCancelButtonPressed() override { cancel_pressed_ = true; }
  void OnOptOutClicked() override { opt_out_clicked_ = true; }

 protected:
  SecurePaymentConfirmationModel model_;
  std::unique_ptr<TestSecurePaymentConfirmationPaymentRequestDelegate>
      test_delegate_;

  std::unique_ptr<SkBitmap> instrument_icon_;

  bool dialog_closed_ = false;
  bool confirm_pressed_ = false;
  bool cancel_pressed_ = false;
  bool opt_out_clicked_ = false;

  base::HistogramTester histogram_tester_;

  base::WeakPtrFactory<SecurePaymentConfirmationDialogViewTest>
      weak_ptr_factory_{this};
};

// Basic test that the view matches the model state.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ViewMatchesModel) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();
}

// Test that clicking the 'Accept' button triggers the expected path and closes
// the dialog.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       AcceptButtonTest) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();

  ClickButton(test_delegate_->dialog_view()->GetOkButton());

  EXPECT_TRUE(confirm_pressed_);
  EXPECT_FALSE(cancel_pressed_);
  EXPECT_FALSE(opt_out_clicked_);

  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      SecurePaymentConfirmationAuthenticationDialogResult::kAccepted, 1);
}

// Test that the 'Accept' button is protected against accidental inputs.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       AcceptButtonIgnoresAccidentalInputs) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();

  // Insert a mock input protector that will ignore the first input and then
  // accepts all subsequent inputs.
  auto mock_input_protector =
      std::make_unique<views::MockInputEventActivationProtector>();
  EXPECT_CALL(*mock_input_protector, IsPossiblyUnintendedInteraction)
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));
  test_delegate_->dialog_view()
      ->GetDialogClientView()
      ->SetInputProtectorForTesting(std::move(mock_input_protector));

  // Because of the input protector, the first press of the button should be
  // ignored.
  ClickButton(test_delegate_->dialog_view()->GetOkButton());
  EXPECT_FALSE(confirm_pressed_);
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      0);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      SecurePaymentConfirmationAuthenticationDialogResult::kAccepted, 0);

  // However a subsequent press should be accepted.
  ClickButton(test_delegate_->dialog_view()->GetOkButton());
  EXPECT_TRUE(confirm_pressed_);
  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      SecurePaymentConfirmationAuthenticationDialogResult::kAccepted, 1);
}

// Test that clicking the 'Cancel' button triggers the expected path and closes
// the dialog.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       CancelButtonTest) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();

  ClickButton(test_delegate_->dialog_view()->GetCancelButton());

  EXPECT_TRUE(cancel_pressed_);
  EXPECT_FALSE(confirm_pressed_);
  EXPECT_FALSE(opt_out_clicked_);

  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      SecurePaymentConfirmationAuthenticationDialogResult::kCanceled, 1);
}

// Test that the progress bar is visible in the view when requested by the
// model.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ProgressBarVisible) {
  CreateModel();
  model_.set_progress_bar_visible(true);
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();
}

// Test that the view can be updated to show the progress bar after initial
// load.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       ShowProgressBar) {
  // Initially the model should be created with the progress bar hidden.
  CreateModel();
  ASSERT_FALSE(model_.progress_bar_visible());

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  // Then update the model to have the bar be visible and check that the view
  // updates to match.
  model_.set_progress_bar_visible(true);
  ASSERT_TRUE(model_.progress_bar_visible());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
}

// Check that the view updates to match model updates.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OnModelUpdated) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  model_.set_title(u"Test Title");
  model_.set_merchant_label(u"Test merchant");
  model_.set_merchant_name(
      absl::optional<std::u16string>(u"Test merchant value"));
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant2.com"));
  model_.set_instrument_label(u"Test instrument");
  model_.set_instrument_value(u"Test instrument value");
  model_.set_total_label(u"Test total");
  model_.set_total_value(u"Test total value");
  model_.set_verify_button_label(u"Test verify");
  model_.set_cancel_button_label(u"Test cancel");

  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
}

// Test the two reasons an instrument icon is updated: The model's bitmap
// pointer changed, or the bitmap itself changed.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       InstrumentIconUpdated) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  // Change the bitmap pointer
  instrument_icon_ =
      std::make_unique<SkBitmap>(CreateMaxSizeInstrumentIcon(SK_ColorGREEN));
  model_.set_instrument_icon(instrument_icon_.get());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();

  // Change the bitmap itself without touching the model's pointer
  *instrument_icon_ = CreateMaxSizeInstrumentIcon(SK_ColorRED);
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
}

// Test that the web contents can be torn down whilst the dialog is visible, and
// that doing so should close the dialog.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       WebContentsClosed) {
  CreateModel();
  InvokeSecurePaymentConfirmationUI();

  GetActiveWebContents()->Close();

  EXPECT_TRUE(dialog_closed_);
  EXPECT_FALSE(confirm_pressed_);
  EXPECT_FALSE(cancel_pressed_);
  EXPECT_FALSE(opt_out_clicked_);
}

// TestBrowserUi-provided UI test.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Test that the visible instrument icon is set correctly to the default icon if
// the model does not provide one.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       DefaultInstrumentIcon) {
  CreateModel();

  instrument_icon_ = std::make_unique<SkBitmap>();
  ASSERT_TRUE(instrument_icon_->drawsNothing());
  model_.set_instrument_icon(instrument_icon_.get());

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();
}

// Test that the merchant label is formatted correctly based on the input
// name/origin provided in the model.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       MerchantLabelFormat) {
  CreateModel();

  // Both merchant name and origin
  model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant"));
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant.com"));
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"Test Merchant (merchant.com)",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

  // Only merchant name, no origin
  model_.set_merchant_name(absl::optional<std::u16string>(u"Test Merchant 2"));
  model_.set_merchant_origin(absl::optional<std::u16string>());
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"Test Merchant 2",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);

  // Only merchant origin, no name
  model_.set_merchant_name(absl::optional<std::u16string>());
  model_.set_merchant_origin(absl::optional<std::u16string>(u"merchant2.com"));
  test_delegate_->dialog_view()->OnModelUpdated();
  ExpectViewMatchesModel();
  ExpectLabelText(
      u"merchant2.com",
      SecurePaymentConfirmationDialogView::DialogViewID::MERCHANT_VALUE);
}

// Test that an oversized instrument icon is resized down to the maximum size.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OversizedInstrumentIcon) {
  CreateModel();

  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, kSecurePaymentConfirmationInstrumentIconMaximumWidthPx * 2,
      kSecurePaymentConfirmationInstrumentIconHeightPx * 2);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconMaximumWidthPx,
            image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());
}

// Test that an undersized instrument icon is resized up to the minimum size.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       UndersizedInstrumentIcon) {
  CreateModel();

  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, kSecurePaymentConfirmationInstrumentIconDefaultWidthPx / 2,
      kSecurePaymentConfirmationInstrumentIconHeightPx / 2);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconDefaultWidthPx,
            image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());
}

// Test that a midsized instrument icon is not resized.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       MidsizedInstrumentIcon) {
  CreateModel();

  int width = (kSecurePaymentConfirmationInstrumentIconDefaultWidthPx +
               kSecurePaymentConfirmationInstrumentIconMaximumWidthPx) /
              2;
  *instrument_icon_ = CreateInstrumentIcon(
      SK_ColorRED, width, kSecurePaymentConfirmationInstrumentIconHeightPx);

  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  views::ImageView* image_view = static_cast<views::ImageView*>(
      test_delegate_->dialog_view()->GetViewByID(static_cast<int>(
          SecurePaymentConfirmationDialogView::DialogViewID::INSTRUMENT_ICON)));
  EXPECT_EQ(width, image_view->GetImageBounds().width());
  EXPECT_EQ(kSecurePaymentConfirmationInstrumentIconHeightPx,
            image_view->GetImageBounds().height());
}

// Test that the opt-out link is only shown when explicitly requested, and that
// clicking it causes the expected path and closes the dialog.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDialogViewTest,
                       OptOutShownWhenRequested) {
  CreateModel();

  // Make sure that by default, opt-out wasn't requested. This means that every
  // other test is correctly testing the 'no opt out' path.
  ASSERT_FALSE(model_.opt_out_visible());

  model_.set_opt_out_visible(true);
  InvokeSecurePaymentConfirmationUI();
  ExpectViewMatchesModel();

  // Now click the opt-out link, and verify that the dialog is closed via the
  // correct path.
  views::StyledLabel* opt_out_label = static_cast<views::StyledLabel*>(
      test_delegate_->dialog_view()->GetFootnoteViewForTesting());
  opt_out_label->ClickFirstLinkForTesting();

  EXPECT_FALSE(cancel_pressed_);
  EXPECT_FALSE(confirm_pressed_);
  EXPECT_TRUE(opt_out_clicked_);

  histogram_tester_.ExpectTotalCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      1);
  histogram_tester_.ExpectBucketCount(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "AuthenticationDialogResult",
      SecurePaymentConfirmationAuthenticationDialogResult::kOptOut, 1);
}

}  // namespace payments
