// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_offer_manager_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/shopping_service_delegate_impl.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/commerce/core/shopping_service.h"
#endif

namespace autofill {

// static
AutofillOfferManager* AutofillOfferManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutofillOfferManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillOfferManagerFactory* AutofillOfferManagerFactory::GetInstance() {
  static base::NoDestructor<AutofillOfferManagerFactory> instance;
  return instance.get();
}

AutofillOfferManagerFactory::AutofillOfferManagerFactory()
    : ProfileKeyedServiceFactory(
          "AutofillOfferManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(CouponServiceFactory::GetInstance());
#endif
}

AutofillOfferManagerFactory::~AutofillOfferManagerFactory() = default;

KeyedService* AutofillOfferManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if !BUILDFLAG(IS_ANDROID)
  CouponService* coupon_service =
      CouponServiceFactory::GetForProfile(Profile::FromBrowserContext(context));
  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(context);
  auto shopping_service_delegate =
      std::make_unique<ShoppingServiceDelegateImpl>(shopping_service);
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context),
      static_cast<CouponServiceDelegate*>(coupon_service),
      std::move(shopping_service_delegate));
#else
  return new AutofillOfferManager(
      PersonalDataManagerFactory::GetForBrowserContext(context),
      /*coupon_service_delegate=*/nullptr,
      /*shopping_service_delegate=*/nullptr);
#endif
}

}  // namespace autofill
