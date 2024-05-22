// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/net/network_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "chrome/updater/win/net/network.h"
#include "chrome/updater/win/net/network_winhttp.h"

namespace updater {

NetworkFetcher::NetworkFetcher(const HINTERNET& session_handle)
    : network_fetcher_(
          base::MakeRefCounted<NetworkFetcherWinHTTP>(session_handle)),
      main_thread_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
}

NetworkFetcher::~NetworkFetcher() {
  network_fetcher_->Close();
}

void NetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  post_request_complete_callback_ = std::move(post_request_complete_callback);
  network_fetcher_->PostRequest(
      url, post_data, post_additional_headers,
      std::move(response_started_callback), std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::PostRequestComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);
  network_fetcher_->DownloadToFile(
      url, file_path, std::move(response_started_callback),
      std::move(progress_callback),
      base::BindOnce(&NetworkFetcher::DownloadToFileComplete,
                     base::Unretained(this)));
}

void NetworkFetcher::PostRequestComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(post_request_complete_callback_)
      .Run(std::make_unique<std::string>(network_fetcher_->GetResponseBody()),
           network_fetcher_->GetNetError(), network_fetcher_->GetHeaderETag(),
           network_fetcher_->GetXHeaderRetryAfterSec());
}

void NetworkFetcher::DownloadToFileComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(download_to_file_complete_callback_)
      .Run(network_fetcher_->GetFilePath(), network_fetcher_->GetNetError(),
           network_fetcher_->GetContentSize());
}

NetworkFetcherFactory::NetworkFetcherFactory()
    : session_handle_(CreateSessionHandle()) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

scoped_hinternet NetworkFetcherFactory::CreateSessionHandle() {
  const auto* os_info = base::win::OSInfo::GetInstance();
  const uint32_t access_type = os_info->version() >= base::win::Version::WIN8_1
                                   ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
                                   : WINHTTP_ACCESS_TYPE_NO_PROXY;
  return scoped_hinternet(
      ::WinHttpOpen(L"Chrome Updater", access_type, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));
}

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return session_handle_.get()
             ? std::make_unique<NetworkFetcher>(session_handle_.get())
             : nullptr;
}

}  // namespace updater
