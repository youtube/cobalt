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
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/loader/cors_preflight.h"
#include "cobalt/loader/fetch_interceptor_coordinator.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "cobalt/network/custom/url_fetcher.h"
#include "cobalt/network/network_module.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
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

// Returns true if |mime_type| is allowed for script-like destinations.
// See:
// https://fetch.spec.whatwg.org/#request-destination-script-like
// https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-mime-type?
bool AllowMimeTypeAsScript(const std::string& mime_type) {
  if (net::MatchesMimeType("audio/*", mime_type)) return false;
  if (net::MatchesMimeType("image/*", mime_type)) return false;
  if (net::MatchesMimeType("video/*", mime_type)) return false;
  if (net::MatchesMimeType("text/csv", mime_type)) return false;
  return true;
}

}  // namespace

NetFetcher::NetFetcher(const GURL& url, bool main_resource,
                       const csp::SecurityCallback& security_callback,
                       Handler* handler,
                       const network::NetworkModule* network_module,
                       const Options& options, RequestMode request_mode,
                       const Origin& origin)
    : Fetcher(handler),
      security_callback_(security_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(start_callback_(
          base::Bind(&NetFetcher::Start, base::Unretained(this)))),
      cors_policy_(network_module->network_delegate()->cors_policy()),
      request_cross_origin_(false),
      origin_(origin),
      request_script_(options.resource_type ==
                      network::disk_cache::kUncompiledScript),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      skip_fetch_intercept_(options.skip_fetch_intercept),
      will_destroy_current_message_loop_(false),
      main_resource_(main_resource) {
  url_fetcher_ = net::URLFetcher::Create(url, options.request_method, this);
  if (!options.headers.IsEmpty()) {
    url_fetcher_->SetExtraRequestHeaders(options.headers.ToString());
  }
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
#ifndef USE_HACKY_COBALT_CHANGES
  std::string content_type =
      std::string(net::HttpRequestHeaders::kResourceType) + ":" +
      std::to_string(options.resource_type);
  url_fetcher_->AddExtraRequestHeader(content_type);
  if ((request_cross_origin_ &&
       (request_mode == kCORSModeSameOriginCredentials)) ||
      request_mode == kCORSModeOmitCredentials) {
    const uint32 kDisableCookiesLoadFlags =
        net::LOAD_NORMAL | net::LOAD_DO_NOT_SAVE_COOKIES |
        net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
    url_fetcher_->SetLoadFlags(kDisableCookiesLoadFlags);
  }
#endif
  network_module->AddClientHintHeaders(*url_fetcher_, network::kCallTypeLoader);

  // Delay the actual start until this function is complete. Otherwise we might
  // call handler's callbacks at an unexpected time- e.g. receiving OnError()
  // while a loader is still being constructed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                start_callback_.callback());
  base::MessageLoop::current()->AddDestructionObserver(this);
}

void NetFetcher::WillDestroyCurrentMessageLoop() {
  will_destroy_current_message_loop_.store(true);
}

void NetFetcher::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const GURL& original_url = url_fetcher_->GetOriginalURL();
  TRACE_EVENT1("cobalt::loader", "NetFetcher::Start", "url",
               original_url.spec());
  if (security_callback_.is_null() ||
      security_callback_.Run(original_url, false /* did not redirect */)) {
    if (skip_fetch_intercept_) {
      url_fetcher_->Start();
      return;
    }
    FetchInterceptorCoordinator::GetInstance()->TryIntercept(
        original_url, main_resource_, url_fetcher_->GetRequestHeaders(),
        task_runner_,
        base::BindOnce(&NetFetcher::OnFetchIntercepted, AsWeakPtr()),
        base::BindOnce(&NetFetcher::ReportLoadTimingInfo, AsWeakPtr()),
        base::BindOnce(&NetFetcher::InterceptFallback, AsWeakPtr()));

  } else {
    std::string msg(base::StringPrintf("URL %s rejected by security policy.",
                                       original_url.spec().c_str()));
    return HandleError(msg).InvalidateThis();
  }
}

void NetFetcher::InterceptFallback() { url_fetcher_->Start(); }

void NetFetcher::OnFetchIntercepted(std::unique_ptr<std::string> body) {
  if (will_destroy_current_message_loop_.load()) {
    return;
  }
  if (task_runner_ != base::ThreadTaskRunnerHandle::Get()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&NetFetcher::OnFetchIntercepted,
                                  base::Unretained(this), std::move(body)));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!body || body->empty()) {
    url_fetcher_->Start();
    return;
  }
  handler()->OnReceivedPassed(this, std::make_unique<std::string>(*body));
  handler()->OnDone(this);
}

void NetFetcher::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT1("cobalt::loader", "NetFetcher::OnURLFetchResponseStarted", "url",
               url_fetcher_->GetOriginalURL().spec());
  if (source->GetURL() != source->GetOriginalURL()) {
    // A redirect occurred. Re-check the security policy.
    if (!security_callback_.is_null() &&
        !security_callback_.Run(source->GetURL(), true /* did redirect */)) {
      std::string msg(base::StringPrintf(
          "URL %s rejected by security policy after a redirect from %s.",
          source->GetURL().spec().c_str(),
          source->GetOriginalURL().spec().c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }

  if ((handler()->OnResponseStarted(this, source->GetResponseHeaders()) ==
       kLoadResponseAbort) ||
      (!IsResponseCodeSuccess(source->GetResponseCode()))) {
    std::string msg(base::StringPrintf("URL %s aborted or failed with code %d",
                                       source->GetURL().spec().c_str(),
                                       source->GetResponseCode()));
    return HandleError(msg).InvalidateThis();
  }

  // net::URLFetcher can not guarantee GetResponseHeaders() always return
  // non-null pointer.
  if (request_cross_origin_ &&
      (!source->GetResponseHeaders() ||
       !CORSPreflight::CORSCheck(*source->GetResponseHeaders(),
                                 origin_.SerializedOrigin(), false,
                                 cors_policy_))) {
    std::string msg(base::StringPrintf(
        "Cross origin request to %s was rejected by Same-Origin-Policy",
        source->GetURL().spec().c_str()));
    return HandleError(msg).InvalidateThis();
  }

  if (request_script_ && source->GetResponseHeaders()) {
    std::string mime_type;
    if (source->GetResponseHeaders()->GetMimeType(&mime_type) &&
        !AllowMimeTypeAsScript(mime_type)) {
      std::string msg(base::StringPrintf(
          "Refused to execute script from '%s' because its MIME type ('%s')"
          " is not executable.",
          source->GetURL().spec().c_str(), mime_type.c_str()));
      return HandleError(msg).InvalidateThis();
    }
  }

  last_url_origin_ = Origin(source->GetURL());
}

void NetFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT1("cobalt::loader", "NetFetcher::OnURLFetchComplete", "url",
               url_fetcher_->GetOriginalURL().spec());
  const net::Error status = source->GetStatus();
  const int response_code = source->GetResponseCode();
  if (status == net::Error::OK && IsResponseCodeSuccess(response_code)) {
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
    if (response_code >= 500 || status == net::ERR_TEMPORARILY_THROTTLED ||
        status == net::ERR_NETWORK_CHANGED ||
        status == net::ERR_NAME_RESOLUTION_FAILED ||
        status == net::ERR_CONNECTION_RESET ||
        status == net::ERR_CONNECTION_CLOSED ||
        status == net::ERR_CONNECTION_ABORTED) {
      did_fail_from_transient_error_ = true;
    }

    std::string msg(
        base::StringPrintf("NetFetcher error on %s: %s, response code %d",
                           source->GetURL().spec().c_str(),
                           net::ErrorToString(status).c_str(), response_code));
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

void NetFetcher::ReportLoadTimingInfo(const net::LoadTimingInfo& timing_info) {
  handler()->SetLoadTimingInfo(timing_info);
}

NetFetcher::~NetFetcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  start_callback_.Cancel();
  if (!will_destroy_current_message_loop_.load()) {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }
}

NetFetcher::ReturnWrapper NetFetcher::HandleError(const std::string& message) {
  url_fetcher_.reset();
  handler()->OnError(this, message);
  return ReturnWrapper();
}

}  // namespace loader
}  // namespace cobalt
