// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <stdint.h>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#if defined(STARBOARD)
#include "cobalt/updater/updater_module.h"
#endif
#include "components/update_client/network.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace {

#if defined(STARBOARD)

using cobalt::updater::UpdaterStatus;
using cobalt::updater::updater_status_string_map;

void CleanupDirectory(base::FilePath& dir) {
  std::stack<std::string> directories;
  base::FileEnumerator file_enumerator(
      dir, true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (auto path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::FileEnumerator::FileInfo info(file_enumerator.GetInfo());

    if (info.IsDirectory()) {
      directories.push(path.value());
    } else {
      SbFileDelete(path.value().c_str());
    }
  }
  while (!directories.empty()) {
    SbFileDelete(directories.top().c_str());
    directories.pop();
  }
}

#endif

const base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

namespace update_client {

#if defined(STARBOARD)
UrlFetcherDownloader::UrlFetcherDownloader(
    std::unique_ptr<CrxDownloader> successor,
    scoped_refptr<Configurator> config)
    : CrxDownloader(std::move(successor)),
      config_(config),
      network_fetcher_factory_(config->GetNetworkFetcherFactory()) {}
#else
UrlFetcherDownloader::UrlFetcherDownloader(
    std::unique_ptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {}
#endif

UrlFetcherDownloader::~UrlFetcherDownloader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

#if defined(STARBOARD)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url) {
  SB_LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot: url=" << url;
  if (!cobalt_slot_management_.ConfirmSlot(download_dir_)) {
    ReportDownloadFailure(url, CrxDownloaderError::SLOT_UNAVAILABLE);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
                                base::Unretained(this), url));
}

void UrlFetcherDownloader::SelectSlot(const GURL& url) {
  SB_LOG(INFO) << "UrlFetcherDownloader::SelectSlot: url=" << url;
  if (!cobalt_slot_management_.SelectSlot(&download_dir_)) {
    ReportDownloadFailure(url, CrxDownloaderError::SLOT_UNAVAILABLE);
    return;
  }
  config_->SetUpdaterStatus(std::string(
      updater_status_string_map.find(UpdaterStatus::kSlotLocked)->second));

  // Use 15 sec delay to allow for other updaters/loaders to settle down.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UrlFetcherDownloader::ConfirmSlot, base::Unretained(this),
                     url),
      base::TimeDelta::FromSeconds(15));
}
#endif

void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    SB_LOG(ERROR) << "Failed to get installation manager";
    ReportDownloadFailure(url);
    return;
  }
  if (!cobalt_slot_management_.Init(installation_api)) {
    ReportDownloadFailure(url);
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::SelectSlot,
                                base::Unretained(this), url));
#else
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UrlFetcherDownloader::CreateDownloadDir,
                     base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
                     base::Unretained(this), url));
#endif
}

#if defined(STARBOARD)
void UrlFetcherDownloader::DoCancelDownload() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  network_fetcher_->CancelDownloadToFile();
}
#endif

void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}

#if defined(STARBOARD)
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
  ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
}

void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url,
                                                 CrxDownloaderError error) {
#else
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(STARBOARD)
  cobalt_slot_management_.CleanupAllDrainFiles(download_dir_);
#endif
  Result result;
#if defined(STARBOARD)
  result.error = static_cast<int>(error);
#else
  result.error = -1;
#endif

  DownloadMetrics download_metrics;
  download_metrics.url = url;
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = -1;
  download_metrics.downloaded_bytes = -1;
  download_metrics.total_bytes = -1;
  download_metrics.download_time_ms = 0;

  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                     base::Unretained(this), false, result, download_metrics));
}

void UrlFetcherDownloader::StartURLFetch(const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  SB_LOG(INFO) << "UrlFetcherDownloader::StartURLFetch: url" << url
               << " download_dir=" << download_dir_;
#endif

  if (download_dir_.empty()) {
#if defined(STARBOARD)
    SB_LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: failed with empty "
                     "download_dir";
#endif
    ReportDownloadFailure(url);
    return;
  }

  const auto file_path = download_dir_.AppendASCII(url.ExtractFileName());
  network_fetcher_ = network_fetcher_factory_->Create();
  network_fetcher_->DownloadToFile(
      url, file_path,
      base::BindOnce(&UrlFetcherDownloader::OnResponseStarted,
                     base::Unretained(this)),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress,
                          base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete,
                     base::Unretained(this)));

  download_start_time_ = base::TimeTicks::Now();
}

void UrlFetcherDownloader::OnNetworkFetcherComplete(base::FilePath file_path,
                                                    int net_error,
                                                    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const base::TimeTicks download_end_time(base::TimeTicks::Now());
  const base::TimeDelta download_time =
      download_end_time >= download_start_time_
          ? download_end_time - download_start_time_
          : base::TimeDelta();

  // Consider a 5xx response from the server as an indication to terminate
  // the request and avoid overloading the server in this case.
  // is not accepting requests for the moment.
  int error = -1;
#if defined(STARBOARD)
  if (!file_path.empty() && response_code_ == 200 && net_error == 0) {
#else
  if (!file_path.empty() && response_code_ == 200) {
#endif
    DCHECK_EQ(0, net_error);
    error = 0;
  } else if (response_code_ != -1) {
    error = response_code_;
  } else {
    error = net_error;
  }

  const bool is_handled = error == 0 || IsHttpServerError(error);

  Result result;
  result.error = error;
  if (!error) {
    result.response = file_path;
#if defined(STARBOARD)
    result.installation_index = cobalt_slot_management_.GetInstallationIndex();
#endif
  }

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = error;
  // Tests expected -1, in case of failures and no content is available.
  download_metrics.downloaded_bytes = error ? -1 : content_size;
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  VLOG(1) << "Downloaded " << content_size << " bytes in "
          << download_time.InMilliseconds() << "ms from " << final_url_.spec()
          << " to " << result.response.value();

#if !defined(STARBOARD)
  // Delete the download directory in the error cases.
  if (error && !download_dir_.empty())
    base::PostTaskWithTraits(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&base::DeleteFile), download_dir_, true));
#else
  if (error && !download_dir_.empty()) {
    // Cleanup the download dir.
    CleanupDirectory(download_dir_);
  }
#endif

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                                base::Unretained(this), is_handled, result,
                                download_metrics));
}

// This callback is used to indicate that a download has been started.
void UrlFetcherDownloader::OnResponseStarted(const GURL& final_url,
                                             int response_code,
                                             int64_t content_length) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "url fetcher response started for: " << final_url.spec();

  final_url_ = final_url;
  response_code_ = response_code;
  total_bytes_ = content_length;
}

void UrlFetcherDownloader::OnDownloadProgress(int64_t current) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CrxDownloader::OnDownloadProgress();
}

}  // namespace update_client
