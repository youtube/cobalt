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

#include "cobalt/xhr/xml_http_request.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/progress_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/dom_parser/xml_decoder.h"
#include "cobalt/loader/cors_preflight.h"
#include "cobalt/loader/fetch_interceptor_coordinator.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/web/context.h"
#include "cobalt/web/csp_delegate.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/xhr/global_stats.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"

namespace cobalt {
namespace xhr {

using web::DOMException;

namespace {

// How many milliseconds must elapse between each progress event notification.
const int kProgressPeriodMs = 50;

const char* kResponseTypes[] = {
    "",             // kDefault
    "text",         // kText
    "json",         // kJson
    "document",     // kDocument
    "blob",         // kBlob
    "arraybuffer",  // kArrayBuffer
};

const char* kForbiddenMethods[] = {
    "connect",
    "trace",
    "track",
};

// https://www.w3.org/TR/resource-timing-1/#dom-performanceresourcetiming-initiatortype
const char* kPerformanceResourceTimingInitiatorType = "xmlhttprequest";

bool MethodNameToRequestType(const std::string& method,
                             net::URLFetcher::RequestType* request_type) {
  if (base::EqualsCaseInsensitiveASCII(method, "get")) {
    *request_type = net::URLFetcher::GET;
  } else if (base::EqualsCaseInsensitiveASCII(method, "post")) {
    *request_type = net::URLFetcher::POST;
  } else if (base::EqualsCaseInsensitiveASCII(method, "head")) {
    *request_type = net::URLFetcher::HEAD;
  } else if (base::EqualsCaseInsensitiveASCII(method, "delete")) {
    *request_type = net::URLFetcher::DELETE_REQUEST;
  } else if (base::EqualsCaseInsensitiveASCII(method, "put")) {
    *request_type = net::URLFetcher::PUT;
  } else {
    return false;
  }
  return true;
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
const char* kStateNames[] = {"Unsent", "Opened", "HeadersReceived", "Loading",
                             "Done"};
const char* kMethodNames[] = {"GET", "POST", "HEAD", "DELETE", "PUT"};

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#endif
const char* RequestTypeToMethodName(net::URLFetcher::RequestType request_type) {
  if (request_type >= 0 && request_type < arraysize(kMethodNames)) {
    return kMethodNames[request_type];
  } else {
    NOTREACHED();
    return "";
  }
}

const char* StateName(XMLHttpRequest::State state) {
  if (state >= 0 && state < arraysize(kStateNames)) {
    return kStateNames[state];
  } else {
    NOTREACHED();
    return "";
  }
}
#if __clang__
#pragma clang diagnostic push
#endif
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

bool IsForbiddenMethod(const std::string& method) {
  for (size_t i = 0; i < arraysize(kForbiddenMethods); ++i) {
    if (base::EqualsCaseInsensitiveASCII(method, kForbiddenMethods[i])) {
      return true;
    }
  }
  return false;
}

base_token::Token RequestErrorTypeName(XMLHttpRequest::RequestErrorType type) {
  switch (type) {
    case XMLHttpRequest::kNetworkError:
      return base::Tokens::error();
    case XMLHttpRequest::kTimeoutError:
      return base::Tokens::timeout();
    case XMLHttpRequest::kAbortError:
      return base::Tokens::abort();
  }
  NOTREACHED();
  return base_token::Token();
}

void FireProgressEvent(XMLHttpRequestEventTarget* target,
                       base_token::Token event_name) {
  if (!target) {
    return;
  }
  target->DispatchEvent(new dom::ProgressEvent(event_name));
}

void FireProgressEvent(XMLHttpRequestEventTarget* target,
                       base_token::Token event_name, uint64 loaded,
                       uint64 total, bool length_computable) {
  if (!target) {
    return;
  }
  target->DispatchEvent(
      new dom::ProgressEvent(event_name, loaded, total, length_computable));
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
int s_xhr_sequence_num_ = 0;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
// https://fetch.spec.whatwg.org/#concept-http-redirect-fetch
// 5. If request's redirect count is twenty, return a network error.
const int kRedirectLimit = 20;

std::string ClipUrl(const GURL& url, size_t length) {
  const std::string& spec = url.possibly_invalid_spec();
  if (spec.size() < length) {
    return spec;
  }

  size_t remain = length - 5;
  size_t head = remain / 2;
  size_t tail = remain - head;

  return spec.substr(0, head) + "[...]" + spec.substr(spec.size() - tail);
}
}  // namespace

bool XMLHttpRequestImpl::verbose_ = false;

XMLHttpRequest::XMLHttpRequest(script::EnvironmentSettings* settings)
    : XMLHttpRequestEventTarget(settings) {
  // Determine which implementation of XHR to use based on being in a window or
  // worker.
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      environment_settings()->context()->GetWindowOrWorkerGlobalScope();
  if (window_or_worker_global_scope->IsWindow()) {
    xhr_impl_ = std::make_unique<DOMXMLHttpRequestImpl>(this);
  } else if (window_or_worker_global_scope->IsDedicatedWorker() ||
             window_or_worker_global_scope->IsServiceWorker()) {
    xhr_impl_ = std::make_unique<XMLHttpRequestImpl>(this);
  }
  xhr::GlobalStats::GetInstance()->Add(this);
#if !defined(COBALT_BUILD_TYPE_GOLD)
  xhr_id_ = ++s_xhr_sequence_num_;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
}

XMLHttpRequest::~XMLHttpRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  xhr::GlobalStats::GetInstance()->Remove(this);
}

void XMLHttpRequest::Abort() { xhr_impl_->Abort(); }
void XMLHttpRequest::Open(const std::string& method, const std::string& url,
                          bool async,
                          const base::Optional<std::string>& username,
                          const base::Optional<std::string>& password,
                          script::ExceptionState* exception_state) {
  xhr_impl_->Open(method, url, async, username, password, exception_state);
}

// Must be called after open(), but before send().
void XMLHttpRequest::SetRequestHeader(const std::string& header,
                                      const std::string& value,
                                      script::ExceptionState* exception_state) {
  xhr_impl_->SetRequestHeader(header, value, exception_state);
}

// Override the MIME type returned by the server.
// Call before Send(), otherwise throws InvalidStateError.
void XMLHttpRequest::OverrideMimeType(const std::string& mime_type,
                                      script::ExceptionState* exception_state) {
  xhr_impl_->OverrideMimeType(mime_type, exception_state);
}

void XMLHttpRequest::Send(const base::Optional<RequestBodyType>& request_body,
                          script::ExceptionState* exception_state) {
  xhr_impl_->Send(request_body, exception_state);
}

void XMLHttpRequest::Fetch(const FetchUpdateCallbackArg& fetch_callback,
                           const FetchModeCallbackArg& fetch_mode_callback,
                           const base::Optional<RequestBodyType>& request_body,
                           script::ExceptionState* exception_state) {
  xhr_impl_->Fetch(fetch_callback, fetch_mode_callback, request_body,
                   exception_state);
}

base::Optional<std::string> XMLHttpRequest::GetResponseHeader(
    const std::string& header) {
  return xhr_impl_->GetResponseHeader(header);
}
std::string XMLHttpRequest::GetAllResponseHeaders() {
  return xhr_impl_->GetAllResponseHeaders();
}

const std::string& XMLHttpRequest::response_text(
    script::ExceptionState* exception_state) {
  return xhr_impl_->response_text(exception_state);
}
scoped_refptr<dom::Document> XMLHttpRequest::response_xml(
    script::ExceptionState* exception_state) {
  return xhr_impl_->response_xml(exception_state);
}
base::Optional<XMLHttpRequest::ResponseType> XMLHttpRequest::response(
    script::ExceptionState* exception_state) {
  return xhr_impl_->response(exception_state);
}

int XMLHttpRequest::ready_state() const { return xhr_impl_->ready_state(); }
int XMLHttpRequest::status() const { return xhr_impl_->status(); }
std::string XMLHttpRequest::status_text() { return xhr_impl_->status_text(); }
void XMLHttpRequest::set_response_type(
    const std::string& response_type, script::ExceptionState* exception_state) {
  xhr_impl_->set_response_type(response_type, exception_state);
}
std::string XMLHttpRequest::response_type(
    script::ExceptionState* exception_state) const {
  return xhr_impl_->response_type(exception_state);
}

uint32 XMLHttpRequest::timeout() const { return xhr_impl_->timeout(); }
void XMLHttpRequest::set_timeout(uint32 timeout) {
  xhr_impl_->set_timeout(timeout);
}
bool XMLHttpRequest::with_credentials(
    script::ExceptionState* exception_state) const {
  return xhr_impl_->with_credentials(exception_state);
}
void XMLHttpRequest::set_with_credentials(
    bool b, script::ExceptionState* exception_state) {
  xhr_impl_->set_with_credentials(b, exception_state);
}

scoped_refptr<XMLHttpRequestUpload> XMLHttpRequest::upload() {
  return xhr_impl_->upload();
}

void XMLHttpRequest::set_verbose(bool verbose) {
  XMLHttpRequestImpl::set_verbose(verbose);
}
bool XMLHttpRequest::verbose() { return XMLHttpRequestImpl::verbose(); }

// net::URLFetcherDelegate interface
void XMLHttpRequest::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  xhr_impl_->OnURLFetchResponseStarted(source);
}
void XMLHttpRequest::OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                                int64_t current, int64_t total,
                                                int64_t current_network_bytes) {
  xhr_impl_->OnURLFetchDownloadProgress(source, current, total,
                                        current_network_bytes,
                                        /* request_done = */ false);
}
void XMLHttpRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  xhr_impl_->OnURLFetchComplete(source);
}

void XMLHttpRequest::OnURLFetchUploadProgress(const net::URLFetcher* source,
                                              int64 current, int64 total) {
  xhr_impl_->OnURLFetchUploadProgress(source, current, total);
}
void XMLHttpRequest::OnRedirect(const net::HttpResponseHeaders& headers) {
  xhr_impl_->OnRedirect(headers);
}

// Called from bindings layer to tie objects' lifetimes to this XHR instance.
XMLHttpRequestUpload* XMLHttpRequest::upload_or_null() {
  return xhr_impl_->upload_or_null();
}

void XMLHttpRequest::ReportLoadTimingInfo(
    const net::LoadTimingInfo& timing_info) {
  xhr_impl_->ReportLoadTimingInfo(timing_info);
}
// Create Performance Resource Timing entry for XMLHttpRequest.
void XMLHttpRequest::GetLoadTimingInfoAndCreateResourceTiming() {
  xhr_impl_->GetLoadTimingInfoAndCreateResourceTiming();
}

void XMLHttpRequest::TraceMembers(script::Tracer* tracer) {
  XMLHttpRequestEventTarget::TraceMembers(tracer);
  xhr_impl_->TraceMembers(tracer);
}

XMLHttpRequestImpl::XMLHttpRequestImpl(XMLHttpRequest* xhr)
    : error_(false),
      is_cross_origin_(false),
      cors_policy_(xhr->environment_settings()
                       ->context()
                       ->fetcher_factory()
                       ->network_module()
                       ->network_delegate()
                       ->cors_policy()),
      is_data_url_(false),
      is_redirect_(false),
      method_(net::URLFetcher::GET),
      response_body_(new URLFetcherResponseWriter::Buffer(
          URLFetcherResponseWriter::Buffer::kString,
          xhr->environment_settings()
              ->context()
              ->web_settings()
              ->xhr_settings()
              .IsTryLockForProgressCheckEnabled())),
      response_type_(XMLHttpRequest::kDefault),
      state_(XMLHttpRequest::kUnsent),
      upload_listener_(false),
      with_credentials_(false),
      xhr_(xhr),
      will_destroy_current_message_loop_(false),
      active_requests_count_(0),
      http_status_(0),
      redirect_times_(0),
      sent_(false),
      settings_(xhr->environment_settings()),
      stop_timeout_(false),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      timeout_ms_(0),
      upload_complete_(false) {
  DCHECK(environment_settings());
  base::MessageLoop::current()->AddDestructionObserver(this);
}

void XMLHttpRequestImpl::Abort() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-abort()-method
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cancel any in-flight request and set error flag.
  TerminateRequest();
  bool abort_is_no_op = state_ == XMLHttpRequest::kUnsent ||
                        state_ == XMLHttpRequest::kDone ||
                        (state_ == XMLHttpRequest::kOpened && !sent_);
  if (!abort_is_no_op) {
    sent_ = false;
    HandleRequestError(XMLHttpRequest::kAbortError);
  }
  ChangeState(XMLHttpRequest::kUnsent);

  response_body_->Clear();
  response_array_buffer_reference_.reset();
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-open()-method
void XMLHttpRequestImpl::Open(const std::string& method, const std::string& url,
                              bool async,
                              const base::Optional<std::string>& username,
                              const base::Optional<std::string>& password,
                              script::ExceptionState* exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  XMLHttpRequest::State previous_state = state_;

  // Cancel any outstanding request.
  TerminateRequest();
  state_ = XMLHttpRequest::kUnsent;

  if (!async) {
    DLOG(ERROR) << "synchronous XHR is not supported";
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  base_url_ = environment_settings()->base_url();

  if (IsForbiddenMethod(method)) {
    web::DOMException::Raise(web::DOMException::kSecurityErr, exception_state);
    return;
  }

  if (!MethodNameToRequestType(method, &method_)) {
    web::DOMException::Raise(web::DOMException::kSyntaxErr, exception_state);
    return;
  }

  request_url_ = base_url_.Resolve(url);
  if (!request_url_.is_valid()) {
    web::DOMException::Raise(web::DOMException::kSyntaxErr, exception_state);
    return;
  }

  web::CspDelegate* csp = csp_delegate();
  if (csp && !csp->CanLoad(web::CspDelegate::kXhr, request_url_, false)) {
    web::DOMException::Raise(web::DOMException::kSecurityErr, exception_state);
    return;
  }

  sent_ = false;
  stop_timeout_ = false;

  PrepareForNewRequest();

  // Check previous state to avoid dispatching readyState event when calling
  // open several times in a row.
  if (previous_state != XMLHttpRequest::kOpened) {
    ChangeState(XMLHttpRequest::kOpened);
  } else {
    state_ = XMLHttpRequest::kOpened;
  }
}

void XMLHttpRequestImpl::SetRequestHeader(
    const std::string& header, const std::string& value,
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-setrequestheader
  if (state_ != XMLHttpRequest::kOpened || sent_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  if (!net::HttpUtil::IsValidHeaderName(header) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    DLOG(WARNING) << "Rejecting invalid header " << header << " : " << value;
    return;
  }

  if (!net::HttpUtil::IsSafeHeader(header, value)) {
    DLOG(WARNING) << "Rejecting unsafe header " << header;
    return;
  }

  // Write the header if it is not set.
  // If it is, append it to the existing one.
  std::string cur_value;
  if (request_headers_.GetHeader(header, &cur_value)) {
    cur_value += ", " + value;
    request_headers_.SetHeader(header, cur_value);
  } else {
    request_headers_.SetHeader(header, value);
  }
}

void XMLHttpRequestImpl::OverrideMimeType(
    const std::string& override_mime, script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-overridemimetype
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (state_ == XMLHttpRequest::kLoading || state_ == XMLHttpRequest::kDone) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  // Try to parse the given override. If it fails, throw an exception.
  // Otherwise, we'll replace the content-type header in the response headers
  // once we have them.
  std::string mime_type;
  std::string charset;
  bool had_charset = false;
  net::HttpUtil::ParseContentType(override_mime, &mime_type, &charset,
                                  &had_charset, NULL);
  if (!mime_type.length()) {
    web::DOMException::Raise(web::DOMException::kSyntaxErr, exception_state);
    return;
  }
  mime_type_override_ = mime_type;
}

void XMLHttpRequestImpl::Send(
    const base::Optional<XMLHttpRequest::RequestBodyType>& request_body,
    script::ExceptionState* exception_state) {
  error_ = false;
  bool in_service_worker = environment_settings()
                               ->context()
                               ->GetWindowOrWorkerGlobalScope()
                               ->IsServiceWorker();
  if (!in_service_worker && method_ == net::URLFetcher::GET) {
    loader::FetchInterceptorCoordinator::GetInstance()->TryIntercept(
        request_url_, /*main_resource=*/false, request_headers_, task_runner_,
        base::BindOnce(&XMLHttpRequestImpl::SendIntercepted, AsWeakPtr()),
        base::BindOnce(&XMLHttpRequestImpl::ReportLoadTimingInfo, AsWeakPtr()),
        base::BindOnce(&XMLHttpRequestImpl::SendFallback, AsWeakPtr(),
                       request_body, exception_state));
    return;
  }
  SendFallback(request_body, exception_state);
}

void XMLHttpRequestImpl::SendIntercepted(
    std::unique_ptr<std::string> response) {
  if (will_destroy_current_message_loop_.load()) {
    return;
  }
  if (task_runner_ != base::ThreadTaskRunnerHandle::Get()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&XMLHttpRequestImpl::SendIntercepted,
                                          AsWeakPtr(), std::move(response)));
    return;
  }
  sent_ = true;
  // Now that a send is happening, prevent this object
  // from being collected until it's complete or aborted
  // if no currently active request has called it before.
  // TODO: consider deduplicating code from |Send()|,
  //       |OnURLFetchDownloadProgress()|, and |OnURLFetchComplete()|.

  // Send().
  IncrementActiveRequests();
  FireProgressEvent(xhr_, base::Tokens::loadstart());
  if (!upload_complete_) {
    FireProgressEvent(upload_, base::Tokens::loadstart());
  }

  // OnURLFetchResponseStarted().
  http_status_ = 200;
  http_response_headers_ = new net::HttpResponseHeaders("");
  ChangeState(XMLHttpRequest::kHeadersReceived);

  // OnURLFetchDownloadProgress().
  ChangeState(XMLHttpRequest::kLoading);
  const auto& xhr_settings =
      environment_settings()->context()->web_settings()->xhr_settings();
  response_body_ = new URLFetcherResponseWriter::Buffer(
      URLFetcherResponseWriter::Buffer::kString,
      xhr_settings.IsTryLockForProgressCheckEnabled());
  response_body_->Write(response->data(), response->size());
  if (fetch_callback_) {
    script::Handle<script::Uint8Array> data =
        script::Uint8Array::New(settings_->context()->global_environment(),
                                response->data(), response->size());
    fetch_callback_->value().Run(data);
  }

  // OnURLFetchComplete().
  if (!upload_complete_ && upload_listener_) {
    upload_complete_ = true;
    FireProgressEvent(upload_, base::Tokens::progress());
    FireProgressEvent(upload_, base::Tokens::load());
    FireProgressEvent(upload_, base::Tokens::loadend());
  }
  ChangeState(XMLHttpRequest::kDone);
  size_t received_length = response->size();
  FireProgressEvent(xhr_, base::Tokens::load(), received_length,
                    received_length,
                    /*length_computable=*/true);
  FireProgressEvent(xhr_, base::Tokens::loadend(), received_length,
                    received_length,
                    /*length_computable=*/true);
  // Undo the ref we added in Send()
  DecrementActiveRequests();

  fetch_callback_.reset();
  fetch_mode_callback_.reset();
}

void XMLHttpRequestImpl::SendFallback(
    const base::Optional<XMLHttpRequest::RequestBodyType>& request_body,
    script::ExceptionState* exception_state) {
  if (will_destroy_current_message_loop_.load()) {
    return;
  }
  if (task_runner_ != base::ThreadTaskRunnerHandle::Get()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&XMLHttpRequestImpl::SendFallback,
                       base::Unretained(this), request_body, exception_state));
    return;
  }
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-send()-method
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Step 1
  if (state_ != XMLHttpRequest::kOpened) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  // Step 2
  if (sent_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }

  // Step 3 - 7
  error_ = false;
  upload_complete_ = false;

  // Add request body, if appropriate.
  if ((method_ == net::URLFetcher::POST || method_ == net::URLFetcher::PUT) &&
      request_body) {
    bool has_content_type =
        request_headers_.HasHeader(net::HttpRequestHeaders::kContentType);
    if (request_body->IsType<std::string>()) {
      request_body_text_.assign(request_body->AsType<std::string>());
      if (!has_content_type) {
        // We're assuming that request_body is UTF-8 encoded.
        request_headers_.SetHeader(net::HttpRequestHeaders::kContentType,
                                   "text/plain;charset=UTF-8");
      }
    } else if (request_body
                   ->IsType<script::Handle<script::ArrayBufferView>>()) {
      script::Handle<script::ArrayBufferView> view =
          request_body->AsType<script::Handle<script::ArrayBufferView>>();
      if (view->ByteLength()) {
        const char* start = reinterpret_cast<const char*>(view->RawData());
        request_body_text_.assign(start + view->ByteOffset(),
                                  view->ByteLength());
      }
    } else if (request_body->IsType<script::Handle<script::ArrayBuffer>>()) {
      script::Handle<script::ArrayBuffer> array_buffer =
          request_body->AsType<script::Handle<script::ArrayBuffer>>();
      if (array_buffer->ByteLength()) {
        const char* start = reinterpret_cast<const char*>(array_buffer->Data());
        request_body_text_.assign(start, array_buffer->ByteLength());
      }
    }
  } else {
    upload_complete_ = true;
  }
  // Step 8
  if (upload_) {
    upload_listener_ = upload_->HasOneOrMoreAttributeEventListener();
  }
  origin_ = environment_settings()->GetOrigin();
  // Step 9
  sent_ = true;
  // Now that a send is happening, prevent this object
  // from being collected until it's complete or aborted
  // if no currently active request has called it before.
  IncrementActiveRequests();
  FireProgressEvent(xhr_, base::Tokens::loadstart());
  if (!upload_complete_) {
    FireProgressEvent(upload_, base::Tokens::loadstart());
  }

  // The loadstart callback may abort or modify the XHR request in some way.
  // 11.3. If state is not opened or the send() flag is unset, then return.
  if (state_ == XMLHttpRequest::kOpened && sent_) {
    this->StartRequest(request_body_text_);

    // Start the timeout timer running, if applicable.
    send_start_time_ = base::TimeTicks::Now();
    if (timeout_ms_) {
      StartTimer(base::TimeDelta());
    }
    // Timer for throttling progress events.
    upload_last_progress_time_ = base::TimeTicks();
    last_progress_time_ = base::TimeTicks();
  }
}

void XMLHttpRequestImpl::Fetch(
    const FetchUpdateCallbackArg& fetch_callback,
    const FetchModeCallbackArg& fetch_mode_callback,
    const base::Optional<XMLHttpRequest::RequestBodyType>& request_body,
    script::ExceptionState* exception_state) {
  fetch_callback_.reset(
      new FetchUpdateCallbackArg::Reference(xhr_, fetch_callback));
  fetch_mode_callback_.reset(
      new FetchModeCallbackArg::Reference(xhr_, fetch_mode_callback));
  Send(request_body, exception_state);
}

base::Optional<std::string> XMLHttpRequestImpl::GetResponseHeader(
    const std::string& header) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getresponseheader()-method
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ == XMLHttpRequest::kUnsent || state_ == XMLHttpRequest::kOpened ||
      error_) {
    return base::nullopt;
  }

  // Set-Cookie should be stripped from the response headers in OnDone().
  if (base::EqualsCaseInsensitiveASCII(header, "set-cookie") ||
      base::EqualsCaseInsensitiveASCII(header, "set-cookie2")) {
    return absl::nullopt;
  }

  bool found;
  std::string value;
  if (net::HttpUtil::IsNonCoalescingHeader(header)) {
    // A non-coalescing header may contain commas in the value, e.g. Date:
    found = http_response_headers_->EnumerateHeader(NULL, header, &value);
  } else {
    found = http_response_headers_->GetNormalizedHeader(header, &value);
  }
  return found ? absl::make_optional(value) : base::nullopt;
}

std::string XMLHttpRequestImpl::GetAllResponseHeaders() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getallresponseheaders()-method
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string output;

  if (state_ == XMLHttpRequest::kUnsent || state_ == XMLHttpRequest::kOpened ||
      error_) {
    return output;
  }

  size_t iter = 0;
  std::string name;
  std::string value;

  while (http_response_headers_->EnumerateHeaderLines(&iter, &name, &value)) {
    output += name;
    output += ": ";
    output += value;
    output += "\r\n";
  }
  return output;
}

const std::string& XMLHttpRequestImpl::response_text(
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetext-attribute
  if (response_type_ != XMLHttpRequest::kDefault &&
      response_type_ != XMLHttpRequest::kText) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
  }
  if (error_ ||
      (state_ != XMLHttpRequest::kLoading && state_ != XMLHttpRequest::kDone)) {
    return base::EmptyString();
  }

  // Note that the conversion from |response_body_| to std::string when |state_|
  // isn't kDone isn't efficient for large responses.  Fortunately this feature
  // is rarely used.
  if (state_ == XMLHttpRequest::kLoading) {
    LOG(WARNING) << "Retrieving responseText while loading can be inefficient.";
    return response_body_->GetTemporaryReferenceOfString();
  }
  return response_body_->GetReferenceOfStringAndSeal();
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsexml-attribute
scoped_refptr<dom::Document> XMLHttpRequestImpl::response_xml(
    script::ExceptionState* exception_state) {
  // Workers don't have access to DOM APIs, including Document objects. Nothing
  // to do.
  // https://www.w3.org/TR/2012/CR-workers-20120501/#apis-available-to-workers
  return NULL;
}

base::Optional<XMLHttpRequest::ResponseType> XMLHttpRequestImpl::response(
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#response
  switch (response_type_) {
    case XMLHttpRequest::kDefault:
    case XMLHttpRequest::kText:
      return XMLHttpRequest::ResponseType(response_text(exception_state));
    case XMLHttpRequest::kArrayBuffer: {
      script::Handle<script::ArrayBuffer> maybe_array_buffer_response =
          response_array_buffer();
      if (maybe_array_buffer_response.IsEmpty()) {
        return base::nullopt;
      }
      return XMLHttpRequest::ResponseType(maybe_array_buffer_response);
    }
    case XMLHttpRequest::kJson:
    case XMLHttpRequest::kDocument:
    case XMLHttpRequest::kBlob:
    case XMLHttpRequest::kResponseTypeCodeMax:
      NOTIMPLEMENTED() << "Unsupported response_type_ "
                       << response_type(exception_state);
  }
  return base::nullopt;
}

int XMLHttpRequestImpl::status() const {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-status-attribute
  if (state_ == XMLHttpRequest::kUnsent || state_ == XMLHttpRequest::kOpened ||
      error_) {
    return 0;
  } else {
    return http_status_;
  }
}

std::string XMLHttpRequestImpl::status_text() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-statustext-attribute
  if (state_ == XMLHttpRequest::kUnsent || state_ == XMLHttpRequest::kOpened ||
      error_) {
    return std::string();
  }

  return http_response_headers_->GetStatusText();
}

void XMLHttpRequestImpl::set_response_type(
    const std::string& response_type, script::ExceptionState* exception_state) {
  if (state_ == XMLHttpRequest::kLoading || state_ == XMLHttpRequest::kDone) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  for (size_t i = 0; i < arraysize(kResponseTypes); ++i) {
    if (response_type == kResponseTypes[i]) {
      DCHECK_LT(i, XMLHttpRequest::kResponseTypeCodeMax);
      response_type_ = static_cast<XMLHttpRequest::ResponseTypeCode>(i);
      return;
    }
  }

  DLOG(WARNING) << "Unexpected response type " << response_type;
}

std::string XMLHttpRequestImpl::response_type(
    script::ExceptionState* unused) const {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetype-attribute
  DCHECK_LT(response_type_, arraysize(kResponseTypes));
  return kResponseTypes[response_type_];
}

void XMLHttpRequestImpl::set_timeout(uint32 timeout) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-timeout-attribute
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  timeout_ms_ = timeout;
  if (timeout_ms_ == 0) {
    stop_timeout_ = true;
    timer_.Stop();
  } else if (sent_) {
    // Timeout was set while request was in flight. Timeout is relative to
    // the start of the request.
    StartTimer(base::TimeTicks::Now() - send_start_time_);
  }
}

bool XMLHttpRequestImpl::with_credentials(
    script::ExceptionState* unused) const {
  return with_credentials_;
}

void XMLHttpRequestImpl::set_with_credentials(
    bool with_credentials, script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-withcredentials-attribute
  if ((state_ != XMLHttpRequest::kUnsent &&
       state_ != XMLHttpRequest::kOpened) ||
      sent_) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  with_credentials_ = with_credentials;
}

scoped_refptr<XMLHttpRequestUpload> XMLHttpRequestImpl::upload() {
  if (!upload_) {
    upload_ = new XMLHttpRequestUpload(environment_settings());
  }
  return upload_;
}

void XMLHttpRequestImpl::OnURLFetchResponseStarted(
    const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  http_status_ = source->GetResponseCode();
  // Don't handle a response without headers.
  if (!source->GetResponseHeaders()) {
    HandleRequestError(XMLHttpRequest::kNetworkError);
    return;
  }
  // Copy the response headers from the fetcher. It's not safe for us to
  // modify the existing ones as they may be in use on the network thread.
  http_response_headers_ =
      new net::HttpResponseHeaders(source->GetResponseHeaders()->raw_headers());

  // Perform a CORS Check on response headers at their arrival and raise
  // Network Error when the Check fails.
  if (is_cross_origin_) {
    if (!loader::CORSPreflight::CORSCheck(*http_response_headers_,
                                          origin_.SerializedOrigin(),
                                          with_credentials_, cors_policy_)) {
      HandleRequestError(XMLHttpRequest::kNetworkError);
      return;
    }
  }

  // Discard these as required by XHR spec.
  http_response_headers_->RemoveHeader("Set-Cookie2");
  http_response_headers_->RemoveHeader("Set-Cookie");

  http_response_headers_->GetMimeType(&response_mime_type_);

  if (mime_type_override_.length()) {
    http_response_headers_->RemoveHeader("Content-Type");
    http_response_headers_->AddHeader("Content-Type", mime_type_override_);
  }

  if (fetch_mode_callback_) {
    fetch_mode_callback_->value().Run(is_cross_origin_);
  }

  // Further filter response headers as XHR's mode is cors
  if (is_cross_origin_) {
    size_t iter = 0;
    std::string name, value;
    std::vector<std::pair<std::string, std::string>> header_names_to_discard;
    std::vector<std::string> expose_headers;
    loader::CORSPreflight::GetServerAllowedHeaders(*http_response_headers_,
                                                   &expose_headers);
    while (http_response_headers_->EnumerateHeaderLines(&iter, &name, &value)) {
      if (!loader::CORSPreflight::IsSafeResponseHeader(name, expose_headers,
                                                       with_credentials_)) {
        header_names_to_discard.push_back(std::make_pair(name, value));
      }
    }
    for (const auto& header : header_names_to_discard) {
      http_response_headers_->RemoveHeaderLine(header.first, header.second);
    }
  }

  if (is_data_url_) {
    size_t iter = 0;
    std::string name, value;
    std::vector<std::pair<std::string, std::string>> header_names_to_discard;
    while (http_response_headers_->EnumerateHeaderLines(&iter, &name, &value)) {
      if (name != net::HttpRequestHeaders::kContentType) {
        header_names_to_discard.push_back(std::make_pair(name, value));
      }
    }
    for (const auto& header : header_names_to_discard) {
      http_response_headers_->RemoveHeaderLine(header.first, header.second);
    }
  }

  ChangeState(XMLHttpRequest::kHeadersReceived);

  UpdateProgress(0);
}

void XMLHttpRequestImpl::OnURLFetchDownloadProgress(
    const net::URLFetcher* source, int64_t /*current*/, int64_t /*total*/,
    int64_t /*current_network_bytes*/, bool request_done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(state_, XMLHttpRequest::kDone);

  if (response_body_->HasProgressSinceLastGetAndReset(request_done) == 0) {
    return;
  }

  // Signal to JavaScript that new data is now available.
  ChangeState(XMLHttpRequest::kLoading);

  if (fetch_callback_) {
    if (response_body_->type() == URLFetcherResponseWriter::Buffer::kString) {
      std::string downloaded_data;
      response_body_->GetAndResetDataAndDownloadProgress(&downloaded_data);
      script::Handle<script::Uint8Array> data = script::Uint8Array::New(
          environment_settings()->context()->global_environment(),
          downloaded_data.data(), downloaded_data.size());
      fetch_callback_->value().Run(data);
    } else {
      DCHECK_EQ(response_body_->type(),
                URLFetcherResponseWriter::Buffer::kBufferPool);
      std::vector<script::PreallocatedArrayBufferData> downloaded_buffers;
      response_body_->GetAndResetDataAndDownloadProgress(request_done,
                                                         &downloaded_buffers);
      for (auto& downloaded_buffer : downloaded_buffers) {
        auto array_buffer =
            script::ArrayBuffer::New(settings_->context()->global_environment(),
                                     std::move(downloaded_buffer));
        script::Handle<script::Uint8Array> data = script::Uint8Array::New(
            settings_->context()->global_environment(), array_buffer, 0,
            array_buffer->ByteLength());
        fetch_callback_->value().Run(data);
      }
    }
  }

  // Send a progress notification if at least 50ms have elapsed.
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta elapsed(now - last_progress_time_);
  if (elapsed > base::TimeDelta::FromMilliseconds(kProgressPeriodMs)) {
    last_progress_time_ = now;
    if (fetch_callback_) {
      // TODO: Investigate if we have to fire progress event with 0 loaded bytes
      //       when used as Fetch API.
      UpdateProgress(0);
    } else {
      UpdateProgress(response_body_->GetAndResetDownloadProgress());
    }
  }
}


void XMLHttpRequestImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (source->GetResponseHeaders()) {
    if (source->GetResponseHeaders()->IsRedirect(NULL)) {
      // To do CORS Check and Send potential preflight, we used
      // SetStopOnRedirect to terminate request on redirect and OnRedict
      // function will deal with the early termination and send preflight if
      // needed.
      OnRedirect(*source->GetResponseHeaders());
      return;
    }
    // Create Performance Resource Timing entry after fetch complete.
    this->GetLoadTimingInfoAndCreateResourceTiming();
  }

  const net::Error status = source->GetStatus();
  if (status == net::OK) {
    stop_timeout_ = true;
    if (error_) {
      // Ensure the fetch callbacks are reset when URL fetch is complete,
      // regardless of error status.
      fetch_callback_.reset();
      fetch_mode_callback_.reset();
      return;
    }

    // Ensure all fetched data is read and transferred to this XHR. This should
    // only be done for successful and error-free fetches.
    OnURLFetchDownloadProgress(source, 0, 0, 0, /* request_done = */ true);

    // The request may have completed too quickly, before URLFetcher's upload
    // progress timer had a chance to inform us upload is finished.
    if (!upload_complete_ && upload_listener_) {
      upload_complete_ = true;
      FireProgressEvent(upload_, base::Tokens::progress());
      FireProgressEvent(upload_, base::Tokens::load());
      FireProgressEvent(upload_, base::Tokens::loadend());
    }
    ChangeState(XMLHttpRequest::kDone);
    UpdateProgress(response_body_->GetAndResetDownloadProgress());
    // Undo the ref we added in Send()
    DecrementActiveRequests();
  } else {
    HandleRequestError(XMLHttpRequest::kNetworkError);
  }

  fetch_callback_.reset();
  fetch_mode_callback_.reset();
}

void XMLHttpRequestImpl::OnURLFetchUploadProgress(const net::URLFetcher* source,
                                                  int64 current_val,
                                                  int64 total_val) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (upload_complete_) {
    return;
  }

  uint64 current = static_cast<uint64>(current_val);
  uint64 total = static_cast<uint64>(total_val);
  if (current == total) {
    upload_complete_ = true;
  }

  // Fire a progress event if either the upload just completed, or if enough
  // time has elapsed since we sent the last one.

  // https://xhr.spec.whatwg.org/#dom-xmlhttprequest-send step 11.4.
  // To process request body for request, run these subsubsteps:
  // 1.not roughly 50ms have passed since these subsubsteps were last invoked,
  // terminate these subsubsteps.
  // 2. If upload listener flag is set, then fire a progress event named
  // progress on the XMLHttpRequestUpload object with request's body's
  // transmitted bytes and request's body's total bytes.
  if (!upload_listener_) {
    return;
  }
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta elapsed(now - upload_last_progress_time_);
  if (upload_complete_ ||
      (elapsed > base::TimeDelta::FromMilliseconds(kProgressPeriodMs))) {
    FireProgressEvent(upload_, base::Tokens::progress(), current, total,
                      total != 0);
    upload_last_progress_time_ = now;
  }

  // To process request end-of-body for request, run these subsubsteps:
  // 2. if upload listener flag is unset, then terminate these subsubsteps.
  if (upload_complete_) {
    FireProgressEvent(upload_, base::Tokens::load(), current, total,
                      total != 0);
    FireProgressEvent(upload_, base::Tokens::loadend(), current, total,
                      total != 0);
  }
}

void XMLHttpRequestImpl::OnRedirect(const net::HttpResponseHeaders& headers) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GURL new_url = url_fetcher_->GetURL();
  // Since we moved redirect from url_request to here, we also need to
  // handle redirecting too many times.
  if (redirect_times_ >= kRedirectLimit) {
    DLOG(INFO) << "XHR's redirect times hit limit, aborting request.";
    HandleRequestError(XMLHttpRequest::kNetworkError);
    return;
  }

  // This function is designed to be called by url_fetcher_core::
  // OnReceivedRedirect
  // https://fetch.spec.whatwg.org/#concept-http-redirect-fetch
  // 7. If request’s mode is "cors", request’s origin is not same origin
  // with actualResponse’s location URL’s origin, and actualResponse’s
  // location URL includes credentials, then return a network error.
  // 8. If CORS flag is set and actualResponse’s location URL includes
  // credentials, then return a network error.
  if (new_url.has_username() || new_url.has_password()) {
    if (loader::Origin(new_url) != loader::Origin(request_url_)) {
      DLOG(INFO) << "XHR is redirected to cross-origin url with credentials, "
                    "aborting request for security reasons.";
      HandleRequestError(XMLHttpRequest::kNetworkError);
      return;
    } else if (is_cross_origin_) {
      DLOG(INFO) << "XHR is redirected with credentials and cors_flag set, "
                    "aborting request for security reasons.";
      HandleRequestError(XMLHttpRequest::kNetworkError);
      return;
    }
  }
  if (!new_url.is_valid()) {
    HandleRequestError(XMLHttpRequest::kNetworkError);
    return;
  }
  // This is a redirect. Re-check the CSP.
  web::CspDelegate* csp = csp_delegate();
  if (csp &&
      !csp->CanLoad(web::CspDelegate::kXhr, new_url, true /* is_redirect */)) {
    HandleRequestError(XMLHttpRequest::kNetworkError);
    return;
  }
  // CORS check for the received response
  if (is_cross_origin_) {
    if (!loader::CORSPreflight::CORSCheck(headers, origin_.SerializedOrigin(),
                                          with_credentials_, cors_policy_)) {
      HandleRequestError(XMLHttpRequest::kNetworkError);
      return;
    }
  }
  is_redirect_ = true;
  // If CORS flag is set and actualResponse’s location URL’s origin is not
  // same origin with request’s current url’s origin, then set request’s
  // origin to a unique opaque origin.
  if (loader::Origin(new_url) != loader::Origin(request_url_)) {
    if (is_cross_origin_) {
      origin_ = loader::Origin();
    } else {
      origin_ = loader::Origin(request_url_);
      is_cross_origin_ = true;
    }
  }
  // Send out preflight if needed
  int http_status_code = headers.response_code();
  if ((http_status_code == 303) ||
      ((http_status_code == 301 || http_status_code == 302) &&
       (method_ == net::URLFetcher::POST))) {
    method_ = net::URLFetcher::GET;
    request_body_text_.clear();
  }
  request_url_ = new_url;
  redirect_times_++;
  this->StartRequest(request_body_text_);
}

void XMLHttpRequestImpl::WillDestroyCurrentMessageLoop() {
  will_destroy_current_message_loop_.store(true);
}

void XMLHttpRequestImpl::ReportLoadTimingInfo(
    const net::LoadTimingInfo& timing_info) {
  load_timing_info_ = timing_info;
}

void XMLHttpRequestImpl::GetLoadTimingInfoAndCreateResourceTiming() {
  // Performance info is only available through window currently. Not available
  // in workers.
  return;
}

void XMLHttpRequestImpl::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(upload_);
}

web::CspDelegate* XMLHttpRequestImpl::csp_delegate() const {
  return environment_settings()
      ->context()
      ->GetWindowOrWorkerGlobalScope()
      ->csp_delegate();
}

void XMLHttpRequestImpl::TerminateRequest() {
  error_ = true;
  corspreflight_.reset(NULL);
  url_fetcher_.reset(NULL);
}

void XMLHttpRequestImpl::HandleRequestError(
    XMLHttpRequest::RequestErrorType request_error_type) {
  // https://www.w3.org/TR/XMLHttpRequest/#timeout-error
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DLOG_IF(INFO, verbose()) << __FUNCTION__ << " ("
                           << RequestErrorTypeName(request_error_type) << ") "
                           << *xhr_ << std::endl
                           << script::StackTraceToString(
                                  environment_settings()
                                      ->context()
                                      ->global_environment()
                                      ->GetStackTrace(0 /*max_frames*/));
  stop_timeout_ = true;
  // Step 1
  TerminateRequest();
  // Steps 2-4
  // Change state and fire readystatechange event.
  ChangeState(XMLHttpRequest::kDone);

  base_token::Token error_name = RequestErrorTypeName(request_error_type);
  // Step 5
  if (!upload_complete_) {
    upload_complete_ = true;
    FireProgressEvent(upload_, base::Tokens::progress());
    FireProgressEvent(upload_, error_name);
    FireProgressEvent(upload_, base::Tokens::loadend());
  }

  // Steps 6-8
  FireProgressEvent(xhr_, base::Tokens::progress());
  FireProgressEvent(xhr_, error_name);
  FireProgressEvent(xhr_, base::Tokens::loadend());

  fetch_callback_.reset();
  fetch_mode_callback_.reset();
  DecrementActiveRequests();
}

void XMLHttpRequestImpl::OnTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!stop_timeout_) {
    HandleRequestError(XMLHttpRequest::kTimeoutError);
  }
}

void XMLHttpRequestImpl::StartTimer(base::TimeDelta time_since_send) {
  // Subtract any time that has already elapsed from the timeout.
  // This is in case the user has set a timeout after send() was already in
  // flight.
  base::TimeDelta delay = std::max(
      base::TimeDelta(),
      base::TimeDelta::FromMilliseconds(timeout_ms_) - time_since_send);

  // Queue the callback even if delay ends up being zero, to preserve the
  // previous semantics.
  timer_.Start(FROM_HERE, delay, this, &XMLHttpRequestImpl::OnTimeout);
}

void XMLHttpRequestImpl::ChangeState(XMLHttpRequest::State new_state) {
  // Always dispatch state change events for LOADING, also known as
  // INTERACTIVE, so that clients can get partial data (XHR streaming).
  // This is to match the behavior of Chrome (which took it from Firefox).
  if (state_ == new_state && new_state != XMLHttpRequest::kLoading) {
    return;
  }

  state_ = new_state;
  if (state_ != XMLHttpRequest::kUnsent) {
    xhr_->DispatchEvent(new web::Event(base::Tokens::readystatechange()));
  }
}

script::Handle<script::ArrayBuffer>
XMLHttpRequestImpl::response_array_buffer() {
  // https://www.w3.org/TR/XMLHttpRequest/#response-entity-body
  if (error_ || state_ != XMLHttpRequest::kDone) {
    // Return a handle holding a nullptr.
    return script::Handle<script::ArrayBuffer>();
  }
  if (!response_array_buffer_reference_) {
    // The request is done so it is safe to only keep the ArrayBuffer and clear
    // |response_body_|.  As |response_body_| will not be used unless the
    // request is re-opened.
    script::PreallocatedArrayBufferData downloaded_data;
    response_body_->GetAndResetData(&downloaded_data);
    auto array_buffer = script::ArrayBuffer::New(
        environment_settings()->context()->global_environment(),
        std::move(downloaded_data));
    response_array_buffer_reference_.reset(
        new script::ScriptValue<script::ArrayBuffer>::Reference(xhr_,
                                                                array_buffer));
    return array_buffer;
  } else {
    return script::Handle<script::ArrayBuffer>(
        *response_array_buffer_reference_);
  }
}

void XMLHttpRequestImpl::UpdateProgress(int64_t received_length) {
  DCHECK(http_response_headers_);
  const int64 content_length = http_response_headers_->GetContentLength();
  const bool length_computable =
      content_length > 0 && received_length <= content_length;
  const uint64 total =
      length_computable ? static_cast<uint64>(content_length) : 0;

  DLOG_IF(INFO, verbose()) << __FUNCTION__ << " (" << received_length << " / "
                           << total << ") " << *xhr_;

  if (state_ == XMLHttpRequest::kDone) {
    FireProgressEvent(xhr_, base::Tokens::load(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
    FireProgressEvent(xhr_, base::Tokens::loadend(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
  } else {
    FireProgressEvent(xhr_, base::Tokens::progress(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
  }
}

void XMLHttpRequestImpl::StartRequest(const std::string& request_body) {
  const auto& xhr_settings =
      environment_settings()->context()->web_settings()->xhr_settings();
  const bool fetch_buffer_pool_enabled =
      xhr_settings.IsFetchBufferPoolEnabled().value_or(false);

  if (fetch_callback_ && fetch_buffer_pool_enabled) {
    LOG(INFO) << "Fetching (backed by BufferPool): "
              << ClipUrl(request_url_, 200);
  } else {
    LOG(INFO) << "Fetching: " << ClipUrl(request_url_, 200);
  }

  response_array_buffer_reference_.reset();

  network::NetworkModule* network_module =
      environment_settings()->context()->fetcher_factory()->network_module();
  url_fetcher_ = net::URLFetcher::Create(request_url_, method_, xhr_);
  ++url_fetcher_generation_;
  url_fetcher_->SetRequestContext(network_module->url_request_context_getter());

  if (fetch_callback_) {
    // FetchBufferPool is by default disabled, but can be explicitly overridden.
    if (fetch_buffer_pool_enabled) {
      response_body_ = new URLFetcherResponseWriter::Buffer(
          URLFetcherResponseWriter::Buffer::kBufferPool,
          xhr_settings.IsTryLockForProgressCheckEnabled(),
          xhr_settings.GetDefaultFetchBufferSize());
    } else {
      response_body_ = new URLFetcherResponseWriter::Buffer(
          URLFetcherResponseWriter::Buffer::kString,
          xhr_settings.IsTryLockForProgressCheckEnabled());
      response_body_->DisablePreallocate();
    }
  } else {
    if (response_type_ == XMLHttpRequest::kArrayBuffer) {
      response_body_ = new URLFetcherResponseWriter::Buffer(
          URLFetcherResponseWriter::Buffer::kArrayBuffer,
          xhr_settings.IsTryLockForProgressCheckEnabled());
    } else {
      response_body_ = new URLFetcherResponseWriter::Buffer(
          URLFetcherResponseWriter::Buffer::kString,
          xhr_settings.IsTryLockForProgressCheckEnabled());
    }
  }
  std::unique_ptr<net::URLFetcherResponseWriter> download_data_writer(
      new URLFetcherResponseWriter(response_body_));
  url_fetcher_->SaveResponseWithWriter(std::move(download_data_writer));
  // Don't retry, let the caller deal with it.
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetExtraRequestHeaders(request_headers_.ToString());
  network_module->AddClientHintHeaders(*url_fetcher_, network::kCallTypeXHR);

  // We want to do cors check and preflight during redirects
  url_fetcher_->SetStopOnRedirect(true);

  if (request_body.size()) {
    // If applicable, the request body Content-Type is already set in
    // request_headers.
    url_fetcher_->SetUploadData("", request_body);
  }

  // We let data url fetch resources freely but with no response headers.
  is_data_url_ = is_data_url_ || request_url_.SchemeIs("data");
  is_cross_origin_ = (is_redirect_ && is_cross_origin_) ||
                     (origin_ != loader::Origin(request_url_) && !is_data_url_);
  is_redirect_ = false;
  // If the CORS flag is set, httpRequest’s method is neither `GET` nor `HEAD`
  // or httpRequest’s mode is "websocket", then append `Origin`/httpRequest’s
  // origin, serialized and UTF-8 encoded, to httpRequest’s header list.
  if (is_cross_origin_ ||
      (method_ != net::URLFetcher::GET && method_ != net::URLFetcher::HEAD)) {
    url_fetcher_->AddExtraRequestHeader("Origin:" + origin_.SerializedOrigin());
  }
  bool dopreflight = false;
  if (is_cross_origin_) {
    corspreflight_.reset(new cobalt::loader::CORSPreflight(
        request_url_, method_, network_module,
        base::Bind(&DOMXMLHttpRequestImpl::CORSPreflightSuccessCallback,
                   base::Unretained(this)),
        origin_.SerializedOrigin(),
        base::Bind(&DOMXMLHttpRequestImpl::CORSPreflightErrorCallback,
                   base::Unretained(this)),
        environment_settings()
            ->context()
            ->GetWindowOrWorkerGlobalScope()
            ->get_preflight_cache()));
    corspreflight_->set_headers(request_headers_);
    corspreflight_->set_cors_policy(cors_policy_);
    // For cross-origin requests, don't send or save auth data / cookies unless
    // withCredentials was set.
    // To make a cross-origin request, add origin, referrer source, credentials,
    // omit credentials flag, force preflight flag
    if (!with_credentials_) {
#ifndef USE_HACKY_COBALT_CHANGES
      const uint32 kDisableCookiesLoadFlags =
          net::LOAD_NORMAL | net::LOAD_DO_NOT_SAVE_COOKIES |
          net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
      url_fetcher_->SetLoadFlags(kDisableCookiesLoadFlags);
#endif
    } else {
      // For credentials mode: If the withCredentials attribute value is true,
      // "include", and "same-origin" otherwise.
      corspreflight_->set_credentials_mode_is_include(true);
    }
    corspreflight_->set_force_preflight(upload_listener_);
    dopreflight = corspreflight_->Send();
  }
  DLOG_IF(INFO, verbose()) << __FUNCTION__ << *xhr_;
  if (!dopreflight) {
    DCHECK(environment_settings()->context()->network_module());
    StartURLFetcher(environment_settings()
                        ->context()
                        ->network_module()
                        ->max_network_delay(),
                    url_fetcher_generation_);
  }
}

void XMLHttpRequestImpl::IncrementActiveRequests() {
  if (active_requests_count_ == 0) {
    prevent_gc_until_send_complete_.reset(
        new script::GlobalEnvironment::ScopedPreventGarbageCollection(
            environment_settings()->context()->global_environment(), xhr_));
  }
  active_requests_count_++;
}

void XMLHttpRequestImpl::DecrementActiveRequests() {
  DCHECK_GT(active_requests_count_, 0);
  active_requests_count_--;
  if (active_requests_count_ == 0) {
    bool is_active = (state_ == XMLHttpRequest::kOpened && sent_) ||
                     state_ == XMLHttpRequest::kHeadersReceived ||
                     state_ == XMLHttpRequest::kLoading;
    bool has_event_listeners =
        xhr_->GetAttributeEventListener(base::Tokens::readystatechange()) ||
        xhr_->GetAttributeEventListener(base::Tokens::progress()) ||
        xhr_->GetAttributeEventListener(base::Tokens::abort()) ||
        xhr_->GetAttributeEventListener(base::Tokens::error()) ||
        xhr_->GetAttributeEventListener(base::Tokens::load()) ||
        xhr_->GetAttributeEventListener(base::Tokens::timeout()) ||
        xhr_->GetAttributeEventListener(base::Tokens::loadend());

    DCHECK_EQ((is_active && has_event_listeners), false);

    prevent_gc_until_send_complete_.reset();
  }
}

// Reset some variables in case the XHR object is reused.
void XMLHttpRequestImpl::PrepareForNewRequest() {
  request_headers_.Clear();
  // Below are variables used for CORS.
  request_body_text_.clear();
  is_cross_origin_ = false;
  redirect_times_ = 0;
  is_data_url_ = false;
  upload_listener_ = false;
  is_redirect_ = false;
}

void XMLHttpRequestImpl::StartURLFetcher(const SbTime max_artificial_delay,
                                         const int url_fetcher_generation) {
  if (max_artificial_delay > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&XMLHttpRequestImpl::StartURLFetcher, base::Unretained(this),
                   0, url_fetcher_generation_),
        base::TimeDelta::FromMicroseconds(base::RandUint64() %
                                          max_artificial_delay));
    return;
  }

  // Note: Checking that "url_fetcher_generation_" != "url_fetcher_generation"
  // is to verify the "url_fetcher_" is currently the same one that was present
  // upon a delayed url fetch. This works because the incoming parameter
  // "url_fetcher_generation" will hold the value at the time of the initial
  // call, and if a delayed binding has waited while a new "url_fetcher_" has
  // changed state, "url_fetcher_generation_" will have incremented.
  if (nullptr != url_fetcher_ &&
      url_fetcher_generation == url_fetcher_generation_) {
    url_fetcher_->Start();
  }
}

void XMLHttpRequestImpl::CORSPreflightErrorCallback() {
  HandleRequestError(XMLHttpRequest::kNetworkError);
}

void XMLHttpRequestImpl::CORSPreflightSuccessCallback() {
  DCHECK(environment_settings()->context()->network_module());
  StartURLFetcher(
      environment_settings()->context()->network_module()->max_network_delay(),
      url_fetcher_generation_);
}

DOMXMLHttpRequestImpl::DOMXMLHttpRequestImpl(XMLHttpRequest* xhr)
    : XMLHttpRequestImpl(xhr) {
  DCHECK(environment_settings());
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsexml-attribute
scoped_refptr<dom::Document> DOMXMLHttpRequestImpl::response_xml(
    script::ExceptionState* exception_state) {
  // 1. If responseType is not the empty string or "document", throw an
  // "InvalidStateError" exception.
  if (response_type_ != XMLHttpRequest::kDefault &&
      response_type_ != XMLHttpRequest::kDocument) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             exception_state);
    return NULL;
  }

  // 2. If the state is not DONE, return null.
  if (state_ != XMLHttpRequest::kDone) {
    return NULL;
  }

  // 3. If the error flag is set, return null.
  if (error_) {
    return NULL;
  }

  // 4. Return the document response entity body.
  return GetDocumentResponseEntityBody();
}

void DOMXMLHttpRequestImpl::GetLoadTimingInfoAndCreateResourceTiming() {
  web::WindowOrWorkerGlobalScope* window_or_worker_global_scope =
      environment_settings()->context()->GetWindowOrWorkerGlobalScope();
  if (!window_or_worker_global_scope->IsWindow()) return;
  if (window_or_worker_global_scope->AsWindow()->performance() == nullptr)
    return;
  window_or_worker_global_scope->AsWindow()
      ->performance()
      ->CreatePerformanceResourceTiming(load_timing_info_,
                                        kPerformanceResourceTimingInitiatorType,
                                        request_url_.spec());
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#document-response-entity-body
scoped_refptr<dom::Document>
DOMXMLHttpRequestImpl::GetDocumentResponseEntityBody() {
  DCHECK_EQ(state_, XMLHttpRequest::kDone);
  if (!environment_settings()
           ->context()
           ->GetWindowOrWorkerGlobalScope()
           ->IsWindow()) {
    return nullptr;
  }

  // Step 1..5
  const std::string final_mime_type =
      mime_type_override_.empty() ? response_mime_type_ : mime_type_override_;
  if (final_mime_type != "text/xml" && final_mime_type != "application/xml") {
    return nullptr;
  }

  // 6. Otherwise, let document be a document that represents the result of
  // parsing the response entity body following the rules set forth in the XML
  // specifications. If that fails (unsupported character encoding, namespace
  // well-formedness error, etc.), return null.
  scoped_refptr<dom::XMLDocument> xml_document =
      new dom::XMLDocument(environment_settings()
                               ->context()
                               ->GetWindowOrWorkerGlobalScope()
                               ->AsWindow()
                               ->html_element_context());
  dom_parser::XMLDecoder xml_decoder(
      xml_document, xml_document, NULL,
      base::polymorphic_downcast<dom::DOMSettings*>(environment_settings())
          ->max_dom_element_depth(),
      base::SourceLocation("[object XMLHttpRequest]", 1, 1),
      base::Bind(&DOMXMLHttpRequestImpl::XMLDecoderLoadCompleteCallback,
                 base::Unretained(this)));
  has_xml_decoder_error_ = false;
  xml_decoder.DecodeChunk(response_body_->GetReferenceOfStringAndSeal().c_str(),
                          response_body_->GetReferenceOfStringAndSeal().size());
  xml_decoder.Finish();
  if (has_xml_decoder_error_) {
    return NULL;
  }

  // Step 7..11 Not needed by Cobalt.

  // 12. Return document.
  return xml_document;
}

void DOMXMLHttpRequestImpl::XMLDecoderLoadCompleteCallback(
    const base::Optional<std::string>& error) {
  if (error) has_xml_decoder_error_ = true;
}

std::ostream& operator<<(std::ostream& out, const XMLHttpRequest& xhr) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  base::StringPiece response_text("");
  if ((xhr.xhr_impl_->state_ == XMLHttpRequest::kDone) &&
      (xhr.xhr_impl_->response_type_ == XMLHttpRequest::kDefault ||
       xhr.xhr_impl_->response_type_ == XMLHttpRequest::kText)) {
    size_t kMaxSize = 4096;
    const auto& response_body =
        xhr.xhr_impl_->response_body_->GetTemporaryReferenceOfString();
    response_text =
        base::StringPiece(reinterpret_cast<const char*>(response_body.data()),
                          std::min(kMaxSize, response_body.size()));
  }

  std::string xhr_out = base::StringPrintf(
      " XHR:\n"
      "\tid:    %d\n"
      "\trequest_url: %s\n"
      "\tstate: %s\n"
      "\tresponse_type: %s\n"
      "\ttimeout_ms: %d\n"
      "\tmethod: %s\n"
      "\thttp_status: %d\n"
      "\twith_credentials: %s\n"
      "\terror: %s\n"
      "\tsent: %s\n"
      "\tstop_timeout: %s\n"
      "\tresponse_body: %s\n",
      xhr.xhr_id_, xhr.xhr_impl_->request_url_.spec().c_str(),
      StateName(xhr.xhr_impl_->state_), xhr.response_type(NULL).c_str(),
      xhr.xhr_impl_->timeout_ms_,
      RequestTypeToMethodName(xhr.xhr_impl_->method_),
      xhr.xhr_impl_->http_status_,
      xhr.xhr_impl_->with_credentials_ ? "true" : "false",
      xhr.xhr_impl_->error_ ? "true" : "false",
      xhr.xhr_impl_->sent_ ? "true" : "false",
      xhr.xhr_impl_->stop_timeout_ ? "true" : "false",
      std::string(response_text).c_str());
  out << xhr_out;
#else
#endif
  return out;
}

}  // namespace xhr
}  // namespace cobalt
