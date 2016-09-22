/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_XHR_XML_HTTP_REQUEST_H_
#define COBALT_XHR_XML_HTTP_REQUEST_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/timer.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/dom/csp_delegate.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/script/union_type.h"
#include "cobalt/xhr/xhr_response_data.h"
#include "cobalt/xhr/xml_http_request_event_target.h"
#include "cobalt/xhr/xml_http_request_upload.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace dom {
class DOMSettings;
}

namespace script {
class EnvironmentSettings;
}

namespace xhr {

class XMLHttpRequest : public XMLHttpRequestEventTarget,
                       net::URLFetcherDelegate {
 public:
  // Note: This is expected to be a DOMSettings object, but we declare it as
  // EnvironmentSettings so that JSC doesn't need to know about dom.
  explicit XMLHttpRequest(script::EnvironmentSettings*);

  typedef script::UnionType2<std::string, scoped_refptr<dom::ArrayBuffer> >
      ResponseType;

  typedef script::UnionType2<std::string, scoped_refptr<dom::ArrayBufferView> >
      RequestBodyType;

  enum State {
    kUnsent = 0,
    kOpened = 1,
    kHeadersReceived = 2,
    kLoading = 3,
    kDone = 4,
  };

  enum ResponseTypeCode {
    kDefault,
    kText,
    kJson,
    kDocument,
    kBlob,
    kArrayBuffer,
    kResponseTypeCodeMax,
  };

  enum RequestErrorType {
    kNetworkError,
    kAbortError,
    kTimeoutError,
  };

  const EventListenerScriptObject* onreadystatechange() const {
    return GetAttributeEventListener(base::Tokens::readystatechange());
  }
  void set_onreadystatechange(const EventListenerScriptObject& event_listener) {
    SetAttributeEventListener(base::Tokens::readystatechange(), event_listener);
  }

  void Abort();
  void Open(const std::string& method, const std::string& url,
            script::ExceptionState* exception_state) {
    return Open(method, url, true, base::nullopt, base::nullopt,
                exception_state);
  }
  void Open(const std::string& method, const std::string& url, bool async,
            script::ExceptionState* exception_state) {
    return Open(method, url, async, base::nullopt, base::nullopt,
                exception_state);
  }
  void Open(const std::string& method, const std::string& url, bool async,
            const base::optional<std::string>& username,
            script::ExceptionState* exception_state) {
    return Open(method, url, async, username, base::nullopt, exception_state);
  }
  void Open(const std::string& method, const std::string& url, bool async,
            const base::optional<std::string>& username,
            const base::optional<std::string>& password,
            script::ExceptionState* exception_state);

  // Must be called after open(), but before send().
  void SetRequestHeader(const std::string& header, const std::string& value,
                        script::ExceptionState* exception_state);

  // Override the MIME type returned by the server.
  // Call before Send(), otherwise throws InvalidStateError.
  void OverrideMimeType(const std::string& mime_type,
                        script::ExceptionState* exception_state);

  void Send(script::ExceptionState* exception_state);
  void Send(const base::optional<RequestBodyType>& request_body,
            script::ExceptionState* exception_state);

  base::optional<std::string> GetResponseHeader(const std::string& header);
  std::string GetAllResponseHeaders();

  const std::string& response_text(script::ExceptionState* exception_state);
  scoped_refptr<dom::Document> response_xml(
      script::ExceptionState* exception_state);
  std::string status_text();
  base::optional<ResponseType> response(
      script::ExceptionState* exception_state);

  int ready_state() const { return static_cast<int>(state_); }
  int status() const;
  std::string status_text() const;
  void set_response_type(const std::string& reponse_type,
                         script::ExceptionState* exception_state);
  std::string response_type(script::ExceptionState* exception_state) const;

  uint32 timeout() const { return timeout_ms_; }
  void set_timeout(uint32 timeout);
  bool with_credentials(script::ExceptionState* exception_state) const;
  void set_with_credentials(bool b, script::ExceptionState* exception_state);

  scoped_refptr<XMLHttpRequestUpload> upload();

  static void set_verbose(bool verbose) { verbose_ = verbose; }
  static bool verbose() { return verbose_; }
  // net::URLFetcherDelegate interface
  void OnURLFetchResponseStarted(const net::URLFetcher* source) OVERRIDE;
  void OnURLFetchDownloadData(const net::URLFetcher* source,
                              scoped_ptr<std::string> download_data) OVERRIDE;
  void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  bool ShouldSendDownloadData() OVERRIDE { return true; }

  void OnURLFetchUploadProgress(const net::URLFetcher* source, int64 current,
                                int64 total) OVERRIDE;

  // Called from bindings layer to tie objects' lifetimes to this XHR instance.
  XMLHttpRequestUpload* upload_or_null() { return upload_.get(); }
  dom::ArrayBuffer* response_array_buffer_or_null() {
    return response_array_buffer_.get();
  }

  friend std::ostream& operator<<(std::ostream& os, const XMLHttpRequest& xhr);
  DEFINE_WRAPPABLE_TYPE(XMLHttpRequest);

 protected:
  ~XMLHttpRequest() OVERRIDE;

  // Return the CSP delegate from the Settings object.
  // virtual for use by tests.
  virtual dom::CspDelegate* csp_delegate() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(XhrTest, GetResponseHeader);
  FRIEND_TEST_ALL_PREFIXES(XhrTest, InvalidMethod);
  FRIEND_TEST_ALL_PREFIXES(XhrTest, Open);
  FRIEND_TEST_ALL_PREFIXES(XhrTest, OverrideMimeType);
  FRIEND_TEST_ALL_PREFIXES(XhrTest, SetRequestHeader);
  // Cancel any inflight request and set error flag.
  void TerminateRequest();
  // Dispatch events based on the type of error.
  void HandleRequestError(RequestErrorType request_error);
  // Callback when timeout fires.
  void OnTimeout();
  // Starts the timeout timer running.
  void StartTimer(base::TimeDelta time_since_send);
  // Update the internal ready state and fire events.
  void ChangeState(State new_state);
  // Return array buffer response body as an ArrayBuffer.
  scoped_refptr<dom::ArrayBuffer> response_array_buffer();

  void UpdateProgress();

  // Prevent this object from being destroyed while there are active requests
  // in flight.
  // https://www.w3.org/TR/2014/WD-XMLHttpRequest-20140130/#garbage-collection
  void PreventGarbageCollection();
  void AllowGarbageCollection();
  void StartRequest(const std::string& request_body);

  // Accessors / mutators for testing.
  const GURL& request_url() const { return request_url_; }
  bool error() const { return error_; }
  bool sent() const { return sent_; }
  const std::string& mime_type_override() const { return mime_type_override_; }
  const net::HttpRequestHeaders& request_headers() const {
    return request_headers_;
  }
  void set_state(State state) { state_ = state; }
  void set_http_response_headers(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
    http_response_headers_ = response_headers;
  }

  scoped_refptr<dom::Document> GetDocumentResponseEntityBody();
  void XMLDecoderErrorCallback(const std::string& error);

  base::ThreadChecker thread_checker_;

  scoped_ptr<net::URLFetcher> url_fetcher_;
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  XhrResponseData response_body_;
  scoped_refptr<dom::ArrayBuffer> response_array_buffer_;
  scoped_refptr<XMLHttpRequestUpload> upload_;

  std::string mime_type_override_;
  GURL base_url_;
  GURL request_url_;
  net::HttpRequestHeaders request_headers_;

  // For handling send() timeout.
  base::OneShotTimer<XMLHttpRequest> timer_;
  base::TimeTicks send_start_time_;

  // Time to throttle progress notifications.
  base::TimeTicks last_progress_time_;
  base::TimeTicks upload_last_progress_time_;

  // All members requiring initialization are grouped below.
  dom::DOMSettings* settings_;
  State state_;
  ResponseTypeCode response_type_;
  uint32 timeout_ms_;
  net::URLFetcher::RequestType method_;
  int http_status_;
  bool with_credentials_;
  bool error_;
  bool sent_;
  bool stop_timeout_;
  bool upload_complete_;

  // For debugging our reference count manipulations.
  bool did_add_ref_;

  static bool verbose_;
  // Unique ID for debugging.
  int xhr_id_;

  bool has_xml_decoder_error_;

  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequest);
};

std::ostream& operator<<(std::ostream& out, const XMLHttpRequest& xhr);

}  // namespace xhr
}  // namespace cobalt

#endif  // COBALT_XHR_XML_HTTP_REQUEST_H_
