// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/base/get_application_key.h"

#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace base {

namespace {
// The Application Origin is like the origin, but it supports nonstandard
// schemes, and preserves the path.
GURL GetApplicationOrigin(const GURL& url) {
  if (!url.is_valid()) {
    return GURL();
  }

  url::Replacements<char> replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();

  return url.ReplaceComponents(replacements);
}
}  // namespace

Optional<std::string> GetApplicationKey(const GURL& url) {
  if (!url.is_valid()) {
    return base::nullopt;
  }

  std::string raw_url = GetApplicationOrigin(url).spec();
  std::string encoded_url;
  base::Base64Encode(raw_url, &encoded_url);

  // Make web-safe.
  base::ReplaceChars(encoded_url, "/", "_", &encoded_url);
  base::ReplaceChars(encoded_url, "+", "-", &encoded_url);

  return encoded_url;
}

}  // namespace base
