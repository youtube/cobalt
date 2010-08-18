// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DNSRR_RESOLVER_H_
#define NET_BASE_DNSRR_RESOLVER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "net/base/completion_callback.h"

namespace net {

// RRResponse contains the result of a successful request for a resource record.
struct RRResponse {
  // name contains the canonical name of the resulting domain. If the queried
  // name was a CNAME then this can differ.
  std::string name;
  // ttl contains the TTL of the resource records.
  uint32 ttl;
  // dnssec is true if the response was DNSSEC validated.
  bool dnssec;
  std::vector<std::string> rrdatas;
  // sigs contains the RRSIG records returned.
  std::vector<std::string> signatures;

  // For testing only
  bool ParseFromResponse(const uint8* data, unsigned len,
                         uint16 rrtype_requested);
};

// DnsRRResolver resolves arbitary DNS resource record types. It should not be
// confused with HostResolver and should not be used to resolve A/AAAA records.
//
// HostResolver exists to lookup addresses and there are many details about
// address resolution over and above DNS (i.e. Bonjour, VPNs etc).
//
// DnsRRResolver should only be used when the data is specifically DNS data and
// the name is a fully qualified DNS domain.
class DnsRRResolver {
 public:
  enum {
    // Try harder to get a DNSSEC signed response. This doesn't mean that the
    // RRResponse will always have the dnssec bit set.
    FLAG_WANT_DNSSEC = 1,
  };

  // Resolve starts the resolution process. When complete, |callback| is called
  // with a result. If the result is |OK| then |response| is filled with the
  // result of the resolution. Note the |callback| is called on the current
  // MessageLoop.
  static bool Resolve(const std::string& name, uint16 rrtype,
                      uint16 flags, CompletionCallback* callback,
                      RRResponse* response);

 private:
  DISALLOW_COPY_AND_ASSIGN(DnsRRResolver);
};

}  // namespace net

#endif // NET_BASE_DNSRR_RESOLVER_H_
