// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_connectors {

ContentAnalysisDownloadsDelegate::ContentAnalysisDownloadsDelegate(
    const std::u16string& filename,
    const std::u16string& custom_message,
    GURL custom_learn_more_url,
    bool bypass_justification_required,
    base::OnceCallback<void()> open_file_callback,
    base::OnceCallback<void()> discard_file_callback,
    download::DownloadItem* download_item)
    : filename_(filename),
      custom_message_(custom_message),
      custom_learn_more_url_(custom_learn_more_url),
      bypass_justification_required_(bypass_justification_required),
      open_file_callback_(std::move(open_file_callback)),
      discard_file_callback_(std::move(discard_file_callback)),
      download_item_(download_item) {
  if (download_item_) {
    download_item_->AddObserver(this);
  }
}

ContentAnalysisDownloadsDelegate::~ContentAnalysisDownloadsDelegate() {
  if (download_item_) {
    download_item_->RemoveObserver(this);
  }
}

void ContentAnalysisDownloadsDelegate::BypassWarnings(
    absl::optional<std::u16string> user_justification) {
  if (download_item_) {
    enterprise_connectors::ScanResult* stored_result =
        static_cast<enterprise_connectors::ScanResult*>(
            download_item_->GetUserData(
                enterprise_connectors::ScanResult::kKey));

    if (stored_result) {
      stored_result->user_justification = user_justification;
    } else {
      auto scan_result = std::make_unique<enterprise_connectors::ScanResult>();
      scan_result->user_justification = user_justification;
      download_item_->SetUserData(enterprise_connectors::ScanResult::kKey,
                                  std::move(scan_result));
    }
  }

  if (open_file_callback_)
    std::move(open_file_callback_).Run();
  ResetCallbacks();
}

void ContentAnalysisDownloadsDelegate::Cancel(bool warning) {
  if (discard_file_callback_)
    std::move(discard_file_callback_).Run();
  ResetCallbacks();
}

void ContentAnalysisDownloadsDelegate::ResetCallbacks() {
  discard_file_callback_.Reset();
  open_file_callback_.Reset();
}

absl::optional<std::u16string>
ContentAnalysisDownloadsDelegate::GetCustomMessage() const {
  if (custom_message_.empty())
    return absl::nullopt;
  return l10n_util::GetStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_CUSTOM_MESSAGE, filename_,
      custom_message_);
}

absl::optional<GURL> ContentAnalysisDownloadsDelegate::GetCustomLearnMoreUrl()
    const {
  if (custom_learn_more_url_.is_empty())
    return absl::nullopt;
  return custom_learn_more_url_;
}

bool ContentAnalysisDownloadsDelegate::BypassRequiresJustification() const {
  return bypass_justification_required_;
}

std::u16string ContentAnalysisDownloadsDelegate::GetBypassJustificationLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOAD_BYPASS_JUSTIFICATION_LABEL);
}

absl::optional<std::u16string>
ContentAnalysisDownloadsDelegate::OverrideCancelButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_DISCARD_FILE_BUTTON);
}

void ContentAnalysisDownloadsDelegate::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DCHECK_EQ(download, download_item_);
  download->RemoveObserver(this);
  download_item_ = nullptr;
}

}  // namespace enterprise_connectors
