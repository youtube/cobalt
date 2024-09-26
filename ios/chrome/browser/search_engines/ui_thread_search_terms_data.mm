// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"

#import <string>

#import "base/check.h"
#import "base/strings/escape.h"
#import "components/google/core/common/google_util.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/google/google_brand.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_thread.h"
#import "rlz/buildflags/buildflags.h"
#import "url/gurl.h"

#if BUILDFLAG(ENABLE_RLZ)
#import "components/rlz/rlz_tracker.h"  // nogncheck
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

UIThreadSearchTermsData::UIThreadSearchTermsData() {
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
}

UIThreadSearchTermsData::~UIThreadSearchTermsData() {}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  if (google_base_url.is_valid())
    return google_base_url.spec();

  return SearchTermsData::GoogleBaseURLValue();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetApplicationContext()->GetApplicationLocale();
}

std::u16string UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!from_app_list);
  DCHECK(thread_checker_.CalledOnValidThread());
  std::u16string rlz_string;
#if BUILDFLAG(ENABLE_RLZ)
  // For organic brandcode do not use rlz at all.
  if (!ios::google_brand::IsOrganic(ios::provider::GetBrandCode())) {
    // This call will may return false until the value has been cached. This
    // normally would mean that a few omnibox searches might not send the RLZ
    // data but this is not really a problem (as the value will eventually be
    // set and cached).
    rlz::RLZTracker::GetAccessPointRlz(rlz::RLZTracker::ChromeOmnibox(),
                                       &rlz_string);
  }
#endif
  return rlz_string;
}

std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::string();
}

std::string UIThreadSearchTermsData::GetSuggestClient(
    RequestSource request_source) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (request_source) {
    case RequestSource::NTP_MODULE:
      return "chrome-ios-ntp";
    case RequestSource::JOURNEYS:
      return "";
    case RequestSource::SEARCHBOX:
    case RequestSource::CROS_APP_LIST:
      return "chrome";
  }
}

std::string UIThreadSearchTermsData::GetSuggestRequestIdentifier(
    RequestSource request_source) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (request_source) {
    case RequestSource::NTP_MODULE:
    case RequestSource::JOURNEYS:
      return "";
    case RequestSource::SEARCHBOX:
    case RequestSource::CROS_APP_LIST:
      return "chrome-ext-ansg";
  }
}

std::string UIThreadSearchTermsData::GoogleImageSearchSource() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string version(version_info::GetProductName() + " " +
                      version_info::GetVersionNumber());
  if (version_info::IsOfficialBuild())
    version += " (Official)";
  version += " " + version_info::GetOSType();
  std::string modifier(GetChannelString());
  if (!modifier.empty())
    version += " " + modifier;
  return version;
}

}  // namespace ios
