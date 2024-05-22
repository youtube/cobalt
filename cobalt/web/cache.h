// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_CACHE_H_
#define COBALT_WEB_CACHE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace cobalt {
namespace web {

class Cache : public script::Wrappable {
 public:
  Cache() = default;

  // Web API: Cache
  //
  script::Handle<script::Promise<script::Handle<script::ValueHandle>>> Match(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request);
  script::HandlePromiseVoid Add(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request);
  script::HandlePromiseVoid Put(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request,
      const script::ValueHandleHolder& response);
  script::HandlePromiseBool Delete(
      script::EnvironmentSettings* environment_settings,
      const script::ValueHandleHolder& request);
  script::Handle<script::Promise<script::Handle<script::ValueHandle>>> Keys(
      script::EnvironmentSettings* environment_settings);

  DEFINE_WRAPPABLE_TYPE(Cache);

 private:
  class Fetcher : public net::URLRequest::Delegate {
   public:
    Fetcher(network::NetworkModule* network_module, const GURL& url,
            base::OnceCallback<void(bool)> callback);

    // net::URLRequest::Delegate implementations.
    void OnResponseStarted(net::URLRequest* request, int net_error) override;
    void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

    const std::string& mime_type() const { return mime_type_; }
    GURL url() const { return url_; }
    int response_code() const { return response_code_; }
    const std::string& status_text() const { return status_text_; }
    base::Value::List headers() { return std::move(headers_); }
    std::vector<uint8_t> BufferToVector() const;
    std::string BufferToString() const;

   private:
    void Notify(bool success);
    void OnDone(bool success);
    void ReadResponse(net::URLRequest* request);
    void Start();

    network::NetworkModule* network_module_;
    GURL url_;
    base::OnceCallback<void(bool)> callback_;
    std::unique_ptr<net::URLRequest> request_;
    std::string mime_type_;
    scoped_refptr<net::GrowableIOBuffer> buffer_;
    int buffer_size_;
    int response_code_;
    base::Value::List headers_;
    std::string status_text_;
  };

  void PerformAdd(
      script::EnvironmentSettings* environment_settings,
      std::unique_ptr<script::ValueHandleHolder::Reference> request_reference,
      std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference);
  void OnFetchCompleted(uint32_t key, bool success);
  void OnFetchCompletedMainThread(uint32_t key, bool success);

  std::map<uint32_t, std::unique_ptr<Fetcher>> fetchers_;
  mutable base::Lock fetcher_lock_;
  std::map<uint32_t, std::pair<std::vector<std::unique_ptr<
                                   script::ValuePromiseVoid::Reference>>,
                               script::EnvironmentSettings*>>
      fetch_contexts_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_CACHE_H_
