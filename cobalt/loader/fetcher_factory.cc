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
#include "base/string_number_conversions.h"
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

void SetRequestRange(int64 start_offset, int64 bytes_to_read,
                     net::URLFetcher* url_fetcher) {
  std::string range_request =
      "Range: bytes=" + base::Int64ToString(start_offset) + "-" +
      base::Int64ToString(start_offset + bytes_to_read - 1);
  url_fetcher->AddExtraRequestHeader(range_request);
}

}  // namespace

FetcherFactory::FetcherFactory(network::NetworkModule* network_module)
    : file_thread_("File"), network_module_(network_module) {
  file_thread_.Start();
}

scoped_ptr<Fetcher> FetcherFactory::CreateFetcher(const GURL& url,
                                                  Fetcher::Handler* handler) {
  if (!url.is_valid()) {
    DLOG(WARNING) << "Invalid url: " << url << ".";
    return scoped_ptr<Fetcher>(NULL);
  }

  scoped_ptr<Fetcher> fetcher;
  if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.message_loop_proxy = file_thread_.message_loop_proxy();
      fetcher.reset(new FileFetcher(file_path, handler, options));
    }
  } else {
    DCHECK(network_module_) << "Network module required.";
    NetFetcher::Options options;
    fetcher.reset(new NetFetcher(url, handler, network_module_, options));
  }
  return fetcher.Pass();
}

scoped_ptr<Fetcher> FetcherFactory::CreateFetcherWithRange(
    const GURL& url, int64 start_offset, int64 bytes_to_read,
    Fetcher::Handler* handler) {
  DCHECK_GE(start_offset, 0);
  DCHECK_GT(bytes_to_read, 0);

  if (!url.is_valid()) {
    DLOG(WARNING) << "Invalid url: " << url << ".";
    return scoped_ptr<Fetcher>(NULL);
  }

  if (start_offset < 0 || bytes_to_read <= 0) {
    DLOG_IF(WARNING, start_offset < 0) << "Invalid start offset : "
                                       << start_offset;
    DLOG_IF(WARNING, bytes_to_read <= 0) << "Invalid size to read : "
                                         << bytes_to_read;
    return scoped_ptr<Fetcher>(NULL);
  }

  scoped_ptr<Fetcher> fetcher;
  if (url.SchemeIsFile()) {
    FilePath file_path;
    if (FileURLToFilePath(url, &file_path)) {
      FileFetcher::Options options;
      options.message_loop_proxy = file_thread_.message_loop_proxy();
      options.start_offset = start_offset;
      options.bytes_to_read = bytes_to_read;
      fetcher.reset(new FileFetcher(file_path, handler, options));
    }
  } else {
    DCHECK(network_module_) << "Network module required.";
    NetFetcher::Options options;
    options.setup_callback =
        base::Bind(SetRequestRange, start_offset, bytes_to_read);
    fetcher.reset(new NetFetcher(url, handler, network_module_, options));
  }
  return fetcher.Pass();
}

}  // namespace loader
}  // namespace cobalt
