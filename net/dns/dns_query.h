// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_QUERY_H_
#define NET_DNS_DNS_QUERY_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace net {

class IOBufferWithSize;

// Represents on-the-wire DNS query message as an object.
class NET_EXPORT_PRIVATE DnsQuery {
 public:
  // Constructs a query message from |qname| which *MUST* be in a valid
  // DNS name format, and |qtype| which must be either kDNS_A or kDNS_AAAA.

  // Every generated object has a random ID, hence two objects generated
  // with the same set of constructor arguments are generally not equal;
  // there is a 1/2^16 chance of them being equal due to size of |id_|.
  DnsQuery(const std::string& qname,
           uint16 qtype,
           const RandIntCallback& rand_int_cb);
  ~DnsQuery();

  // Clones |this| verbatim, with ID field of the header regenerated.
  DnsQuery* CloneWithNewId() const;

  // DnsQuery field accessors.
  uint16 id() const;
  uint16 qtype() const;

  // Returns the size of the Question section of the query.  Used when
  // matching the response.
  size_t question_size() const;

  // Returns pointer to the Question section of the query.  Used when
  // matching the response.
  const char* question_data() const;

  // IOBuffer accessor to be used for writing out the query.
  IOBufferWithSize* io_buffer() const { return io_buffer_; }

 private:
  const std::string qname() const;

  // Randomizes ID field of the query message.
  void RandomizeId();

  // Size of the DNS name (*NOT* hostname) we are trying to resolve; used
  // to calculate offsets.
  size_t qname_size_;

  // Contains query bytes to be consumed by higher level Write() call.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  // PRNG function for generating IDs.
  RandIntCallback rand_int_cb_;

  DISALLOW_COPY_AND_ASSIGN(DnsQuery);
};

}  // namespace net

#endif  // NET_DNS_DNS_QUERY_H_
