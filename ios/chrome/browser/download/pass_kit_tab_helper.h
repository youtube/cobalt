// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/web_state_user_data.h"

@protocol WebContentCommands;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// Key of the UMA Download.IOSDownloadPassKitResult histogram.
extern const char kUmaDownloadPassKitResult[];

// Enum for the Download.IOSDownloadPassKitResult UMA histogram to report the
// results of the PassKit download.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
enum class DownloadPassKitResult {
  Successful = 0,
  // PassKit download failed for a reason other than wrong MIME type or 401/403
  // HTTP response.
  OtherFailure,
  // PassKit download failed due to either a 401 or 403 HTTP response.
  UnauthorizedFailure,
  // PassKit download did not download the correct MIME type. This can happen
  // when web server redirects to login page instead of returning PassKit data.
  WrongMimeTypeFailure,
  Count
};

// TabHelper which downloads pkpass file, constructs PKPass object and passes
// that PKPass to the delegate.
class PassKitTabHelper : public web::WebStateUserData<PassKitTabHelper>,
                         public web::DownloadTaskObserver {
 public:
  PassKitTabHelper(const PassKitTabHelper&) = delete;
  PassKitTabHelper& operator=(const PassKitTabHelper&) = delete;

  ~PassKitTabHelper() override;

  // Asynchronously downloads pkpass file using the given `task`. Asks delegate
  // to present "Add pkpass" dialog when the download is complete.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

  // Set the web content handler, used to display the passkit UI.
  void SetWebContentsHandler(id<WebContentCommands> handler);

 protected:
  // Allow subclassing from PassKitTabHelper for testing purposes.
  explicit PassKitTabHelper(web::WebState* web_state);

 private:
  friend class web::WebStateUserData<PassKitTabHelper>;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Called when the downloaded data is available.
  void OnDownloadDataRead(std::unique_ptr<web::DownloadTask> task,
                          NSData* data);

  web::WebState* web_state_;
  __weak id<WebContentCommands> handler_ = nil;
  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  base::WeakPtrFactory<PassKitTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_PASS_KIT_TAB_HELPER_H_
