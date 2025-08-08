// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/update_client/background_downloader_mac.h"

#import <Foundation/Foundation.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

// Callback invoked by DownloadDelegate when a download has finished.
using DelegateDownloadCompleteCallback = base::RepeatingCallback<
    void(const GURL&, const base::FilePath&, int, int64_t, int64_t)>;

// Callback invoked by DownloadDelegate when download metrics are available.
using DelegateMetricsCollectedCallback =
    base::RepeatingCallback<void(const GURL& url, uint64_t download_time_ms)>;
using OnDownloadCompleteCallback = update_client::
    BackgroundDownloaderSharedSession::OnDownloadCompleteCallback;

base::FilePath URLToFilename(const GURL& url) {
  uint32_t hash = base::PersistentHash(url.spec());
  return base::FilePath::FromASCII(
      base::HexEncode(reinterpret_cast<uint8_t*>(&hash), sizeof(hash)));
}

// The age at which unclaimed downloads should be evicted from the cache.
constexpr base::TimeDelta kMaxCachedDownloadAge = base::Days(2);

// These methods have been copied from //net/base/mac/url_conversions.h to
// avoid introducing a dependancy on //net.
NSURL* NSURLWithGURL(const GURL& url) {
  if (!url.is_valid()) {
    return nil;
  }

  // NSURL strictly enforces RFC 1738 which requires that certain characters
  // are always encoded. These characters are: "<", ">", """, "#", "%", "{",
  // "}", "|", "\", "^", "~", "[", "]", and "`".
  //
  // GURL leaves some of these characters unencoded in the path, query, and
  // ref. This function manually encodes those components, and then passes the
  // result to NSURL.
  GURL::Replacements replacements;
  std::string escaped_path = base::EscapeNSURLPrecursor(url.path());
  std::string escaped_query = base::EscapeNSURLPrecursor(url.query());
  std::string escaped_ref = base::EscapeNSURLPrecursor(url.ref());
  if (!escaped_path.empty()) {
    replacements.SetPathStr(escaped_path);
  }
  if (!escaped_query.empty()) {
    replacements.SetQueryStr(escaped_query);
  }
  if (!escaped_ref.empty()) {
    replacements.SetRefStr(escaped_ref);
  }
  GURL escaped_url = url.ReplaceComponents(replacements);

  NSString* escaped_url_string =
      [NSString stringWithUTF8String:escaped_url.spec().c_str()];
  return [NSURL URLWithString:escaped_url_string];
}

GURL GURLWithNSURL(NSURL* url) {
  if (url) {
    return GURL(url.absoluteString.UTF8String);
  }
  return GURL();
}

// Detects and removes old files from the download cache.
void CleanDownloadCache(const base::FilePath& download_cache) {
  base::FileEnumerator(download_cache, false, base::FileEnumerator::FILES)
      .ForEach([](const base::FilePath& download) {
        base::File::Info info;
        if (base::GetFileInfo(download, &info) &&
            base::Time::Now() - info.creation_time > kMaxCachedDownloadAge) {
          base::DeleteFile(download);
        }
      });
}

}  // namespace

@interface DownloadDelegate : NSObject <NSURLSessionDownloadDelegate>
- (instancetype)initWithDownloadCache:(base::FilePath)downloadCache
             downloadCompleteCallback:
                 (DelegateDownloadCompleteCallback)downloadCompleteCallback
             metricsCollectedCallback:
                 (DelegateMetricsCollectedCallback)metricsCollectedCallback;
@end

@implementation DownloadDelegate {
  base::FilePath _download_cache;
  DelegateDownloadCompleteCallback _download_complete_callback;
  DelegateMetricsCollectedCallback _metrics_collected_callback;
  scoped_refptr<base::SequencedTaskRunner> _callback_runner;
}

- (instancetype)initWithDownloadCache:(base::FilePath)downloadCache
             downloadCompleteCallback:
                 (DelegateDownloadCompleteCallback)downloadCompleteCallback
             metricsCollectedCallback:
                 (DelegateMetricsCollectedCallback)metricsCollectedCallback {
  if (self = [super init]) {
    _download_cache = downloadCache;
    _download_complete_callback = downloadCompleteCallback;
    _metrics_collected_callback = metricsCollectedCallback;
    _callback_runner = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return self;
}

- (void)endTask:(NSURLSessionTask*)task
    withLocation:(absl::optional<base::FilePath>)location
       withError:(int)error {
  _callback_runner->PostTask(
      FROM_HERE, base::BindOnce(_download_complete_callback,
                                GURLWithNSURL(task.originalRequest.URL),
                                location.value_or(base::FilePath()), error,
                                task.countOfBytesReceived,
                                task.countOfBytesExpectedToReceive));
}

#pragma mark - NSURLSessionDownloadDelegate

- (void)URLSession:(NSURLSession*)session
                 downloadTask:(NSURLSessionDownloadTask*)downloadTask
    didFinishDownloadingToURL:(NSURL*)location {
  if (!base::PathExists(_download_cache) &&
      !base::CreateDirectory(_download_cache)) {
    LOG(ERROR) << "Failed to create download cache directory at: "
               << _download_cache;
    [self endTask:downloadTask
        withLocation:absl::nullopt
           withError:static_cast<int>(update_client::CrxDownloaderError::
                                          MAC_BG_CANNOT_CREATE_DOWNLOAD_CACHE)];
    return;
  }

  const base::FilePath temp_path =
      base::apple::NSStringToFilePath(location.path);
  base::FilePath cache_path = _download_cache.Append(
      URLToFilename(GURLWithNSURL(downloadTask.originalRequest.URL)));
  if (!base::Move(temp_path, cache_path)) {
    DLOG(ERROR)
        << "Failed to move the downloaded file from the temporary location: "
        << temp_path << " to: " << cache_path;
    [self endTask:downloadTask
        withLocation:absl::nullopt
           withError:static_cast<int>(update_client::CrxDownloaderError::
                                          MAC_BG_MOVE_TO_CACHE_FAIL)];
    return;
  }

  [self endTask:downloadTask
      withLocation:cache_path
         withError:static_cast<int>(update_client::CrxDownloaderError::NONE)];
}

#pragma mark - NSURLSessionDelegate

- (void)URLSession:(NSURLSession*)session
                    task:(nonnull NSURLSessionTask*)task
    didCompleteWithError:(nullable NSError*)error {
  if (error) {
    [self endTask:task withLocation:absl::nullopt withError:error.code];
  }
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics {
  _callback_runner->PostTask(
      FROM_HERE, base::BindOnce(_metrics_collected_callback,
                                GURLWithNSURL(task.originalRequest.URL),
                                metrics.taskInterval.duration *
                                    base::TimeTicks::kMillisecondsPerSecond));
}

@end

namespace update_client {

class BackgroundDownloaderSharedSessionImpl {
 public:
  BackgroundDownloaderSharedSessionImpl(const base::FilePath& download_cache,
                                        const std::string& session_identifier)
      : download_cache_(download_cache) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DownloadDelegate* delegate = [[DownloadDelegate alloc]
           initWithDownloadCache:download_cache_
        downloadCompleteCallback:
            base::BindRepeating(
                [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl>
                       weak_this,
                   const GURL& url, const base::FilePath& location, int error,
                   int64_t downloaded_bytes, int64_t total_bytes) {
                  if (weak_this) {
                    weak_this->OnDownloadComplete(
                        url, location, error, downloaded_bytes, total_bytes);
                  }
                },
                weak_factory_.GetWeakPtr())
        metricsCollectedCallback:
            base::BindRepeating(
                [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl>
                       weak_this,
                   const GURL& url, uint64_t download_time_ms) {
                  if (weak_this) {
                    weak_this->OnMetricsCollected(url, download_time_ms);
                  }
                },
                weak_factory_.GetWeakPtr())];

    NSURLSessionConfiguration* config = [NSURLSessionConfiguration
        backgroundSessionConfigurationWithIdentifier:base::SysUTF8ToNSString(
                                                         session_identifier)];
    session_ = [NSURLSession sessionWithConfiguration:config
                                             delegate:delegate
                                        delegateQueue:nil];
  }

  ~BackgroundDownloaderSharedSessionImpl() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void DoStartDownload(const GURL& url, OnDownloadCompleteCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindRepeating(&CleanDownloadCache, download_cache_),
        base::Minutes(10));

    if (!session_) {
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.error =
          static_cast<int>(CrxDownloaderError::MAC_BG_SESSION_INVALIDATED);
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kImmediateError);
      callback.Run(false, {metrics.error, base::FilePath()}, metrics);
      return;
    }

    if (downloads_.contains(url)) {
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.error =
          static_cast<int>(CrxDownloaderError::MAC_BG_DUPLICATE_DOWNLOAD);
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kImmediateError);
      callback.Run(false, {metrics.error, base::FilePath()}, metrics);
      return;
    }

    if (HandleDownloadFromCache(url, callback)) {
      metrics::RecordBDMStartDownloadOutcome(
          metrics::BDMStartDownloadOutcome::kDownloadRecoveredFromCache);
      return;
    }

    downloads_.emplace(url, callback);
    HasOngoingDownload(
        url, base::BindOnce(
                 [](base::WeakPtr<BackgroundDownloaderSharedSessionImpl> impl,
                    const GURL& url, bool has_download) {
                   if (!impl) {
                     return;
                   }
                   metrics::RecordBDMStartDownloadOutcome(
                       has_download ? metrics::BDMStartDownloadOutcome::
                                          kSessionHasOngoingDownload
                                    : metrics::BDMStartDownloadOutcome::
                                          kNewDownloadTaskCreated);
                   if (!has_download) {
                     impl->CreateAndResumeDownloadTask(url);
                   }
                 },
                 weak_factory_.GetWeakPtr(), url));
  }

  void InvalidateAndCancel() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (session_) {
      [session_ invalidateAndCancel];
      session_ = nullptr;
    }
  }

 private:
  struct DownloadResult {
    DownloadResult(bool is_handled,
                   const CrxDownloader::Result& result,
                   const CrxDownloader::DownloadMetrics& download_metrics)
        : is_handled(is_handled),
          result(result),
          download_metrics(download_metrics) {}

    explicit DownloadResult(uint64_t download_time_ms) {
      download_metrics.download_time_ms = download_time_ms;
    }

    bool is_handled = false;
    CrxDownloader::Result result;
    CrxDownloader::DownloadMetrics download_metrics;
  };

  void CreateAndResumeDownloadTask(const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NSMutableURLRequest* urlRequest =
        [[NSMutableURLRequest alloc] initWithURL:NSURLWithGURL(url)];
    NSURLSessionDownloadTask* downloadTask =
        [session_ downloadTaskWithRequest:urlRequest];

    [downloadTask resume];
  }

  // Looks for a completed download in the cache. Returns false if the cache
  // does not contain `url`.
  bool HandleDownloadFromCache(const GURL& url,
                               OnDownloadCompleteCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::FilePath cached_path = download_cache_.Append(URLToFilename(url));
    if (!base::PathExists(cached_path)) {
      return false;
    }

    if (results_.contains(url)) {
      // The download was completed by this
      // BackgroundDownloaderSharedSessionImpl, thus the metrics are available.
      DownloadResult result = results_.at(url);
      callback.Run(result.is_handled, result.result, result.download_metrics);
    } else {
      int64_t download_size = -1;
      if (!base::GetFileSize(cached_path, &download_size)) {
        LOG(ERROR) << "Failed determine file size for " << cached_path;
      }
      CrxDownloader::DownloadMetrics metrics = GetDefaultMetrics(url);
      metrics.downloaded_bytes = download_size;
      metrics.total_bytes = download_size;
      callback.Run(true,
                   {static_cast<int>(CrxDownloaderError::NONE), cached_path},
                   metrics);
    }

    return true;
  }

  // Replies with true if the background session contains a non-canceled
  // download task for `url`.
  void HasOngoingDownload(const GURL& url,
                          base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(session_);
    // A copy of the URL is needed so that a reference is not captured by the
    // block.
    GURL url_for_block(url);
    scoped_refptr<base::SequencedTaskRunner> reply_sequence =
        base::SequencedTaskRunner::GetCurrentDefault();
    __block base::OnceCallback<void(bool)> block_scoped_callback =
        std::move(callback);
    [session_ getAllTasksWithCompletionHandler:^(
                  NSArray<__kindof NSURLSessionTask*>* _Nonnull tasks) {
      bool has_url = false;
      for (NSURLSessionTask* task in tasks) {
        if (url_for_block == GURLWithNSURL([task originalRequest].URL)) {
          // It has been observed that download tasks which have been
          // reassociated with this process via the recreation of a NSURLSession
          // with a background identifier report a state of
          // NSURLSessionTaskStateRunning but do not make progress.
          // Interestingly, calling resume on these tasks (which is documented
          // as having no effect on running tasks) seems to get things moving
          // again.
          [task resume];
          has_url = true;
          break;
        }
      }
      reply_sequence->PostTask(
          FROM_HERE, base::BindOnce(std::move(block_scoped_callback), has_url));
    }];
  }

  // Called by the delegate when the download has completed.
  void OnDownloadComplete(const GURL& url,
                          const base::FilePath& location,
                          int error,
                          int64_t downloaded_bytes,
                          int64_t total_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool had_result = results_.contains(url);
    bool is_handled = error == 0 || (error >= 500 && error < 600);
    CrxDownloader::Result result = {error, location};
    CrxDownloader::DownloadMetrics download_metrics =
        had_result ? results_.at(url).download_metrics
                   : CrxDownloader::DownloadMetrics{};
    download_metrics.url = url;
    download_metrics.downloader =
        update_client::CrxDownloader::DownloadMetrics::kBackgroundMac;
    download_metrics.error = error;
    download_metrics.downloaded_bytes = downloaded_bytes;
    download_metrics.total_bytes = total_bytes;
    results_.insert_or_assign(
        url, DownloadResult(is_handled, result, download_metrics));

    if (had_result) {
      OnDownloadResultReady(url);
    }
  }

  // Called by the delegate when the download has completed.
  void OnMetricsCollected(const GURL& url, uint64_t download_time_ms) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    bool had_result = results_.contains(url);
    DownloadResult result =
        had_result ? results_.at(url) : DownloadResult(download_time_ms);
    result.download_metrics.download_time_ms = download_time_ms;
    results_.insert_or_assign(url, std::move(result));

    if (had_result) {
      OnDownloadResultReady(url);
    }
  }

  // Called when both completion and metrics have been recorded for a download.
  // If the download was specifically requested via `DoStartDownload`,
  // completion is signaled to the caller. Otherwise, the result is stored until
  // the download is retrieved from cache.
  void OnDownloadResultReady(const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(results_.contains(url));

    bool requestor_known = downloads_.contains(url);
    metrics::RecordBDMResultRequestorKnown(requestor_known);
    if (requestor_known) {
      DownloadResult result = results_.at(url);
      downloads_.at(url).Run(result.is_handled, result.result,
                             result.download_metrics);
      results_.erase(url);
      downloads_.erase(url);
    }
  }

  // Returns a `CrxDownloader::DownloadMetrics` with url and downloader set.
  static CrxDownloader::DownloadMetrics GetDefaultMetrics(const GURL& url) {
    CrxDownloader::DownloadMetrics metrics;
    metrics.url = url;
    metrics.downloader =
        CrxDownloader::DownloadMetrics::Downloader::kBackgroundMac;
    return metrics;
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const base::FilePath download_cache_;
  NSURLSession* session_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the (possibly partial) results of a download.
  base::flat_map<GURL, DownloadResult> results_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks which downloads have been requested. This is used to notify the
  // CrxDownloader only of the downloads it has requested in the lifetime of
  // this BackgroundDownloaderSharedSessionImpl, as opposed to downloads which
  // were started by a previous BackgroundDownloaderSharedSessionImpl.
  base::flat_map<GURL, OnDownloadCompleteCallback> downloads_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<BackgroundDownloaderSharedSessionImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

BackgroundDownloader::BackgroundDownloader(
    scoped_refptr<CrxDownloader> successor,
    scoped_refptr<BackgroundDownloaderSharedSession> impl,
    scoped_refptr<base::SequencedTaskRunner> background_sequence_)
    : CrxDownloader(std::move(successor)),
      shared_session_(impl),
      background_sequence_(background_sequence_) {}

BackgroundDownloader::~BackgroundDownloader() = default;

base::OnceClosure BackgroundDownloader::DoStartDownload(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DoStartDownload(
      url, base::BindPostTaskToCurrentDefault(
               base::BindRepeating(&BackgroundDownloader::OnDownloadComplete,
                                   base::WrapRefCounted(this)),
               FROM_HERE));
}

base::OnceClosure BackgroundDownloader::DoStartDownload(
    const GURL& url,
    OnDownloadCompleteCallback on_download_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundDownloaderSharedSession::DoStartDownload,
                     shared_session_, url, on_download_complete_callback));
  return base::DoNothing();
}

// BackgroundDownloaderSharedSessionProxy manages an implementation bound to a
// background sequence.
class BackgroundDownloaderSharedSessionProxy
    : public BackgroundDownloaderSharedSession {
 public:
  BackgroundDownloaderSharedSessionProxy(
      scoped_refptr<base::SequencedTaskRunner> background_sequence,
      const base::FilePath& download_cache,
      const std::string& session_identifier)
      : impl_(background_sequence, download_cache, session_identifier) {}

  void DoStartDownload(
      const GURL& url,
      OnDownloadCompleteCallback on_download_complete_callback) override {
    impl_.AsyncCall(&BackgroundDownloaderSharedSessionImpl::DoStartDownload)
        .WithArgs(url, std::move(on_download_complete_callback));
  }

  void InvalidateAndCancel() override {
    impl_.AsyncCall(
        &BackgroundDownloaderSharedSessionImpl::InvalidateAndCancel);
  }

 private:
  ~BackgroundDownloaderSharedSessionProxy() override = default;

  base::SequenceBound<BackgroundDownloaderSharedSessionImpl> impl_;
};

scoped_refptr<BackgroundDownloaderSharedSession>
MakeBackgroundDownloaderSharedSession(
    scoped_refptr<base::SequencedTaskRunner> background_sequence,
    const base::FilePath& download_cache,
    const std::string& session_identifier) {
  return base::MakeRefCounted<BackgroundDownloaderSharedSessionProxy>(
      background_sequence, download_cache, session_identifier);
}

}  // namespace update_client
