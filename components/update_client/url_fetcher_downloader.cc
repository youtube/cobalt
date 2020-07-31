// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <stdint.h>
#include <stack>
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
#include "components/update_client/network.h"
#include "components/update_client/utils.h"

#if defined(OS_STARBOARD)
#include "cobalt/updater/utils.h"
#include "starboard/configuration_constants.h"
#include "starboard/loader_app/drain_file.h"
#endif

#include "url/gurl.h"

namespace {

#if defined(OS_STARBOARD)
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

UrlFetcherDownloader::UrlFetcherDownloader(
    std::unique_ptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {
#if defined(OS_STARBOARD)
  installation_api_ = static_cast<const CobaltExtensionInstallationManagerApi*>(
      SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
#endif
}

UrlFetcherDownloader::~UrlFetcherDownloader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

#if defined(OS_STARBOARD)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url) {
  SB_LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot " << url;
  if (!DrainFileRankAndCheck(download_dir_.value().c_str(), app_key_.c_str())) {
    SB_LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot: failed to lock slot ";
    ReportDownloadFailure(url, CrxDownloader::Error::CRX_DOWNLOADER_ABORT);
    return;
  }

  // TODO: Double check the installed_version.

  // Use the installation slot
  if (installation_api_->ResetInstallation(installation_index_) ==
      IM_EXT_ERROR) {
    SB_LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot: failed to reset slot ";
    ReportDownloadFailure(url);
    return;
  }
  // Remove all files and directories except for our ranking drain file.
  DrainFilePrepareDirectory(download_dir_.value().c_str(), app_key_.c_str());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::StartURLFetch,
                                base::Unretained(this), url));
}

void UrlFetcherDownloader::SelectSlot(const GURL& url) {
  SB_LOG(INFO) << "UrlFetcherDownloader::SelectSlot url=" << url;
  int max_slots = installation_api_->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get max number of slots";
    ReportDownloadFailure(url);
    return;
  }

  // default invalid version
  base::Version slot_candidate_version;
  int slot_candidate = -1;
  base::FilePath slot_candidate_path;

  // Iterate over all writeable slots - index >= 1.
  for (int i = 1; i < max_slots; i++) {
    SB_LOG(INFO) << "UrlFetcherDownloader::SelectSlot iterating slot=" << i;
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api_->GetInstallationPath(i, installation_path.data(),
                                               installation_path.size()) ==
        IM_EXT_ERROR) {
      SB_LOG(ERROR) << "UrlFetcherDownloader::SelectSlot: Failed to get "
                       "installation path for slot="
                    << i;
      continue;
    }

    SB_DLOG(INFO) << "UrlFetcherDownloader::SelectSlot: installation_path = "
                  << installation_path.data();

    base::FilePath installation_dir(
        std::string(installation_path.begin(), installation_path.end()));

    // Cleanup expired drain files.
    DrainFileClear(installation_dir.value().c_str(), app_key_.c_str(), true);

    // Cleanup all drain files from the current app.
    DrainFileRemove(installation_dir.value().c_str(), app_key_.c_str());
    base::Version version =
        cobalt::updater::ReadEvergreenVersion(installation_dir);
    if (!version.IsValid()) {
      SB_LOG(INFO)
          << "UrlFetcherDownloader::SelectSlot installed version invalid";
      if (!DrainFileDraining(installation_dir.value().c_str(), "")) {
        SB_LOG(INFO) << "UrlFetcherDownloader::SelectSlot not draining";
        // found empty slot
        slot_candidate = i;
        slot_candidate_path = installation_dir;
        break;
      } else {
        // There is active draining from another updater so bail out.
        SB_LOG(ERROR) << "UrlFetcherDownloader::SelectSlot bailing out";
        ReportDownloadFailure(url, CrxDownloader::Error::CRX_DOWNLOADER_ABORT);
        return;
      }
    } else if ((!slot_candidate_version.IsValid() ||
                slot_candidate_version > version)) {
      if (!DrainFileDraining(installation_dir.value().c_str(), "")) {
        // found a slot with older version that's not draining.
        SB_LOG(INFO) << "UrlFetcherDownloader::SelectSlot slot candidate: "
                     << i;
        slot_candidate_version = version;
        slot_candidate = i;
        slot_candidate_path = installation_dir;
      } else {
        SB_LOG(ERROR) << "UrlFetcherDownloader::SelectSlot bailing out";
        // There is active draining from another updater so bail out.
        ReportDownloadFailure(url, CrxDownloader::Error::CRX_DOWNLOADER_ABORT);
        return;
      }
    }
  }

  installation_index_ = slot_candidate;
  download_dir_ = slot_candidate_path;

  if (installation_index_ == -1 ||
      !DrainFileTryDrain(download_dir_.value().c_str(), app_key_.c_str())) {
    SB_LOG(ERROR)
        << "UrlFetcherDownloader::SelectSlot unable to find a slot, candidate="
        << installation_index_;
    ReportDownloadFailure(url);
    return;
  } else {
    // Use 15 sec delay to allow for other updaters/loaders to settle down.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UrlFetcherDownloader::ConfirmSlot,
                       base::Unretained(this), url),
        base::TimeDelta::FromSeconds(15));
  }
}
#endif

void UrlFetcherDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(OS_STARBOARD)
  SB_LOG(INFO) << "UrlFetcherDownloader::DoStartDownload url=" << url;
  // Make sure the index is reset
  installation_index_ = IM_EXT_INVALID_INDEX;
  if (!installation_api_) {
    SB_LOG(ERROR) << "Failed to get installation manager";
    ReportDownloadFailure(url);
    return;
  }

  char app_key[IM_EXT_MAX_APP_KEY_LENGTH];
  if (installation_api_->GetAppKey(app_key, IM_EXT_MAX_APP_KEY_LENGTH) ==
      IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get app key.";
    ReportDownloadFailure(url);
    return;
  }
  app_key_ = app_key;
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

void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}

#if defined(OS_STARBOARD)
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
  ReportDownloadFailure(url, CrxDownloader::Error::CRX_DOWNLOADER_RETRY);
}

void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url,
                                                 CrxDownloader::Error error) {
#else
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url) {
#endif
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(OS_STARBOARD)
  if (!download_dir_.empty() && !app_key_.empty()) {
    // Cleanup all drain files of the current app.
    DrainFileRemove(download_dir_.value().c_str(), app_key_.c_str());
  }
#endif
  Result result;
#if defined(OS_STARBOARD)
  result.error = error;
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

#if defined(OS_STARBOARD)
  SB_LOG(INFO) << "UrlFetcherDownloader::StartURLFetch: url" << url
               << " download_dir=" << download_dir_;
#endif

  if (download_dir_.empty()) {
#if defined(OS_STARBOARD)
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
  if (!file_path.empty() && response_code_ == 200) {
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
#if defined(OS_STARBOARD)
    result.installation_index = installation_index_;
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

#if !defined(OS_STARBOARD)
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
