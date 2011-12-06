// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_response.h"

#include "net/base/io_buffer.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsRecordParserTest, Constructor) {
  const char data[] = { 0 };

  EXPECT_FALSE(DnsRecordParser().IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 1, 0).IsValid());
  EXPECT_TRUE(DnsRecordParser(data, 1, 1).IsValid());

  EXPECT_FALSE(DnsRecordParser(data, 1, 0).AtEnd());
  EXPECT_TRUE(DnsRecordParser(data, 1, 1).AtEnd());
}

TEST(DnsRecordParserTest, ParseName) {
  const uint8 data[] = {
      // all labels "foo.example.com"
      0x03, 'f', 'o', 'o',
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
      0x03, 'c', 'o', 'm',
      // byte 0x10
      0x00,
      // byte 0x11
      // part label, part pointer, "bar.example.com"
      0x03, 'b', 'a', 'r',
      0xc0, 0x04,
      // byte 0x17
      // all pointer to "bar.example.com", 2 jumps
      0xc0, 0x11,
      // byte 0x1a
  };

  std::string out;
  DnsRecordParser parser(data, sizeof(data), 0);
  ASSERT_TRUE(parser.IsValid());

  EXPECT_EQ(0x11, parser.ParseName(data + 0x00, &out));
  EXPECT_EQ("foo.example.com", out);
  // Check that the last "." is never stored.
  out.clear();
  EXPECT_EQ(0x1, parser.ParseName(data + 0x10, &out));
  EXPECT_EQ("", out);
  out.clear();
  EXPECT_EQ(0x6, parser.ParseName(data + 0x11, &out));
  EXPECT_EQ("bar.example.com", out);
  out.clear();
  EXPECT_EQ(0x2, parser.ParseName(data + 0x17, &out));
  EXPECT_EQ("bar.example.com", out);

  // Parse name without storing it.
  EXPECT_EQ(0x11, parser.ParseName(data + 0x00, NULL));
  EXPECT_EQ(0x1, parser.ParseName(data + 0x10, NULL));
  EXPECT_EQ(0x6, parser.ParseName(data + 0x11, NULL));
  EXPECT_EQ(0x2, parser.ParseName(data + 0x17, NULL));

  // Check that it works even if initial position is different.
  parser = DnsRecordParser(data, sizeof(data), 0x12);
  EXPECT_EQ(0x6, parser.ParseName(data + 0x11, NULL));
}

TEST(DnsRecordParserTest, ParseNameFail) {
  const uint8 data[] = {
      // label length beyond packet
      0x30, 'x', 'x',
      0x00,
      // pointer offset beyond packet
      0xc0, 0x20,
      // pointer loop
      0xc0, 0x08,
      0xc0, 0x06,
      // incorrect label type (currently supports only direct and pointer)
      0x80, 0x00,
      // truncated name (missing root label)
      0x02, 'x', 'x',
  };

  DnsRecordParser parser(data, sizeof(data), 0);
  ASSERT_TRUE(parser.IsValid());

  std::string out;
  EXPECT_EQ(0, parser.ParseName(data + 0x00, &out));
  EXPECT_EQ(0, parser.ParseName(data + 0x04, &out));
  EXPECT_EQ(0, parser.ParseName(data + 0x08, &out));
  EXPECT_EQ(0, parser.ParseName(data + 0x0a, &out));
  EXPECT_EQ(0, parser.ParseName(data + 0x0c, &out));
  EXPECT_EQ(0, parser.ParseName(data + 0x0e, &out));
}

TEST(DnsRecordParserTest, ParseRecord) {
  const uint8 data[] = {
      // Type CNAME record.
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
      0x03, 'c', 'o', 'm',
      0x00,
      0x00, 0x05,              // TYPE is CNAME.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x01, 0x24, 0x74,  // TTL is 0x00012474.
      0x00, 0x06,              // RDLENGTH is 6 bytes.
      0x03, 'f', 'o', 'o',     // compressed name in record
      0xc0, 0x00,
      // Type A record.
      0x03, 'b', 'a', 'r',     // compressed owner name
      0xc0, 0x00,
      0x00, 0x01,              // TYPE is A.
      0x00, 0x01,              // CLASS is IN.
      0x00, 0x20, 0x13, 0x55,  // TTL is 0x00201355.
      0x00, 0x04,              // RDLENGTH is 4 bytes.
      0x7f, 0x02, 0x04, 0x01,  // IP is 127.2.4.1
  };

  std::string out;
  DnsRecordParser parser(data, sizeof(data), 0);

  DnsResourceRecord record;
  EXPECT_TRUE(parser.ParseRecord(&record));
  EXPECT_EQ("example.com", record.name);
  EXPECT_EQ(dns_protocol::kTypeCNAME, record.type);
  EXPECT_EQ(dns_protocol::kClassIN, record.klass);
  EXPECT_EQ(0x00012474u, record.ttl);
  EXPECT_EQ(6u, record.rdata.length());
  EXPECT_EQ(6, parser.ParseName(record.rdata.data(), &out));
  EXPECT_EQ("foo.example.com", out);
  EXPECT_FALSE(parser.AtEnd());

  EXPECT_TRUE(parser.ParseRecord(&record));
  EXPECT_EQ("bar.example.com", record.name);
  EXPECT_EQ(dns_protocol::kTypeA, record.type);
  EXPECT_EQ(dns_protocol::kClassIN, record.klass);
  EXPECT_EQ(0x00201355u, record.ttl);
  EXPECT_EQ(4u, record.rdata.length());
  EXPECT_EQ(base::StringPiece("\x7f\x02\x04\x01"), record.rdata);
  EXPECT_TRUE(parser.AtEnd());

  // Test truncated record.
  parser = DnsRecordParser(data, sizeof(data) - 2, 0);
  EXPECT_TRUE(parser.ParseRecord(&record));
  EXPECT_FALSE(parser.AtEnd());
  EXPECT_FALSE(parser.ParseRecord(&record));
}

TEST(DnsResponseTest, InitParse) {
  // This includes \0 at the end.
  const char qname_data[] = "\x0A""codereview""\x08""chromium""\x03""org";
  const base::StringPiece qname(qname_data, sizeof(qname_data));
  // Compilers want to copy when binding temporary to const &, so must use heap.
  scoped_ptr<DnsQuery> query(new DnsQuery(0xcafe, qname, dns_protocol::kTypeA));

  const uint8 response_data[] = {
    // Header
    0xca, 0xfe,               // ID
    0x81, 0x80,               // Standard query response, RA, no error
    0x00, 0x01,               // 1 question
    0x00, 0x02,               // 2 RRs (answers)
    0x00, 0x00,               // 0 authority RRs
    0x00, 0x00,               // 0 additional RRs

    // Question
    // This part is echoed back from the respective query.
    0x0a, 'c', 'o', 'd', 'e', 'r', 'e', 'v', 'i', 'e', 'w',
    0x08, 'c', 'h', 'r', 'o', 'm', 'i', 'u', 'm',
    0x03, 'o', 'r', 'g',
    0x00,
    0x00, 0x01,        // TYPE is A.
    0x00, 0x01,        // CLASS is IN.

    // Answer 1
    0xc0, 0x0c,        // NAME is a pointer to name in Question section.
    0x00, 0x05,        // TYPE is CNAME.
    0x00, 0x01,        // CLASS is IN.
    0x00, 0x01,        // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74,
    0x00, 0x12,        // RDLENGTH is 18 bytes.
    // ghs.l.google.com in DNS format.
    0x03, 'g', 'h', 's',
    0x01, 'l',
    0x06, 'g', 'o', 'o', 'g', 'l', 'e',
    0x03, 'c', 'o', 'm',
    0x00,

    // Answer 2
    0xc0, 0x35,         // NAME is a pointer to name in Answer 1.
    0x00, 0x01,         // TYPE is A.
    0x00, 0x01,         // CLASS is IN.
    0x00, 0x00,         // TTL (4 bytes) is 53 seconds.
    0x00, 0x35,
    0x00, 0x04,         // RDLENGTH is 4 bytes.
    0x4a, 0x7d,         // RDATA is the IP: 74.125.95.121
    0x5f, 0x79,
  };

  DnsResponse resp;
  memcpy(resp.io_buffer()->data(), response_data, sizeof(response_data));

  // Reject too short.
  EXPECT_FALSE(resp.InitParse(query->io_buffer()->size() - 1, *query));

  // Reject wrong id.
  scoped_ptr<DnsQuery> other_query(query->CloneWithNewId(0xbeef));
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *other_query));

  // Reject wrong question.
  scoped_ptr<DnsQuery> wrong_query(
      new DnsQuery(0xcafe, qname, dns_protocol::kTypeCNAME));
  EXPECT_FALSE(resp.InitParse(sizeof(response_data), *wrong_query));

  // Accept matching question.
  EXPECT_TRUE(resp.InitParse(sizeof(response_data), *query));

  // Check header access.
  EXPECT_EQ(0x81, resp.flags0());
  EXPECT_EQ(0x80, resp.flags1());
  EXPECT_EQ(0x0, resp.rcode());
  EXPECT_EQ(2, resp.answer_count());

  DnsResourceRecord record;
  DnsRecordParser parser = resp.Parser();
  EXPECT_TRUE(parser.ParseRecord(&record));
  EXPECT_TRUE(parser.ParseRecord(&record));
  EXPECT_FALSE(parser.ParseRecord(&record));
}

}  // namespace

}  // namespace net
