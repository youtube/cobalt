// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/capsule.h"

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_ip_address.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/web_transport.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::webtransport::StreamType;

namespace quiche {
namespace test {

class CapsuleParserPeer {
 public:
  static std::string* buffered_data(CapsuleParser* capsule_parser) {
    return &capsule_parser->buffered_data_;
  }
};

namespace {

class MockCapsuleParserVisitor : public CapsuleParser::Visitor {
 public:
  MockCapsuleParserVisitor() {
    ON_CALL(*this, OnCapsule(_)).WillByDefault(Return(true));
  }
  ~MockCapsuleParserVisitor() override = default;
  MOCK_METHOD(bool, OnCapsule, (const Capsule& capsule), (override));
  MOCK_METHOD(void, OnCapsuleParseFailure, (absl::string_view error_message),
              (override));
};

class CapsuleTest : public QuicheTest {
 public:
  CapsuleTest() : capsule_parser_(&visitor_) {}

  void ValidateParserIsEmpty() {
    EXPECT_CALL(visitor_, OnCapsule(_)).Times(0);
    EXPECT_CALL(visitor_, OnCapsuleParseFailure(_)).Times(0);
    capsule_parser_.ErrorIfThereIsRemainingBufferedData();
    EXPECT_TRUE(CapsuleParserPeer::buffered_data(&capsule_parser_)->empty());
  }

  void TestSerialization(const Capsule& capsule,
                         const std::string& expected_bytes) {
    quiche::QuicheBuffer serialized_capsule =
        SerializeCapsule(capsule, SimpleBufferAllocator::Get());
    quiche::test::CompareCharArraysWithHexError(
        "Serialized capsule", serialized_capsule.data(),
        serialized_capsule.size(), expected_bytes.data(),
        expected_bytes.size());
  }

  ::testing::StrictMock<MockCapsuleParserVisitor> visitor_;
  CapsuleParser capsule_parser_;
};

TEST_F(CapsuleTest, DatagramCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "00"                // DATAGRAM capsule type
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // HTTP Datagram payload
  );
  std::string datagram_payload = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  Capsule expected_capsule = Capsule::Datagram(datagram_payload);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, LegacyDatagramCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "80ff37a0"          // LEGACY_DATAGRAM capsule type
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // HTTP Datagram payload
  );
  std::string datagram_payload = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  Capsule expected_capsule = Capsule::LegacyDatagram(datagram_payload);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, LegacyDatagramWithoutContextCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "80ff37a5"          // LEGACY_DATAGRAM_WITHOUT_CONTEXT capsule type
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // HTTP Datagram payload
  );
  std::string datagram_payload = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  Capsule expected_capsule =
      Capsule::LegacyDatagramWithoutContext(datagram_payload);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, CloseWebTransportStreamCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "6843"        // CLOSE_WEBTRANSPORT_STREAM capsule type
      "09"          // capsule length
      "00001234"    // 0x1234 error code
      "68656c6c6f"  // "hello" error message
  );
  Capsule expected_capsule = Capsule::CloseWebTransportSession(
      /*error_code=*/0x1234, /*error_message=*/"hello");
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, AddressAssignCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "9ECA6A00"  // ADDRESS_ASSIGN capsule type
      "1A"        // capsule length = 26
      // first assigned address
      "00"        // request ID = 0
      "04"        // IP version = 4
      "C000022A"  // 192.0.2.42
      "1F"        // prefix length = 31
      // second assigned address
      "01"                                // request ID = 1
      "06"                                // IP version = 6
      "20010db8123456780000000000000000"  // 2001:db8:1234:5678::
      "40"                                // prefix length = 64
  );
  Capsule expected_capsule = Capsule::AddressAssign();
  quiche::QuicheIpAddress ip_address1;
  ip_address1.FromString("192.0.2.42");
  PrefixWithId assigned_address1;
  assigned_address1.request_id = 0;
  assigned_address1.ip_prefix =
      quiche::QuicheIpPrefix(ip_address1, /*prefix_length=*/31);
  expected_capsule.address_assign_capsule().assigned_addresses.push_back(
      assigned_address1);
  quiche::QuicheIpAddress ip_address2;
  ip_address2.FromString("2001:db8:1234:5678::");
  PrefixWithId assigned_address2;
  assigned_address2.request_id = 1;
  assigned_address2.ip_prefix =
      quiche::QuicheIpPrefix(ip_address2, /*prefix_length=*/64);
  expected_capsule.address_assign_capsule().assigned_addresses.push_back(
      assigned_address2);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, AddressRequestCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "9ECA6A01"  // ADDRESS_REQUEST capsule type
      "1A"        // capsule length = 26
      // first requested address
      "00"        // request ID = 0
      "04"        // IP version = 4
      "C000022A"  // 192.0.2.42
      "1F"        // prefix length = 31
      // second requested address
      "01"                                // request ID = 1
      "06"                                // IP version = 6
      "20010db8123456780000000000000000"  // 2001:db8:1234:5678::
      "40"                                // prefix length = 64
  );
  Capsule expected_capsule = Capsule::AddressRequest();
  quiche::QuicheIpAddress ip_address1;
  ip_address1.FromString("192.0.2.42");
  PrefixWithId requested_address1;
  requested_address1.request_id = 0;
  requested_address1.ip_prefix =
      quiche::QuicheIpPrefix(ip_address1, /*prefix_length=*/31);
  expected_capsule.address_request_capsule().requested_addresses.push_back(
      requested_address1);
  quiche::QuicheIpAddress ip_address2;
  ip_address2.FromString("2001:db8:1234:5678::");
  PrefixWithId requested_address2;
  requested_address2.request_id = 1;
  requested_address2.ip_prefix =
      quiche::QuicheIpPrefix(ip_address2, /*prefix_length=*/64);
  expected_capsule.address_request_capsule().requested_addresses.push_back(
      requested_address2);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, RouteAdvertisementCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "9ECA6A02"  // ROUTE_ADVERTISEMENT capsule type
      "2C"        // capsule length = 44
      // first IP address range
      "04"        // IP version = 4
      "C0000218"  // 192.0.2.24
      "C000022A"  // 192.0.2.42
      "00"        // ip protocol = 0
      // second IP address range
      "06"                                // IP version = 6
      "00000000000000000000000000000000"  // ::
      "ffffffffffffffffffffffffffffffff"  // all ones IPv6 address
      "01"                                // ip protocol = 1 (ICMP)
  );
  Capsule expected_capsule = Capsule::RouteAdvertisement();
  IpAddressRange ip_address_range1;
  ip_address_range1.start_ip_address.FromString("192.0.2.24");
  ip_address_range1.end_ip_address.FromString("192.0.2.42");
  ip_address_range1.ip_protocol = 0;
  expected_capsule.route_advertisement_capsule().ip_address_ranges.push_back(
      ip_address_range1);
  IpAddressRange ip_address_range2;
  ip_address_range2.start_ip_address.FromString("::");
  ip_address_range2.end_ip_address.FromString(
      "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
  ip_address_range2.ip_protocol = 1;
  expected_capsule.route_advertisement_capsule().ip_address_ranges.push_back(
      ip_address_range2);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportStreamData) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d3b"  // WT_STREAM without FIN
      "04"        // capsule length
      "17"        // stream ID
      "abcdef"    // stream payload
  );
  Capsule expected_capsule = Capsule(WebTransportStreamDataCapsule());
  expected_capsule.web_transport_stream_data().stream_id = 0x17;
  expected_capsule.web_transport_stream_data().data = "\xab\xcd\xef";
  expected_capsule.web_transport_stream_data().fin = false;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}
TEST_F(CapsuleTest, WebTransportStreamDataWithFin) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d3c"  // data with FIN
      "04"        // capsule length
      "17"        // stream ID
      "abcdef"    // stream payload
  );
  Capsule expected_capsule = Capsule(WebTransportStreamDataCapsule());
  expected_capsule.web_transport_stream_data().stream_id = 0x17;
  expected_capsule.web_transport_stream_data().data = "\xab\xcd\xef";
  expected_capsule.web_transport_stream_data().fin = true;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportResetStream) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d39"  // WT_RESET_STREAM
      "02"        // capsule length
      "17"        // stream ID
      "07"        // error code
  );
  Capsule expected_capsule = Capsule(WebTransportResetStreamCapsule());
  expected_capsule.web_transport_reset_stream().stream_id = 0x17;
  expected_capsule.web_transport_reset_stream().error_code = 0x07;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportStopSending) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d3a"  // WT_STOP_SENDING
      "02"        // capsule length
      "17"        // stream ID
      "07"        // error code
  );
  Capsule expected_capsule = Capsule(WebTransportStopSendingCapsule());
  expected_capsule.web_transport_stop_sending().stream_id = 0x17;
  expected_capsule.web_transport_stop_sending().error_code = 0x07;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportMaxStreamData) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d3e"  // WT_MAX_STREAM_DATA
      "02"        // capsule length
      "17"        // stream ID
      "10"        // max stream data
  );
  Capsule expected_capsule = Capsule(WebTransportMaxStreamDataCapsule());
  expected_capsule.web_transport_max_stream_data().stream_id = 0x17;
  expected_capsule.web_transport_max_stream_data().max_stream_data = 0x10;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportMaxStreamsBi) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d3f"  // WT_MAX_STREAMS (bidi)
      "01"        // capsule length
      "17"        // max streams
  );
  Capsule expected_capsule = Capsule(WebTransportMaxStreamsCapsule());
  expected_capsule.web_transport_max_streams().stream_type =
      StreamType::kBidirectional;
  expected_capsule.web_transport_max_streams().max_stream_count = 0x17;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, WebTransportMaxStreamsUni) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "990b4d40"  // WT_MAX_STREAMS (unidi)
      "01"        // capsule length
      "17"        // max streams
  );
  Capsule expected_capsule = Capsule(WebTransportMaxStreamsCapsule());
  expected_capsule.web_transport_max_streams().stream_type =
      StreamType::kUnidirectional;
  expected_capsule.web_transport_max_streams().max_stream_count = 0x17;
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, UnknownCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "17"                // unknown capsule type of 0x17
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // unknown capsule data
  );
  std::string unknown_capsule_data = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  Capsule expected_capsule = Capsule::Unknown(0x17, unknown_capsule_data);
  {
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
  TestSerialization(expected_capsule, capsule_fragment);
}

TEST_F(CapsuleTest, TwoCapsules) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "00"                // DATAGRAM capsule type
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // HTTP Datagram payload
      "00"                // DATAGRAM capsule type
      "08"                // capsule length
      "b1b2b3b4b5b6b7b8"  // HTTP Datagram payload
  );
  std::string datagram_payload1 = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  std::string datagram_payload2 = absl::HexStringToBytes("b1b2b3b4b5b6b7b8");
  Capsule expected_capsule1 = Capsule::Datagram(datagram_payload1);
  Capsule expected_capsule2 = Capsule::Datagram(datagram_payload2);
  {
    InSequence s;
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule1));
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule2));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  ValidateParserIsEmpty();
}

TEST_F(CapsuleTest, TwoCapsulesPartialReads) {
  std::string capsule_fragment1 = absl::HexStringToBytes(
      "00"        // first capsule DATAGRAM capsule type
      "08"        // first capsule length
      "a1a2a3a4"  // first half of HTTP Datagram payload of first capsule
  );
  std::string capsule_fragment2 = absl::HexStringToBytes(
      "a5a6a7a8"  // second half of HTTP Datagram payload 1
      "00"        // second capsule DATAGRAM capsule type
  );
  std::string capsule_fragment3 = absl::HexStringToBytes(
      "08"                // second capsule length
      "b1b2b3b4b5b6b7b8"  // HTTP Datagram payload of second capsule
  );
  capsule_parser_.ErrorIfThereIsRemainingBufferedData();
  std::string datagram_payload1 = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  std::string datagram_payload2 = absl::HexStringToBytes("b1b2b3b4b5b6b7b8");
  Capsule expected_capsule1 = Capsule::Datagram(datagram_payload1);
  Capsule expected_capsule2 = Capsule::Datagram(datagram_payload2);
  {
    InSequence s;
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule1));
    EXPECT_CALL(visitor_, OnCapsule(expected_capsule2));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment1));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment2));
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment3));
  }
  ValidateParserIsEmpty();
}

TEST_F(CapsuleTest, TwoCapsulesOneByteAtATime) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "00"                // DATAGRAM capsule type
      "08"                // capsule length
      "a1a2a3a4a5a6a7a8"  // HTTP Datagram payload
      "00"                // DATAGRAM capsule type
      "08"                // capsule length
      "b1b2b3b4b5b6b7b8"  // HTTP Datagram payload
  );
  std::string datagram_payload1 = absl::HexStringToBytes("a1a2a3a4a5a6a7a8");
  std::string datagram_payload2 = absl::HexStringToBytes("b1b2b3b4b5b6b7b8");
  Capsule expected_capsule1 = Capsule::Datagram(datagram_payload1);
  Capsule expected_capsule2 = Capsule::Datagram(datagram_payload2);
  for (size_t i = 0; i < capsule_fragment.size(); i++) {
    if (i < capsule_fragment.size() / 2 - 1) {
      EXPECT_CALL(visitor_, OnCapsule(_)).Times(0);
      ASSERT_TRUE(
          capsule_parser_.IngestCapsuleFragment(capsule_fragment.substr(i, 1)));
    } else if (i == capsule_fragment.size() / 2 - 1) {
      EXPECT_CALL(visitor_, OnCapsule(expected_capsule1));
      ASSERT_TRUE(
          capsule_parser_.IngestCapsuleFragment(capsule_fragment.substr(i, 1)));
      EXPECT_TRUE(CapsuleParserPeer::buffered_data(&capsule_parser_)->empty());
    } else if (i < capsule_fragment.size() - 1) {
      EXPECT_CALL(visitor_, OnCapsule(_)).Times(0);
      ASSERT_TRUE(
          capsule_parser_.IngestCapsuleFragment(capsule_fragment.substr(i, 1)));
    } else {
      EXPECT_CALL(visitor_, OnCapsule(expected_capsule2));
      ASSERT_TRUE(
          capsule_parser_.IngestCapsuleFragment(capsule_fragment.substr(i, 1)));
      EXPECT_TRUE(CapsuleParserPeer::buffered_data(&capsule_parser_)->empty());
    }
  }
  capsule_parser_.ErrorIfThereIsRemainingBufferedData();
  EXPECT_TRUE(CapsuleParserPeer::buffered_data(&capsule_parser_)->empty());
}

TEST_F(CapsuleTest, PartialCapsuleThenError) {
  std::string capsule_fragment = absl::HexStringToBytes(
      "00"        // DATAGRAM capsule type
      "08"        // capsule length
      "a1a2a3a4"  // first half of HTTP Datagram payload
  );
  EXPECT_CALL(visitor_, OnCapsule(_)).Times(0);
  {
    EXPECT_CALL(visitor_, OnCapsuleParseFailure(_)).Times(0);
    ASSERT_TRUE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
  }
  {
    EXPECT_CALL(visitor_,
                OnCapsuleParseFailure(
                    "Incomplete capsule left at the end of the stream"));
    capsule_parser_.ErrorIfThereIsRemainingBufferedData();
  }
}

TEST_F(CapsuleTest, RejectOverlyLongCapsule) {
  std::string capsule_fragment = absl::HexStringToBytes(
                                     "17"        // unknown capsule type of 0x17
                                     "80123456"  // capsule length
                                     ) +
                                 std::string(1111111, '?');
  EXPECT_CALL(visitor_, OnCapsuleParseFailure(
                            "Refusing to buffer too much capsule data"));
  EXPECT_FALSE(capsule_parser_.IngestCapsuleFragment(capsule_fragment));
}

}  // namespace
}  // namespace test
}  // namespace quiche
