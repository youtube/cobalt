// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_url_sb_client.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"

namespace safe_browsing {

DownloadUrlSBClient::DownloadUrlSBClient(
    download::DownloadItem* item,
    DownloadProtectionService* service,
    CheckDownloadCallback callback,
    const scoped_refptr<SafeBrowsingUIManager>& ui_manager,
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
    : item_(item),
      sha256_hash_(item->GetHash()),
      url_chain_(item->GetUrlChain()),
      referrer_url_(item->GetReferrerUrl()),
      total_type_(DOWNLOAD_URL_CHECKS_TOTAL),
      dangerous_type_(DOWNLOAD_URL_CHECKS_MALWARE),
      service_(service),
      callback_(std::move(callback)),
      ui_manager_(ui_manager),
      start_time_(base::TimeTicks::Now()),
      database_manager_(database_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(item_);
  DCHECK(service_);
  download_item_observation_.Observe(item_.get());
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item_));
  extended_reporting_level_ =
      profile ? GetExtendedReportingLevel(*profile->GetPrefs())
              : SBER_LEVEL_OFF;
  is_enhanced_protection_ =
      profile ? IsEnhancedProtectionEnabled(*profile->GetPrefs()) : false;
}

// Implements DownloadItem::Observer.
void DownloadUrlSBClient::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DCHECK(download_item_observation_.IsObservingSource(item_.get()));
  download_item_observation_.Reset();
  item_ = nullptr;
}

void DownloadUrlSBClient::StartCheck() {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);
  if (!database_manager_.get() ||
      database_manager_->CheckDownloadUrl(url_chain_, this)) {
    CheckDone(SB_THREAT_TYPE_SAFE);
  } else {
    // Add a reference to this object to prevent it from being destroyed
    // before url checking result is returned.
    AddRef();
  }
}

bool DownloadUrlSBClient::IsDangerous(SBThreatType threat_type) const {
  return threat_type == SB_THREAT_TYPE_URL_BINARY_MALWARE;
}

// Implements SafeBrowsingDatabaseManager::Client.
void DownloadUrlSBClient::OnCheckDownloadUrlResult(
    const std::vector<GURL>& url_chain,
    SBThreatType threat_type) {
  CheckDone(threat_type);
  UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                      base::TimeTicks::Now() - start_time_);
  Release();
}

DownloadUrlSBClient::~DownloadUrlSBClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void DownloadUrlSBClient::CheckDone(SBThreatType threat_type) {
  DownloadCheckResult result = IsDangerous(threat_type)
                                   ? DownloadCheckResult::DANGEROUS
                                   : DownloadCheckResult::SAFE;
  UpdateDownloadCheckStats(total_type_);
  if (threat_type != SB_THREAT_TYPE_SAFE) {
    UpdateDownloadCheckStats(dangerous_type_);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadUrlSBClient::ReportMalware, this, threat_type));
  } else {
    // Identify download referrer chain, which will be used in
    // ClientDownloadRequest.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadUrlSBClient::IdentifyReferrerChain, this));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result));
}

void DownloadUrlSBClient::ReportMalware(SBThreatType threat_type) {
  std::string post_data;
  if (!sha256_hash_.empty()) {
    post_data +=
        base::HexEncode(sha256_hash_.data(), sha256_hash_.size()) + "\n";
  }
  for (size_t i = 0; i < url_chain_.size(); ++i) {
    post_data += url_chain_[i].spec() + "\n";
  }

  std::unique_ptr<HitReport> hit_report = std::make_unique<HitReport>();
  hit_report->malicious_url = url_chain_.back();
  hit_report->page_url = url_chain_.front();
  hit_report->referrer_url = referrer_url_;
  hit_report->is_subresource = true;
  hit_report->threat_type = threat_type;
  hit_report->threat_source = database_manager_->GetThreatSource();
  hit_report->post_data = post_data;
  hit_report->extended_reporting_level = extended_reporting_level_;
  hit_report->is_enhanced_protection = is_enhanced_protection_;
  hit_report->is_metrics_reporting_active =
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();

  ui_manager_->MaybeReportSafeBrowsingHit(
      std::move(hit_report), content::DownloadItemUtils::GetWebContents(item_));
}

void DownloadUrlSBClient::IdentifyReferrerChain() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!item_)
    return;

  item_->SetUserData(ReferrerChainData::kDownloadReferrerChainDataKey,
                     service_->IdentifyReferrerChain(*item_));
}

void DownloadUrlSBClient::UpdateDownloadCheckStats(SBStatsType stat_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DownloadChecks", stat_type,
                            DOWNLOAD_CHECKS_MAX);
}

}  // namespace safe_browsing
