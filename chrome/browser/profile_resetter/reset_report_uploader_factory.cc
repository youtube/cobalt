// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/reset_report_uploader_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
ResetReportUploaderFactory* ResetReportUploaderFactory::GetInstance() {
  return base::Singleton<ResetReportUploaderFactory>::get();
}

// static
ResetReportUploader* ResetReportUploaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ResetReportUploader*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

ResetReportUploaderFactory::ResetReportUploaderFactory()
    : ProfileKeyedServiceFactory(
          "ResetReportUploaderFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

ResetReportUploaderFactory::~ResetReportUploaderFactory() {}

KeyedService* ResetReportUploaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ResetReportUploader(context->GetDefaultStoragePartition()
                                     ->GetURLLoaderFactoryForBrowserProcess());
}
