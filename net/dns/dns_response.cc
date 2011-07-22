// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_query.h"

namespace net {

// RFC 1035, section 4.2.1: Messages carried by UDP are restricted to 512
// bytes (not counting the IP nor UDP headers).
static const int kMaxResponseSize = 512;

DnsResponse::DnsResponse(DnsQuery* query)
    : query_(query),
      io_buffer_(new IOBufferWithSize(kMaxResponseSize + 1)) {
  DCHECK(query_);
}

DnsResponse::~DnsResponse() {
}

int DnsResponse::Parse(int nbytes, IPAddressList* ip_addresses) {
  // Response includes query, it should be at least that size.
  if (nbytes < query_->io_buffer()->size() || nbytes > kMaxResponseSize)
    return ERR_DNS_MALFORMED_RESPONSE;

  DnsResponseBuffer response(reinterpret_cast<uint8*>(io_buffer_->data()),
                             io_buffer_->size());
  uint16 id;
  if (!response.U16(&id) || id != query_->id()) // Make sure IDs match.
    return ERR_DNS_MALFORMED_RESPONSE;

  uint8 flags, rcode;
  if (!response.U8(&flags) || !response.U8(&rcode))
    return ERR_DNS_MALFORMED_RESPONSE;

  if (flags & 2) // TC is set -- server wants TCP, we don't support it (yet?).
    return ERR_DNS_SERVER_REQUIRES_TCP;

  rcode &= 0x0f;         // 3 means NXDOMAIN, the rest means server failed.
  if (rcode && (rcode != 3))
    return ERR_DNS_SERVER_FAILED;

  uint16 query_count, answer_count, authority_count, additional_count;
  if (!response.U16(&query_count) ||
      !response.U16(&answer_count) ||
      !response.U16(&authority_count) ||
      !response.U16(&additional_count)) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }

  if (query_count != 1) // Sent a single question, shouldn't have changed.
    return ERR_DNS_MALFORMED_RESPONSE;

  base::StringPiece question; // Make sure question section is echoed back.
  if (!response.Block(&question, query_->question_size()) ||
      memcmp(question.data(), query_->question_data(),
             query_->question_size())) {
    return ERR_DNS_MALFORMED_RESPONSE;
  }

  if (answer_count < 1)
    return ERR_NAME_NOT_RESOLVED;

  IPAddressList rdatas;
  while (answer_count--) {
    uint32 ttl;
    uint16 rdlength, qtype, qclass;
    if (!response.DNSName(NULL) ||
        !response.U16(&qtype) ||
        !response.U16(&qclass) ||
        !response.U32(&ttl) ||
        !response.U16(&rdlength)) {
      return ERR_DNS_MALFORMED_RESPONSE;
    }
    if (qtype == query_->qtype() &&
        qclass == kClassIN &&
        (rdlength == kIPv4AddressSize || rdlength == kIPv6AddressSize)) {
      base::StringPiece rdata;
      if (!response.Block(&rdata, rdlength))
        return ERR_DNS_MALFORMED_RESPONSE;
      rdatas.push_back(IPAddressNumber(rdata.begin(), rdata.end()));
    } else if (!response.Skip(rdlength))
      return ERR_DNS_MALFORMED_RESPONSE;
  }

  if (rdatas.empty())
    return ERR_NAME_NOT_RESOLVED;

  if (ip_addresses)
    ip_addresses->swap(rdatas);
  return OK;
}

}  // namespace net
