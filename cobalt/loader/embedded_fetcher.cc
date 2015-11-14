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

#include "cobalt/loader/embedded_fetcher.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/loader/embedded_resources.h"  // Generated.

namespace cobalt {
namespace loader {

EmbeddedFetcher::EmbeddedFetcher(const std::string& key, Handler* handler,
                                 const Options& options)
    : Fetcher(handler), key_(key) {
  // Post the data access as a separate task so that whatever is going to
  // handle the data gets a chance to get ready for it.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedFetcher::GetEmbeddedData, base::Unretained(this),
                 options.start_offset, options.bytes_to_read));
}

void EmbeddedFetcher::GetEmbeddedData(int64 start_offset, int64 bytes_to_read) {
  const char kDataNotFoundError[] = "Embedded data not found.";

  GeneratedResourceMap resource_map;
  LoaderEmbeddedResources::GenerateMap(resource_map);

  if (resource_map.find(key_) == resource_map.end()) {
    handler()->OnError(this, kDataNotFoundError);
  }

  FileContents file_contents = resource_map[key_];
  const char* data = reinterpret_cast<const char*>(file_contents.data);
  size_t size = static_cast<size_t>(file_contents.size);
  data += start_offset;
  size = std::min(size, static_cast<size_t>(bytes_to_read));

  handler()->OnReceived(this, data, size);
  handler()->OnDone(this);
}

EmbeddedFetcher::~EmbeddedFetcher() {}

}  // namespace loader
}  // namespace cobalt
