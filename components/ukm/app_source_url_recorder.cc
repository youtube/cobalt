// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/app_source_url_recorder.h"

#include "base/atomic_sequence_num.h"
#include "components/crx_file/id_util.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

SourceId AssignNewAppId() {
  static base::AtomicSequenceNumber seq;
  return ConvertToSourceId(seq.GetNext() + 1, SourceIdType::APP_ID);
}

SourceId AppSourceUrlRecorder::GetSourceIdForChromeApp(
    const std::string& app_id) {
  DCHECK(!app_id.empty());
  return GetSourceIdForUrl(GetURLForChromeApp(app_id), AppType::kChromeApp);
}

GURL AppSourceUrlRecorder::GetURLForChromeApp(const std::string& app_id) {
  return GURL("app://" + app_id);
}

SourceId AppSourceUrlRecorder::GetSourceIdForChromeExtension(
    const std::string& id) {
  return GetSourceIdForUrl(GetURLForChromeExtension(id), AppType::kExtension);
}

GURL AppSourceUrlRecorder::GetURLForChromeExtension(const std::string& id) {
  return GURL("chrome-extension://" + id);
}

SourceId AppSourceUrlRecorder::GetSourceIdForArcPackageName(
    const std::string& package_name) {
  DCHECK(!package_name.empty());
  return GetSourceIdForUrl(GetURLForArcPackageName(package_name),
                           AppType::kArc);
}

GURL AppSourceUrlRecorder::GetURLForArcPackageName(
    const std::string& package_name) {
  return GURL("app://" + package_name);
}

SourceId AppSourceUrlRecorder::GetSourceIdForArc(
    const std::string& package_name) {
  return GetSourceIdForUrl(GetURLForArc(package_name), AppType::kArc);
}

GURL AppSourceUrlRecorder::GetURLForArc(const std::string& package_name) {
  const std::string package_name_hash =
      crx_file::id_util::GenerateId(package_name);
  return GURL("app://play/" + package_name_hash);
}

SourceId AppSourceUrlRecorder::GetSourceIdForPWA(const GURL& url) {
  return GetSourceIdForUrl(url, AppType::kPWA);
}

GURL AppSourceUrlRecorder::GetURLForPWA(const GURL& url) {
  return url;
}

SourceId AppSourceUrlRecorder::GetSourceIdForBorealis(const std::string& app) {
  return GetSourceIdForUrl(GetURLForBorealis(app), AppType::kBorealis);
}

GURL AppSourceUrlRecorder::GetURLForBorealis(const std::string& app) {
  return GURL("app://borealis/" + app);
}

SourceId AppSourceUrlRecorder::GetSourceIdForCrostini(
    const std::string& desktop_id,
    const std::string& app_name) {
  return GetSourceIdForUrl(GetURLForCrostini(desktop_id, app_name),
                           AppType::kCrostini);
}

GURL AppSourceUrlRecorder::GetURLForCrostini(const std::string& desktop_id,
                                             const std::string& app_name) {
  return GURL("app://" + desktop_id + "/" + app_name);
}

SourceId AppSourceUrlRecorder::GetSourceIdForUrl(const GURL& url,
                                                 AppType app_type) {
  ukm::DelegatingUkmRecorder* const recorder =
      ukm::DelegatingUkmRecorder::Get();
  if (!recorder) {
    return kInvalidSourceId;
  }

  const SourceId source_id = AssignNewAppId();
  if (base::FeatureList::IsEnabled(kUkmAppLogging)) {
    recorder->UpdateAppURL(source_id, url, app_type);
  }
  return source_id;
}

void AppSourceUrlRecorder::MarkSourceForDeletion(SourceId source_id) {
  if (GetSourceIdType(source_id) != SourceIdType::APP_ID) {
    DLOG(FATAL) << "AppSourceUrlRecorder::MarkSourceForDeletion invoked on "
                << "non-APP_ID type SourceId: " << source_id;
    return;
  }

  ukm::DelegatingUkmRecorder* const recorder =
      ukm::DelegatingUkmRecorder::Get();
  if (recorder) {
    recorder->MarkSourceForDeletion(source_id);
  }
}

}  // namespace ukm
