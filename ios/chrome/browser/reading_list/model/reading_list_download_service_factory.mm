// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"

#import "base/files/file_path.h"
#import "base/no_destructor.h"
#import "components/dom_distiller/core/distiller.h"
#import "components/dom_distiller/core/distiller_url_fetcher.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
ReadingListDownloadService* ReadingListDownloadServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ReadingListDownloadService>(
      profile, /*create=*/true);
}

// static
ReadingListDownloadServiceFactory*
ReadingListDownloadServiceFactory::GetInstance() {
  static base::NoDestructor<ReadingListDownloadServiceFactory> instance;
  return instance.get();
}

ReadingListDownloadServiceFactory::ReadingListDownloadServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ReadingListDownloadService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

ReadingListDownloadServiceFactory::~ReadingListDownloadServiceFactory() {}

std::unique_ptr<KeyedService>
ReadingListDownloadServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
      distiller_page_factory =
          std::make_unique<reading_list::ReadingListDistillerPageFactory>(
              context);

  auto distiller_url_fetcher_factory =
      std::make_unique<dom_distiller::DistillerURLFetcherFactory>(
          context->GetSharedURLLoaderFactory());

  dom_distiller::proto::DomDistillerOptions options;
  auto distiller_factory =
      std::make_unique<dom_distiller::DistillerFactoryImpl>(
          std::move(distiller_url_fetcher_factory), options);

  return std::make_unique<ReadingListDownloadService>(
      ReadingListModelFactory::GetForProfile(profile), profile->GetPrefs(),
      profile->GetStatePath(), profile->GetSharedURLLoaderFactory(),
      std::move(distiller_factory), std::move(distiller_page_factory));
}
