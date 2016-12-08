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

#ifndef COBALT_LOADER_FETCHER_H_
#define COBALT_LOADER_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "cobalt/loader/loader_types.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"

namespace cobalt {
namespace loader {

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
      UNREFERENCED_PARAMETER(fetcher);
      UNREFERENCED_PARAMETER(headers);
      return kLoadResponseContinue;
    }
    virtual void OnReceived(Fetcher* fetcher, const char* data,
                            size_t size) = 0;
    virtual void OnDone(Fetcher* fetcher) = 0;
    virtual void OnError(Fetcher* fetcher, const std::string& error) = 0;

   protected:
    Handler() {}
    virtual ~Handler() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Handler);
  };

  // Concrete Fetcher subclass should start fetching immediately in constructor.
  explicit Fetcher(Handler* handler) : handler_(handler) {}

  // Concrete Fetcher subclass should cancel fetching in destructor.
  virtual ~Fetcher() = 0;

 protected:
  Handler* handler() const { return handler_; }

 private:
  Handler* handler_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_FETCHER_H_
