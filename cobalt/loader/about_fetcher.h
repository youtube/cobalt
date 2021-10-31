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

#ifndef COBALT_LOADER_ABOUT_FETCHER_H_
#define COBALT_LOADER_ABOUT_FETCHER_H_

#if defined(ENABLE_ABOUT_SCHEME)

#include "base/memory/weak_ptr.h"
#include "cobalt/loader/fetcher.h"

namespace cobalt {
namespace loader {

// For fetching the about: scheme.
// This will return an empty document regardless of the path.
class AboutFetcher : public Fetcher {
 public:
  explicit AboutFetcher(Handler* handler);

  void Fetch();

 private:
  base::WeakPtrFactory<AboutFetcher> weak_ptr_factory_;
};

}  // namespace loader
}  // namespace cobalt

#endif  // defined(ENABLE_ABOUT_SCHEME)
#endif  // COBALT_LOADER_ABOUT_FETCHER_H_
