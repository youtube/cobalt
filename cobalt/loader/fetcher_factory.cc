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

#include "base/file_path.h"
#include "base/path_service.h"
#include "cobalt/loader/file_fetcher.h"

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

scoped_ptr<Fetcher> FetcherFactory::CreateFetcher(const GURL& url,
                                                  Fetcher::Handler* handler) {
  if (!url.is_valid()) return scoped_ptr<Fetcher>(NULL);
  if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.io_message_loop = thread_.message_loop_proxy();
      return scoped_ptr<Fetcher>(new FileFetcher(file_path, handler, options));
    }
  } else {
    // TODO(***REMOVED***): Support other schemes such as http.
    NOTIMPLEMENTED() << "Resource URL type not supported.";
  }
  return scoped_ptr<Fetcher>(NULL);
}

}  // namespace loader
}  // namespace cobalt
