// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_CLIENT_H_
#define NET_DNS_DNS_CLIENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"

namespace base {
class StringPiece;
}

namespace net {

class BoundNetLog;
class ClientSocketFactory;
class DnsResponse;
class DnsSession;

// DnsClient performs asynchronous DNS queries. DnsClient takes care of
// retransmissions, DNS server fallback (or round-robin), suffix search, and
// simple response validation ("does it match the query") to fight poisoning.
// It does NOT perform caching, aggregation or prioritization of requests.
//
// Destroying DnsClient does NOT affect any already created Requests.
//
// TODO(szym): consider adding flags to MakeRequest to indicate options:
// -- don't perform suffix search
// -- query both A and AAAA at once
// -- accept truncated response (and/or forbid TCP)
class NET_EXPORT_PRIVATE DnsClient {
 public:
  class Request;
  // Callback for complete requests. Note that DnsResponse might be NULL if
  // the DNS server(s) could not be reached.
  typedef base::Callback<void(Request* req,
                              int result,
                              const DnsResponse* resp)> RequestCallback;

  // A data-holder for a request made to the DnsClient.
  // Destroying the request cancels the underlying network effort.
  class NET_EXPORT_PRIVATE Request {
   public:
    Request(const base::StringPiece& qname,
            uint16 qtype,
            const RequestCallback& callback);
    virtual ~Request();

    const std::string& qname() const { return qname_; }

    uint16 qtype() const { return qtype_; }

    virtual int Start() = 0;

    void DoCallback(int result, const DnsResponse* response) {
      callback_.Run(this, result, response);
    }

   private:
    std::string qname_;
    uint16 qtype_;
    RequestCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  virtual ~DnsClient() {}

  // Makes asynchronous DNS query for the given |qname| and |qtype| (assuming
  // QCLASS == IN). The caller is responsible for destroying the returned
  // request whether to cancel it or after its completion.
  // (Destroying DnsClient does not abort the requests.)
  virtual Request* CreateRequest(
      const base::StringPiece& qname,
      uint16 qtype,
      const RequestCallback& callback,
      const BoundNetLog& source_net_log) WARN_UNUSED_RESULT = 0;

  // Creates a socket-based DnsClient using the |session|.
  static DnsClient* CreateClient(DnsSession* session) WARN_UNUSED_RESULT;
};

}  // namespace net

#endif  // NET_DNS_DNS_CLIENT_H_

