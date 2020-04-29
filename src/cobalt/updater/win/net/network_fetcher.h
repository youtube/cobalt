// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_
#define CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_

#include <windows.h>

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace updater {

class NetworkFetcherWinHTTP;

class NetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  explicit NetworkFetcher(const HINTERNET& session_handle_);
  ~NetworkFetcher() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      ProgressCallback progress_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override;

 private:
  THREAD_CHECKER(thread_checker_);

  void PostRequestComplete();
  void DownloadToFileComplete();

  scoped_refptr<NetworkFetcherWinHTTP> network_fetcher_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DownloadToFileCompleteCallback download_to_file_complete_callback_;
  PostRequestCompleteCallback post_request_complete_callback_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcher);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NETWORK_FETCHER_H_
