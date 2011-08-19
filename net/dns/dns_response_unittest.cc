// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/base/io_buffer.h"
#include "net/dns/dns_query.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// DNS response consists of a header followed by a question followed by
// answer.  Header format, question format and response format are
// described below.  For the meaning of specific fields, please see RFC
// 1035.

// Header format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      ID                       |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    QDCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ANCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    NSCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                    ARCOUNT                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Question format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                     QNAME                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QTYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     QCLASS                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// Answser format.
//                                  1  1  1  1  1  1
//    0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                                               |
//  /                                               /
//  /                      NAME                     /
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TYPE                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                     CLASS                     |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                      TTL                      |
//  |                                               |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//  |                   RDLENGTH                    |
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--|
//  /                     RDATA                     /
//  /                                               /
//  +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

// TODO(agayev): add more thorough tests.
TEST(DnsResponseTest, ResponseWithCnameA) {
  const std::string kQname("\012codereview\010chromium\003org", 25);
  DnsQuery q1(kQname, kDNS_A, base::Bind(&base::RandInt));

  uint8 id_hi = q1.id() >> 8, id_lo = q1.id() & 0xff;

  uint8 ip[] = {              // codereview.chromium.org resolves to
    0x4a, 0x7d, 0x5f, 0x79    // 74.125.95.121
  };

  IPAddressList expected_ips;
  expected_ips.push_back(IPAddressNumber(ip, ip + arraysize(ip)));

  uint8 response_data[] = {
    // Header
    id_hi, id_lo,             // ID
    0x81, 0x80,               // Standard query response, no error
    0x00, 0x01,               // 1 question
    0x00, 0x02,               // 2 RRs (answers)
    0x00, 0x00,               // 0 authority RRs
    0x00, 0x00,               // 0 additional RRs

    // Question
    0x0a, 0x63, 0x6f, 0x64,   // This part is echoed back from the
    0x65, 0x72, 0x65, 0x76,   // respective query.
    0x69, 0x65, 0x77, 0x08,
    0x63, 0x68, 0x72, 0x6f,
    0x6d, 0x69, 0x75, 0x6d,
    0x03, 0x6f, 0x72, 0x67,
    0x00,
    0x00, 0x01,
    0x00, 0x01,

    // Answer 1
    0xc0, 0x0c,        // NAME is a pointer to name in Question section.
    0x00, 0x05,        // TYPE is CNAME.
    0x00, 0x01,        // CLASS is IN.
    0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74,
    0x00, 0x12,               // RDLENGTH is 18 bytse.
    0x03, 0x67, 0x68, 0x73,   // ghs.l.google.com in DNS format.
    0x01, 0x6c, 0x06, 0x67,
    0x6f, 0x6f, 0x67, 0x6c,
    0x65, 0x03, 0x63, 0x6f,
    0x6d, 0x00,

    // Answer 2
    0xc0, 0x35,         // NAME is a pointer to name in Question section.
    0x00, 0x01,         // TYPE is A.
    0x00, 0x01,         // CLASS is IN.
    0x00, 0x00,         // TTL (4 bytes) is 53 seconds.
    0x00, 0x35,
    0x00, 0x04,                 // RDLENGTH is 4 bytes.
    ip[0], ip[1], ip[2], ip[3], // RDATA is the IP.
  };

  // Create a response object and simulate reading into it.
  DnsResponse r1(&q1);
  memcpy(r1.io_buffer()->data(), &response_data[0],
         r1.io_buffer()->size());

  // Verify resolved IPs.
  int response_size = arraysize(response_data);
  IPAddressList actual_ips;
  EXPECT_EQ(OK, r1.Parse(response_size, &actual_ips));
  EXPECT_EQ(expected_ips, actual_ips);
}

}  // namespace net
