// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/large_icon_service_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service_impl.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "ui/gfx/favicon_size.h"

namespace {

favicon::LargeFaviconProvider* GetLargeFaviconProvider(
    content::BrowserContext* context) {
  return LargeIconServiceFactory::GetInstance()->GetForBrowserContext(context);
}

#if BUILDFLAG(IS_ANDROID)
// Seems like on Android `1 dip == 1 px`. The value of `kDipForServerRequests`
// can be overridden by `features::kLargeFaviconFromGoogle`.
const int kDipForServerRequests = 24;
const favicon_base::IconType kIconTypeForServerRequests =
    favicon_base::IconType::kTouchIcon;
const char kGoogleServerClientParam[] = "chrome";
#else
const int kDipForServerRequests = 16;
const favicon_base::IconType kIconTypeForServerRequests =
    favicon_base::IconType::kFavicon;
const char kGoogleServerClientParam[] = "chrome_desktop";
#endif

}  // namespace

// static
favicon::LargeIconService* LargeIconServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<favicon::LargeIconService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
LargeIconServiceFactory* LargeIconServiceFactory::GetInstance() {
  static base::NoDestructor<LargeIconServiceFactory> instance;
  return instance.get();
}

LargeIconServiceFactory::LargeIconServiceFactory()
    : ProfileKeyedServiceFactory(
          "LargeIconService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  favicon::SetLargeFaviconProviderGetter(
      base::BindRepeating(&GetLargeFaviconProvider));
}

LargeIconServiceFactory::~LargeIconServiceFactory() = default;

std::unique_ptr<KeyedService>
LargeIconServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);

  return std::make_unique<favicon::LargeIconServiceImpl>(
      favicon_service,
      std::make_unique<image_fetcher::ImageFetcherImpl>(
          std::make_unique<ImageDecoderImpl>(),
          profile->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess()),
      desired_size_in_dip_for_server_requests(), kIconTypeForServerRequests,
      kGoogleServerClientParam);
}

// static
int LargeIconServiceFactory::desired_size_in_dip_for_server_requests() {
  if (base::FeatureList::IsEnabled(features::kLargeFaviconFromGoogle)) {
    return features::kLargeFaviconFromGoogleSizeInDip.Get();
  }
  return kDipForServerRequests;
}

bool LargeIconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
