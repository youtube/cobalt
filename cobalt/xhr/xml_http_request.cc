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
#include "net/http/http_util.h"

namespace cobalt {
namespace xhr {

namespace {

const char* s_response_types[] = {
  "",  // kDefault
  "text",  // kText
  "json",  // kJson
  "document",  // kDocument
  "blob",  // kBlob
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
  : settings_(base::polymorphic_downcast<dom::DOMSettings*>(settings))
  , state_(kUnsent)
  , response_type_(kDefault)
  , timeout_ms_(0)
  , method_(net::URLFetcher::GET)
  , http_status_(0)
  , with_credentials_(false)
  , error_(false)
  , sent_(false)
  , did_add_ref_(false) {
  DCHECK(settings_);
}

XMLHttpRequest::~XMLHttpRequest() {
  DLOG(INFO) << "Xml http request going away.";
}

void XMLHttpRequest::Open(const std::string& method, const std::string& url) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-open()-method

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
  // TODO(***REMOVED***): Clear all other response entities.
  response_text_.clear();
  ChangeState(kOpened);
}

void XMLHttpRequest::Abort() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-abort()-method
  // Terminate the request and update state.
  net_fetcher_.reset(NULL);

  // If nothing is going on, just reset state to Unsent and we're done.
  if (state_ == kUnsent || state_ == kDone || (state_ == kOpened && !sent_)) {
    ChangeState(kUnsent);
    return;
  }
  // Undo the ref we added in Send()
  ReleaseExtraRef();
  ChangeState(kDone);
  sent_ = false;

  FireProgressEvent(dom::EventNames::GetInstance()->progress());
  FireProgressEvent(dom::EventNames::GetInstance()->abort());
  FireProgressEvent(dom::EventNames::GetInstance()->loadend());

  ChangeState(kUnsent);
}

int XMLHttpRequest::status() const {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-status-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return 0;
  } else {
    return http_status_;
  }
}

void XMLHttpRequest::SetRequestHeader(
    const std::string& header, const std::string& value) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-setrequestheader
  NOTIMPLEMENTED();
}

void XMLHttpRequest::OverrideMimeType(const std::string& override_mime) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#dom-xmlhttprequest-overridemimetype
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

void XMLHttpRequest::set_response_type(const std::string& response_type) {
  if (state_ == kLoading || state_ == kDone) {
    DLOG(ERROR) << "InvalidStateError: " << state_
                << " for set_response_type().";
    // TODO(***REMOVED***): Throw InvalidStateError exception
    NOTIMPLEMENTED();
    return;
  }
  for (int i = 0; i < arraysize(s_response_types); ++i) {
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

base::optional<std::string> XMLHttpRequest::response() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#response
  if (response_type_ == kText) {
    return response_text();
  } else {
    NOTIMPLEMENTED() << "Unsupported response_type_ " << response_type();
    return base::nullopt;
  }
}

std::string XMLHttpRequest::response_text() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsetext-attribute
  if (error_ || (state_ != kLoading && state_ != kDone)) {
    return std::string();
  }

  return response_text_;
}

void XMLHttpRequest::set_onreadystatechange(
      const scoped_refptr<dom::EventListener>& listener) {
  SetAttributeEventListener(
      dom::EventNames::GetInstance()->readystatechange(), listener);
}

void XMLHttpRequest::Send() {
  Send(base::nullopt);
}

void XMLHttpRequest::Send(const base::optional<std::string>& request_body) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-send()-method
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
  net_fetcher_.reset(new loader::NetFetcher(
      request_url_, this, network_module, net_options));
}

void XMLHttpRequest::set_timeout(uint32 timeout) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-timeout-attribute
  NOTIMPLEMENTED();
}

void XMLHttpRequest::set_with_credentials(bool with_credentials) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-withcredentials-attribute
  NOTIMPLEMENTED();
}

base::optional<std::string> XMLHttpRequest::response_xml() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-responsexml-attribute
  NOTIMPLEMENTED();
  return std::string();
}

base::optional<std::string> XMLHttpRequest::GetResponseHeader(
    const std::string& header) {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getresponseheader()-method
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return base::nullopt;
  }

  std::string value;
  http_response_headers_->GetNormalizedHeader(header, &value);
  return value;
}

std::string XMLHttpRequest::GetAllResponseHeaders() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-getallresponseheaders()-method
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return std::string();
  }

  std::string output;
  http_response_headers_->GetNormalizedHeaders(&output);
  return output;
}

std::string XMLHttpRequest::status_text() {
  // http://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#the-statustext-attribute
  if (state_ == kUnsent || state_ == kOpened || error_) {
    return std::string();
  }

  return http_response_headers_->GetStatusText();
}

void XMLHttpRequest::AddExtraRef() {
  AddRef();
  DCHECK(!did_add_ref_);
  did_add_ref_ = true;
}

void XMLHttpRequest::ReleaseExtraRef() {
  DCHECK(did_add_ref_);
  did_add_ref_ = false;
  Release();
}

void XMLHttpRequest::ChangeState(XMLHttpRequest::State new_state) {
  if (state_ == new_state) {
    return;
  }

  state_ = new_state;
  if (state_ != kUnsent) {
    DispatchEvent(new dom::Event(
        dom::EventNames::GetInstance()->readystatechange()));
  }
}

void XMLHttpRequest::FireProgressEvent(const std::string& event_name) {
  DispatchEvent(new dom::ProgressEvent(event_name));
}

void XMLHttpRequest::OnReceived(const char* data, size_t size) {
  // TODO(***REMOVED***): What to do with data/size here?
  NOTIMPLEMENTED();
}

void XMLHttpRequest::OnDone() {
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
  if (!url_fetcher->GetResponseAsString(&response_text_)) {
    DLOG(ERROR) << "URL fetcher failed to get string response.";
  }

  // TODO(***REMOVED***): We may need to switch to URLRequest so we can
  // have more control. We can't call most methods on url_fetcher
  // until it delivers an OnURLFetchComplete() callback.
  // So we can't really do these state changes any earlier.
  ChangeState(kHeadersReceived);
  ChangeState(kLoading);

  // Undo the ref we added in Send()
  ReleaseExtraRef();

  ChangeState(kDone);
}

void XMLHttpRequest::OnError(const std::string& error) {
  Abort();
  ChangeState(kDone);
}


}  // namespace xhr
}  // namespace cobalt
