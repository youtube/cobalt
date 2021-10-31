// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/updater/network_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/url_fetcher_string_writer.h"

namespace {

bool IsResponseCodeSuccess(int response_code) {
  // NetworkFetcher only considers success to be if the network request
  // was successful *and* we get a 2xx response back.
  return response_code / 100 == 2;
}

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cobalt_updater_network_fetcher",
                                        "cobalt_updater_network_fetcher");

// Returns the string value of a header of the server response or an empty
// string if the header is not available. Only the first header is returned
// if multiple instances of the same header are present.
std::string GetStringHeader(const net::HttpResponseHeaders* headers,
                            const char* header_name) {
  if (!headers) {
    return {};
  }

  std::string header_value;
  return headers->EnumerateHeader(nullptr, header_name, &header_value)
             ? header_value
             : std::string{};
}

// Returns the integral value of a header of the server response or -1 if
// if the header is not available or a conversion error has occured.
int64_t GetInt64Header(const net::HttpResponseHeaders* headers,
                       const char* header_name) {
  if (!headers) {
    return -1;
  }

  return headers->GetInt64HeaderValue(header_name);
}

}  // namespace

namespace cobalt {
namespace updater {

NetworkFetcher::NetworkFetcher(const network::NetworkModule* network_module)
    : network_module_(network_module) {}

NetworkFetcher::~NetworkFetcher() {}

void NetworkFetcher::PostRequest(
    const GURL& url, const std::string& post_data,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SB_LOG(INFO) << "PostRequest url = " << url;
  SB_LOG(INFO) << "PostRequest post_data = " << post_data;

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);
  post_request_complete_callback_ = std::move(post_request_complete_callback);

  CreateUrlFetcher(url, net::URLFetcher::POST);

  std::unique_ptr<loader::URLFetcherStringWriter> download_data_writer(
      new loader::URLFetcherStringWriter());
  url_fetcher_->SaveResponseWithWriter(std::move(download_data_writer));

  for (const auto& header : post_additional_headers) {
    url_fetcher_->AddExtraRequestHeader(header.first + ": " + header.second);
  }

  url_fetcher_->SetUploadData("application/json", post_data);

  url_fetcher_type_ = kUrlFetcherTypePostRequest;

  url_fetcher_->Start();
}

void NetworkFetcher::DownloadToFile(
    const GURL& url, const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SB_LOG(INFO) << "DownloadToFile url = " << url;
  SB_LOG(INFO) << "DownloadToFile file_path = " << file_path;

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);
  download_to_file_complete_callback_ =
      std::move(download_to_file_complete_callback);

  CreateUrlFetcher(url, net::URLFetcher::GET);

  url_fetcher_->SaveResponseToFileAtPath(
      file_path, base::SequencedTaskRunnerHandle::Get());

  url_fetcher_type_ = kUrlFetcherTypeDownloadToFile;

  url_fetcher_->Start();
}

void NetworkFetcher::CancelDownloadToFile() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SB_LOG(INFO) << "Canceling DownloadToFile";
  url_fetcher_.reset();
}

void NetworkFetcher::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(response_started_callback_)
      .Run(source->GetURL(), source->GetResponseCode(),
           source->GetResponseHeaders()
               ? source->GetResponseHeaders()->GetContentLength()
               : -1);
}

void NetworkFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const net::URLRequestStatus& status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  if (url_fetcher_type_ == kUrlFetcherTypePostRequest) {
    OnPostRequestComplete(source, status.error());
  } else if (url_fetcher_type_ == kUrlFetcherTypeDownloadToFile) {
    OnDownloadToFileComplete(source, status.error());
  }

  if (!status.is_success() || !IsResponseCodeSuccess(response_code)) {
    std::string msg(base::StringPrintf(
        "NetworkFetcher error on %s : %s, response code %d",
        source->GetURL().spec().c_str(),
        net::ErrorToString(status.error()).c_str(), response_code));
    return HandleError(msg).InvalidateThis();
  }
  url_fetcher_.reset();
}

void NetworkFetcher::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                                int64_t current, int64_t total,
                                                int64_t current_network_bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  progress_callback_.Run(current);
}

void NetworkFetcher::CreateUrlFetcher(
    const GURL& url, const net::URLFetcher::RequestType request_type) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  url_fetcher_ = net::URLFetcher::Create(url, request_type, this,
                                         kNetworkTrafficAnnotation);

  url_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter().get());

  // Request mode is kCORSModeOmitCredentials.
  const uint32 kDisableCookiesAndCacheLoadFlags =
      net::LOAD_NORMAL | net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA |
      net::LOAD_DISABLE_CACHE;
  url_fetcher_->SetLoadFlags(kDisableCookiesAndCacheLoadFlags);

  url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(
      kMaxRetriesOnNetworkChange);
}

void NetworkFetcher::OnPostRequestComplete(const net::URLFetcher* source,
                                           const int status_error) {
  std::unique_ptr<std::string> response_body(new std::string);
  auto* download_data_writer =
      base::polymorphic_downcast<loader::URLFetcherStringWriter*>(
          source->GetResponseWriter());
  if (download_data_writer) {
    download_data_writer->GetAndResetData(response_body.get());
  }

  if (response_body->empty()) {
    SB_LOG(ERROR) << "PostRequest got empty response.";
  }

  SB_LOG(INFO) << "OnPostRequestComplete response_body = "
               << *response_body.get();

  net::HttpResponseHeaders* response_headers = source->GetResponseHeaders();
  std::move(post_request_complete_callback_)
      .Run(std::move(response_body), status_error,
           GetStringHeader(response_headers,
                           update_client::NetworkFetcher::kHeaderEtag),
           GetInt64Header(response_headers,
                          update_client::NetworkFetcher::kHeaderXRetryAfter));
}

void NetworkFetcher::OnDownloadToFileComplete(const net::URLFetcher* source,
                                              const int status_error) {
  base::FilePath response_file;
  if (!source->GetResponseAsFilePath(true, &response_file)) {
    SB_LOG(ERROR) << "DownloadToFile failed to get response from a file";
  }
  SB_LOG(INFO) << "OnDownloadToFileComplete response_file = " << response_file;

  std::move(download_to_file_complete_callback_)
      .Run(response_file, status_error,
           source->GetResponseHeaders()
               ? source->GetResponseHeaders()->GetContentLength()
               : -1);
}

NetworkFetcher::ReturnWrapper NetworkFetcher::HandleError(
    const std::string& message) {
  url_fetcher_.reset();
  SB_LOG(ERROR) << message;
  return ReturnWrapper();
}

NetworkFetcherFactoryCobalt::NetworkFetcherFactoryCobalt(
    network::NetworkModule* network_module)
    : network_module_(network_module) {}

NetworkFetcherFactoryCobalt::~NetworkFetcherFactoryCobalt() = default;

std::unique_ptr<update_client::NetworkFetcher>
NetworkFetcherFactoryCobalt::Create() const {
  return std::make_unique<NetworkFetcher>(network_module_);
}

}  // namespace updater
}  // namespace cobalt
