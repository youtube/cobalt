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

#ifndef COBALT_LOADER_FETCHER_H_
#define COBALT_LOADER_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/loader/loader_types.h"
#include "net/http/http_response_headers.h"
#include "net/base/load_timing_info.h"
#include "url/gurl.h"

namespace cobalt {
namespace loader {

// https://fetch.spec.whatwg.org/#concept-request-mode
// Right now Cobalt only needs two modes.
// We mix credentials mode with request mode for simplicity.
// https://fetch.spec.whatwg.org/#concept-request-credentials-mode
enum RequestMode {
  kNoCORSMode,
  kCORSModeOmitCredentials,
  kCORSModeSameOriginCredentials,
  kCORSModeIncludeCredentials,
};

class Fetcher {
 public:
  class Handler {
   public:
    // The function will be called by supported Fetcher (like NetFetcher) before
    // any OnReceived() is called so the Handler can preview the response.
    virtual LoadResponseType OnResponseStarted(
        Fetcher* fetcher,
        const scoped_refptr<net::HttpResponseHeaders>& headers)
        WARN_UNUSED_RESULT {
      return kLoadResponseContinue;
    }
    virtual void OnReceived(Fetcher* fetcher, const char* data,
                            size_t size) = 0;
    virtual void OnDone(Fetcher* fetcher) = 0;
    virtual void OnError(Fetcher* fetcher, const std::string& error) = 0;

    // By default, |OnReceivedPassed| forwards the std::unique_ptr<std::string>
    // data into |OnReceived|.  Implementations have the opportunity to hold
    // onto the std::unique_ptr through overriding |OnReceivedPassed|.
    virtual void OnReceivedPassed(Fetcher* fetcher,
                                  std::unique_ptr<std::string> data) {
      OnReceived(fetcher, data->data(), data->length());
    }

    virtual void SetLoadTimingInfo(
        const net::LoadTimingInfo& timing_info) {
      if (!load_timing_info_callback_.is_null()) {
        load_timing_info_callback_.Run(timing_info);
      }
    }

    virtual void SetLoadTimingInfoCallback(
        const base::Callback<void(const net::LoadTimingInfo&)>& callback) {
      load_timing_info_callback_ = callback;
    }

   protected:
    Handler() {}
    virtual ~Handler() {}
    base::Callback<void(const net::LoadTimingInfo&)>
        load_timing_info_callback_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Handler);
  };

  // Concrete Fetcher subclass should start fetching immediately in constructor.
  explicit Fetcher(Handler* handler) : handler_(handler) {}

  // Concrete Fetcher subclass should cancel fetching in destructor.
  virtual ~Fetcher() = 0;

  // Indicates whether the resource is cross-origin.
  virtual Origin last_url_origin() const { return Origin(); }

  // Whether or not the fetcher failed from an error that is considered
  // transient, indicating that the same fetch may later succeed.
  virtual bool did_fail_from_transient_error() const { return false; }

  virtual void SetLoadTimingInfoCallback(
      const base::Callback<void(const net::LoadTimingInfo&)>& callback) {
    handler_->SetLoadTimingInfoCallback(callback);
  }

 protected:
  Handler* handler() const { return handler_; }

 private:
  Handler* handler_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCHER_H_
