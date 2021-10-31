// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

namespace cobalt {
namespace loader {

bool Origin::Tuple::operator==(const Tuple& rhs) const {
  return scheme == rhs.scheme && host == rhs.host && port == rhs.port;
}

// static
base::Optional<Origin::Tuple> Origin::GetTupleFromURL(
    const GURL& url, bool recurse_into_blob_paths) {
  if (!url.is_valid()) return base::nullopt;
  if (!url.has_scheme()) return base::nullopt;
  if (!url.has_host() && !url.SchemeIs("file")) return base::nullopt;

  if (url.SchemeIs("blob") && recurse_into_blob_paths) {
    // In the case of blobs, recurse into their path to determine the origin
    // tuple.
    return GetTupleFromURL(GURL(url.path()), false);
  }

  if (!url.SchemeIs("https") && !url.SchemeIs("http") && !url.SchemeIs("ftp") &&
      !url.SchemeIs("ws") && !url.SchemeIs("wss") && !url.SchemeIs("file")) {
    return base::nullopt;
  }

  return Origin::Tuple{url.scheme(), url.host(),
                       url.has_port() ? url.port() : ""};
}

Origin::Origin() {}

Origin::Origin(const GURL& url) : tuple_(GetTupleFromURL(url, true)) {}

std::string Origin::SerializedOrigin() const {
  if (tuple_) {
    return tuple_->scheme + "://" + tuple_->host +
           (tuple_->port.empty() ? "" : ":" + tuple_->port);
  } else {
    return "null";
  }
}

bool Origin::operator==(const Origin& rhs) const {
  if (!tuple_ || !rhs.tuple_) {
    return false;
  } else {
    return *tuple_ == *rhs.tuple_;
  }
}

}  // namespace loader
}  // namespace cobalt
