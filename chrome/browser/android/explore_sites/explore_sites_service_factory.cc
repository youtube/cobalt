// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/explore_sites_service_factory.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"

#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/android/explore_sites/explore_sites_service_impl.h"
#include "chrome/browser/android/explore_sites/explore_sites_store.h"
#include "chrome/browser/android/explore_sites/history_statistics_reporter.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace explore_sites {
const base::FilePath::CharType kExploreSitesStoreDirname[] =
    FILE_PATH_LITERAL("Explore");

class URLLoaderFactoryGetterImpl
    : public ExploreSitesServiceImpl::URLLoaderFactoryGetter {
 public:
  explicit URLLoaderFactoryGetterImpl(Profile* profile) : profile_(profile) {}

  URLLoaderFactoryGetterImpl(const URLLoaderFactoryGetterImpl&) = delete;
  URLLoaderFactoryGetterImpl& operator=(const URLLoaderFactoryGetterImpl&) =
      delete;

  scoped_refptr<network::SharedURLLoaderFactory> GetFactory() override {
    return profile_->GetURLLoaderFactory();
  }

 private:
  raw_ptr<Profile> profile_;
};

ExploreSitesServiceFactory::ExploreSitesServiceFactory()
    : ProfileKeyedServiceFactory(
          "ExploreSitesService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
}
ExploreSitesServiceFactory::~ExploreSitesServiceFactory() = default;

// static
ExploreSitesServiceFactory* ExploreSitesServiceFactory::GetInstance() {
  return base::Singleton<ExploreSitesServiceFactory>::get();
}

// static
ExploreSitesService* ExploreSitesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExploreSitesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

bool ExploreSitesServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Always create this service with BrowserContext. This service is lightweight
  // but ensures various background activities are on if they are needed.
  return true;
}

KeyedService* ExploreSitesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::FilePath store_path =
      profile->GetPath().Append(kExploreSitesStoreDirname);
  auto explore_sites_store =
      std::make_unique<ExploreSitesStore>(background_task_runner, store_path);
  auto url_loader_factory_getter =
      std::make_unique<URLLoaderFactoryGetterImpl>(profile);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  auto history_stats_reporter = std::make_unique<HistoryStatisticsReporter>(
      history_service, profile->GetPrefs());

  return new ExploreSitesServiceImpl(std::move(explore_sites_store),
                                     std::move(url_loader_factory_getter),
                                     std::move(history_stats_reporter));
}
}  // namespace explore_sites
