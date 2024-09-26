// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller.h"
#include "chrome/browser/ui/views/controls/obscurable_label_with_toggle_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace content {
class WebContents;
}

namespace autofill {

// This class serves as a base view to any of the bubble views that are part of
// the flow for when the user submits a form with an IBAN (International Bank
// Account Number) value that Autofill has not previously saved.
class SaveIbanBubbleView : public AutofillBubbleBase,
                           public LocationBarBubbleDelegateView,
                           public views::TextfieldController {
 public:
  // Bubble will be anchored to `anchor_view`.
  SaveIbanBubbleView(views::View* anchor_view,
                     content::WebContents* web_contents,
                     IbanBubbleController* controller);

  SaveIbanBubbleView(const SaveIbanBubbleView&) = delete;
  SaveIbanBubbleView& operator=(const SaveIbanBubbleView&) = delete;

  void Show(DisplayReason reason);

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void AddedToWidget() override;
  std::u16string GetWindowTitle() const override;
  void WindowClosing() override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

 protected:
  ~SaveIbanBubbleView() override;

  virtual void CreateMainContentView();

  IbanBubbleController* controller() const { return controller_; }

  // Attributes IDs to the dialog's DialogDelegate-supplied buttons. This is for
  // testing purposes, which is needed when the browser tries to find the view
  // by ID and clicks on it.
  void AssignIdsToDialogButtonsForTesting();

  void OnDialogAccepted();
  void OnDialogCancelled();

  // LocationBarBubbleDelegateView:
  void Init() override;

 private:
  friend class SaveIbanBubbleViewFullFormBrowserTest;

  // Helper function to update value of `nickname_length_label_`;
  void UpdateNicknameLengthLabel();

  raw_ptr<views::Textfield> nickname_textfield_ = nullptr;
  raw_ptr<views::Label> nickname_length_label_ = nullptr;

  // The view that toggles the masking/unmasking of the IBAN value displayed in
  // the bubble.
  raw_ptr<ObscurableLabelWithToggleButton> iban_value_and_toggle_;

  raw_ptr<IbanBubbleController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_SAVE_IBAN_BUBBLE_VIEW_H_
