// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader.h"

#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "components/update_client/background_downloader_win.h"
#endif
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "components/update_client/url_fetcher_downloader.h"
#include "components/update_client/utils.h"

#if BUILDFLAG(IS_STARBOARD)
#include "base/logging.h"

// TODO(b/448186580): Replace LOG with D(V)LOG
#endif

namespace update_client {

CrxDownloader::CrxDownloader(scoped_refptr<CrxDownloader> successor)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      successor_(std::move(successor)) {}

CrxDownloader::~CrxDownloader() = default;

void CrxDownloader::set_progress_callback(
    const ProgressCallback& progress_callback) {
  progress_callback_ = progress_callback;
}

GURL CrxDownloader::url() const {
  return current_url_ != urls_.end() ? *current_url_ : GURL();
}

const std::vector<CrxDownloader::DownloadMetrics>
CrxDownloader::download_metrics() const {
  if (!successor_) {
    return download_metrics_;
  }

  std::vector<DownloadMetrics> retval(successor_->download_metrics());
  retval.insert(retval.begin(), download_metrics_.begin(),
                download_metrics_.end());
  return retval;
}

base::OnceClosure CrxDownloader::StartDownloadFromUrl(
    const GURL& url,
    const std::string& expected_hash,
#if defined(IN_MEMORY_UPDATES)
    std::string* dst,
#endif                                        
    DownloadCallback download_callback) {
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "CrxDownloader::StartDownloadFromUrl: url=" << url;
#endif
  std::vector<GURL> urls;
  urls.push_back(url);
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst);
  return StartDownload(urls, expected_hash, dst, std::move(download_callback));
#else
  return StartDownload(urls, expected_hash, std::move(download_callback));
#endif
}

base::OnceClosure CrxDownloader::StartDownload(
    const std::vector<GURL>& urls,
    const std::string& expected_hash,
#if defined(IN_MEMORY_UPDATES)
    std::string* dst,
#endif                                  
    DownloadCallback download_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(IN_MEMORY_UPDATES)
  CHECK(dst);
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
    return base::DoNothing();
  }

#if defined(IN_MEMORY_UPDATES)
  dst_str_ = dst;
#endif

  urls_ = urls;
  expected_hash_ = expected_hash;
  current_url_ = urls_.begin();
  download_callback_ = std::move(download_callback);

#if defined(IN_MEMORY_UPDATES)
  return DoStartDownload(*current_url_, dst);
#else
  return DoStartDownload(*current_url_);
#endif
}

#if BUILDFLAG(IS_STARBOARD)
void CrxDownloader::CancelDownload() {
  LOG(INFO) << "CrxDownloader::CancelDownload";
  DoCancelDownload();
}
#endif

void CrxDownloader::OnDownloadComplete(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "CrxDownloader::OnDownloadComplete";
#endif

  // Release any references held by the progress callback, in case the
  // CrxDownloader outlives the receiver of the progress_callback. (This is
  // often the case in tests.)
  progress_callback_.Reset();

  metrics::RecordCRXDownloadComplete(result.error);
  if (result.error) {
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CrxDownloader::HandleDownloadError, this,
                                  is_handled, result, download_metrics));
    return;
  }

  CHECK_EQ(0, download_metrics.error);
  CHECK(is_handled);
#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "CrxDownloader::OnDownloadComplete, verifying response";
#endif

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
#if defined(IN_MEMORY_UPDATES)
          // Verifies the hash of a CRX package downloaded to a string in memory
          [](std::string* dst_str, const std::string& expected_hash) {
            if (VerifyHash256(dst_str, expected_hash))
#else      
          // Verifies the hash of a CRX file. Returns NONE or BAD_HASH if
          // the hash of the CRX does not match the |expected_hash|. The input
          // file is deleted in case of errors.
          [](const base::FilePath& filepath, const std::string& expected_hash) {
            if (VerifyFileHash256(filepath, expected_hash))
#endif
              return CrxDownloaderError::NONE;
#if BUILDFLAG(IS_STARBOARD)
#if !defined(IN_MEMORY_UPDATES)
            base::DeleteFile(filepath);
#endif  // !defined(IN_MEMORY_UPDATES)
#else  // BUILDFLAG(IS_STARBOARD)
            DeleteFileAndEmptyParentDirectory(filepath);
#endif  // BUILDFLAG(IS_STARBOARD)
            return CrxDownloaderError::BAD_HASH;
          },
#if defined(IN_MEMORY_UPDATES)
          dst_str_, expected_hash_),
#else
          result.response, expected_hash_),
#endif  
      base::BindOnce(
          // Handles CRX verification result, and retries the download from
          // a different URL if the verification fails.
          [](scoped_refptr<CrxDownloader> downloader, Result result,
             DownloadMetrics download_metrics, CrxDownloaderError error) {
            if (error == CrxDownloaderError::NONE) {
              downloader->download_metrics_.push_back(download_metrics);
              downloader->main_task_runner()->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(downloader->download_callback_),
                                 result));
              return;
            }
#if !defined(IN_MEMORY_UPDATES)
            result.response.clear();
#endif
            result.error = static_cast<int>(error);
            download_metrics.error = result.error;
            downloader->main_task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(&CrxDownloader::HandleDownloadError, downloader,
                               true, result, download_metrics));
          },
          scoped_refptr<CrxDownloader>(this), result, download_metrics));
}

void CrxDownloader::OnDownloadProgress(int64_t downloaded_bytes,
                                       int64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (progress_callback_.is_null()) {
    return;
  }

  progress_callback_.Run(downloaded_bytes, total_bytes);
}

void CrxDownloader::HandleDownloadError(
    bool is_handled,
    const Result& result,
    const DownloadMetrics& download_metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(0, result.error);
#if !defined(IN_MEMORY_UPDATES)
  CHECK(result.response.empty());
#endif
  CHECK_NE(0, download_metrics.error);

#if BUILDFLAG(IS_STARBOARD)
  LOG(INFO) << "CrxDownloader::HandleDownloadError";
#endif

  download_metrics_.push_back(download_metrics);

#if BUILDFLAG(IS_STARBOARD)
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

#if BUILDFLAG(IS_STARBOARD)
  }
#endif
  // The download ends here since there is no url nor downloader to handle this
  // download request further.
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_callback_), result));
}

}  // namespace update_client
