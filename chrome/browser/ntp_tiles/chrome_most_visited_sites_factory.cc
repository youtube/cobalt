// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"

#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ntp_tiles/chrome_custom_links_manager_factory.h"
#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/features.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/ntp_tiles/icon_cacher_impl.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/storage_partition.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
namespace {

class SupervisorBridge : public ntp_tiles::MostVisitedSitesSupervisor,
                         public SupervisedUserServiceObserver {
 public:
  explicit SupervisorBridge(Profile* profile);
  ~SupervisorBridge() override;

  void SetObserver(Observer* observer) override;
  bool IsBlocked(const GURL& url) override;
  bool IsChildProfile() override;

  // SupervisedUserServiceObserver implementation.
  void OnURLFilterChanged() override;

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<Observer> supervisor_observer_;
  base::ScopedObservation<SupervisedUserService, SupervisedUserServiceObserver>
      register_observation_{this};
};

SupervisorBridge::SupervisorBridge(Profile* profile)
    : profile_(profile), supervisor_observer_(nullptr) {
  register_observation_.Observe(
      SupervisedUserServiceFactory::GetForProfile(profile_));
}

SupervisorBridge::~SupervisorBridge() {}

void SupervisorBridge::SetObserver(Observer* new_observer) {
  if (new_observer) {
    DCHECK(!supervisor_observer_);
  } else {
    DCHECK(supervisor_observer_);
  }

  supervisor_observer_ = new_observer;
}

bool SupervisorBridge::IsBlocked(const GURL& url) {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  auto* url_filter = supervised_user_service->GetURLFilter();
  return url_filter->GetFilteringBehaviorForURL(url) ==
         supervised_user::SupervisedUserURLFilter::FilteringBehavior::BLOCK;
}

bool SupervisorBridge::IsChildProfile() {
  return profile_->IsChild();
}

void SupervisorBridge::OnURLFilterChanged() {
  if (supervisor_observer_) {
    supervisor_observer_->OnBlockedSitesChanged();
  }
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

// static
std::unique_ptr<ntp_tiles::MostVisitedSites>
ChromeMostVisitedSitesFactory::NewForProfile(Profile* profile) {
  // MostVisitedSites doesn't exist in incognito profiles.
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  std::unique_ptr<data_decoder::DataDecoder> data_decoder;
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          image_fetcher::features::kBatchImageDecoding)) {
    data_decoder = std::make_unique<data_decoder::DataDecoder>();
  }
#endif

  bool is_default_chrome_app_migrated;
#if BUILDFLAG(IS_ANDROID)
  is_default_chrome_app_migrated = false;
#else
  is_default_chrome_app_migrated = true;
#endif

  auto most_visited_sites = std::make_unique<ntp_tiles::MostVisitedSites>(
      profile->GetPrefs(), TopSitesFactory::GetForProfile(profile),
#if BUILDFLAG(IS_ANDROID)
      ChromePopularSitesFactory::NewForProfile(profile),
#else
      nullptr,
#endif
#if !BUILDFLAG(IS_ANDROID)
      ChromeCustomLinksManagerFactory::NewForProfile(profile),
#else
      nullptr,
#endif
      std::make_unique<ntp_tiles::IconCacherImpl>(
          FaviconServiceFactory::GetForProfile(
              profile, ServiceAccessType::IMPLICIT_ACCESS),
          LargeIconServiceFactory::GetForBrowserContext(profile),
          std::make_unique<image_fetcher::ImageFetcherImpl>(
              std::make_unique<ImageDecoderImpl>(),
              profile->GetDefaultStoragePartition()
                  ->GetURLLoaderFactoryForBrowserProcess()),
          std::move(data_decoder)),
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
      std::make_unique<SupervisorBridge>(profile),
#else
      nullptr,
#endif
      is_default_chrome_app_migrated);
  return most_visited_sites;
}
