// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/net_fetcher.h"

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/cors_preflight.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "cobalt/network/network_module.h"
#include "net/url_request/url_fetcher.h"
#if defined(STARBOARD)
#include "starboard/configuration.h"
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#define HANDLE_CORE_DUMP
#include "base/lazy_instance.h"
#include STARBOARD_CORE_DUMP_HANDLER_INCLUDE
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#endif  // defined(STARBOARD)

namespace cobalt {
namespace loader {

namespace {

#if defined(HANDLE_CORE_DUMP)

class NetFetcherLog {
 public:
  NetFetcherLog() : total_fetched_bytes_(0) {
    SbCoreDumpRegisterHandler(CoreDumpHandler, this);
  }
  ~NetFetcherLog() { SbCoreDumpUnregisterHandler(CoreDumpHandler, this); }

  static void CoreDumpHandler(void* context) {
    SbCoreDumpLogInteger(
        "NetFetcher total fetched bytes",
        static_cast<NetFetcherLog*>(context)->total_fetched_bytes_);
  }

  void IncrementFetchedBytes(int length) { total_fetched_bytes_ += length; }

 private:
  int total_fetched_bytes_;
  DISALLOW_COPY_AND_ASSIGN(NetFetcherLog);
};

base::LazyInstance<NetFetcherLog>::DestructorAtExit net_fetcher_log =
    LAZY_INSTANCE_INITIALIZER;

#endif  // defined(HANDLE_CORE_DUMP)

bool IsResponseCodeSuccess(int response_code) {
  // NetFetcher only considers success to be if the network request
  // was successful *and* we get a 2xx response back.
  // We also accept a response code of -1 for URLs where response code is not
  // meaningful, like "data:"
  return response_code == -1 || response_code / 100 == 2;
}
}  // namespace

NetFetcher::NetFetcher(const GURL& url,
                       const csp::SecurityCallback& security_callback,
                       Handler* handler,
                       const network::NetworkModule* network_module,
                       const Options& options, RequestMode request_mode,
                       const Origin& origin)
    : Fetcher(handler),
      security_callback_(security_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(start_callback_(
          base::Bind(&NetFetcher::Start, base::Unretained(this)))),
      request_cross_origin_(false),
      origin_(origin) {
  url_fetcher_ = net::URLFetcher::Create(url, options.request_method, this);
  url_fetcher_->SetRequestContext(
      network_module->url_request_context_getter().get());
  std::unique_ptr<URLFetcherStringWriter> download_data_writer(
      new URLFetcherStringWriter());
  url_fetcher_->SaveResponseWithWriter(std::move(download_data_writer));
  if (request_mode != kNoCORSMode && !url.SchemeIs("data") &&
      origin != Origin(url)) {
    request_cross_origin_ = true;
    url_fetcher_->AddExtraRequestHeader("Origin:" + origin.SerializedOrigin());
  }
  if ((request_cross_origin_ &&
       (request_mode == kCORSModeSameOriginCredentials)) ||
      request_mode == kCORSModeOmitCredentials) {
    const uint32 kDisableCookiesLoadFlags =
        net::LOAD_NORMAL | net::LOAD_DO_NOT_SAVE_COOKIES |
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
    url_fetcher_->SetLoadFlags(kDisableCookiesLoadFlags);
  }

  // Delay the actual start until this function is complete. Otherwise we might
  // call handler's callbacks at an unexpected time- e.g. receiving OnError()
  // while a loader is still being constructed.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE, start_callback_.callback());
}

void NetFetcher::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const GURL& original_url = url_fetcher_->GetOriginalURL();
  if (security_callback_.is_null() ||
      security_callback_.Run(original_url, false /* did not redirect */)) {
    url_fetcher_->Start();
  } else {
    std::string msg(base::StringPrintf("URL %s rejected by security policy.",
                                       original_url.spec().c_str()));
    return HandleError(msg).InvalidateThis();
  }
}

void NetFetcher::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (source->GetURL() != source->GetOriginalURL()) {
    // A redirect occured. Re-check the security policy.
    if (!security_callback_.is_null() &&
        !security_callback_.Run(source->GetURL(), true /* did redirect */)) {
      std::string msg(base::StringPrintf(
          "URL %s rejected by security policy after a redirect from %s.",
          source->GetURL().spec().c_str(),
          source->GetOriginalURL().spec().c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }

  if (IsResponseCodeSuccess(source->GetResponseCode())) {
    if (handler()->OnResponseStarted(this, source->GetResponseHeaders()) ==
        kLoadResponseAbort) {
      std::string msg(
          base::StringPrintf("Handler::OnResponseStarted aborted URL %s",
                             source->GetURL().spec().c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }

  // net::URLFetcher can not guarantee GetResponseHeaders() always return
  // non-null pointer.
  if (request_cross_origin_ &&
      (!source->GetResponseHeaders() ||
       !CORSPreflight::CORSCheck(*source->GetResponseHeaders(),
                                 origin_.SerializedOrigin(), false))) {
    std::string msg(base::StringPrintf(
        "Cross origin request to %s was rejected by Same-Origin-Policy",
        source->GetURL().spec().c_str()));
    return HandleError(msg).InvalidateThis();
  }

  last_url_origin_ = Origin(source->GetURL());
}

void NetFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const net::URLRequestStatus& status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  if (status.is_success() && IsResponseCodeSuccess(response_code)) {
    auto* download_data_writer =
        base::polymorphic_downcast<URLFetcherStringWriter*>(
            source->GetResponseWriter());
    std::string data;
    download_data_writer->GetAndResetData(&data);
    if (!data.empty()) {
      DLOG(INFO) << "in OnURLFetchComplete data still has bytes: "
                 << data.size();
      handler()->OnReceivedPassed(
          this, std::unique_ptr<std::string>(new std::string(std::move(data))));
    }
    handler()->OnDone(this);
  } else {
    // Check for response codes and errors that are considered transient. These
    // are the ones that net::URLFetcherCore is willing to attempt retries on,
    // along with ERR_NAME_RESOLUTION_FAILED, which indicates a socket error.
    if (response_code >= 500 ||
        status.error() == net::ERR_TEMPORARILY_THROTTLED ||
        status.error() == net::ERR_NETWORK_CHANGED ||
        status.error() == net::ERR_NAME_RESOLUTION_FAILED ||
        status.error() == net::ERR_CONNECTION_RESET ||
        status.error() == net::ERR_CONNECTION_CLOSED ||
        status.error() == net::ERR_CONNECTION_ABORTED) {
      did_fail_from_transient_error_ = true;
    }

    std::string msg(base::StringPrintf(
        "NetFetcher error on %s: %s, response code %d",
        source->GetURL().spec().c_str(),
        net::ErrorToString(status.error()).c_str(), response_code));
    return HandleError(msg).InvalidateThis();
  }
}

void NetFetcher::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                            int64_t current, int64_t total,
                                            int64_t current_network_bytes) {
  if (IsResponseCodeSuccess(source->GetResponseCode())) {
    auto* download_data_writer =
        base::polymorphic_downcast<URLFetcherStringWriter*>(
            source->GetResponseWriter());
    std::string data;
    download_data_writer->GetAndResetData(&data);
    if (data.empty()) {
      return;
    }
#if defined(HANDLE_CORE_DUMP)
    net_fetcher_log.Get().IncrementFetchedBytes(
        static_cast<int>(data.length()));
#endif
    handler()->OnReceivedPassed(
        this, std::unique_ptr<std::string>(new std::string(std::move(data))));
  }
}

void NetFetcher::ReportLoadTimingInfo(
    const net::LoadTimingInfo& timing_info) {
  handler()->SetLoadTimingInfo(timing_info);
}

NetFetcher::~NetFetcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  start_callback_.Cancel();
}

NetFetcher::ReturnWrapper NetFetcher::HandleError(const std::string& message) {
  url_fetcher_.reset();
  handler()->OnError(this, message);
  return ReturnWrapper();
}

}  // namespace loader
}  // namespace cobalt
