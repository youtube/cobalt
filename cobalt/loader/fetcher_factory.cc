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

#include "cobalt/loader/fetcher_factory.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "cobalt/loader/about_fetcher.h"
#include "cobalt/loader/blob_fetcher.h"
#include "cobalt/loader/cache_fetcher.h"
#include "cobalt/loader/embedded_fetcher.h"
#include "cobalt/loader/file_fetcher.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace loader {
namespace {

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

std::string ClipUrl(const GURL& url, size_t length) {
  const std::string& spec = url.possibly_invalid_spec();
  if (spec.size() < length) {
    return spec;
  }

  return spec.substr(0, length - 5) + "[...]";
}

}  // namespace

FetcherFactory::FetcherFactory(network::NetworkModule* network_module)
    : file_thread_("File"),
      network_module_(network_module),
      read_cache_callback_() {
  file_thread_.Start();
}

FetcherFactory::FetcherFactory(network::NetworkModule* network_module,
                               const FilePath& extra_search_dir)
    : file_thread_("File"),
      network_module_(network_module),
      extra_search_dir_(extra_search_dir),
      read_cache_callback_() {
  file_thread_.Start();
}

FetcherFactory::FetcherFactory(
    network::NetworkModule* network_module, const FilePath& extra_search_dir,
    const BlobFetcher::ResolverCallback& blob_resolver,
    const base::Callback<int(const std::string&, scoped_array<char>*)>&
        read_cache_callback)
    : file_thread_("File"),
      network_module_(network_module),
      extra_search_dir_(extra_search_dir),
      blob_resolver_(blob_resolver),
      read_cache_callback_(read_cache_callback) {
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
    LOG(ERROR) << "URL is invalid: " << url;
    return scoped_ptr<Fetcher>(NULL);
  }

  DLOG(INFO) << "Fetching: " << ClipUrl(url, 60);
  scoped_ptr<Fetcher> fetcher;
  if (url.SchemeIs(kEmbeddedScheme)) {
    EmbeddedFetcher::Options options;
    fetcher.reset(
        new EmbeddedFetcher(url, url_security_callback, handler, options));
  } else if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.message_loop_proxy = file_thread_.message_loop_proxy();
      options.extra_search_dir = extra_search_dir_;
      fetcher.reset(new FileFetcher(file_path, handler, options));
    } else {
      LOG(ERROR) << "File URL cannot be converted to file path: " << url;
    }
  }
#if defined(ENABLE_ABOUT_SCHEME)
  else if (url.SchemeIs(kAboutScheme)) {  // NOLINT(readability/braces)
    fetcher.reset(new AboutFetcher(handler));
  }
#endif
  else if (url.SchemeIs("blob")) {  // NOLINT(readability/braces)
    if (!blob_resolver_.is_null()) {
      fetcher.reset(new BlobFetcher(url, handler, blob_resolver_));
    } else {
      LOG(ERROR) << "Fetcher factory not provided the blob registry, "
                    "could not fetch the URL: "
                 << url;
    }
  } else if (url.SchemeIs(kCacheScheme)) {
    if (read_cache_callback_.is_null()) {
      LOG(ERROR) << "read_cache_callback_ must be provided to CacheFetcher for "
                    "accessing h5vcc-cache:// . This is not available in the "
                    "main WebModule.";
      DCHECK(!read_cache_callback_.is_null());
      return fetcher.Pass();
    }

    fetcher.reset(new CacheFetcher(url, url_security_callback, handler,
                                   read_cache_callback_));
  } else {  // NOLINT(readability/braces)
    DCHECK(network_module_) << "Network module required.";
    NetFetcher::Options options;
    fetcher.reset(new NetFetcher(url, url_security_callback, handler,
                                 network_module_, options));
  }
  return fetcher.Pass();
}

}  // namespace loader
}  // namespace cobalt
