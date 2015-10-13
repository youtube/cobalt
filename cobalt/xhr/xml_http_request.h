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

#ifndef XHR_XML_HTTP_REQUEST_H_
#define XHR_XML_HTTP_REQUEST_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/timer.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/array_buffer_view.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/script/union_type.h"
#include "cobalt/xhr/xml_http_request_event_target.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace dom {
class DOMSettings;
}

namespace script {
class EnvironmentSettings;
}

namespace xhr {

class XMLHttpRequest : public XMLHttpRequestEventTarget,
                       loader::Fetcher::Handler {
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

  void Abort();
  void Open(const std::string& method, const std::string& url);

  // Must be called after open(), but before send().
  void SetRequestHeader(const std::string& header, const std::string& value);

  // Override the MIME type returned by the server.
  // Call before Send(), otherwise throws InvalidStateError.
  void OverrideMimeType(const std::string& mime_type);

  void Send();
  void Send(const base::optional<RequestBodyType>& request_body);

  base::optional<std::string> GetResponseHeader(const std::string& header);
  std::string GetAllResponseHeaders();

  std::string response_text();
  base::optional<std::string> response_xml();
  std::string status_text();
  base::optional<ResponseType> response();

  int ready_state() const { return static_cast<int>(state_); }
  int status() const;
  std::string status_text() const;
  void set_response_type(const std::string& reponse_type);
  std::string response_type() const;

  uint32 timeout() const { return timeout_ms_; }
  void set_timeout(uint32 timeout);
  bool with_credentials() const { return with_credentials_; }
  void set_with_credentials(bool b);

  scoped_refptr<dom::EventListener> onreadystatechange() const {
    return on_ready_state_change_listener_;
  }
  void set_onreadystatechange(
      const scoped_refptr<dom::EventListener>& listener);

  // loader::Fetcher::Handler interface
  void OnReceived(loader::Fetcher* fetcher, const char* data,
                  size_t size) OVERRIDE;
  void OnDone(loader::Fetcher* fetcher) OVERRIDE;
  void OnError(loader::Fetcher* fetcher, const std::string& error) OVERRIDE;

  // Accessors mainly for testing, not part of the IDL interface.
  const GURL& request_url() const { return request_url_; }
  bool error() const { return error_; }
  bool sent() const { return sent_; }
  const std::string& mime_type_override() const { return mime_type_override_; }
  const net::HttpRequestHeaders& request_headers() const {
    return request_headers_;
  }

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequest);

 protected:
  ~XMLHttpRequest() OVERRIDE;

 private:
  enum RequestErrorType {
    kNetworkError,
    kAbortError,
    kTimeoutError,
  };

  // Dispatch a progress event to any listeners.
  void FireProgressEvent(const std::string& event_name);
  void FireProgressEvent(const std::string& event_name, uint64 loaded,
                         uint64 total, bool length_computable);
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

  // Ref this object to prevent it from being destroyed while
  // there are active requests in flight.
  void AddExtraRef();
  void ReleaseExtraRef();

  base::ThreadChecker thread_checker_;

  scoped_refptr<dom::EventListener> on_ready_state_change_listener_;
  scoped_ptr<loader::NetFetcher> net_fetcher_;
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  std::vector<uint8> response_body_;

  std::string mime_type_override_;
  GURL base_url_;
  GURL request_url_;
  net::HttpRequestHeaders request_headers_;

  // For handling send() timeout.
  base::OneShotTimer<XMLHttpRequest> timer_;
  base::Time send_start_time_;

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

  // For debugging our reference count manipulations.
  bool did_add_ref_;

  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequest);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // XHR_XML_HTTP_REQUEST_H_
