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
      network_fetcher_factory_(config->GetNetworkFetcherFactory()) {
  LOG(INFO) << "UrlFetcherDownloader::UrlFetcherDownloader";
}
#else
UrlFetcherDownloader::UrlFetcherDownloader(
    std::unique_ptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {}
#endif

UrlFetcherDownloader::~UrlFetcherDownloader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::~UrlFetcherDownloader";
#endif
}

#if defined(STARBOARD)
#if defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url, std::string* dst) {
#else  // defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url) {
#endif  // defined(IN_MEMORY_UPDATES)
  LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot: url=" << url;
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::ConfirmSlot: Download already cancelled";
    ReportDownloadFailure(url);
    return;
  }
#if defined(IN_MEMORY_UPDATES)
  if (!cobalt_slot_management_.ConfirmSlot(installation_dir_)) {
#else  // defined(IN_MEMORY_UPDATES)
  if (!cobalt_slot_management_.ConfirmSlot(download_dir_)) {
#endif  // defined(IN_MEMORY_UPDATES)
    ReportDownloadFailure(url, CrxDownloaderError::SLOT_UNAVAILABLE);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
#if defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url, dst));
#else  // defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url));
#endif  // defined(IN_MEMORY_UPDATES)
}

#if defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::SelectSlot(const GURL& url, std::string* dst) {
#else  // defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::SelectSlot(const GURL& url) {
#endif  // defined(IN_MEMORY_UPDATES)
  LOG(INFO) << "UrlFetcherDownloader::SelectSlot: url=" << url;
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::SelectSlot: Download already cancelled";
    ReportDownloadFailure(url);
    return;
  }
#if defined(IN_MEMORY_UPDATES)
  if (!cobalt_slot_management_.SelectSlot(&installation_dir_)) {
#else  // defined(IN_MEMORY_UPDATES)
  if (!cobalt_slot_management_.SelectSlot(&download_dir_)) {
#endif  // defined(IN_MEMORY_UPDATES)
    ReportDownloadFailure(url, CrxDownloaderError::SLOT_UNAVAILABLE);
    return;
  }
  config_->SetUpdaterStatus(std::string(
      updater_status_string_map.find(UpdaterStatus::kSlotLocked)->second));

  // Use 15 sec delay to allow for other updaters/loaders to settle down.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UrlFetcherDownloader::ConfirmSlot, base::Unretained(this),
#if defined(IN_MEMORY_UPDATES)
                     url, dst),
#else  // defined(IN_MEMORY_UPDATES)
                     url),
#endif  // defined(IN_MEMORY_UPDATES)
      base::TimeDelta::FromSeconds(15));
}
#endif  // defined(STARBOARD)

#if defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::DoStartDownload(const GURL& url, std::string* dst) {
#else  // defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
#endif  // defined(IN_MEMORY_UPDATES)
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::DoStartDownload";
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::DoStartDownload: Download already cancelled";
    ReportDownloadFailure(url);
    return;
  }
  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    LOG(ERROR) << "Failed to get installation manager";
    ReportDownloadFailure(url);
    return;
  }
  if (!cobalt_slot_management_.Init(installation_api)) {
    ReportDownloadFailure(url);
    return;
  }
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::SelectSlot,
#if defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url, dst));
#else  // defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url));
#endif  // defined(IN_MEMORY_UPDATES)
#else  // defined(STARBOARD)
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UrlFetcherDownloader::CreateDownloadDir,
                     base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
                     base::Unretained(this), url));
#endif  // defined(STARBOARD)
}

#if defined(STARBOARD)
void UrlFetcherDownloader::DoCancelDownload() {
  LOG(INFO) << "UrlFetcherDownloader::DoCancelDownload";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_cancelled_ = true;
  if (network_fetcher_.get()) {
    network_fetcher_->Cancel();
  }
}
#endif

#if !defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}
#endif

#if defined(STARBOARD)
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
  LOG(INFO) << "UrlFetcherDownloader::ReportDownloadFailure";
  ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
}

void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url,
                                                 CrxDownloaderError error) {
#else
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(STARBOARD)
  cobalt_slot_management_.CleanupAllDrainFiles();
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

#if defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::StartURLFetch(const GURL& url, std::string* dst) {
#else
void UrlFetcherDownloader::StartURLFetch(const GURL& url) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst != nullptr);
#endif

#if defined(STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::StartURLFetch: url" << url
#if defined(IN_MEMORY_UPDATES)
               << " installation_dir=" << installation_dir_;
#else  // defined(IN_MEMORY_UPDATES)
               << " download_dir=" << download_dir_;
#endif  // defined(IN_MEMORY_UPDATES)
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: Download already cancelled";
    ReportDownloadFailure(url);
    return;
  }
#endif  // defined(STARBOARD)

#if defined(IN_MEMORY_UPDATES)
  if (installation_dir_.empty()) {
#else
  if (download_dir_.empty()) {
#endif
#if defined(STARBOARD)
    LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: failed with empty "
#if defined(IN_MEMORY_UPDATES)
               << "installation_dir";
#else  // defined(IN_MEMORY_UPDATES)
               << "download_dir";
#endif  // defined(IN_MEMORY_UPDATES)
#endif  // defined(STARBOARD)
    ReportDownloadFailure(url);
    return;
  }

  network_fetcher_ = network_fetcher_factory_->Create();
#if defined(IN_MEMORY_UPDATES)
  network_fetcher_->DownloadToString(
      url,
      dst,
      base::BindOnce(&UrlFetcherDownloader::OnResponseStarted,
                     base::Unretained(this)),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress,
                          base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete,
                     base::Unretained(this)));
#else
  const auto file_path = download_dir_.AppendASCII(url.ExtractFileName());
  network_fetcher_->DownloadToFile(
      url, file_path,
      base::BindOnce(&UrlFetcherDownloader::OnResponseStarted,
                     base::Unretained(this)),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress,
                          base::Unretained(this)),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete,
                     base::Unretained(this)));
#endif

  download_start_time_ = base::TimeTicks::Now();
}

void UrlFetcherDownloader::OnNetworkFetcherComplete(
#if defined(IN_MEMORY_UPDATES)
    std::string* dst,
#else
    base::FilePath file_path,
#endif
    int net_error,
    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::OnNetworkFetcherComplete";
#endif

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
#if defined(IN_MEMORY_UPDATES)
  if (response_code_ == 200 && net_error == 0) {
#else  // defined(IN_MEMORY_UPDATES)
  if (!file_path.empty() && response_code_ == 200 && net_error == 0) {
#endif  // defined(IN_MEMORY_UPDATES)
#else  // defined(STARBOARD)
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
#if defined(IN_MEMORY_UPDATES)
    result.installation_dir = installation_dir_;
#else
    result.response = file_path;
#endif
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
#if defined(IN_MEMORY_UPDATES)
          << " to string";
#else
          << " to " << result.response.value();
#endif

#if !defined(IN_MEMORY_UPDATES)
#if !defined(STARBOARD)
  // Delete the download directory in the error cases.
  if (error && !download_dir_.empty())
    base::PostTaskWithTraits(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&base::DeleteFile), download_dir_, true));
#else  // defined(STARBOARD)
  if (error && !download_dir_.empty()) {
    // Cleanup the download dir.
    CleanupDirectory(download_dir_);
  }
#endif  // defined(STARBOARD)
#endif  // !defined(IN_MEMORY_UPDATES)

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

#if defined(STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::OnResponseStarted";
#endif

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
