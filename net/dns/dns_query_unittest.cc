// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_query.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/base/dns_util.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// DNS query consists of a header followed by a question.  Header format
// and question format are described below.  For the meaning of specific
// fields, please see RFC 1035.

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

TEST(DnsQueryTest, ConstructorTest) {
  std::string kQname("\003www\006google\003com", 16);
  DnsQuery q1(kQname, kDNS_A, base::Bind(&base::RandInt));
  EXPECT_EQ(kDNS_A, q1.qtype());

  uint8 id_hi = q1.id() >> 8, id_lo = q1.id() & 0xff;

  // See the top of the file for the description of a DNS query.
  const uint8 query_data[] = {
    // Header
    id_hi, id_lo,
    0x01, 0x00,               // Flags -- set RD (recursion desired) bit.
    0x00, 0x01,               // Set QDCOUNT (question count) to 1, all the
                              // rest are 0 for a query.
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,

    // Question
    0x03, 0x77, 0x77, 0x77,   // QNAME: www.google.com in DNS format.
    0x06, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65,
    0x03, 0x63, 0x6f, 0x6d, 0x00,

    0x00, 0x01,               // QTYPE: A query.
    0x00, 0x01,               // QCLASS: IN class.
  };

  int expected_size = arraysize(query_data);
  EXPECT_EQ(expected_size, q1.io_buffer()->size());
  EXPECT_EQ(0, memcmp(q1.io_buffer()->data(), query_data, expected_size));
}

TEST(DnsQueryTest, CloneTest) {
  std::string kQname("\003www\006google\003com", 16);
  DnsQuery q1(kQname, kDNS_A, base::Bind(&base::RandInt));

  scoped_ptr<DnsQuery> q2(q1.CloneWithNewId());
  EXPECT_EQ(q1.io_buffer()->size(), q2->io_buffer()->size());
  EXPECT_EQ(q1.qtype(), q2->qtype());
  EXPECT_EQ(q1.question_size(), q2->question_size());
  EXPECT_EQ(0, memcmp(q1.question_data(), q2->question_data(),
                      q1.question_size()));
}

TEST(DnsQueryTest, RandomIdTest) {
  std::string kQname("\003www\006google\003com", 16);

  // Since id fields are 16-bit values, we iterate to reduce the
  // probability of collision, to avoid a flaky test.
  bool ids_are_random = false;
  for (int i = 0; i < 1000; ++i) {
    DnsQuery q1(kQname, kDNS_A, base::Bind(&base::RandInt));
    DnsQuery q2(kQname, kDNS_A, base::Bind(&base::RandInt));
    scoped_ptr<DnsQuery> q3(q1.CloneWithNewId());
    ids_are_random = q1.id () != q2.id() && q1.id() != q3->id();
    if (ids_are_random)
      break;
  }
  EXPECT_TRUE(ids_are_random);
}

}  // namespace net
