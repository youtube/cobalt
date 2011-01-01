// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_server.h"

#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"

namespace net {

// static
ProxyServer ProxyServer::FromDictionary(Scheme scheme,
                                        CFDictionaryRef dict,
                                        CFStringRef host_key,
                                        CFStringRef port_key) {
  if (scheme == SCHEME_INVALID || scheme == SCHEME_DIRECT) {
    // No hostname port to extract; we are done.
    return ProxyServer(scheme, HostPortPair());
  }

  CFStringRef host_ref =
      (CFStringRef)base::mac::GetValueFromDictionary(dict, host_key,
                                                    CFStringGetTypeID());
  if (!host_ref) {
    LOG(WARNING) << "Could not find expected key "
                 << base::SysCFStringRefToUTF8(host_key)
                 << " in the proxy dictionary";
    return ProxyServer();  // Invalid.
  }
  std::string host = base::SysCFStringRefToUTF8(host_ref);

  CFNumberRef port_ref =
      (CFNumberRef)base::mac::GetValueFromDictionary(dict, port_key,
                                                    CFNumberGetTypeID());
  int port;
  if (port_ref) {
    CFNumberGetValue(port_ref, kCFNumberIntType, &port);
  } else {
    port = GetDefaultPortForScheme(scheme);
  }

  return ProxyServer(scheme, HostPortPair(host, port));
}

}  // namespace net
