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

#ifndef COBALT_BROWSER_URL_HANDLER_H_
#define COBALT_BROWSER_URL_HANDLER_H_

#include "base/callback.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

class BrowserModule;

// Helper class that registers a URL handler callback with the specified
// browser module.
class URLHandler {
 public:
  // Type for a callback to potentially handle a URL before it is used to
  // initialize a new WebModule. The callback should return true if it handled
  // the URL, false otherwise.
  typedef base::Callback<bool(const GURL& url)> URLHandlerCallback;

  URLHandler(BrowserModule* browser_module, const URLHandlerCallback& callback);
  ~URLHandler();

 protected:
  BrowserModule* browser_module() const { return browser_module_; }

 private:
  BrowserModule* browser_module_;
  URLHandlerCallback callback_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_URL_HANDLER_H_
