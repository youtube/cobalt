// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_LIST_H_
#define NET_PROXY_PROXY_LIST_H_
#pragma once

#include <string>
#include <vector>

#include "net/proxy/proxy_retry_info.h"

namespace net {

class ProxyServer;

// This class is used to hold a list of proxies returned by GetProxyForUrl or
// manually configured. It handles proxy fallback if multiple servers are
// specified.
class ProxyList {
 public:
  ProxyList();
  ~ProxyList();

  // Initializes the proxy list to a string containing one or more proxy servers
  // delimited by a semicolon.
  void Set(const std::string& proxy_uri_list);

  // Set the proxy list to a single entry, |proxy_server|.
  void SetSingleProxyServer(const ProxyServer& proxy_server);

  // De-prioritizes the proxies that we have cached as not working, by moving
  // them to the end of the fallback list.
  void DeprioritizeBadProxies(const ProxyRetryInfoMap& proxy_retry_info);

  // Delete any entry which doesn't have one of the specified proxy schemes.
  // |scheme_bit_field| is a bunch of ProxyServer::Scheme bitwise ORed together.
  void RemoveProxiesWithoutScheme(int scheme_bit_field);

  // Returns true if there is nothing left in the ProxyList.
  bool IsEmpty() const;

  // Returns the first proxy server in the list. It is only valid to call
  // this if !IsEmpty().
  const ProxyServer& Get() const;

  // Sets the list by parsing the pac result |pac_string|.
  // Some examples for |pac_string|:
  //   "DIRECT"
  //   "PROXY foopy1"
  //   "PROXY foopy1; SOCKS4 foopy2:1188"
  // Does a best-effort parse, and silently discards any errors.
  void SetFromPacString(const std::string& pac_string);

  // Returns a PAC-style semicolon-separated list of valid proxy servers.
  // For example: "PROXY xxx.xxx.xxx.xxx:xx; SOCKS yyy.yyy.yyy:yy".
  std::string ToPacString() const;

  // Marks the current proxy server as bad and deletes it from the list.  The
  // list of known bad proxies is given by proxy_retry_info.  Returns true if
  // there is another server available in the list.
  bool Fallback(ProxyRetryInfoMap* proxy_retry_info);

 private:
  // List of proxies.
  std::vector<ProxyServer> proxies_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_LIST_H_
