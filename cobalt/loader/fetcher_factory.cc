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

#include "cobalt/loader/fetcher_factory.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/loader/about_fetcher.h"
#include "cobalt/loader/blob_fetcher.h"
#include "cobalt/loader/cache_fetcher.h"
#include "cobalt/loader/embedded_fetcher.h"
#include "cobalt/loader/error_fetcher.h"
#include "cobalt/loader/file_fetcher.h"
#include "cobalt/loader/net_fetcher.h"
#include "cobalt/network/network_module.h"

namespace cobalt {
namespace loader {
namespace {

#if defined(ENABLE_ABOUT_SCHEME)
const char kAboutScheme[] = "about";
#endif

bool FileURLToFilePath(const GURL& url, base::FilePath* file_path) {
  DCHECK(url.is_valid() && url.SchemeIsFile());
  std::string path = url.path();
  DCHECK_EQ('/', path[0]);
  path.erase(0, 1);
  *file_path = base::FilePath(path);
  return !file_path->empty();
}

std::string ClipUrl(const GURL& url, size_t length) {
  const std::string& spec = url.possibly_invalid_spec();
  if (spec.size() < length) {
    return spec;
  }

  size_t remain = length - 5;
  size_t head = remain / 2;
  size_t tail = remain - head;

  return spec.substr(0, head) + "[...]" + spec.substr(spec.size() - tail);
}

}  // namespace

FetcherFactory::FetcherFactory(network::NetworkModule* network_module)
    : network_module_(network_module) {}

FetcherFactory::FetcherFactory(
    network::NetworkModule* network_module,
    const BlobFetcher::ResolverCallback& blob_resolver)
    : network_module_(network_module), blob_resolver_(blob_resolver) {}

FetcherFactory::FetcherFactory(
    network::NetworkModule* network_module,
    const base::FilePath& extra_search_dir,
    const BlobFetcher::ResolverCallback& blob_resolver,
    const base::Callback<int(const std::string&, std::unique_ptr<char[]>*)>&
        read_cache_callback)
    : network_module_(network_module),
      extra_search_dir_(extra_search_dir),
      blob_resolver_(blob_resolver),
      read_cache_callback_(read_cache_callback) {}

std::unique_ptr<Fetcher> FetcherFactory::CreateFetcher(
    const GURL& url, bool main_resource,
    const network::disk_cache::ResourceType type, Fetcher::Handler* handler) {
  return CreateSecureFetcher(url, main_resource, csp::SecurityCallback(),
                             kNoCORSMode, Origin(), type,
                             net::HttpRequestHeaders(),
                             /*skip_fetch_intercept=*/false, handler);
}

std::unique_ptr<Fetcher> FetcherFactory::CreateSecureFetcher(
    const GURL& url, bool main_resource,
    const csp::SecurityCallback& url_security_callback,
    RequestMode request_mode, const Origin& origin,
    const network::disk_cache::ResourceType type,
    net::HttpRequestHeaders headers, bool skip_fetch_intercept,
    Fetcher::Handler* handler) {
  LOG(INFO) << "Fetching: " << ClipUrl(url, 200);

  if (!url.is_valid()) {
    std::stringstream error_message;
    error_message << "URL is invalid: " << url;
    return std::unique_ptr<Fetcher>(
        new ErrorFetcher(handler, error_message.str()));
  }

  if ((url.SchemeIs("https") || url.SchemeIs("http") || url.SchemeIs("data")) &&
      network_module_) {
    NetFetcher::Options options;
    options.resource_type = type;
    options.headers = std::move(headers);
    options.skip_fetch_intercept = skip_fetch_intercept;
    return std::unique_ptr<Fetcher>(
        new NetFetcher(url, main_resource, url_security_callback, handler,
                       network_module_, options, request_mode, origin));
  }

  if (url.SchemeIs("blob") && !blob_resolver_.is_null()) {
    return std::unique_ptr<Fetcher>(
        new BlobFetcher(url, handler, blob_resolver_));
  }

  if (url.SchemeIs(kEmbeddedScheme)) {
    EmbeddedFetcher::Options options;
    return std::unique_ptr<Fetcher>(
        new EmbeddedFetcher(url, url_security_callback, handler, options));
  }

  // h5vcc-cache: scheme requires read_cache_callback_ which is not available
  // in the main WebModule.
  if (url.SchemeIs(kCacheScheme) && !read_cache_callback_.is_null()) {
    return std::unique_ptr<Fetcher>(new CacheFetcher(
        url, url_security_callback, handler, read_cache_callback_));
  }

  if (url.SchemeIsFile()) {
    base::FilePath file_path;
    if (!FileURLToFilePath(url, &file_path)) {
      std::stringstream error_message;
      error_message << "File URL cannot be converted to file path: " << url;
      return std::unique_ptr<Fetcher>(
          new ErrorFetcher(handler, error_message.str()));
    }

    FileFetcher::Options options;
    if (!file_thread_.IsRunning()) {
      file_thread_.Start();
    }
    options.message_loop_proxy = file_thread_.task_runner();
    options.extra_search_dir = extra_search_dir_;
    return std::unique_ptr<Fetcher>(
        new FileFetcher(file_path, handler, options));
  }

#if defined(ENABLE_ABOUT_SCHEME)
  if (url.SchemeIs(kAboutScheme)) {
    return std::unique_ptr<Fetcher>(new AboutFetcher(handler));
  }
#endif

  std::stringstream error_message;
  error_message << "Scheme " << url.scheme() << ": is not supported";
  return std::unique_ptr<Fetcher>(
      new ErrorFetcher(handler, error_message.str()));
}

}  // namespace loader
}  // namespace cobalt
