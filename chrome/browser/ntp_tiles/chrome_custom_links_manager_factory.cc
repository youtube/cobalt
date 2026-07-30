// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_custom_links_manager_factory.h"

#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

std::unique_ptr<ntp_tiles::CustomLinksManager>
ChromeCustomLinksManagerFactory::NewForProfile(Profile* profile) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  size_t max_links = ntp_tiles::kMaxNumCustomLinks;
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(chrome::android::kUseWebUiNtpAndroid)) {
    max_links = 10;
  }
#endif
  return std::make_unique<ntp_tiles::CustomLinksManagerImpl>(
      ntp_tiles::CustomLinksManagerImpl::Options{
          .prefs = profile->GetPrefs(),
          .history_service = history_service,
          .max_links = max_links});
}
