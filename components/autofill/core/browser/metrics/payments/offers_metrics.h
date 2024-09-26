// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_

#include <memory>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

namespace autofill::autofill_metrics {

// Metrics to track event when the offer notification bubble is closed.
enum class OfferNotificationBubbleResultMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The user explicitly acknowledged the bubble by clicking the ok button.
  OFFER_NOTIFICATION_BUBBLE_ACKNOWLEDGED = 0,
  // The user explicitly closed the prompt with the close button or ESC.
  OFFER_NOTIFICATION_BUBBLE_CLOSED = 1,
  // The user did not interact with the prompt.
  OFFER_NOTIFICATION_BUBBLE_NOT_INTERACTED = 2,
  // The prompt lost focus and was deactivated.
  OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS = 3,
  kMaxValue = OFFER_NOTIFICATION_BUBBLE_LOST_FOCUS,
};

// Metrics to track event when the offer notification infobar is closed.
enum class OfferNotificationInfoBarResultMetric {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // User acknowledged the infobar by clicking the ok button.
  OFFER_NOTIFICATION_INFOBAR_ACKNOWLEDGED = 0,
  // User explicitly closed the infobar with the close button.
  OFFER_NOTIFICATION_INFOBAR_CLOSED = 1,
  // InfoBar was shown but user did not interact with the it.
  OFFER_NOTIFICATION_INFOBAR_IGNORED = 2,
  kMaxValue = OFFER_NOTIFICATION_INFOBAR_IGNORED,
};

// Metrics to track events related to the offers suggestions popup.
enum class OffersSuggestionsPopupEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Default value, should never be used.
  kUndefined = 0,
  // The offer suggestions popup was shown. One event is logged in this metric
  // bucket per time that the overall popup is shown.
  kOffersSuggestionsPopupShown = 1,
  // The offers suggestions popup was shown. Logged once if the user repeatedly
  // displays suggestions for the same field.
  kOffersSuggestionsPopupShownOnce = 2,
  kMaxValue = kOffersSuggestionsPopupShownOnce,
};

// Metrics to track events related to individual offer suggestions in the
// offers suggestions popup.
enum class OffersSuggestionsEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Default value, should never be used.
  kUndefined = 0,
  // An individual offer suggestion was shown. One event is logged in this
  // metric bucket for each individual offer suggestion in the suggestions
  // popup. For instance, if there are three offers suggested, this bucket would
  // log three times.
  kOfferSuggestionShown = 1,
  // An individual offer suggestion was shown. Logged once if the user
  // repeatedly displays suggestions for the same field.
  kOfferSuggestionShownOnce = 2,
  // An individual offer suggestion was selected.
  kOfferSuggestionSelected = 3,
  // An individual offer suggestion was selected. Logged once if the user
  // repeatedly selects suggestions for the same field.
  kOfferSuggestionSelectedOnce = 4,
  // The user selected to see the offer details page in the footer.
  kOfferSuggestionSeeOfferDetailsSelected = 5,
  // The user selected to see the offer details page in the footer. Logged once
  // if the user repeatedly selects to see the offer details page in the footer
  // for the same field.
  kOfferSuggestionSeeOfferDetailsSelectedOnce = 6,
  kMaxValue = kOfferSuggestionSeeOfferDetailsSelectedOnce,
};

void LogOfferNotificationBubbleOfferMetric(
    AutofillOfferData::OfferType offer_type,
    bool is_reshow);

void LogOfferNotificationBubblePromoCodeButtonClicked(
    AutofillOfferData::OfferType offer_type);

void LogOfferNotificationBubbleResultMetric(
    AutofillOfferData::OfferType offer_type,
    OfferNotificationBubbleResultMetric metric,
    bool is_reshow);

void LogOfferNotificationBubbleSuppressed(
    AutofillOfferData::OfferType offer_type);

// Log that the offers suggestions popup was shown. If |first_time_being_logged|
// is true, it represents that it has not been logged yet for the promo code
// offer field that the user is on, so additional logging is needed for the
// histogram that denotes showing the offers suggestions popup once for a field.
void LogOffersSuggestionsPopupShown(bool first_time_being_logged);

// Log the offers suggestions popup |event| for the corresponding |offer_type|.
void LogIndividualOfferSuggestionEvent(OffersSuggestionsEvent event,
                                       AutofillOfferData::OfferType offer_type);

void LogOfferNotificationInfoBarDeepLinkClicked();
void LogOfferNotificationInfoBarResultMetric(
    OfferNotificationInfoBarResultMetric metric);
void LogOfferNotificationInfoBarShown();

// Logs the offer data associated with a profile. This should be called each
// time a Chrome profile is launched.
void LogStoredOfferMetrics(
    const std::vector<std::unique_ptr<AutofillOfferData>>& offers);

// Logs whether the synced autofill offer data is valid.
void LogSyncedOfferDataBeingValid(bool invalid);

// Log the presence of the offer notification icon shows on navigation event
// for |offer_type|.
void LogPageLoadsWithOfferIconShown(AutofillOfferData::OfferType offer_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
