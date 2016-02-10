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

#ifndef COBALT_LOADER_EMBEDDED_FETCHER_H_
#define COBALT_LOADER_EMBEDDED_FETCHER_H_

#include <limits>
#include <string>

#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// EmbeddedFetcher is for fetching data embedded in the binary, probably
// generated using generate_data_header.py. Data will be in the form of a
// map entry, so the first constructor parameter is the map key. The map will
// be defined within a header file that must be included in the source file.
class EmbeddedFetcher : public Fetcher {
 public:
  struct Options {
   public:
    Options()
        : start_offset(0), bytes_to_read(std::numeric_limits<int64>::max()) {}

    int64 start_offset;
    int64 bytes_to_read;
  };

  EmbeddedFetcher(const std::string& key, Handler* handler,
                  const Options& options);

  ~EmbeddedFetcher() OVERRIDE;

 private:
  void GetEmbeddedData(int64 start_offset, int64 bytes_to_read);

  std::string key_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_EMBEDDED_FETCHER_H_
