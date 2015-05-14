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

#include "cobalt/loader/fetcher_factory.h"

#include <string>

#include "base/file_path.h"
#include "base/path_service.h"
#include "cobalt/loader/file_fetcher.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace loader {
namespace {

bool FileURLToFilePath(const GURL& url, FilePath* file_path) {
  DCHECK(url.is_valid() && url.SchemeIsFile());
  std::string path = url.path();
  DCHECK_EQ('/', path[0]);
  path.erase(0, 1);
  *file_path = FilePath(path);
  return !file_path->empty();
}

}  // namespace

FetcherFactory::FetcherFactory(network::NetworkModule* network_module)
    : io_thread_("File IO")
    , network_module_(network_module) {
    io_thread_.Start();
}

scoped_ptr<Fetcher> FetcherFactory::CreateFetcher(const GURL& url,
                                                  Fetcher::Handler* handler) {
  if (!url.is_valid()) {
    DLOG(WARNING) << "Invalid url " << url;
    return scoped_ptr<Fetcher>(NULL);
  }

  scoped_ptr<Fetcher> fetcher;
  if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.io_message_loop = io_thread_.message_loop_proxy();
      fetcher.reset(new FileFetcher(file_path, handler, options));
    }
  } else {
    DCHECK(network_module_) << "Network module required";
    NetFetcher::Options options;
    fetcher.reset(new NetFetcher(url, handler, network_module_, options));
  }
  return fetcher.Pass();
}

}  // namespace loader
}  // namespace cobalt
