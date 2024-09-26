// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

namespace {

// The version of this client supporting tailored warnings.
// Please update the description of TailoredInfo field in csd.proto when
// changing this value.
// Note: The name of this variable is checked by PRESUBMIT. Please update the
// PRESUBMIT script before renaming this variable.
constexpr int kTailoredWarningVersion = 1;

DownloadRequestMaker::TabUrls TabUrlsFromWebContents(
    content::WebContents* web_contents) {
  DownloadRequestMaker::TabUrls result;
  if (web_contents) {
    content::NavigationEntry* entry =
        web_contents->GetController().GetVisibleEntry();
    if (entry) {
      result.url = entry->GetURL();
      result.referrer = entry->GetReferrer().url;
    }
  }
  return result;
}

}  // namespace

// static
std::unique_ptr<DownloadRequestMaker>
DownloadRequestMaker::CreateFromDownloadItem(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    download::DownloadItem* item) {
  std::vector<ClientDownloadRequest::Resource> resources;
  for (size_t i = 0; i < item->GetUrlChain().size(); ++i) {
    ClientDownloadRequest::Resource resource;
    resource.set_url(ShortURLForReporting(item->GetUrlChain()[i]));
    if (i == item->GetUrlChain().size() - 1) {
      // The last URL in the chain is the download URL.
      resource.set_type(ClientDownloadRequest::DOWNLOAD_URL);
      resource.set_referrer(ShortURLForReporting(item->GetReferrerUrl()));
      DVLOG(2) << "dl url " << resource.url();
      if (!item->GetRemoteAddress().empty()) {
        resource.set_remote_ip(item->GetRemoteAddress());
        DVLOG(2) << "  dl url remote addr: " << resource.remote_ip();
      }
      DVLOG(2) << "dl referrer " << resource.referrer();
    } else {
      DVLOG(2) << "dl redirect " << i << " " << resource.url();
      resource.set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
    }

    resources.push_back(std::move(resource));
  }

  return std::make_unique<DownloadRequestMaker>(
      binary_feature_extractor,
      content::DownloadItemUtils::GetBrowserContext(item),
      TabUrls{item->GetTabUrl(), item->GetTabReferrerUrl()},
      item->GetTargetFilePath(), item->GetFullPath(), item->GetURL(),
      item->GetHash(), item->GetReceivedBytes(), resources,
      item->HasUserGesture(),
      static_cast<ReferrerChainData*>(
          item->GetUserData(ReferrerChainData::kDownloadReferrerChainDataKey)));
}

// static
std::unique_ptr<DownloadRequestMaker>
DownloadRequestMaker::CreateFromFileSystemAccess(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    DownloadProtectionService* service,
    const content::FileSystemAccessWriteItem& item) {
  ClientDownloadRequest::Resource resource;
  resource.set_url(
      ShortURLForReporting(GetFileSystemAccessDownloadUrl(item.frame_url)));
  resource.set_type(ClientDownloadRequest::DOWNLOAD_URL);
  if (item.frame_url.is_valid())
    resource.set_referrer(ShortURLForReporting(item.frame_url));

  std::unique_ptr<ReferrerChainData> referrer_chain_data;
  if (service)
    service->IdentifyReferrerChain(item);

  return std::make_unique<DownloadRequestMaker>(
      binary_feature_extractor, item.browser_context,
      TabUrlsFromWebContents(item.web_contents), item.target_file_path,
      item.full_path, GetFileSystemAccessDownloadUrl(item.frame_url),
      item.sha256_hash, item.size,
      std::vector<ClientDownloadRequest::Resource>{resource},
      item.has_user_gesture, referrer_chain_data.get());
}

DownloadRequestMaker::DownloadRequestMaker(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    content::BrowserContext* browser_context,
    TabUrls tab_urls,
    base::FilePath target_file_path,
    base::FilePath full_path,
    GURL source_url,
    std::string sha256_hash,
    int64_t length,
    const std::vector<ClientDownloadRequest::Resource>& resources,
    bool is_user_initiated,
    ReferrerChainData* referrer_chain_data)
    : browser_context_(browser_context),
      request_(std::make_unique<ClientDownloadRequest>()),
      binary_feature_extractor_(binary_feature_extractor),
      tab_urls_(tab_urls),
      target_file_path_(target_file_path),
      full_path_(full_path) {
  request_->set_url(ShortURLForReporting(source_url));
  request_->mutable_digests()->set_sha256(sha256_hash);
  request_->set_length(length);
  for (const ClientDownloadRequest::Resource& resource : resources) {
    *request_->add_resources() = resource;
  }

  request_->set_user_initiated(is_user_initiated);

  if (referrer_chain_data &&
      !referrer_chain_data->GetReferrerChain()->empty()) {
    request_->mutable_referrer_chain()->Swap(
        referrer_chain_data->GetReferrerChain());
    request_->mutable_referrer_chain_options()
        ->set_recent_navigations_to_collect(
            referrer_chain_data->recent_navigations_to_collect());
  }
}

DownloadRequestMaker::~DownloadRequestMaker() = default;

void DownloadRequestMaker::Start(DownloadRequestMaker::Callback callback) {
  callback_ = std::move(callback);

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  bool is_under_advanced_protection =
      profile && AdvancedProtectionStatusManagerFactory::GetForProfile(profile)
                     ->IsUnderAdvancedProtection();

  *request_->mutable_population() =
      GetUserPopulationForProfileWithCookieTheftExperiments(profile);
  if (base::FeatureList::IsEnabled(kNestedArchives)) {
    request_->mutable_population()->add_finch_active_groups(
        "SafeBrowsingArchiveImprovements.Enabled");
  }
  request_->set_request_ap_verdicts(is_under_advanced_protection);
  request_->set_locale(g_browser_process->GetApplicationLocale());
  request_->set_file_basename(target_file_path_.BaseName().AsUTF8Unsafe());

  PopulateTailoredInfo();

  file_analyzer_->Start(
      target_file_path_, full_path_,
      base::BindOnce(&DownloadRequestMaker::OnFileFeatureExtractionDone,
                     weakptr_factory_.GetWeakPtr()));
  start_time_ = base::Time::Now();
}

void DownloadRequestMaker::OnFileFeatureExtractionDone(
    FileAnalyzer::Results results) {
  base::UmaHistogramMediumTimes(
      "SBClientDownload.FileFeatureExtractionDuration",
      base::Time::Now() - start_time_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  request_->set_download_type(results.type);
  request_->set_archive_valid(results.archive_summary.parser_status() ==
                              ClientDownloadRequest::ArchiveSummary::VALID);
  request_->set_archive_file_count(results.archive_summary.file_count());
  request_->set_archive_directory_count(
      results.archive_summary.directory_count());
  request_->mutable_archived_binary()->CopyFrom(results.archived_binaries);
  request_->mutable_signature()->CopyFrom(results.signature_info);
  request_->mutable_image_headers()->CopyFrom(results.image_headers);
  request_->mutable_document_summary()->CopyFrom(results.document_summary);
  request_->mutable_archive_summary()->CopyFrom(results.archive_summary);

#if BUILDFLAG(IS_MAC)
  if (!results.disk_image_signature.empty()) {
    request_->set_udif_code_signature(results.disk_image_signature.data(),
                                      results.disk_image_signature.size());
  }
  if (!results.detached_code_signatures.empty()) {
    request_->mutable_detached_code_signature()->CopyFrom(
        results.detached_code_signatures);
  }
#endif

  GetTabRedirects();
}

void DownloadRequestMaker::GetTabRedirects() {
  start_time_ = base::Time::Now();
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_urls_.url.is_valid()) {
    OnGotTabRedirects({});
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile) {
    OnGotTabRedirects({});
    return;
  }

  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history) {
    OnGotTabRedirects({});
    return;
  }

  history->QueryRedirectsTo(
      tab_urls_.url,
      base::BindOnce(&DownloadRequestMaker::OnGotTabRedirects,
                     weakptr_factory_.GetWeakPtr()),
      &request_tracker_);
}

void DownloadRequestMaker::OnGotTabRedirects(
    history::RedirectList redirect_list) {
  base::UmaHistogramMediumTimes("SBClientDownload.GetTabRedirectsDuration",
                                base::Time::Now() - start_time_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (size_t i = 0; i < redirect_list.size(); ++i) {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    DVLOG(2) << "tab redirect " << i << " " << redirect_list[i].spec();
    resource->set_url(ShortURLForReporting(redirect_list[i]));
    resource->set_type(ClientDownloadRequest::TAB_REDIRECT);
  }
  if (tab_urls_.url.is_valid()) {
    ClientDownloadRequest::Resource* resource = request_->add_resources();
    resource->set_url(ShortURLForReporting(tab_urls_.url));
    DVLOG(2) << "tab url " << resource->url();
    resource->set_type(ClientDownloadRequest::TAB_URL);
    if (tab_urls_.referrer.is_valid()) {
      resource->set_referrer(ShortURLForReporting(tab_urls_.referrer));
      DVLOG(2) << "tab referrer " << resource->referrer();
    }
  }

  std::move(callback_).Run(std::move(request_));
}

void DownloadRequestMaker::PopulateTailoredInfo() {
  ClientDownloadRequest::TailoredInfo tailored_info;
  tailored_info.set_version(kTailoredWarningVersion);
  *request_->mutable_tailored_info() = tailored_info;
}

}  // namespace safe_browsing
