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

#include "cobalt/loader/origin.h"

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
namespace loader {

Origin::Origin() : is_opaque_(true) {}

Origin::Origin(const GURL& url) : is_opaque_(false) {
  if (url.is_valid() && url.has_scheme() && url.has_host()) {
    if (url.SchemeIs("blob")) {
      // Let path_url be the result of parsing URL's path.
      // Return a new opaque origin, if url is failure, and url's origin
      // otherwise.
      GURL path_url(url.path());
      if (path_url.is_valid() && path_url.has_host() && path_url.has_scheme() &&
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

std::string Origin::SerializedOrigin() const {
  if (is_opaque_) {
    return "null";
  }
  return origin_str_;
}

bool Origin::operator==(const Origin& rhs) const {
  if (is_opaque_ || rhs.is_opaque_) {
    return false;
  } else {
    return origin_str_ == rhs.origin_str_;
  }
}

}  // namespace loader
}  // namespace cobalt
