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

#include "cobalt/loader/embedded_fetcher.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cobalt/base/localized_strings.h"
#include "cobalt/loader/embedded_resources.h"  // Generated.

namespace cobalt {
namespace loader {

namespace {
bool EmbeddedURLToKey(const GURL& url, std::string* key) {
  DCHECK(url.is_valid() && url.SchemeIs(kEmbeddedScheme));
  *key = url.path();
  DCHECK_EQ('/', (*key)[0]);
  DCHECK_EQ('/', (*key)[1]);
  (*key).erase(0, 2);
  return !key->empty();
}
}  // namespace

const char kEmbeddedScheme[] = "h5vcc-embedded";

EmbeddedFetcher::EmbeddedFetcher(const GURL& url,
                                 const csp::SecurityCallback& security_callback,
                                 Handler* handler, const Options& options)
    : Fetcher(handler),
      url_(url),
      security_callback_(security_callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Embedded assets are available in-memory and can be loaded synchronously.
  Fetch(options);
}

EmbeddedFetcher::~EmbeddedFetcher() {}

void EmbeddedFetcher::Fetch(const Options& options) {
  if (!IsAllowedByCsp()) {
    std::string msg(base::StringPrintf("URL %s rejected by security policy.",
                                       url_.spec().c_str()));
    handler()->OnError(this, msg);
    return;
  }

  std::string key;
  if (!EmbeddedURLToKey(url_, &key)) {
    std::string msg(
        base::StringPrintf("Invalid embedded URL: %s.", url_.spec().c_str()));
    handler()->OnError(this, msg);
    return;
  }

  GetEmbeddedData(key, options.start_offset, options.bytes_to_read);
}

void EmbeddedFetcher::GetEmbeddedData(const std::string& key,
                                      int64 start_offset, int64 bytes_to_read) {
  const char kDataNotFoundError[] = "Embedded data not found.";

  GeneratedResourceMap resource_map;
  LoaderEmbeddedResources::GenerateMap(resource_map);

  if (resource_map.find(key) == resource_map.end()) {
    handler()->OnError(this, kDataNotFoundError);
    return;
  }

  FileContents file_contents = resource_map[key];
  const char* data = reinterpret_cast<const char*>(file_contents.data);
  size_t size = static_cast<size_t>(file_contents.size);
  data += start_offset;

  // If the key is a template file, localized the strings in the file data.
  if (base::EndsWith(key, ".template", base::CompareCase::SENSITIVE)) {
    std::string output_file(data);
    LocalizeFileData(&output_file);
    const char* final_data = output_file.c_str();
    size = std::min(static_cast<size_t>(output_file.size()),
                    static_cast<size_t>(bytes_to_read));
    handler()->OnReceived(this, final_data, size);
  } else {
    size = std::min(size, static_cast<size_t>(bytes_to_read));
    handler()->OnReceived(this, data, size);
  }
  handler()->OnDone(this);
}

bool EmbeddedFetcher::IsAllowedByCsp() {
  bool did_redirect = false;
  if (security_callback_.is_null() ||
      security_callback_.Run(url_, did_redirect)) {
    return true;
  } else {
    return false;
  }
}

void EmbeddedFetcher::LocalizeFileData(std::string* output_file) {
  base::LocalizedStrings* localized_strings =
      base::LocalizedStrings::GetInstance();
  std::string opening_delimiter = "[[";
  std::vector<std::string> parts =
      base::SplitStringUsingSubstr(*output_file, opening_delimiter,
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t idx = 1; idx < parts.size(); ++idx) {
    std::string& value = parts[idx];
    std::string closing_delimiter = "]]";
    std::vector<std::string> key_result = base::SplitStringUsingSubstr(
        value, closing_delimiter, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    size_t key_results = key_result.size();

    if (key_results > 1) {
      std::string& message_postfix = key_result[1];
      std::string localized_message =
          localized_strings->GetString(key_result[0], "");
      value = localized_message + message_postfix;
    } else {
      value = key_result[0];
    }
  }

  *output_file = base::JoinString(parts, std::string());
}

}  // namespace loader
}  // namespace cobalt
