// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::features {

// Features

// When enabled, Android N+ devices will be supported for FIDO authentication.
BASE_FEATURE(kAutofillEnableAndroidNKeyForFidoAuthentication,
             "AutofillEnableAndroidNKeyForFidoAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card art images (instead of network icons) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardArtImage,
             "AutofillEnableCardArtImage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, server will return card art images of the exact required
// dimension.
BASE_FEATURE(kAutofillEnableCardArtServerSideStretching,
             "AutofillEnableCardArtServerSideStretching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, card product name (instead of issuer network) will be shown in
// Payments Autofill UI.
BASE_FEATURE(kAutofillEnableCardProductName,
             "AutofillEnableCardProductName",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, we will store CVC for both local and server credit cards. This
// will also allow the users to autofill their CVCs on checkout pages.
BASE_FEATURE(kAutofillEnableCvcStorageAndFilling,
             "AutofillEnableCvcStorageAndFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the user encounters the yellow path (challenge path) in the
// VCN retrieval flow and the server denotes that the card is eligible for email
// OTP authentication, email OTP authentication will be offered as one of the
// challenge options.
BASE_FEATURE(kAutofillEnableEmailOtpForVcnYellowPath,
             "AutofillEnableEmailOtpForVcnYellowPath",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, user's will see network card art images and network icons which
// are larger, having a white border, and don't have the standard grey overlay
// applied to them.
BASE_FEATURE(kAutofillEnableNewCardArtAndNetworkImages,
             "AutofillEnableNewCardArtAndNetworkImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a progress dialog will display while authenticating with FIDO.
// TODO(crbug.com/1337380): Clean up kAutofillEnableFIDOProgressDialog when it's
// fully rolled out.
BASE_FEATURE(kAutofillEnableFIDOProgressDialog,
             "AutofillEnableFIDOProgressDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, server card retrieval will begin with a risk-based check
// instead of jumping straight to CVC or biometric auth.
BASE_FEATURE(kAutofillEnableFpanRiskBasedAuthentication,
             "AutofillEnableFpanRiskBasedAuthentication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, enable manual falling component for virtual cards on Android.
BASE_FEATURE(kAutofillEnableManualFallbackForVirtualCards,
             "AutofillEnableManualFallbackForVirtualCards",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the merchant_domain field is included in requests to unmask a
// card.
BASE_FEATURE(kAutofillEnableMerchantDomainInUnmaskCardRequest,
             "AutofillEnableMerchantDomainInUnmaskCardRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, client side URL filtering will be triggered for the merchant
// opt-out use-case, so that virtual card suggestions are not shown on websites
// that are opted-out of virtual cards.
BASE_FEATURE(kAutofillEnableMerchantOptOutClientSideUrlFiltering,
             "AutofillEnableMerchantOptOutClientSideUrlFiltering",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, the GPay logo will be moved to the right side in payments
// autofill dialogs and bubbles on desktop.
BASE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnDesktop,
             "AutofillEnableMovingGPayLogoToTheRightOnDesktop",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the GPay logo will be moved to the right side in payments
// autofill dialogs and bubbles on clank.
BASE_FEATURE(kAutofillEnableMovingGPayLogoToTheRightOnClank,
             "AutofillEnableMovingGPayLogoToTheRightOnClank",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the user will see a new banner logo and text in the bubble
// offering to Upstream their cards onto Google Pay.
BASE_FEATURE(kAutofillEnableNewSaveCardBubbleUi,
             "AutofillEnableNewSaveCardBubbleUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, offers will be displayed in the Clank keyboard accessory during
// downstream.
BASE_FEATURE(kAutofillEnableOffersInClankKeyboardAccessory,
             "AutofillEnableOffersInClankKeyboardAccessory",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the bottom sheet for save card and VCN enrollment will be
// displayed instead of the info bar on Android.
BASE_FEATURE(kAutofillEnablePaymentsAndroidBottomSheet,
             "AutofillEnablePaymentsAndroidBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, in use-cases where we would not have triggered any user-visible
// authentication to autofill payment methods, we will trigger a device
// authentication.
BASE_FEATURE(kAutofillEnablePaymentsMandatoryReauth,
             "AutofillEnablePaymentsMandatoryReauth",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, some extra metrics logging for Autofill Downstream will start.
BASE_FEATURE(kAutofillEnableRemadeDownstreamMetrics,
             "AutofillEnableRemadeDownstreamMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to offer upload save for IBANs
// (International Bank Account Numbers) and autofill server-based IBANs.
BASE_FEATURE(kAutofillEnableServerIban,
             "AutofillEnableServerIban",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, if the user interacts with the manual fallback bottom sheet
// on Android, it'll remain sticky until the user dismisses it.
BASE_FEATURE(kAutofillEnableStickyManualFallbackForCards,
             "AutofillEnableStickyManualFallbackForCards",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the user will have the ability to update the virtual card
// enrollment of a credit card through their chrome browser after certain
// autofill flows (for example, downstream and upstream), and from the settings
// page.
BASE_FEATURE(kAutofillEnableUpdateVirtualCardEnrollment,
             "AutofillEnableUpdateVirtualCardEnrollment",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// When enabled, after a successful authentication to autofill a virtual card,
// the user will be prompted to opt-in to FIDO if the user is not currently
// opted-in, and if the user is opted-in already and the virtual card is FIDO
// eligible the user will be prompted to register the virtual card into FIDO.
BASE_FEATURE(kAutofillEnableVirtualCardFidoEnrollment,
             "AutofillEnableVirtualCardFidoEnrollment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will show metadata along with other card information
// when the virtual card is presented to users.
BASE_FEATURE(kAutofillEnableVirtualCardMetadata,
             "AutofillEnableVirtualCardMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, legal term of save card view and virtual card enroll view will
// be moved before action buttons and icon will be moved after titles in those
// views.
BASE_FEATURE(kAutofillMoveLegalTermsAndIconForNewCardEnrollment,
             "AutofillMoveLegalTermsAndIconForNewCardEnrollment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Autofill will attempt to find standalone CVC fields for VCN
// card on file when parsing forms.
BASE_FEATURE(kAutofillParseVcnCardOnFileStandaloneCvcFields,
             "AutofillParseVcnCardOnFileStandaloneCvcFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill suggestions that consist of a local and server
// version of the same card will attempt to fill the server card upon selection
// instead of the local card.
BASE_FEATURE(kAutofillSuggestServerCardInsteadOfLocalCard,
             "AutofillSuggestServerCardInsteadOfLocalCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, GPay-related links direct to the newer GPay Web site instead of
// the legacy Payments Center.
BASE_FEATURE(kAutofillUpdateChromeSettingsLinkToGPayWeb,
             "AutofillUpdateChromeSettingsLinkToGPayWeb",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because the feature state depends on the user's country.
// The set of launched countries is listed in autofill_experiments.cc, and this
// flag remains as a way to easily enable upload credit card save for testers,
// as well as enable non-fully-launched countries on a trial basis.
BASE_FEATURE(kAutofillUpstream,
             "AutofillUpstream",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome allows credit card upload to Google Payments if the
// user's email domain is from a common email provider (thus unlikely to be an
// enterprise or education user).
BASE_FEATURE(kAutofillUpstreamAllowAdditionalEmailDomains,
             "AutofillUpstreamAllowAdditionalEmailDomains",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome allows credit card upload to Google Payments, no matter
// the user's email domain.
BASE_FEATURE(kAutofillUpstreamAllowAllEmailDomains,
             "AutofillUpstreamAllowAllEmailDomains",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
// When enabled, use two '•' when displaying the last four digits of a credit
// card number. (E.g., '•• 8888' rather than '•••• 8888').
BASE_FEATURE(kAutofillUseTwoDotsForLastFourDigits,
             "AutofillUseTwoDotsForLastFourDigits",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this and the above `kAutofillEnablePaymentsMandatoryReauth` are both
// enabled, in use-cases where we would not have triggered any user-visible
// authentication to autofill payment methods, we will trigger a device
// authentication on Bling.
BASE_FEATURE(kAutofillEnablePaymentsMandatoryReauthOnBling,
             "AutofillEnablePaymentsMandatoryReauthOnBling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When this is enabled, virtual card enrollment and retrieval will be enabled
// on Bling.
BASE_FEATURE(kAutofillEnableVirtualCards,
             "AutofillEnableVirtualCards",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// When enabled, Chrome will offer to pay with accounts supporting Pix.
BASE_FEATURE(kEnablePixPayments,
             "EnablePixPayments",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool ShouldShowImprovedUserConsentForCreditCardSave() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  // The new user consent UI is fully launched on MacOS, Windows and Linux.
  return true;
#else
  // Chrome OS does not have the new UI.
  return false;
#endif
}

}  // namespace autofill::features
