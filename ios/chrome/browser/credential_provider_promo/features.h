// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_FEATURES_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_FEATURES_H_

#import "base/feature_list.h"

// Feature flag to enable the Credential Provider Extension Promo feature.
BASE_DECLARE_FEATURE(kCredentialProviderExtensionPromo);

extern const char kCredentialProviderExtensionPromoOnPasswordSavedParam[];
extern const char kCredentialProviderExtensionPromoOnPasswordCopiedParam[];
extern const char kCredentialProviderExtensionPromoOnLoginWithAutofillParam[];

// Returns true if Credential Provider Extension Promo feature is enabled on
// some event.
bool IsCredentialProviderExtensionPromoEnabled();

// Returns true if Credential Provider Extension Promo feature is enabled on
// password saved event.
bool IsCredentialProviderExtensionPromoEnabledOnPasswordSaved();

// Returns true if Credential Provider Extension Promo feature is enabled on
// password copied event.
bool IsCredentialProviderExtensionPromoEnabledOnPasswordCopied();

// Returns true if Credential Provider Extension Promo feature is enabled on
// login with autofill event.
bool IsCredentialProviderExtensionPromoEnabledOnLoginWithAutofill();

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_FEATURES_H_
