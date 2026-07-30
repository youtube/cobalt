// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/web/public/download/download_controller_delegate.h"

namespace web {
class DownloadController;
class DownloadTask;
class WebState;
}  // namespace web

// Keyed Service which acts as web::DownloadController delegate and routes
// download tasks to the appropriate TabHelper for download.
class BrowserDownloadService : public KeyedService,
                               public web::DownloadControllerDelegate {
 public:
  explicit BrowserDownloadService(web::DownloadController* download_controller);

  BrowserDownloadService(const BrowserDownloadService&) = delete;
  BrowserDownloadService& operator=(const BrowserDownloadService&) = delete;

  ~BrowserDownloadService() override;

  // Returns whether all downloads (both to the local filesystem and to Google
  // Drive) should be restricted. This is more permissive than
  // `ShouldRestrictLocalDownloads` because a download might still be allowed
  // if it can be saved to Google Drive, even if local downloads are restricted.
  static bool ShouldRestrictAllDownloads(web::WebState* web_state);

  // Returns whether downloading to the local filesystem is restricted by
  // policy. Unlike `ShouldRestrictAllDownloads`, this does not check if the
  // download can be saved to Google Drive.
  static bool ShouldRestrictLocalDownloads(web::WebState* web_state);

 private:
  // web::DownloadControllerDelegate overrides:
  void OnDownloadCreated(web::DownloadController*,
                         web::WebState*,
                         std::unique_ptr<web::DownloadTask>) override;
  void OnDownloadControllerDestroyed(web::DownloadController*) override;

  raw_ptr<web::DownloadController> download_controller_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_BROWSER_DOWNLOAD_SERVICE_H_
