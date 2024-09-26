// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"

#include <unordered_map>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

namespace autofill::autofill_metrics {

void LogOfferNotificationBubbleOfferMetric(
    AutofillOfferData::OfferType offer_type,
    bool is_reshow) {
  std::string histogram_name = "Autofill.OfferNotificationBubbleOffer.";
  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      histogram_name += "CardLinkedOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += "GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, is_reshow);
}

void LogOfferNotificationBubblePromoCodeButtonClicked(
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name =
      "Autofill.OfferNotificationBubblePromoCodeButtonClicked.";
  // Switch to different sub-histogram depending on offer type being displayed.
  // Card-linked offers do not have a promo code button.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += "GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, true);
}

void LogOfferNotificationBubbleResultMetric(
    AutofillOfferData::OfferType offer_type,
    OfferNotificationBubbleResultMetric metric,
    bool is_reshow) {
  DCHECK_LE(metric, OfferNotificationBubbleResultMetric::kMaxValue);
  std::string histogram_name = "Autofill.OfferNotificationBubbleResult.";
  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      histogram_name += "CardLinkedOffer.";
      break;
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += "GPayPromoCodeOffer.";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer.";
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  // Add subhistogram for |is_reshow| decision.
  histogram_name += is_reshow ? "Reshows" : "FirstShow";
  base::UmaHistogramEnumeration(histogram_name, metric);
}

void LogOfferNotificationBubbleSuppressed(
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name = "Autofill.OfferNotificationBubbleSuppressed.";
  // Switch to different sub-histogram depending on offer type being suppressed.
  // Card-linked offers will not be suppressed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += "GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += "FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, true);
}

void LogOfferNotificationInfoBarDeepLinkClicked() {
  base::RecordAction(base::UserMetricsAction(
      "Autofill_OfferNotificationInfoBar_DeepLinkClicked"));
}

void LogOfferNotificationInfoBarResultMetric(
    OfferNotificationInfoBarResultMetric metric) {
  DCHECK_LE(metric, OfferNotificationInfoBarResultMetric::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OfferNotificationInfoBarResult.CardLinkedOffer", metric);
}

void LogOfferNotificationInfoBarShown() {
  base::UmaHistogramBoolean(
      "Autofill.OfferNotificationInfoBarOffer.CardLinkedOffer", true);
}

void LogStoredOfferMetrics(
    const std::vector<std::unique_ptr<AutofillOfferData>>& offers) {
  std::unordered_map<AutofillOfferData::OfferType, int> offer_count;
  for (const std::unique_ptr<AutofillOfferData>& offer : offers) {
    // This function should only be run when the profile is loaded, which means
    // the only offers that should be available are the ones that are stored on
    // disk. Since free listing coupons are not stored on disk, we should never
    // have any loaded here.
    DCHECK_NE(offer->GetOfferType(),
              AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER);

    offer_count[offer->GetOfferType()]++;

    std::string related_merchant_count_histogram_name =
        "Autofill.Offer.StoredOfferRelatedMerchantCount";
    // Switch to different sub-histogram depending on offer type being
    // displayed.
    switch (offer->GetOfferType()) {
      case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
        related_merchant_count_histogram_name += ".GPayPromoCodeOffer";
        break;
      case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
        related_merchant_count_histogram_name += ".CardLinkedOffer";
        break;
      case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      case AutofillOfferData::OfferType::UNKNOWN:
        NOTREACHED();
        continue;
    }
    base::UmaHistogramCounts1000(related_merchant_count_histogram_name,
                                 offer->GetMerchantOrigins().size());

    if (offer->GetOfferType() ==
        AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER) {
      base::UmaHistogramCounts1000("Autofill.Offer.StoredOfferRelatedCardCount",
                                   offer->GetEligibleInstrumentIds().size());
    }
  }

  base::UmaHistogramCounts1000(
      "Autofill.Offer.StoredOfferCount2.GPayPromoCodeOffer",
      offer_count[AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER]);

  base::UmaHistogramCounts1000(
      "Autofill.Offer.StoredOfferCount2.CardLinkedOffer",
      offer_count[AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER]);
}

void LogOffersSuggestionsPopupShown(bool first_time_being_logged) {
  if (first_time_being_logged) {
    // We log that the offers suggestions popup was shown once for this field
    // while autofilling if it is the first time being logged.
    base::UmaHistogramEnumeration(
        "Autofill.Offer.SuggestionsPopupShown2",
        autofill::autofill_metrics::OffersSuggestionsPopupEvent::
            kOffersSuggestionsPopupShownOnce);
  }

  // We log every time the offers suggestions popup is shown, regardless if the
  // user is repeatedly clicking the same field.
  base::UmaHistogramEnumeration(
      "Autofill.Offer.SuggestionsPopupShown2",
      autofill::autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown);
}

void LogIndividualOfferSuggestionEvent(
    OffersSuggestionsEvent event,
    AutofillOfferData::OfferType offer_type) {
  std::string histogram_name = "Autofill.Offer.Suggestion2";

  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += ".GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }

  base::UmaHistogramEnumeration(histogram_name, event);
}

// static
void LogSyncedOfferDataBeingValid(bool valid) {
  base::UmaHistogramBoolean("Autofill.Offer.SyncedOfferDataBeingValid", valid);
}

void LogPageLoadsWithOfferIconShown(AutofillOfferData::OfferType offer_type) {
  std::string histogram_name = "Autofill.PageLoadsWithOfferIconShowing";
  // Switch to different sub-histogram depending on offer type being displayed.
  switch (offer_type) {
    case AutofillOfferData::OfferType::FREE_LISTING_COUPON_OFFER:
      histogram_name += ".FreeListingCouponOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_CARD_LINKED_OFFER:
      histogram_name += "CardLinkedOffer";
      break;
    case AutofillOfferData::OfferType::GPAY_PROMO_CODE_OFFER:
      histogram_name += ".GPayPromoCodeOffer";
      break;
    case AutofillOfferData::OfferType::UNKNOWN:
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(histogram_name, true);
}

}  // namespace autofill::autofill_metrics
