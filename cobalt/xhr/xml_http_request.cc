// Copyright 2015 Google Inc. All Rights Reserved.
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
#include <utility>

#include "base/compiler_specific.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/progress_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/dom_parser/xml_decoder.h"
#include "cobalt/loader/cors_preflight.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/xhr/xhr_modify_headers.h"
#include "nb/memory_scope.h"
#include "net/http/http_util.h"

namespace cobalt {
namespace xhr {

using dom::DOMException;

namespace {

// How many milliseconds must elapse between each progress event notification.
const int kProgressPeriodMs = 50;

// Allocate 64KB on receiving the first chunk to avoid allocating small buffer
// too many times.
const size_t kInitialReceivingBufferSize = 64 * 1024;

const char* kResponseTypes[] = {
    "",             // kDefault
    "text",         // kText
    "json",         // kJson
    "document",     // kDocument
    "blob",         // kBlob
    "arraybuffer",  // kArrayBuffer
};

const char* kForbiddenMethods[] = {
    "connect", "trace", "track",
};

bool MethodNameToRequestType(const std::string& method,
                             net::URLFetcher::RequestType* request_type) {
  if (LowerCaseEqualsASCII(method, "get")) {
    *request_type = net::URLFetcher::GET;
  } else if (LowerCaseEqualsASCII(method, "post")) {
    *request_type = net::URLFetcher::POST;
  } else if (LowerCaseEqualsASCII(method, "head")) {
    *request_type = net::URLFetcher::HEAD;
  } else if (LowerCaseEqualsASCII(method, "delete")) {
    *request_type = net::URLFetcher::DELETE_REQUEST;
  } else if (LowerCaseEqualsASCII(method, "put")) {
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
#endif  // defined(COBALT_BUILD_TYPE_GOLD)

bool IsForbiddenMethod(const std::string& method) {
  for (size_t i = 0; i < arraysize(kForbiddenMethods); ++i) {
    if (LowerCaseEqualsASCII(method, kForbiddenMethods[i])) {
      return true;
    }
  }
  return false;
}

base::Token RequestErrorTypeName(XMLHttpRequest::RequestErrorType type) {
  switch (type) {
    case XMLHttpRequest::kNetworkError:
      return base::Tokens::error();
    case XMLHttpRequest::kTimeoutError:
      return base::Tokens::timeout();
    case XMLHttpRequest::kAbortError:
      return base::Tokens::abort();
  }
  NOTREACHED();
  return base::Token();
}

void FireProgressEvent(XMLHttpRequestEventTarget* target,
                       base::Token event_name) {
  if (!target) {
    return;
  }
  target->DispatchEvent(new dom::ProgressEvent(event_name));
}

void FireProgressEvent(XMLHttpRequestEventTarget* target,
                       base::Token event_name, uint64 loaded, uint64 total,
                       bool length_computable) {
  if (!target) {
    return;
  }
  target->DispatchEvent(
      new dom::ProgressEvent(event_name, loaded, total, length_computable));
}

int s_xhr_sequence_num_ = 0;
// https://fetch.spec.whatwg.org/#concept-http-redirect-fetch
// 5. If request's redirect count is twenty, return a network error.
const int kRedirectLimit = 20;

}  // namespace

bool XMLHttpRequest::verbose_ = false;

XMLHttpRequest::XMLHttpRequest(script::EnvironmentSettings* settings)
    : settings_(base::polymorphic_downcast<dom::DOMSettings*>(settings)),
      state_(kUnsent),
      response_type_(kDefault),
      timeout_ms_(0),
      method_(net::URLFetcher::GET),
      http_status_(0),
      with_credentials_(false),
      error_(false),
      sent_(false),
      stop_timeout_(false),
      upload_complete_(false),
      active_requests_count_(0),
      upload_listener_(false),
      is_cross_origin_(false),
      is_redirect_(false),
      redirect_times_(0),
      is_data_url_(false) {
  DCHECK(settings_);
  dom::GlobalStats::GetInstance()->Add(this);
  xhr_id_ = ++s_xhr_sequence_num_;
}

void XMLHttpRequest::Abort() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-abort()-method
  DCHECK(thread_checker_.CalledOnValidThread());
  // Cancel any in-flight request and set error flag.
  TerminateRequest();
  bool abort_is_no_op =
      state_ == kUnsent || state_ == kDone || (state_ == kOpened && !sent_);
  if (!abort_is_no_op) {
    sent_ = false;
    HandleRequestError(kAbortError);
  }
  ChangeState(kUnsent);

  response_body_.Clear();
  response_array_buffer_ = NULL;
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-open()-method
void XMLHttpRequest::Open(const std::string& method, const std::string& url,
                          bool async,
                          const base::optional<std::string>& username,
                          const base::optional<std::string>& password,
                          script::ExceptionState* exception_state) {
  TRACK_MEMORY_SCOPE("XHR");
  UNREFERENCED_PARAMETER(username);
  UNREFERENCED_PARAMETER(password);

  DCHECK(thread_checker_.CalledOnValidThread());

  State previous_state = state_;

  // Cancel any outstanding request.
  TerminateRequest();
  state_ = kUnsent;

  if (!async) {
    DLOG(ERROR) << "synchronous XHR is not supported";
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  base_url_ = settings_->base_url();

  if (IsForbiddenMethod(method)) {
    DOMException::Raise(DOMException::kSecurityErr, exception_state);
    return;
  }

  if (!MethodNameToRequestType(method, &method_)) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  request_url_ = base_url_.Resolve(url);
  if (!request_url_.is_valid()) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }

  dom::CspDelegate* csp = csp_delegate();
  if (csp && !csp->CanLoad(dom::CspDelegate::kXhr, request_url_, false)) {
    DOMException::Raise(DOMException::kSecurityErr, exception_state);
    return;
  }

  sent_ = false;
  stop_timeout_ = false;

  PrepareForNewRequest();

  // Check previous state to avoid dispatching readyState event when calling
  // open several times in a row.
  if (previous_state != kOpened) {
    ChangeState(kOpened);
  } else {
    state_ = kOpened;
  }
}

void XMLHttpRequest::SetRequestHeader(const std::string& header,
                                      const std::string& value,
                                      script::ExceptionState* exception_state) {
  TRACK_MEMORY_SCOPE("XHR");
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-setrequestheader
  if (state_ != kOpened || sent_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (!net::HttpUtil::IsSafeHeader(header)) {
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

void XMLHttpRequest::OverrideMimeType(const std::string& override_mime,
                                      script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-overridemimetype
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kLoading || state_ == kDone) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
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
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
    return;
  }
  mime_type_override_ = mime_type;
}

void XMLHttpRequest::Send(script::ExceptionState* exception_state) {
  Send(base::nullopt, exception_state);
}

void XMLHttpRequest::Send(const base::optional<RequestBodyType>& request_body,
                          script::ExceptionState* exception_state) {
  TRACK_MEMORY_SCOPE("XHR");
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-send()-method
  DCHECK(thread_checker_.CalledOnValidThread());
  // Step 1
  if (state_ != kOpened) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  // Step 2
  if (sent_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  // Step 3 - 7
  error_ = false;
  upload_complete_ = false;

#if defined(COBALT_ENABLE_XHR_HEADER_FILTERING)
  CobaltXhrModifyHeader(request_url_, &request_headers_);
#endif

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
    } else if (request_body->IsType<scoped_refptr<dom::ArrayBufferView> >()) {
      scoped_refptr<dom::ArrayBufferView> view =
          request_body->AsType<scoped_refptr<dom::ArrayBufferView> >();
      if (view->byte_length()) {
        const char* start = reinterpret_cast<const char*>(view->base_address());
        request_body_text_.assign(start + view->byte_offset(),
                                  view->byte_length());
      }
    } else if (request_body->IsType<scoped_refptr<dom::ArrayBuffer> >()) {
      scoped_refptr<dom::ArrayBuffer> array_buffer =
          request_body->AsType<scoped_refptr<dom::ArrayBuffer> >();
      if (array_buffer->byte_length()) {
        const char* start = reinterpret_cast<const char*>(array_buffer->data());
        request_body_text_.assign(start, array_buffer->byte_length());
      }
    }
  } else {
    upload_complete_ = true;
  }
  // Step 8
  if (upload_) {
    upload_listener_ = upload_->HasOneOrMoreAttributeEventListener();
  }
  origin_ = settings_->document_origin();
  // Step 9
  sent_ = true;
  // Now that a send is happening, prevent this object
  // from being collected until it's complete or aborted
  // if no currently active request has called it before.
  IncrementActiveRequests();
  FireProgressEvent(this, base::Tokens::loadstart());
  if (!upload_complete_) {
    FireProgressEvent(upload_, base::Tokens::loadstart());
  }

  StartRequest(request_body_text_);

  // Start the timeout timer running, if applicable.
  send_start_time_ = base::TimeTicks::Now();
  if (timeout_ms_) {
    StartTimer(base::TimeDelta());
  }
  // Timer for throttling progress events.
  upload_last_progress_time_ = base::TimeTicks();
  last_progress_time_ = base::TimeTicks();
}

void XMLHttpRequest::Fetch(const FetchUpdateCallbackArg& fetch_callback,
                           const FetchModeCallbackArg& fetch_mode_callback,
                           const base::optional<RequestBodyType>& request_body,
                           script::ExceptionState* exception_state) {
  fetch_callback_.reset(
      new FetchUpdateCallbackArg::Reference(this, fetch_callback));
  fetch_mode_callback_.reset(
      new FetchModeCallbackArg::Reference(this, fetch_mode_callback));
  Send(request_body, exception_state);
}

base::optional<std::string> XMLHttpRequest::GetResponseHeader(
    const std::string& header) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getresponseheader()-method
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kUnsent || state_ == kOpened || error_) {
    return base::nullopt;
  }

  // Set-Cookie should be stripped from the response headers in OnDone().
  if (LowerCaseEqualsASCII(header, "set-cookie") ||
      LowerCaseEqualsASCII(header, "set-cookie2")) {
    return base::nullopt;
  }

  bool found;
  std::string value;
  if (net::HttpUtil::IsNonCoalescingHeader(header)) {
    // A non-coalescing header may contain commas in the value, e.g. Date:
    found = http_response_headers_->EnumerateHeader(NULL, header, &value);
  } else {
    found = http_response_headers_->GetNormalizedHeader(header, &value);
  }
  return found ? base::make_optional(value) : base::nullopt;
}

std::string XMLHttpRequest::GetAllResponseHeaders() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getallresponseheaders()-method
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string output;

  if (state_ == kUnsent || state_ == kOpened || error_) {
    return output;
  }

  void* iter = NULL;
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

const std::string& XMLHttpRequest::response_text(
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetext-attribute
  if (response_type_ != kDefault && response_type_ != kText) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
  }
  if (error_ || (state_ != kLoading && state_ != kDone)) {
    return EmptyString();
  }

  return response_body_.string();
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsexml-attribute
scoped_refptr<dom::Document> XMLHttpRequest::response_xml(
    script::ExceptionState* exception_state) {
  // 1. If responseType is not the empty string or "document", throw an
  // "InvalidStateError" exception.
  if (response_type_ != kDefault && response_type_ != kDocument) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return NULL;
  }

  // 2. If the state is not DONE, return null.
  if (state_ != kDone) {
    return NULL;
  }

  // 3. If the error flag is set, return null.
  if (error_) {
    return NULL;
  }

  // 4. Return the document response entity body.
  return GetDocumentResponseEntityBody();
}

base::optional<XMLHttpRequest::ResponseType> XMLHttpRequest::response(
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#response
  switch (response_type_) {
    case kDefault:
    case kText:
      return ResponseType(response_text(exception_state));
    case kArrayBuffer:
      return ResponseType(response_array_buffer());
    case kJson:
    case kDocument:
    case kBlob:
    case kResponseTypeCodeMax:
    default:
      NOTIMPLEMENTED() << "Unsupported response_type_ "
                       << response_type(exception_state);
      return base::nullopt;
  }
}

int XMLHttpRequest::status() const {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-status-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return 0;
  } else {
    return http_status_;
  }
}

std::string XMLHttpRequest::status_text() {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-statustext-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return std::string();
  }

  return http_response_headers_->GetStatusText();
}

void XMLHttpRequest::set_response_type(
    const std::string& response_type, script::ExceptionState* exception_state) {
  if (state_ == kLoading || state_ == kDone) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             exception_state);
    return;
  }
  for (size_t i = 0; i < arraysize(kResponseTypes); ++i) {
    if (response_type == kResponseTypes[i]) {
      DCHECK_LT(i, kResponseTypeCodeMax);
      response_type_ = static_cast<ResponseTypeCode>(i);
      return;
    }
  }

  DLOG(WARNING) << "Unexpected response type " << response_type;
}

std::string XMLHttpRequest::response_type(
    script::ExceptionState* /* unused */) const {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetype-attribute
  DCHECK_LT(response_type_, arraysize(kResponseTypes));
  return kResponseTypes[response_type_];
}

void XMLHttpRequest::set_timeout(uint32 timeout) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-timeout-attribute
  DCHECK(thread_checker_.CalledOnValidThread());

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

bool XMLHttpRequest::with_credentials(
    script::ExceptionState* /*unused*/) const {
  return with_credentials_;
}

void XMLHttpRequest::set_with_credentials(
    bool with_credentials, script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-withcredentials-attribute
  if ((state_ != kUnsent && state_ != kOpened) || sent_) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }
  with_credentials_ = with_credentials;
}

scoped_refptr<XMLHttpRequestUpload> XMLHttpRequest::upload() {
  if (!upload_) {
    upload_ = new XMLHttpRequestUpload();
  }
  return upload_;
}

void XMLHttpRequest::OnURLFetchResponseStarted(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  http_status_ = source->GetResponseCode();
  // Don't handle a response without headers.
  if (!source->GetResponseHeaders()) {
    HandleRequestError(kNetworkError);
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
                                          with_credentials_)) {
      HandleRequestError(kNetworkError);
      return;
    }
  }

  // Discard these as required by XHR spec.
  http_response_headers_->RemoveHeader("Set-Cookie2");
  http_response_headers_->RemoveHeader("Set-Cookie");

  http_response_headers_->GetMimeType(&response_mime_type_);

  if (mime_type_override_.length()) {
    http_response_headers_->RemoveHeader("Content-Type");
    http_response_headers_->AddHeader(std::string("Content-Type: ") +
                                      mime_type_override_);
  }

  if (fetch_mode_callback_) {
    fetch_mode_callback_->value().Run(is_cross_origin_);
  }

  // Reserve space for the content in the case of a regular XHR request.
  DCHECK_EQ(response_body_.size(), 0);
  if (!fetch_callback_) {
    const int64 content_length = http_response_headers_->GetContentLength();

    // If we know the eventual content length, allocate the total response body.
    // Otherwise just reserve a reasonably large initial chunk.
    size_t bytes_to_reserve = content_length > 0
                                  ? static_cast<size_t>(content_length)
                                  : kInitialReceivingBufferSize;
    response_body_.Reserve(bytes_to_reserve);
  }

  // Further filter response headers as XHR's mode is cors
  if (is_cross_origin_) {
    void* iter = NULL;
    std::string name, value;
    std::vector<std::pair<std::string, std::string> > header_names_to_discard;
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
    void* iter = NULL;
    std::string name, value;
    std::vector<std::pair<std::string, std::string> > header_names_to_discard;
    while (http_response_headers_->EnumerateHeaderLines(&iter, &name, &value)) {
      if (name != net::HttpRequestHeaders::kContentType) {
        header_names_to_discard.push_back(std::make_pair(name, value));
      }
    }
    for (const auto& header : header_names_to_discard) {
      http_response_headers_->RemoveHeaderLine(header.first, header.second);
    }
  }

  ChangeState(kHeadersReceived);

  UpdateProgress();
}

void XMLHttpRequest::OnURLFetchDownloadData(
    const net::URLFetcher* source, scoped_ptr<std::string> download_data) {
  TRACK_MEMORY_SCOPE("XHR");
  UNREFERENCED_PARAMETER(source);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(state_, kDone);

  // Preserve the response body only for regular XHR requests. Fetch requests
  // process the response in pieces, so do not need to keep the whole response.
  if (!fetch_callback_) {
    response_body_.Append(reinterpret_cast<const uint8*>(download_data->data()),
                          download_data->size());
  }

  ChangeState(kLoading);

  if (fetch_callback_) {
    scoped_refptr<dom::Uint8Array> data = new dom::Uint8Array(
          settings_,
          reinterpret_cast<const uint8*>(download_data->data()),
          static_cast<uint32>(download_data->size()),
          NULL);
    fetch_callback_->value().Run(data);
  }

  // Send a progress notification if at least 50ms have elapsed.
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta elapsed(now - last_progress_time_);
  if (elapsed > base::TimeDelta::FromMilliseconds(kProgressPeriodMs)) {
    last_progress_time_ = now;
    UpdateProgress();
  }
}

void XMLHttpRequest::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source->GetResponseHeaders()) {
    if (source->GetResponseHeaders()->IsRedirect(NULL)) {
      // To do CORS Check and Send potential preflight, we used
      // SetStopOnRedirect to terminate request on redirect and OnRedict
      // function will deal with the early termination and send preflight if
      // needed.
      OnRedirect(*source->GetResponseHeaders());
      return;
    }
  }
  fetch_callback_.reset();
  fetch_mode_callback_.reset();
  const net::URLRequestStatus& status = source->GetStatus();
  if (status.is_success()) {
    stop_timeout_ = true;
    if (error_) {
      return;
    }

    ChangeState(kDone);
    UpdateProgress();
    // Undo the ref we added in Send()
    DecrementActiveRequests();
  } else {
    HandleRequestError(kNetworkError);
  }
}

// Reset some variables in case the XHR object is reused.
void XMLHttpRequest::PrepareForNewRequest() {
  response_body_.Clear();
  request_headers_.Clear();
  response_array_buffer_ = NULL;
  // Below are variables used for CORS.
  request_body_text_.clear();
  is_cross_origin_ = false;
  redirect_times_ = 0;
  is_data_url_ = false;
  upload_listener_ = false;
  is_redirect_ = false;
}

void XMLHttpRequest::OnURLFetchUploadProgress(const net::URLFetcher* source,
                                              int64 current_val,
                                              int64 total_val) {
  TRACK_MEMORY_SCOPE("XHR");
  UNREFERENCED_PARAMETER(source);
  DCHECK(thread_checker_.CalledOnValidThread());
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
  // transmiteted bytes and request's body's total bytes.
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

void XMLHttpRequest::OnRedirect(const net::HttpResponseHeaders& headers) {
  GURL new_url = url_fetcher_->GetURL();
  // Since we moved redirect from url_request to here, we also need to
  // handle redirecting too many times.
  if (redirect_times_ >= kRedirectLimit) {
    DLOG(INFO) << "XHR's redirect times hit limit, aborting request.";
    HandleRequestError(kNetworkError);
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
      HandleRequestError(kNetworkError);
      return;
    } else if (is_cross_origin_) {
      DLOG(INFO) << "XHR is redirected with credentials and cors_flag set, "
                    "aborting request for security reasons.";
      HandleRequestError(kNetworkError);
      return;
    }
  }
  if (!new_url.is_valid()) {
    HandleRequestError(kNetworkError);
    return;
  }
  // This is a redirect. Re-check the CSP.
  if (!csp_delegate()->CanLoad(dom::CspDelegate::kXhr, new_url,
                               true /* is_redirect */)) {
    HandleRequestError(kNetworkError);
    return;
  }
  // CORS check for the received resposne
  if (is_cross_origin_) {
    if (!loader::CORSPreflight::CORSCheck(headers, origin_.SerializedOrigin(),
                                          with_credentials_)) {
      HandleRequestError(kNetworkError);
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
  StartRequest(request_body_text_);
}

void XMLHttpRequest::TraceMembers(script::Tracer* tracer) {
  XMLHttpRequestEventTarget::TraceMembers(tracer);

  tracer->Trace(response_array_buffer_);
  tracer->Trace(upload_);
}

XMLHttpRequest::~XMLHttpRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  dom::GlobalStats::GetInstance()->Remove(this);
}

dom::CspDelegate* XMLHttpRequest::csp_delegate() const {
  DCHECK(settings_);
  if (settings_->window() && settings_->window()->document()) {
    return settings_->window()->document()->csp_delegate();
  } else {
    return NULL;
  }
}

void XMLHttpRequest::TerminateRequest() {
  error_ = true;
  corspreflight_.reset(NULL);
  url_fetcher_.reset(NULL);
}

void XMLHttpRequest::HandleRequestError(
    XMLHttpRequest::RequestErrorType request_error_type) {
  // https://www.w3.org/TR/XMLHttpRequest/#timeout-error
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG_IF(INFO, verbose())
      << __FUNCTION__ << " (" << RequestErrorTypeName(request_error_type)
      << ") " << *this << std::endl
      << script::StackTraceToString(
             settings_->global_environment()->GetStackTrace(0 /*max_frames*/));
  stop_timeout_ = true;
  // Step 1
  TerminateRequest();
  // Steps 2-4
  // Change state and fire readystatechange event.
  ChangeState(kDone);

  base::Token error_name = RequestErrorTypeName(request_error_type);
  // Step 5
  if (!upload_complete_) {
    upload_complete_ = true;
    FireProgressEvent(upload_, base::Tokens::progress());
    FireProgressEvent(upload_, error_name);
    FireProgressEvent(upload_, base::Tokens::loadend());
  }

  // Steps 6-8
  FireProgressEvent(this, base::Tokens::progress());
  FireProgressEvent(this, error_name);
  FireProgressEvent(this, base::Tokens::loadend());

  fetch_callback_.reset();
  DecrementActiveRequests();
}

void XMLHttpRequest::OnTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!stop_timeout_) {
    HandleRequestError(kTimeoutError);
  }
}

void XMLHttpRequest::StartTimer(base::TimeDelta time_since_send) {
  // Subtract any time that has already elapsed from the timeout.
  // This is in case the user has set a timeout after send() was already in
  // flight.
  base::TimeDelta delay = std::max(
      base::TimeDelta(),
      base::TimeDelta::FromMilliseconds(timeout_ms_) - time_since_send);

  // Queue the callback even if delay ends up being zero, to preserve the
  // previous semantics.
  timer_.Start(FROM_HERE, delay, this, &XMLHttpRequest::OnTimeout);
}

void XMLHttpRequest::ChangeState(XMLHttpRequest::State new_state) {
  // Always dispatch state change events for LOADING, also known as
  // INTERACTIVE, so that clients can get partial data (XHR streaming).
  // This is to match the behavior of Chrome (which took it from Firefox).
  if (state_ == new_state && new_state != kLoading) {
    return;
  }

  state_ = new_state;
  if (state_ != kUnsent) {
    DispatchEvent(new dom::Event(base::Tokens::readystatechange()));
  }
}

scoped_refptr<dom::ArrayBuffer> XMLHttpRequest::response_array_buffer() {
  TRACK_MEMORY_SCOPE("XHR");
  // https://www.w3.org/TR/XMLHttpRequest/#response-entity-body
  if (error_ || state_ != kDone) {
    return NULL;
  }
  if (!response_array_buffer_) {
    // The request is done so it is safe to only keep the ArrayBuffer and clear
    // |response_body_|.  As |response_body_| will not be used unless the
    // request is re-opened.
    response_array_buffer_ =
        new dom::ArrayBuffer(settings_, response_body_.data(),
                             static_cast<uint32>(response_body_.size()));
    response_body_.Clear();
  }
  return response_array_buffer_;
}

void XMLHttpRequest::UpdateProgress() {
  DCHECK(http_response_headers_);
  const int64 content_length = http_response_headers_->GetContentLength();
  const int64 received_length = static_cast<int64>(response_body_.size());
  const bool length_computable =
      content_length > 0 && received_length <= content_length;
  const uint64 total =
      length_computable ? static_cast<uint64>(content_length) : 0;

  DLOG_IF(INFO, verbose()) << __FUNCTION__ << " (" << received_length << " / "
                           << total << ") " << *this;

  if (state_ == kDone) {
    FireProgressEvent(this, base::Tokens::load(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
    FireProgressEvent(this, base::Tokens::loadend(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
  } else {
    FireProgressEvent(this, base::Tokens::progress(),
                      static_cast<uint64>(received_length), total,
                      length_computable);
  }
}

void XMLHttpRequest::IncrementActiveRequests() {
  if (active_requests_count_ == 0) {
    PreventGarbageCollection();
  }
  active_requests_count_++;
}

void XMLHttpRequest::DecrementActiveRequests() {
  DCHECK_GT(active_requests_count_, 0);
  active_requests_count_--;
  if (active_requests_count_ == 0) {
    AllowGarbageCollection();
  }
}

void XMLHttpRequest::PreventGarbageCollection() {
  settings_->global_environment()->PreventGarbageCollection(
      make_scoped_refptr(this));
}

void XMLHttpRequest::AllowGarbageCollection() {
  bool is_active = (state_ == kOpened && sent_) || state_ == kHeadersReceived ||
                   state_ == kLoading;
  bool has_event_listeners =
      GetAttributeEventListener(base::Tokens::readystatechange()) ||
      GetAttributeEventListener(base::Tokens::progress()) ||
      GetAttributeEventListener(base::Tokens::abort()) ||
      GetAttributeEventListener(base::Tokens::error()) ||
      GetAttributeEventListener(base::Tokens::load()) ||
      GetAttributeEventListener(base::Tokens::timeout()) ||
      GetAttributeEventListener(base::Tokens::loadend());

  DCHECK_EQ((is_active && has_event_listeners), false);

  settings_->javascript_engine()->ReportExtraMemoryCost(
      response_body_.capacity());
  settings_->global_environment()->AllowGarbageCollection(
      make_scoped_refptr(this));
}

void XMLHttpRequest::StartRequest(const std::string& request_body) {
  TRACK_MEMORY_SCOPE("XHR");
  network::NetworkModule* network_module =
      settings_->fetcher_factory()->network_module();
  url_fetcher_.reset(net::URLFetcher::Create(request_url_, method_, this));
  url_fetcher_->SetRequestContext(network_module->url_request_context_getter());
  // Don't cache the response, just send it to us in OnURLFetchDownloadData().
  url_fetcher_->DiscardResponse();
  // Don't retry, let the caller deal with it.
  url_fetcher_->SetAutomaticallyRetryOn5xx(false);
  url_fetcher_->SetExtraRequestHeaders(request_headers_.ToString());

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
        base::Bind(&XMLHttpRequest::CORSPreflightSuccessCallback,
                   base::Unretained(this)),
        origin_.SerializedOrigin(),
        base::Bind(&XMLHttpRequest::CORSPreflightErrorCallback,
                   base::Unretained(this)),
        settings_->window()->get_preflight_cache()));
    corspreflight_->set_headers(request_headers_);
    // For cross-origin requests, don't send or save auth data / cookies unless
    // withCredentials was set.
    // To make a cross-origin request, add origin, referrer source, credentials,
    // omit credentials flag, force preflight flag
    if (!with_credentials_) {
      const uint32 kDisableCookiesLoadFlags =
          net::LOAD_NORMAL | net::LOAD_DO_NOT_SAVE_COOKIES |
          net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SEND_AUTH_DATA;
      url_fetcher_->SetLoadFlags(kDisableCookiesLoadFlags);
    } else {
      // For credentials mode: If the withCredentials attribute value is true,
      // "include", and "same-origin" otherwise.
      corspreflight_->set_credentials_mode_is_include(true);
    }
    corspreflight_->set_force_preflight(upload_listener_);
    dopreflight = corspreflight_->Send();
  }
  DLOG_IF(INFO, verbose()) << __FUNCTION__ << *this;
  if (!dopreflight) {
    url_fetcher_->Start();
  }
}

void XMLHttpRequest::CORSPreflightErrorCallback() {
  HandleRequestError(XMLHttpRequest::kNetworkError);
}

void XMLHttpRequest::CORSPreflightSuccessCallback() {
  if (url_fetcher_) {
    url_fetcher_->Start();
  }
}

std::ostream& operator<<(std::ostream& out, const XMLHttpRequest& xhr) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
  base::StringPiece response_text("");
  if ((xhr.state_ == XMLHttpRequest::kDone) &&
      (xhr.response_type_ == XMLHttpRequest::kDefault ||
       xhr.response_type_ == XMLHttpRequest::kText)) {
    size_t kMaxSize = 4096;
    response_text = base::StringPiece(
        reinterpret_cast<const char*>(xhr.response_body_.data()),
        std::min(kMaxSize, xhr.response_body_.size()));
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
      xhr.xhr_id_, xhr.request_url_.spec().c_str(), StateName(xhr.state_),
      xhr.response_type(NULL).c_str(), xhr.timeout_ms_,
      RequestTypeToMethodName(xhr.method_), xhr.http_status_,
      xhr.with_credentials_ ? "true" : "false", xhr.error_ ? "true" : "false",
      xhr.sent_ ? "true" : "false", xhr.stop_timeout_ ? "true" : "false",
      response_text.as_string().c_str());
  out << xhr_out;
#else
  UNREFERENCED_PARAMETER(xhr);
#endif
  return out;
}

// https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#document-response-entity-body
scoped_refptr<dom::Document> XMLHttpRequest::GetDocumentResponseEntityBody() {
  // Step 1..5
  const std::string final_mime_type =
      mime_type_override_.empty() ? response_mime_type_ : mime_type_override_;
  if (final_mime_type != "text/xml" && final_mime_type != "application/xml") {
    return NULL;
  }

  // 6. Otherwise, let document be a document that represents the result of
  // parsing the response entity body following the rules set forth in the XML
  // specifications. If that fails (unsupported character encoding, namespace
  // well-formedness error, etc.), return null.
  scoped_refptr<dom::XMLDocument> xml_document =
      new dom::XMLDocument(settings_->window()->html_element_context());
  dom_parser::XMLDecoder xml_decoder(
      xml_document, xml_document, NULL, settings_->max_dom_element_depth(),
      base::SourceLocation("[object XMLHttpRequest]", 1, 1), base::Closure(),
      base::Bind(&XMLHttpRequest::XMLDecoderErrorCallback,
                 base::Unretained(this)));
  has_xml_decoder_error_ = false;
  xml_decoder.DecodeChunk(response_body_.string().c_str(),
                          response_body_.string().size());
  xml_decoder.Finish();
  if (has_xml_decoder_error_) {
    return NULL;
  }

  // Step 7..11 Not needed by Cobalt.

  // 12. Return document.
  return xml_document;
}

void XMLHttpRequest::XMLDecoderErrorCallback(const std::string&) {
  has_xml_decoder_error_ = true;
}

}  // namespace xhr
}  // namespace cobalt
