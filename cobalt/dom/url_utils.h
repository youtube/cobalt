// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_URL_UTILS_H_
#define COBALT_DOM_URL_UTILS_H_

#include <string>

#include "base/callback.h"
#include "googleurl/src/gurl.h"

namespace {
// Returns true if the url can have non-opaque origin.
// Blob URLs needs to be pre-processed.
bool NonBlobURLCanHaveTupleOrigin(const GURL& url) {
  if (url.SchemeIs("https")) {
    return true;
  } else if (url.SchemeIs("http")) {
    return true;
  } else if (url.SchemeIs("ftp")) {
    return true;
  } else if (url.SchemeIs("ws")) {
    return true;
  } else if (url.SchemeIs("wss")) {
    return true;
  }
  return false;
}
// Returns false if url should be opaque.
// Otherwise, extract origin tuple from the url and assemble them as a string
// for easier access and comparison.
bool NonBlobURLGetOriginStr(const GURL& url, std::string* output) {
  if (!NonBlobURLCanHaveTupleOrigin(url)) {
    return false;
  }
  *output = url.scheme() + "://" + url.host() +
            (url.has_port() ? ":" + url.port() : "");
  return true;
}
}  // namespace

namespace cobalt {
namespace dom {

// Utility methods and properties that help work with URL.
//   https://www.w3.org/TR/2014/WD-url-1-20141209/#dom-urlutils
// NOTE: This interface has NoInterfaceObject prefix in IDL, therefore
// it is not a wrappable. Other interfaces that implement it can own or inherit
// from it to use it as a utility.
// It is possible to make the URL invalid by using the setters. The user should
// check url().is_valid().
// If a custom update steps callback is provided, this URLUtils object will not
// update the url_ field on any setters, in case the object is destroyed in the
// callback. The callback should call set_url if necessary.
class URLUtils {
 public:
  // https://html.spec.whatwg.org/multipage/origin.html#concept-origin
  class Origin {
   public:
    // To create an opaque origin, use Origin().
    Origin() : is_opaque_(true) {}
    // Initialize an origin to the url's origin.
    // https://url.spec.whatwg.org/#concept-url-origin
    Origin(const GURL& url) : is_opaque_(false) {
      if (url.is_valid() && url.has_scheme() && url.has_host()) {
        if (url.SchemeIs("blob")) {
          // Let path_url be the result of parsing URL's path.
          // Return a new opaque origin, if url is failure, and url's origin
          // otherwise.
          GURL path_url(url.path());
          if (path_url.is_valid() && path_url.has_host() &&
              path_url.has_scheme() &&
              NonBlobURLGetOriginStr(path_url, &origin_str_)) {
            return;
          }
        } else if (NonBlobURLGetOriginStr(url, &origin_str_)) {
          // Assign a tuple origin if given url is allowed to have one.
          return;
        }
      }
      // Othwise, return a new opaque origin.
      is_opaque_ = true;
    }
    // https://html.spec.whatwg.org/multipage/origin.html#ascii-serialisation-of-an-origin
    std::string SerializedOrigin() const {
      if (is_opaque_) {
        return "null";
      }
      return origin_str_;
    }
    // Only document has an origin and no elements inherit document's origin, so
    // opaque origin comparison can always return false.
    // https://html.spec.whatwg.org/multipage/origin.html#same-origin
    bool operator==(const Origin& rhs) {
      if (is_opaque_ || rhs.is_opaque_) {
        return false;
      } else {
        return origin_str_ == rhs.origin_str_;
      }
    }
    // Returns true if two origins are different(cross-origin).
    bool operator!=(const Origin& rhs) { return !(*this == rhs); }

   private:
    bool is_opaque_;
    std::string origin_str_;
  };

  typedef base::Callback<void(const std::string&)> UpdateStepsCallback;

  explicit URLUtils(const GURL& url, bool is_opaque = false);
  explicit URLUtils(const UpdateStepsCallback& update_steps)
      : update_steps_(update_steps), origin_(Origin()) {}
  URLUtils(const GURL& url, const UpdateStepsCallback& update_steps,
           bool is_opaque = false);

  // From the spec: URLUtils.
  //
  std::string href() const;
  void set_href(const std::string& href);

  std::string protocol() const;
  void set_protocol(const std::string& protocol);

  std::string host() const;
  void set_host(const std::string& host);

  std::string hostname() const;
  void set_hostname(const std::string& hostname);

  std::string port() const;
  void set_port(const std::string& port);

  std::string pathname() const;
  void set_pathname(const std::string& pathname);

  std::string hash() const;
  void set_hash(const std::string& hash);

  std::string search() const;
  void set_search(const std::string& search);

  std::string origin() const;

  // Custom, not in any spec.
  //
  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  const Origin& OriginObject() const { return origin_; }

 private:
  // From the spec: URLUtils.
  void RunPreUpdateSteps(const GURL& new_url, const std::string& value);

  GURL url_;
  UpdateStepsCallback update_steps_;
  Origin origin_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_URL_UTILS_H_
