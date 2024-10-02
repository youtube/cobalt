// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_DIALOG_VIEW_IDS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_DIALOG_VIEW_IDS_H_

#include "components/autofill/core/browser/field_types.h"

// This defines an enumeration of IDs that can uniquely identify a view within
// the scope of various credit-card-related Autofill bubbles and dialogs.

namespace autofill {

enum DialogViewId : int {
  VIEW_ID_NONE = 0,

  // The following views are contained in SaveCardBubbleViews.
  MAIN_CONTENT_VIEW_LOCAL,   // The main content view for a local
                             // save bubble
  MAIN_CONTENT_VIEW_UPLOAD,  // The main content view for an upload
                             // save bubble
  FOOTNOTE_VIEW,             // The footnote view of either an upload
                             // save bubble or a manage cards view.
  SIGN_IN_PROMO_VIEW,        // Contains the sign-in promo view
  MANAGE_CARDS_VIEW,         // The manage cards view
  MANAGE_IBANS_VIEW,         // The manage IBANs view
  EXPIRATION_DATE_VIEW,      // Contains the dropdowns for expiration date
  USER_INFORMATION_VIEW,     // User avatar/display picture and email address.

  // The sub-view that contains the sign-in button in the promo.
  SIGN_IN_VIEW,

  // The main content view for a migration offer bubble.
  MAIN_CONTENT_VIEW_MIGRATION_BUBBLE,

  // The main content view for the main migration dialog.
  MAIN_CONTENT_VIEW_MIGRATION_OFFER_DIALOG,

  // The following are views::LabelButton objects (clickable).
  OK_BUTTON,            // Can say [Save], [Next], [Confirm],
                        // or [Done] depending on context
  CANCEL_BUTTON,        // Typically says [No thanks]
  CLOSE_BUTTON,         // Typically says [Close]
  MANAGE_CARDS_BUTTON,  // Typically says [Manage cards]
  MANAGE_IBANS_BUTTON,  // Typically says [Manage payments]

  TOGGLE_IBAN_VALUE_MASKING_BUTTON,  // Used to mask/unmask IBAN value.

  // The following are views::Link objects (clickable).
  LEARN_MORE_LINK,

  // The following are views::Textfield objects.
  CARDHOLDER_NAME_TEXTFIELD,  // Used for cardholder name entry/confirmation
  NICKNAME_TEXTFIELD,         // Used for IBAN nickname entry/confirmation

  // The following are views::TooltipIcon objects.
  CARDHOLDER_NAME_TOOLTIP,  // Appears during cardholder name entry/confirmation
  UPLOAD_EXPLANATION_TOOLTIP,  // Appears for implicitly-syncing upload saves

  // The following are views::Combobox objects.
  EXPIRATION_DATE_DROPBOX_MONTH,
  EXPIRATION_DATE_DROPBOX_YEAR,

  // The following are views::Label objects.
  EXPIRATION_DATE_LABEL,  // Appears during save offer bubble
  IBAN_VALUE_LABEL,       // Shows or hides during IBAN offer bubble
  NICKNAME_LABEL,         // Appears during manage saved IBAN bubble.
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_DIALOG_VIEW_IDS_H_
