/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/xhr/xml_http_request.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event_names.h"
#include "cobalt/dom/progress_event.h"
#include "cobalt/dom/window.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_engine.h"
#include "net/http/http_util.h"

namespace cobalt {
namespace xhr {

namespace {

const char* s_response_types[] = {
    "",             // kDefault
    "text",         // kText
    "json",         // kJson
    "document",     // kDocument
    "blob",         // kBlob
    "arraybuffer",  // kArrayBuffer
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

}  // namespace

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
      did_add_ref_(false) {
  DCHECK(settings_);
}

void XMLHttpRequest::Abort() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-abort()-method
  DCHECK(thread_checker_.CalledOnValidThread());
  // Terminate the request and update state.
  TerminateRequest();
  bool abort_is_no_op =
      state_ == kUnsent || state_ == kDone || (state_ == kOpened && !sent_);
  if (!abort_is_no_op) {
    // Undo the ref we added in Send()
    ReleaseExtraRef();
    sent_ = false;
    HandleRequestError(kAbortError);
  }
  ChangeState(kUnsent);
}

void XMLHttpRequest::Open(const std::string& method, const std::string& url) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-open()-method
  DCHECK(thread_checker_.CalledOnValidThread());

  // Cancel any outstanding request.
  Abort();
  state_ = kUnsent;

  base_url_ = settings_->base_url();
  if (!MethodNameToRequestType(method, &method_)) {
    DLOG(ERROR) << "TypeError: Invalid method " << method;
    // TODO(***REMOVED***): Throw TypeError exception
    NOTIMPLEMENTED();
    return;
  }

  DLOG(INFO) << "Resolving " << url << " against base " << base_url_;
  request_url_ = base_url_.Resolve(url);
  if (!request_url_.is_valid()) {
    DLOG(ERROR) << "SyntaxError: Invalid request URL " << url;
    // TODO(***REMOVED***): Throw SyntaxError exception
    NOTIMPLEMENTED();
    return;
  }

  sent_ = false;
  stop_timeout_ = false;

  response_body_.clear();
  request_headers_.Clear();
  ChangeState(kOpened);
}

void XMLHttpRequest::SetRequestHeader(const std::string& header,
                                      const std::string& value) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-setrequestheader
  if (state_ != kOpened || sent_) {
    DLOG(ERROR) << "InvalidStateError: state_ was " << state_;
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

void XMLHttpRequest::OverrideMimeType(const std::string& override_mime) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-overridemimetype
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kLoading || state_ == kDone) {
    DLOG(ERROR) << "InvalidStateError: state_ was " << state_;
    NOTIMPLEMENTED();
    return;
  }

  // Try to parse the given override. If it fails, throw an exception.
  // Otherwise, we'll replace the content-type header in the response headers
  // once we have them.
  std::string mime_type;
  std::string charset;
  bool had_charset;
  net::HttpUtil::ParseContentType(override_mime, &mime_type, &charset,
                                   &had_charset, NULL);
  if (!mime_type.length()) {
    DLOG(ERROR) << "TypeError: Failed to parse mime_type " << override_mime;
    NOTIMPLEMENTED();
    return;
  }
  mime_type_override_ = override_mime;
}

void XMLHttpRequest::Send() {
  Send(base::nullopt);
}

void XMLHttpRequest::Send(const base::optional<RequestBodyType>& request_body) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-send()-method
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kOpened) {
    DLOG(ERROR) << "InvalidStateError: state_ is not OPENED";
    NOTIMPLEMENTED();
    return;
  }
  if (sent_) {
    DLOG(ERROR) << "InvalidStateError: sent_ flag is set.";
    NOTIMPLEMENTED();
    return;
  }
  error_ = false;
  sent_ = true;
  // Now that a send is happening, prevent this object
  // from being garbage collected until it's complete or aborted.
  AddExtraRef();
  FireProgressEvent(dom::EventNames::GetInstance()->loadstart());

  network::NetworkModule* network_module =
      settings_->fetcher_factory()->network_module();
  loader::NetFetcher::Options net_options;
  net_options.request_method = method_;

  // Add request body, if appropriate.
  if (method_ == net::URLFetcher::POST || method_ == net::URLFetcher::PUT) {
    bool has_content_type =
        request_headers_.HasHeader(net::HttpRequestHeaders::kContentType);
    if (request_body->IsType<std::string>()) {
      net_options.request_body = request_body->AsType<std::string>();
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
        net_options.request_body.assign(start + view->byte_offset(),
                                        view->byte_length());
      }
    }
  }
  net_options.request_headers = request_headers_.ToString();

  net_fetcher_.reset(
      new loader::NetFetcher(request_url_, this, network_module, net_options));

  // Start the timeout timer running, if applicable.
  send_start_time_ = base::Time::Now();
  if (timeout_ms_) {
    StartTimer(base::TimeDelta());
  }
}

base::optional<std::string> XMLHttpRequest::GetResponseHeader(
    const std::string& header) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getresponseheader()-method
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kUnsent || state_ == kOpened || error_) {
    return base::nullopt;
  }

  std::string value;
  http_response_headers_->GetNormalizedHeader(header, &value);
  return value;
}

std::string XMLHttpRequest::GetAllResponseHeaders() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getallresponseheaders()-method
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kUnsent || state_ == kOpened || error_) {
    return std::string();
  }

  std::string output;
  http_response_headers_->GetNormalizedHeaders(&output);
  return output;
}

std::string XMLHttpRequest::response_text() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetext-attribute
  if (error_ || (state_ != kLoading && state_ != kDone)) {
    return std::string();
  }

  return std::string(response_body_.begin(), response_body_.end());
}

base::optional<std::string> XMLHttpRequest::response_xml() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsexml-attribute
  NOTIMPLEMENTED();
  return std::string();
}


base::optional<XMLHttpRequest::ResponseType> XMLHttpRequest::response() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#response
  switch (response_type_) {
    case kDefault:
    case kText:
      return ResponseType(response_text());
    case kArrayBuffer:
      return ResponseType(response_array_buffer());
    case kJson:
    case kDocument:
    case kBlob:
    case kResponseTypeCodeMax:
    default:
      NOTIMPLEMENTED() << "Unsupported response_type_ " << response_type();
      return base::nullopt;
  }
}

int XMLHttpRequest::status() const {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-status-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return 0;
  } else {
    return http_status_;
  }
}

std::string XMLHttpRequest::status_text() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-statustext-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return std::string();
  }

  return http_response_headers_->GetStatusText();
}

void XMLHttpRequest::set_response_type(const std::string& response_type) {
  if (state_ == kLoading || state_ == kDone) {
    DLOG(ERROR) << "InvalidStateError: " << state_
                << " for set_response_type().";
    // TODO(***REMOVED***): Throw InvalidStateError exception
    NOTIMPLEMENTED();
    return;
  }
  for (int i = 0; i < static_cast<int>(arraysize(s_response_types)); ++i) {
    if (response_type == s_response_types[i]) {
      DCHECK_LT(i, kResponseTypeCodeMax);
      response_type_ = static_cast<ResponseTypeCode>(i);
      return;
    }
  }

  DLOG(WARNING) << "Unexpected response type " << response_type;
}

std::string XMLHttpRequest::response_type() const {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetype-attribute
  DCHECK_LT(response_type_, arraysize(s_response_types));
  return s_response_types[response_type_];
}

void XMLHttpRequest::set_timeout(uint32 timeout) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-timeout-attribute
  DCHECK(thread_checker_.CalledOnValidThread());

  timeout_ms_ = timeout;
  if (timeout_ms_ == 0) {
    stop_timeout_ = true;
    timer_.Stop();
  } else if (sent_) {
    // Timeout was set while request was in flight. Timeout is relative to
    // the start of the request.
    StartTimer(base::Time::Now() - send_start_time_);
  }
}

void XMLHttpRequest::set_with_credentials(bool with_credentials) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-withcredentials-attribute
  UNREFERENCED_PARAMETER(with_credentials);
  NOTIMPLEMENTED();
}

void XMLHttpRequest::set_onreadystatechange(
    const scoped_refptr<dom::EventListener>& listener) {
  SetAttributeEventListener(dom::EventNames::GetInstance()->readystatechange(),
                            listener);
}

void XMLHttpRequest::OnReceived(const char* data, size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  response_body_.insert(response_body_.end(), data, data + size);
}

void XMLHttpRequest::OnDone() {
  DCHECK(thread_checker_.CalledOnValidThread());
  stop_timeout_ = true;
  net::URLFetcher* url_fetcher = net_fetcher_->url_fetcher();
  http_status_ = url_fetcher->GetResponseCode();
  http_response_headers_ = url_fetcher->GetResponseHeaders();

  // Discard these as required by XHR spec.
  http_response_headers_->RemoveHeader("Set-Cookie2");
  http_response_headers_->RemoveHeader("Set-Cookie");

  if (mime_type_override_.length()) {
    http_response_headers_->RemoveHeader("Content-Type");
    http_response_headers_->AddHeader(
        std::string("Content-Type: ") + mime_type_override_);
  }

  // TODO(***REMOVED***): We may need to switch to URLRequest so we can
  // have more control. We can't call most methods on url_fetcher
  // until it delivers an OnURLFetchComplete() callback.
  // So we can't really do these state changes any earlier.
  ChangeState(kHeadersReceived);
  ChangeState(kLoading);


  ChangeState(kDone);

  // Compute sizes for our final progress event.
  // This should really be done in OnReceived(), but we don't have
  // the response headers yet.
  const int64 content_length = http_response_headers_->GetContentLength();
  const int64 received_length = static_cast<int64>(response_body_.size());
  const bool length_computable =
      content_length > 0 && received_length <= content_length;
  const uint64 total =
      length_computable ? static_cast<uint64>(content_length) : 0;

  FireProgressEvent(dom::EventNames::GetInstance()->progress(),
                    static_cast<uint64>(received_length), total,
                    length_computable);
  FireProgressEvent(dom::EventNames::GetInstance()->load());
  FireProgressEvent(dom::EventNames::GetInstance()->loadend());

  // Undo the ref we added in Send()
  ReleaseExtraRef();
}

void XMLHttpRequest::OnError(const std::string& error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UNREFERENCED_PARAMETER(error);
  HandleRequestError(kNetworkError);
}

XMLHttpRequest::~XMLHttpRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG(INFO) << "Xml http request going away.";
}

void XMLHttpRequest::FireProgressEvent(const std::string& event_name) {
  DispatchEvent(new dom::ProgressEvent(event_name));
}

void XMLHttpRequest::FireProgressEvent(const std::string& event_name,
                                       uint64 loaded, uint64 total,
                                       bool length_computable) {
  DispatchEvent(
      new dom::ProgressEvent(event_name, loaded, total, length_computable));
}

void XMLHttpRequest::TerminateRequest() {
  error_ = true;
  net_fetcher_.reset(NULL);
}

void XMLHttpRequest::HandleRequestError(
    XMLHttpRequest::RequestErrorType request_error_type) {
  // http://www.w3.org/TR/XMLHttpRequest/#timeout-error
  DCHECK(thread_checker_.CalledOnValidThread());
  TerminateRequest();
  ChangeState(kDone);

  FireProgressEvent(dom::EventNames::GetInstance()->progress());
  switch (request_error_type) {
    case kNetworkError:
      FireProgressEvent(dom::EventNames::GetInstance()->error());
      break;
    case kTimeoutError:
      FireProgressEvent(dom::EventNames::GetInstance()->timeout());
      break;
    case kAbortError:
      FireProgressEvent(dom::EventNames::GetInstance()->abort());
      break;
  }
  FireProgressEvent(dom::EventNames::GetInstance()->loadend());
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
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(timeout_ms_) - time_since_send,
               this, &XMLHttpRequest::OnTimeout);
}

void XMLHttpRequest::ChangeState(XMLHttpRequest::State new_state) {
  if (state_ == new_state) {
    return;
  }

  state_ = new_state;
  if (state_ != kUnsent) {
    DispatchEvent(
        new dom::Event(dom::EventNames::GetInstance()->readystatechange()));
  }
}

scoped_refptr<dom::ArrayBuffer> XMLHttpRequest::response_array_buffer() {
  // http://www.w3.org/TR/XMLHttpRequest/#response-entity-body
  if (error_ || state_ != kDone) {
    return NULL;
  }
  scoped_refptr<dom::ArrayBuffer> array_buffer =
      new dom::ArrayBuffer(static_cast<uint32>(response_body_.size()));
  array_buffer->bytes() = response_body_;
  return array_buffer;
}

void XMLHttpRequest::AddExtraRef() {
  settings_->global_object()->PreventGarbageCollection(
      make_scoped_refptr(this));
  DCHECK(!did_add_ref_);
  did_add_ref_ = true;
}

void XMLHttpRequest::ReleaseExtraRef() {
  DCHECK(did_add_ref_);
  did_add_ref_ = false;
  settings_->javascript_engine()->ReportExtraMemoryCost(response_body_.size());
  settings_->global_object()->AllowGarbageCollection(make_scoped_refptr(this));
}

}  // namespace xhr
}  // namespace cobalt
