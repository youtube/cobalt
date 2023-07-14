// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "components/update_client/background_downloader_win.h"
#endif
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/url_fetcher_downloader.h"
#include "components/update_client/utils.h"

namespace update_client {

CrxDownloader::DownloadMetrics::DownloadMetrics()
    : downloader(kNone),
      error(0),
      downloaded_bytes(-1),
      total_bytes(-1),
      download_time_ms(0) {}

// On Windows, the first downloader in the chain is a background downloader,
// which uses the BITS service.
#if defined(STARBOARD)
std::unique_ptr<CrxDownloader> CrxDownloader::Create(
    bool is_background_download,
    scoped_refptr<Configurator> config) {
  std::unique_ptr<CrxDownloader> url_fetcher_downloader =
      std::make_unique<UrlFetcherDownloader>(nullptr, config);
#else
std::unique_ptr<CrxDownloader> CrxDownloader::Create(
    bool is_background_download,
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory) {
  std::unique_ptr<CrxDownloader> url_fetcher_downloader =
      std::make_unique<UrlFetcherDownloader>(nullptr, network_fetcher_factory);
#endif

#if defined(OS_WIN)
  if (is_background_download) {
    return std::make_unique<BackgroundDownloader>(
        std::move(url_fetcher_downloader));
  }
#endif

  return url_fetcher_downloader;
}

CrxDownloader::CrxDownloader(std::unique_ptr<CrxDownloader> successor)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      successor_(std::move(successor)) {
#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::CrxDownloader";
#endif
}

CrxDownloader::~CrxDownloader() {
#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::~CrxDownloader";
#endif
}

void CrxDownloader::set_progress_callback(
    const ProgressCallback& progress_callback) {
  progress_callback_ = progress_callback;
}

GURL CrxDownloader::url() const {
  return current_url_ != urls_.end() ? *current_url_ : GURL();
}

const std::vector<CrxDownloader::DownloadMetrics>
CrxDownloader::download_metrics() const {
  if (!successor_)
    return download_metrics_;

  std::vector<DownloadMetrics> retval(successor_->download_metrics());
  retval.insert(retval.begin(), download_metrics_.begin(),
                download_metrics_.end());
  return retval;
}

void CrxDownloader::StartDownloadFromUrl(const GURL& url,
                                         const std::string& expected_hash,
#if defined(IN_MEMORY_UPDATES)
                                         std::string* dst,
#endif
                                         DownloadCallback download_callback) {
#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::StartDownloadFromUrl: url=" << url;
#endif

  std::vector<GURL> urls;
  urls.push_back(url);
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst != nullptr);
  StartDownload(urls, expected_hash, dst, std::move(download_callback));
#else
  StartDownload(urls, expected_hash, std::move(download_callback));
#endif
}

void CrxDownloader::StartDownload(const std::vector<GURL>& urls,
                                  const std::string& expected_hash,
#if defined(IN_MEMORY_UPDATES)
                                  std::string* dst,
#endif
                                  DownloadCallback download_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst != nullptr);
#endif

  auto error = CrxDownloaderError::NONE;
  if (urls.empty()) {
    error = CrxDownloaderError::NO_URL;
  } else if (expected_hash.empty()) {
    error = CrxDownloaderError::NO_HASH;
  }

  if (error != CrxDownloaderError::NONE) {
    Result result;
    result.error = static_cast<int>(error);
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_callback), result));
    return;
  }

#if defined(IN_MEMORY_UPDATES)
  dst_str_ = dst;
#endif

  urls_ = urls;
  expected_hash_ = expected_hash;
  current_url_ = urls_.begin();
  download_callback_ = std::move(download_callback);

#if defined(IN_MEMORY_UPDATES)
  DoStartDownload(*current_url_, dst);
#else
  DoStartDownload(*current_url_);
#endif
}

#if defined(STARBOARD)
void CrxDownloader::CancelDownload() {
  LOG(INFO) << "CrxDownloader::CancelDownload";
  DoCancelDownload();
}
#endif

void CrxDownloader::OnDownloadComplete(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::OnDownloadComplete";
#endif
  if (!result.error)
    base::PostTaskWithTraits(
        FROM_HERE, kTaskTraits,
        base::BindOnce(&CrxDownloader::VerifyResponse, base::Unretained(this),
                       is_handled, result, download_metrics));
  else
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError,
                                  base::Unretained(this), is_handled, result,
                                  download_metrics));
}

void CrxDownloader::OnDownloadProgress() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (progress_callback_.is_null())
    return;

  progress_callback_.Run();
}

// The function mutates the values of the parameters |result| and
// |download_metrics|.
void CrxDownloader::VerifyResponse(bool is_handled,
                                   Result result,
                                   DownloadMetrics download_metrics) {
  DCHECK_EQ(0, result.error);
  DCHECK_EQ(0, download_metrics.error);
  DCHECK(is_handled);
#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::VerifyResponse";
#endif

#if defined(IN_MEMORY_UPDATES)
  if (VerifyHash256(dst_str_, expected_hash_)) {
#else
  if (VerifyFileHash256(result.response, expected_hash_)) {
#endif
    download_metrics_.push_back(download_metrics);
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_callback_), result));
    return;
  }

  // The download was successful but the response is not trusted. Clean up
  // the download, mutate the result, and try the remaining fallbacks when
  // handling the error.
  result.error = static_cast<int>(CrxDownloaderError::BAD_HASH);
  download_metrics.error = result.error;
#if defined(STARBOARD)
#if !defined(IN_MEMORY_UPDATES)
  base::DeleteFile(result.response, false);
#endif  // !defined(IN_MEMORY_UPDATES)
#else  // defined(STARBOARD)
  DeleteFileAndEmptyParentDirectory(result.response);
#endif  // defined(STARBOARD)

#if !defined(IN_MEMORY_UPDATES)
  result.response.clear();
#endif

  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError,
                                base::Unretained(this), is_handled, result,
                                download_metrics));
}

void CrxDownloader::HandleDownloadError(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(0, result.error);
#if !defined(IN_MEMORY_UPDATES)
  DCHECK(result.response.empty());
#endif
  DCHECK_NE(0, download_metrics.error);

#if defined(STARBOARD)
  LOG(INFO) << "CrxDownloader::HandleDownloadError";
#endif

  download_metrics_.push_back(download_metrics);

#if defined(STARBOARD)
  if (result.error != static_cast<int>(CrxDownloaderError::SLOT_UNAVAILABLE)) {
#endif

    // If an error has occured, try the next url if there is any,
    // or try the successor in the chain if there is any successor.
    // If this downloader has received a 5xx error for the current url,
    // as indicated by the |is_handled| flag, remove that url from the list of
    // urls so the url is never tried again down the chain.
    if (is_handled) {
      current_url_ = urls_.erase(current_url_);
    } else {
      ++current_url_;
    }

    // Try downloading from another url from the list.
    if (current_url_ != urls_.end()) {
#if defined(IN_MEMORY_UPDATES)
      // TODO(b/158043520): manually test that Cobalt can update using a
      // successor URL when an error occurs on the first URL, and/or consider
      // adding an Evergreen end-to-end test case for this behavior. This is
      // important since the unit tests are currently disabled (b/290410288).
      DoStartDownload(*current_url_, dst_str_);
#else
      DoStartDownload(*current_url_);
#endif
      return;
    }

    // Try downloading using the next downloader.
    if (successor_ && !urls_.empty()) {
#if defined(IN_MEMORY_UPDATES)
      successor_->StartDownload(urls_, expected_hash_, dst_str_,
                                std::move(download_callback_));
#else
      successor_->StartDownload(urls_, expected_hash_,
                                std::move(download_callback_));
#endif
      return;
    }

#if defined(STARBOARD)
  }
#endif

  // The download ends here since there is no url nor downloader to handle this
  // download request further.
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_callback_), result));
}

}  // namespace update_client
