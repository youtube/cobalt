// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/app_key_internal.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/types.h"
#include "third_party/modp_b64/modp_b64.h"

namespace starboard {
namespace loader_app {
namespace {

// For general information on the URL format see:
//
//  https://en.wikipedia.org/wiki/URL
//
// For general information on the URL character set see:
//
//  https://developers.google.com/maps/documentation/urls/url-encoding

// Best-effort attempt to find and strip the username and password from a URL,
// expecting that the format is conforming:
//
//  ... ://<USERNAME>:<PASSWORD>@ ...
void StripUserInfo(std::string* url) {
  if (!url)
    return;

  const size_t colon_slash_slash = url->find("://");

  // The user information is preceded by "://".
  if (colon_slash_slash == std::string::npos)
    return;

  const size_t at = url->find_first_of('@');

  // The user information is followed by "@".
  if (at == std::string::npos)
    return;

  // The URL is malformed.
  if (colon_slash_slash >= at)
    return;

  url->erase(colon_slash_slash + 3, at - (colon_slash_slash + 2));
}

// Best-effort attempt to find and strip the query from a URL, expecting that
// the format is conforming:
//
//  ... ?<QUERY> ...
void StripQuery(std::string* url) {
  const size_t question_mark = url->find_first_of('?');

  // The query is preceded by "?".
  if (question_mark == std::string::npos)
    return;

  url->erase(question_mark, url->size() - question_mark);
}

// Best-effort attempt to find and strip the fragment from a URL, expecting that
// the format is conforming:
//
//  ... #<FRAGMENT>
void StripFragment(std::string* url) {
  const size_t pound = url->find_first_of('#');

  // The fragment is preceded by "#".
  if (pound == std::string::npos)
    return;

  url->erase(pound, url->size() - pound);
}

}  // namespace

std::string ExtractAppKey(const std::string& url) {
  if (url.empty())
    return "";

  std::string output = url;

  StripUserInfo(&output);
  StripQuery(&output);
  StripFragment(&output);

  return output;
}

std::string EncodeAppKey(const std::string& app_key) {
  std::string output;

  // modp_b64_encode_len includes room for the null byte.
  output.resize(modp_b64_encode_len(app_key.size()));

  // modp_b64_encode_len() returns at least 1, so output[0] is safe to use.
  const size_t output_size = modp_b64_encode(
      &(output[0]), reinterpret_cast<const char*>(app_key.data()),
      app_key.size());

  output.resize(output_size);

  // Replace the '+' and '/' characters with '-' and '_' so that they are safe
  // to use in a URL and with the filesystem.
  std::replace(output.begin(), output.end(), '+', '-');
  std::replace(output.begin(), output.end(), '/', '_');

  return output;
}

}  // namespace loader_app
}  // namespace starboard
