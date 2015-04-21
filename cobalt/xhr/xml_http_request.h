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

#include "base/optional.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/xhr/xml_http_request_event_target.h"
#include "googleurl/src/gurl.h"
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

  void Open(const std::string& method, const std::string& url);

  // Must be called after open(), but before send().
  void SetRequestHeader(const std::string& header, const std::string& value);

  // Override the MIME type returned by the server.
  // Call before Send(), otherwise throws InvalidStateError.
  void OverrideMimeType(const std::string& mime_type);

  void Send();
  void Send(const base::optional<std::string>& request_body);
  void Abort();
  base::optional<std::string> GetResponseHeader(const std::string& header);
  std::string GetAllResponseHeaders();

  std::string response_text();
  base::optional<std::string> response_xml();
  std::string status_text();
  // TODO(***REMOVED***): This should return a union type of all possible
  // response objects. For now we only support response_text().
  base::optional<std::string> response();

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
  void OnReceived(const char* data, size_t size) OVERRIDE;
  void OnDone() OVERRIDE;
  void OnError(const std::string& error) OVERRIDE;

  // Accessors mainly for testing, not part of the IDL interface.
  const GURL& request_url() const { return request_url_; }
  bool error() const { return error_; }
  bool sent() const { return sent_; }
  const std::string& mime_type_override() const { return mime_type_override_; }

  DEFINE_WRAPPABLE_TYPE(XMLHttpRequest);

 protected:
  ~XMLHttpRequest() OVERRIDE;

 private:
  // Update the internal ready state and fire events.
  void ChangeState(State new_state);

  // Ref this object to prevent it from being destroyed while
  // there are active requests in flight.
  void AddExtraRef();
  void ReleaseExtraRef();

  // Dispatch a progress event to any listeners.
  void FireProgressEvent(const std::string& event_name);

  scoped_refptr<dom::EventListener> on_ready_state_change_listener_;
  scoped_ptr<loader::NetFetcher> net_fetcher_;
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  std::string response_text_;
  std::string mime_type_override_;
  GURL base_url_;
  GURL request_url_;

  dom::DOMSettings* settings_;
  State state_;
  ResponseTypeCode response_type_;
  int32 timeout_ms_;
  net::URLFetcher::RequestType method_;
  int http_status_;
  bool with_credentials_;
  bool error_;
  bool sent_;

  // For debugging our reference count manipulations.
  bool did_add_ref_;

  DISALLOW_COPY_AND_ASSIGN(XMLHttpRequest);
};

}  // namespace xhr
}  // namespace cobalt

#endif  // XHR_XML_HTTP_REQUEST_H_
