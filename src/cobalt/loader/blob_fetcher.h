/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_BLOB_FETCHER_H_
#define COBALT_LOADER_BLOB_FETCHER_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// For fetching the 'blob:' scheme.
class BlobFetcher : public Fetcher {
 public:
  // Returns true if the blob is succesfully fetched, and only then writes
  // the address of the buffer and its size to |data| and |size|. |data| can
  // be NULL when size is 0. This callback avoids a dependency from the
  // fetcher to the actual blob implementation.
  typedef base::Callback<bool(const GURL& url, const char** data, size_t* size)>
      ResolverCallback;

  explicit BlobFetcher(const GURL& url, Handler* handler,
                       const ResolverCallback& resolver_callback);

  void Fetch();

  ~BlobFetcher() OVERRIDE;

 private:
  void GetData();

  GURL url_;
  ResolverCallback resolver_callback_;
  base::WeakPtrFactory<BlobFetcher> weak_ptr_factory_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_BLOB_FETCHER_H_
