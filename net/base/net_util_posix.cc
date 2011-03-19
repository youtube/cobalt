// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <ifaddrs.h>
#include <sys/types.h>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"

namespace net {

bool FileURLToFilePath(const GURL& url, FilePath* path) {
  *path = FilePath();
  std::string& file_path_str = const_cast<std::string&>(path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  // Firefox seems to ignore the "host" of a file url if there is one. That is,
  // file://foo/bar.txt maps to /bar.txt.
  // TODO(dhg): This should probably take into account UNCs which could
  // include a hostname other than localhost or blank
  std::string old_path = url.path();

  if (old_path.empty())
    return false;

  // GURL stores strings as percent-encoded 8-bit, this will undo if possible.
  old_path = UnescapeURLComponent(old_path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = old_path;
    ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    old_path.swap(new_path);
  } while (new_path != old_path);

  file_path_str.assign(old_path);

  return !file_path_str.empty();
}

bool GetNetworkList(NetworkInterfaceList* networks) {
  // getifaddrs() may require IO operations.
  base::ThreadRestrictions::AssertIOAllowed();

  ifaddrs *ifaddr;
  if (getifaddrs(&ifaddr) < 0) {
    PLOG(ERROR) << "getifaddrs";
    return false;
  }

  for (ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    int family = ifa->ifa_addr->sa_family;
    if (family == AF_INET || family == AF_INET6) {
      IPEndPoint address;
      std::string name = ifa->ifa_name;
      if (address.FromSockAddr(ifa->ifa_addr,
                               sizeof(ifa->ifa_addr)) &&
          name.substr(0, 2) != "lo") {
        networks->push_back(NetworkInterface(name, address.address()));
      }
    }
  }

  freeifaddrs(ifaddr);

  return true;
}

}  // namespace net
