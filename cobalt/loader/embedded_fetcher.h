// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_EMBEDDED_FETCHER_H_
#define COBALT_LOADER_EMBEDDED_FETCHER_H_

#include <limits>
#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/csp/content_security_policy.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

extern const char kEmbeddedScheme[];

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

  EmbeddedFetcher(const GURL& url,
                  const csp::SecurityCallback& security_callback,
                  Handler* handler, const Options& options);

  ~EmbeddedFetcher() override;

 private:
  void Fetch(const Options& options);
  void GetEmbeddedData(const std::string& key, int64 start_offset,
                       int64 bytes_to_read);
  bool IsAllowedByCsp();

  GURL url_;
  csp::SecurityCallback security_callback_;
  base::WeakPtrFactory<EmbeddedFetcher> weak_ptr_factory_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_EMBEDDED_FETCHER_H_
