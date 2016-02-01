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

#include "base/bind.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "cobalt/loader/about_fetcher.h"
#include "cobalt/loader/embedded_fetcher.h"
#include "cobalt/loader/file_fetcher.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace loader {
namespace {

const char kEmbeddedScheme[] = "h5vcc-embedded";
#if defined(ENABLE_ABOUT_SCHEME)
const char kAboutScheme[] = "about";
#endif

bool FileURLToFilePath(const GURL& url, FilePath* file_path) {
  DCHECK(url.is_valid() && url.SchemeIsFile());
  std::string path = url.path();
  DCHECK_EQ('/', path[0]);
  path.erase(0, 1);
  *file_path = FilePath(path);
  return !file_path->empty();
}

bool EmbeddedURLToKey(const GURL& url, std::string* key) {
  DCHECK(url.is_valid() && url.SchemeIs(kEmbeddedScheme));
  *key = url.path();
  DCHECK_EQ('/', (*key)[0]);
  DCHECK_EQ('/', (*key)[1]);
  (*key).erase(0, 2);
  return !key->empty();
}

}  // namespace

FetcherFactory::FetcherFactory(network::NetworkModule* network_module)
    : file_thread_("File"), network_module_(network_module) {
  file_thread_.Start();
}

FetcherFactory::FetcherFactory(network::NetworkModule* network_module,
                               const FilePath& extra_search_dir)
    : file_thread_("File"),
      network_module_(network_module),
      extra_search_dir_(extra_search_dir) {
  file_thread_.Start();
}

scoped_ptr<Fetcher> FetcherFactory::CreateFetcher(const GURL& url,
                                                  Fetcher::Handler* handler) {
  return CreateSecureFetcher(url, csp::SecurityCallback(), handler).Pass();
}

scoped_ptr<Fetcher> FetcherFactory::CreateSecureFetcher(
    const GURL& url, const csp::SecurityCallback& url_security_callback,
    Fetcher::Handler* handler) {
  if (!url.is_valid()) {
    DLOG(WARNING) << "Invalid url: " << url << ".";
    return scoped_ptr<Fetcher>(NULL);
  }

  scoped_ptr<Fetcher> fetcher;
  if (url.SchemeIs(kEmbeddedScheme)) {
    std::string key;
    if (EmbeddedURLToKey(url, &key)) {
      EmbeddedFetcher::Options options;
      fetcher.reset(new EmbeddedFetcher(key, handler, options));
    }
  } else if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.message_loop_proxy = file_thread_.message_loop_proxy();
      options.extra_search_dir = extra_search_dir_;
      fetcher.reset(new FileFetcher(file_path, handler, options));
    }
  }
#if defined(ENABLE_ABOUT_SCHEME)
  else if (url.SchemeIs(kAboutScheme)) {  // NOLINT(readability/braces)
    fetcher.reset(new AboutFetcher(handler));
  }
#endif
  else {  // NOLINT(readability/braces)
    DCHECK(network_module_) << "Network module required.";
    NetFetcher::Options options;
    fetcher.reset(new NetFetcher(url, url_security_callback, handler,
                                 network_module_, options));
  }
  return fetcher.Pass();
}

}  // namespace loader
}  // namespace cobalt
