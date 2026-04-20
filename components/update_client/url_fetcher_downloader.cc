// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/url_fetcher_downloader.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_STARBOARD)
#include <stack>
#include "base/files/file_enumerator.h"
#include "base/notreached.h"
#endif

// TODO(b/449223391): Document the usage of base::Unretained()
// TODO(b/448186580): Replace LOG with D(V)LOG

namespace {

#if BUILDFLAG(IS_STARBOARD)
// Can't simply use base::DeletePathRecursively() because the empty dirs
// need to be preserved.
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
      unlink(path.value().c_str());
    }
  }
  while (!directories.empty()) {
    rmdir(directories.top().c_str());
    directories.pop();
  }
}

#endif

}  // namespace

namespace update_client {

#if BUILDFLAG(IS_STARBOARD)
UrlFetcherDownloader::UrlFetcherDownloader(
    scoped_refptr<CrxDownloader> successor,
    scoped_refptr<Configurator> config)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(config->GetNetworkFetcherFactory()),
      config_(std::move(config)) {
  DLOG(INFO) << "UrlFetcherDownloader::UrlFetcherDownloader";
}
#else
UrlFetcherDownloader::UrlFetcherDownloader(
    scoped_refptr<CrxDownloader> successor,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory)
    : CrxDownloader(std::move(successor)),
      network_fetcher_factory_(network_fetcher_factory) {}
#endif

UrlFetcherDownloader::~UrlFetcherDownloader() = default;


#if BUILDFLAG(IS_STARBOARD)
#if defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url, std::string* dst) {
#else  // defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::ConfirmSlot(const GURL& url) {
#endif  // defined(IN_MEMORY_UPDATES)
  LOG(INFO) << "UrlFetcherDownloader::ConfirmSlot: url=" << url;
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::ConfirmSlot: Download already cancelled";
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
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

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
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
  // Use 15 sec delay to allow for other updaters/loaders to settle down.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&UrlFetcherDownloader::ConfirmSlot, base::Unretained(this),
#if defined(IN_MEMORY_UPDATES)
                     url, dst),
#else  // defined(IN_MEMORY_UPDATES)
                     url),
#endif  // defined(IN_MEMORY_UPDATES)
      base::Seconds(15));
}
#endif  // BUILDFLAG(IS_STARBOARD)

#if defined(IN_MEMORY_UPDATES)
base::OnceClosure UrlFetcherDownloader::DoStartDownload(const GURL& url, std::string* dst) {
#else  // defined(IN_MEMORY_UPDATES)
base::OnceClosure UrlFetcherDownloader::DoStartDownload(const GURL& url) {
#endif  // defined(IN_MEMORY_UPDATES)
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "UrlFetcherDownloader::DoStartDownload";
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::DoStartDownload: Download already cancelled";
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
    return base::BindOnce(&UrlFetcherDownloader::Cancel, this);
  }
  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    LOG(ERROR) << "Failed to get installation manager";
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
    return base::BindOnce(&UrlFetcherDownloader::Cancel, this);
  }
  if (!cobalt_slot_management_.Init(installation_api)) {
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
    return base::BindOnce(&UrlFetcherDownloader::Cancel, this);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::SelectSlot,
#if defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url, dst));
#else  // defined(IN_MEMORY_UPDATES)
                                base::Unretained(this), url));
#endif  // defined(IN_MEMORY_UPDATES)
#else  // BUILDFLAG(IS_STARBOARD)
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, kTaskTraits,
      base::BindOnce(&UrlFetcherDownloader::CreateDownloadDir, this),
      base::BindOnce(&UrlFetcherDownloader::StartURLFetch, this, url));
#endif  // BUILDFLAG(IS_STARBOARD)
  return base::BindOnce(&UrlFetcherDownloader::Cancel, this);
}

#if BUILDFLAG(IS_STARBOARD)
void UrlFetcherDownloader::DoCancelDownload() {
  LOG(INFO) << "UrlFetcherDownloader::DoCancelDownload";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_cancelled_ = true;
  // TODO(b/431862767): enable this in a follow-up PR with the Cobalt network fetcher implementation
  NOTIMPLEMENTED();
  // if (network_fetcher_.get()) {
  //   network_fetcher_->Cancel();
  // }
}
#endif

#if !defined(IN_MEMORY_UPDATES)
void UrlFetcherDownloader::CreateDownloadDir() {
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome_url_fetcher_"),
                               &download_dir_);
}
#endif

#if BUILDFLAG(IS_STARBOARD)
void UrlFetcherDownloader::ReportDownloadFailure(const GURL& url,
                                                 CrxDownloaderError error) {
  LOG(INFO) << "UrlFetcherDownloader::ReportDownloadFailure";                                                
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cobalt_slot_management_.CleanupAllDrainFiles();
  Result result;
  result.error = static_cast<int>(error);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst != nullptr);
#endif

  LOG(INFO) << "UrlFetcherDownloader::StartURLFetch: url" << url
#if defined(IN_MEMORY_UPDATES)
               << " installation_dir=" << installation_dir_;
#else  // defined(IN_MEMORY_UPDATES)
               << " download_dir=" << download_dir_;
#endif  // defined(IN_MEMORY_UPDATES)
  if (is_cancelled_) {
    LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: Download already cancelled";
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
    return;
  }

#if defined(IN_MEMORY_UPDATES)
  if (installation_dir_.empty()) {
    LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: failed with empty installation_dir";
#else
  if (download_dir_.empty()) {
    LOG(ERROR) << "UrlFetcherDownloader::StartURLFetch: failed with empty download_dir";
#endif  // defined(IN_MEMORY_UPDATES)
    ReportDownloadFailure(url, CrxDownloaderError::GENERIC_ERROR);
    return;
  }

  network_fetcher_ = network_fetcher_factory_->Create();
#if defined(IN_MEMORY_UPDATES)
  network_fetcher_->DownloadToString(
      url,
      dst,
      base::BindRepeating(&UrlFetcherDownloader::OnResponseStarted, this),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress, this),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete, this));
#else
  file_path_ = download_dir_.AppendASCII(url.ExtractFileName());
  network_fetcher_->DownloadToFile(
      url, file_path_,
      base::BindRepeating(&UrlFetcherDownloader::OnResponseStarted, this),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress, this),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete, this));
#endif

  download_start_time_ = base::TimeTicks::Now();
}

#else
void UrlFetcherDownloader::StartURLFetch(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cancelled_ || download_dir_.empty()) {
    Result result;
    result.error =
        static_cast<int>(cancelled_ ? CrxDownloaderError::CANCELLED
                                    : CrxDownloaderError::NO_DOWNLOAD_DIR);

    DownloadMetrics download_metrics;
    download_metrics.url = url;
    download_metrics.downloader = DownloadMetrics::kUrlFetcher;
    download_metrics.error = -1;
    download_metrics.downloaded_bytes = -1;
    download_metrics.total_bytes = -1;
    download_metrics.download_time_ms = 0;

    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete,
                                  this, false, result, download_metrics));
    return;
  }

  file_path_ = download_dir_.AppendUTF8(url.ExtractFileName());
  network_fetcher_ = network_fetcher_factory_->Create();
  cancel_callback_ = network_fetcher_->DownloadToFile(
      url, file_path_,
      base::BindRepeating(&UrlFetcherDownloader::OnResponseStarted, this),
      base::BindRepeating(&UrlFetcherDownloader::OnDownloadProgress, this),
      base::BindOnce(&UrlFetcherDownloader::OnNetworkFetcherComplete, this));

  download_start_time_ = base::TimeTicks::Now();
}
#endif // BUILDFLAG(IS_STARBOARD)

void UrlFetcherDownloader::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelled_ = true;
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
}

void UrlFetcherDownloader::OnNetworkFetcherComplete(int net_error,
                                                    int64_t content_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_STARBOARD)
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
  int extra_code1 = 0;
#if BUILDFLAG(IS_STARBOARD) && !defined(IN_MEMORY_UPDATES)
  if (!file_path_.empty() && !net_error && response_code_ == 200) {
#else
  if (!net_error && response_code_ == 200) {
#endif
    error = 0;
  } else if (response_code_ != -1) {
    error = response_code_;
    extra_code1 = net_error;
  } else {
    error = net_error;
  }

  const bool is_handled = error == 0 || IsHttpServerError(error);

  Result result;
  result.error = error;
  result.extra_code1 = extra_code1;
  if (!error) {
#if defined(IN_MEMORY_UPDATES)
    result.installation_dir = installation_dir_;
#else
    result.response = file_path_;
#endif
#if BUILDFLAG(IS_STARBOARD)
    result.installation_index = cobalt_slot_management_.GetInstallationIndex();
#endif
  }

  DownloadMetrics download_metrics;
  download_metrics.url = url();
  download_metrics.downloader = DownloadMetrics::kUrlFetcher;
  download_metrics.error = error;
  download_metrics.extra_code1 = extra_code1;
  // Tests expected -1, in case of failures and no content is available.
  download_metrics.downloaded_bytes = error ? -1 : content_size;
  download_metrics.total_bytes = total_bytes_;
  download_metrics.download_time_ms = download_time.InMilliseconds();

  VLOG(1) << "Downloaded " << content_size << " bytes in "
          << download_time.InMilliseconds() << "ms from " << url().spec()
#if defined(IN_MEMORY_UPDATES)
          << " to string";
#else
          << " to " << result.response.value();
#endif

#if !defined(IN_MEMORY_UPDATES)
#if !BUILDFLAG(IS_STARBOARD)
  // Delete the download directory in the error cases.
  if (error && !download_dir_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, kTaskTraits,
        base::BindOnce(IgnoreResult(&RetryDeletePathRecursively),
                       download_dir_));
    }
#else  // BUILDFLAG(IS_STARBOARD)
  if (error && !download_dir_.empty()) {
    // Cleanup the download dir.
    CleanupDirectory(download_dir_);
  }
#endif  // BUILDFLAG(IS_STARBOARD)
#endif  // !defined(IN_MEMORY_UPDATES)                      

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UrlFetcherDownloader::OnDownloadComplete, this,
                                is_handled, result, download_metrics));
  network_fetcher_ = nullptr;
}

// This callback is used to indicate that a download has been started.
void UrlFetcherDownloader::OnResponseStarted(int response_code,
                                             int64_t content_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "url fetcher response started for: " << url().spec();

  response_code_ = response_code;
  total_bytes_ = content_length;
}

void UrlFetcherDownloader::OnDownloadProgress(int64_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CrxDownloader::OnDownloadProgress(current, total_bytes_);
}

}  // namespace update_client
