// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

class GURL;

namespace views {
class Label;
class Throbber;
class Widget;
}  // namespace views

namespace autofill {

// Gets the user avatar icon if available, or else a placeholder.
ui::ImageModel GetProfileAvatar(const AccountInfo& account_info);

// Defines a title view with an icon, a separator, and a label, to be used
// by dialogs that need to present the Google or Google Pay logo with a
// separator and custom horizontal padding.
class TitleWithIconAndSeparatorView : public views::TableLayoutView {
 public:
  METADATA_HEADER(TitleWithIconAndSeparatorView);

  // TODO(crbug.com/1433075): This enum is also used by
  // TitleWithIconAfterLabelView, and should be refactored to be outside of
  // TitleWithIconAndSeparatorView.
  enum class Icon {
    // Google Pay icon. The "Pay" portion is recolored for light/dark mode.
    GOOGLE_PAY,
    // Google super G.
    GOOGLE_G,
  };

  TitleWithIconAndSeparatorView(const std::u16string& window_title,
                                Icon icon_to_show);
  ~TitleWithIconAndSeparatorView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Defines a title view with a label and an icon, to be used by dialogs
// that need to present the Google or Google Pay logo and custom
// horizontal padding.
//
// Unlike TitleWithIconAndSeparatorView, this view has no separator and places
// the icon after the title rather than before.
class TitleWithIconAfterLabelView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(TitleWithIconAfterLabelView);

  TitleWithIconAfterLabelView(const std::u16string& window_title,
                              TitleWithIconAndSeparatorView::Icon icon_to_show);
  ~TitleWithIconAfterLabelView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// An intermediary method to switch between TitleWithIconAndSeparatorView and
// TitleWithIconAfterLabelView protected by
// kAutofillEnableMovingGPayLogoToTheRightOnDesktop.
// TODO(crbug.com/1459361): Remove this method once
// kAutofillEnableMovingGPayLogoToTheRightOnDesktop is fully launched.
std::unique_ptr<views::View> CreateTitleView(
    const std::u16string& window_title,
    TitleWithIconAndSeparatorView::Icon icon_to_show);

// Defines a view with legal message. This class handles the legal message
// parsing and the links clicking events.
class LegalMessageView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(LegalMessageView);

  using LinkClickedCallback = base::RepeatingCallback<void(const GURL&)>;

  // Along with the legal message lines and link callbacks, we are sending the
  // user email and avatar as optional params. These will be displayed at the
  // bottom line of this view if they have value.
  LegalMessageView(const LegalMessageLines& legal_message_lines,
                   absl::optional<std::u16string> optional_user_email,
                   absl::optional<ui::ImageModel> optional_user_avatar,
                   LinkClickedCallback callback);
  ~LegalMessageView() override;
};

PaymentsBubbleClosedReason GetPaymentsBubbleClosedReasonFromWidget(
    const views::Widget* widget);

// TODO(crbug.com/1249665): Replace all payments' progress bar usages with this.
// Creates a progress bar with an explanatory text below.
class ProgressBarWithTextView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(ProgressBarWithTextView);

  explicit ProgressBarWithTextView(const std::u16string& progress_bar_text);
  ~ProgressBarWithTextView() override;

 private:
  // views::View:
  void OnThemeChanged() override;
  void AddedToWidget() override;

  raw_ptr<views::Label> progress_label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
