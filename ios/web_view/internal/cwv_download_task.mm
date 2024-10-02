// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_download_task_internal.h"

#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#import "ios/web/public/download/download_task.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web_view/internal/cwv_web_view_internal.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

int64_t const CWVDownloadSizeUnknown = -1;

NSErrorDomain const CWVDownloadErrorDomain =
    @"org.chromium.chromewebview.DownloadErrorDomain";

NSInteger const CWVDownloadErrorFailed = -100;
NSInteger const CWVDownloadErrorAborted = -101;

@interface CWVDownloadTask ()

// Called when the download task has started, downloaded a chunk of data or
// the download has been completed.
- (void)downloadWasUpdated;

@end

namespace {
// Bridges C++ observer method calls to Objective-C.
class DownloadTaskObserverBridge : public web::DownloadTaskObserver {
 public:
  explicit DownloadTaskObserverBridge(CWVDownloadTask* task) : task_(task) {}

  void OnDownloadUpdated(web::DownloadTask* task) override {
    [task_ downloadWasUpdated];
  }

 private:
  __weak CWVDownloadTask* task_ = nil;
};
}  // namespace

@implementation CWVDownloadTask {
  std::unique_ptr<DownloadTaskObserverBridge> _observerBridge;
  std::unique_ptr<web::DownloadTask> _internalTask;
}

@synthesize delegate = _delegate;

- (NSString*)suggestedFileName {
  return base::SysUTF8ToNSString(
      _internalTask->GenerateFileName().AsUTF8Unsafe());
}

- (NSString*)MIMEType {
  return base::SysUTF8ToNSString(_internalTask->GetMimeType());
}

- (NSURL*)originalURL {
  return net::NSURLWithGURL(_internalTask->GetOriginalUrl());
}

- (int64_t)totalBytes {
  return _internalTask->GetTotalBytes();
}

- (int64_t)receivedBytes {
  return _internalTask->GetReceivedBytes();
}

- (double)progress {
  int percent = _internalTask->GetPercentComplete();
  // percent == -1 means unknown.
  return percent == -1 ? NAN : percent / 100.0;
}

- (instancetype)initWithInternalTask:
    (std::unique_ptr<web::DownloadTask>)internalTask {
  self = [super init];
  if (self) {
    _observerBridge = std::make_unique<DownloadTaskObserverBridge>(self);
    _internalTask = std::move(internalTask);
    _internalTask->AddObserver(_observerBridge.get());
  }
  return self;
}

- (void)dealloc {
  _internalTask->RemoveObserver(_observerBridge.get());
}

- (void)startDownloadToLocalFileAtPath:(NSString*)path {
  _internalTask->Start(base::FilePath(base::SysNSStringToUTF8(path)));
}

- (void)cancel {
  _internalTask->Cancel();
}

#pragma mark - Private

- (void)downloadWasUpdated {
  switch (_internalTask->GetState()) {
    case web::DownloadTask::State::kInProgress: {
      if ([_delegate
              respondsToSelector:@selector(downloadTaskProgressDidChange:)]) {
        [_delegate downloadTaskProgressDidChange:self];
      }
      break;
    }
    case web::DownloadTask::State::kComplete:
    case web::DownloadTask::State::kFailed:
    case web::DownloadTask::State::kFailedNotResumable: {
      int errorCode = _internalTask->GetErrorCode();
      [self notifyFinishWithErrorCode:errorCode];
      break;
    }
    case web::DownloadTask::State::kNotStarted:
    case web::DownloadTask::State::kCancelled: {
      // Nothing to be done in these states.
      // Note that state kCancelled is immediately followed by state kComplete
      // with error code net::ERR_ABORTED, which is handled above.
      break;
    }
  }
}

- (void)notifyFinishWithErrorCode:(int)errorCode {
  NSError* error = nil;
  if (errorCode != net::OK) {
    // Use CWVDownloadErrorFailed for any errors other than net::ERR_ABORTED
    // because a detailed error code is likely not very useful. Text
    // representation of the error is still available via
    // error.localizedDescription.
    NSInteger cwvErrorCode = errorCode == net::ERR_ABORTED
                                 ? CWVDownloadErrorAborted
                                 : CWVDownloadErrorFailed;
    NSString* errorDescription =
        base::SysUTF8ToNSString(net::ErrorToShortString(errorCode));
    error = [NSError
        errorWithDomain:CWVDownloadErrorDomain
                   code:cwvErrorCode
               userInfo:@{NSLocalizedDescriptionKey : errorDescription}];
  }
  if ([_delegate
          respondsToSelector:@selector(downloadTask:didFinishWithError:)]) {
    [_delegate downloadTask:self didFinishWithError:error];
  }
}

@end
