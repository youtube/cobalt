// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;
using std::map;
using std::string;
using std::vector;

namespace net {

namespace {

char* AsChars(unsigned char* data) {
  return reinterpret_cast<char*>(data);
}

}  // namespace

namespace test {

class TestCryptoVisitor : public ::net::CryptoFramerVisitorInterface {
 public:
  TestCryptoVisitor()
      : error_count_(0) {
  }

  ~TestCryptoVisitor() {}

  virtual void OnError(CryptoFramer* framer) {
    LOG(ERROR) << "CryptoFramer Error: " << framer->error();
    error_count_++;
  }

  virtual void OnHandshakeMessage(const CryptoHandshakeMessage& message) {
    message_tags_.push_back(message.tag);
    message_maps_.push_back(map<CryptoTag, string>());
    CryptoTagValueMap::const_iterator it = message.tag_value_map.begin();
    while (it != message.tag_value_map.end()) {
      message_maps_.back()[it->first] = it->second.as_string();
      ++it;
    }
  }

  CryptoFramer framer_;

  // Counters from the visitor callbacks.
  int error_count_;

  CryptoTagVector message_tags_;
  vector<map<CryptoTag, string> > message_maps_;
};

}  // namespace test

TEST(CryptoFramerTest, ConstructHandshakeMessage) {
  CryptoHandshakeMessage message;
  message.tag = 0xFFAA7733;
  message.tag_value_map[0x12345678] = "abcdef";
  message.tag_value_map[0x12345679] = "ghijk";
  message.tag_value_map[0x1234567A] = "lmnopqr";

  unsigned char packet[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x03, 0x00,
    // tag 1
    0x78, 0x56, 0x34, 0x12,
    // tag 2
    0x79, 0x56, 0x34, 0x12,
    // tag 3
    0x7A, 0x56, 0x34, 0x12,
    // len 1
    0x06, 0x00,
    // len 2
    0x05, 0x00,
    // len 3
    0x07, 0x00,
    // padding
    0xAB, 0xAB,
    // value 1
    'a',  'b',  'c',  'd',
    'e',  'f',
    // value 2
    'g',  'h',  'i',  'j',
    'k',
    // value 3
    'l',  'm',  'n',  'o',
    'p',  'q',  'r',
  };

  CryptoFramer framer;
  scoped_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  ASSERT_TRUE(data.get() != NULL);
  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST(CryptoFramerTest, ConstructHandshakeMessageWithTwoKeys) {
  CryptoHandshakeMessage message;
  message.tag = 0xFFAA7733;
  message.tag_value_map[0x12345678] = "abcdef";
  message.tag_value_map[0x12345679] = "ghijk";

  unsigned char packet[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x02, 0x00,
    // tag 1
    0x78, 0x56, 0x34, 0x12,
    // tag 2
    0x79, 0x56, 0x34, 0x12,
    // len 1
    0x06, 0x00,
    // len 2
    0x05, 0x00,
    // value 1
    'a',  'b',  'c',  'd',
    'e',  'f',
    // value 2
    'g',  'h',  'i',  'j',
    'k',
  };

  CryptoFramer framer;
  scoped_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  ASSERT_TRUE(data.get() != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST(CryptoFramerTest, ConstructHandshakeMessageTooManyEntries) {
  CryptoHandshakeMessage message;
  message.tag = 0xFFAA7733;
  for (uint32 key = 1; key <= kMaxEntries + 1; ++key) {
    message.tag_value_map[key] = "abcdef";
  }

  CryptoFramer framer;
  scoped_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  EXPECT_TRUE(data.get() == NULL);
}


TEST(CryptoFramerTest, ConstructHandshakeMessageInvalidLength) {
  CryptoHandshakeMessage message;
  message.tag = 0xFFAA7733;
  message.tag_value_map[0x12345678] = "";

  CryptoFramer framer;
  scoped_ptr<QuicData> data(framer.ConstructHandshakeMessage(message));
  EXPECT_TRUE(data.get() == NULL);
}

TEST(CryptoFramerTest, ProcessInput) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x02, 0x00,
    // tag 1
    0x78, 0x56, 0x34, 0x12,
    // tag 2
    0x79, 0x56, 0x34, 0x12,
    // len 1
    0x06, 0x00,
    // len 2
    0x05, 0x00,
    // value 1
    'a',  'b',  'c',  'd',
    'e',  'f',
    // value 2
    'g',  'h',  'i',  'j',
    'k',
  };

  EXPECT_TRUE(framer.ProcessInput(StringPiece(AsChars(input),
                                              arraysize(input))));
  ASSERT_EQ(1u, visitor.message_tags_.size());
  EXPECT_EQ(0xFFAA7733, visitor.message_tags_[0]);
  EXPECT_EQ(2u, visitor.message_maps_[0].size());
  EXPECT_EQ("abcdef",visitor.message_maps_[0][0x12345678]);
  EXPECT_EQ("ghijk", visitor.message_maps_[0][0x12345679]);
}

TEST(CryptoFramerTest, ProcessInputIncrementally) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x02, 0x00,
    // tag 1
    0x78, 0x56, 0x34, 0x12,
    // tag 2
    0x79, 0x56, 0x34, 0x12,
    // len 1
    0x06, 0x00,
    // len 2
    0x05, 0x00,
    // value 1
    'a',  'b',  'c',  'd',
    'e',  'f',
    // value 2
    'g',  'h',  'i',  'j',
    'k',
  };

  for (size_t i = 0; i < arraysize(input); i++) {
    EXPECT_TRUE(framer.ProcessInput(StringPiece(AsChars(input)+ i, 1)));
  }
  ASSERT_EQ(1u, visitor.message_tags_.size());
  EXPECT_EQ(0xFFAA7733, visitor.message_tags_[0]);
  EXPECT_EQ(2u, visitor.message_maps_[0].size());
  EXPECT_EQ("abcdef",visitor.message_maps_[0][0x12345678]);
  EXPECT_EQ("ghijk", visitor.message_maps_[0][0x12345679]);
}

TEST(CryptoFramerTest, ProcessInputTagsOutOfOrder) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x02, 0x00,
    // tag 1
    0x79, 0x56, 0x34, 0x12,
    // tag 2
    0x78, 0x56, 0x34, 0x12,
  };

  EXPECT_FALSE(framer.ProcessInput(StringPiece(AsChars(input),
                                               arraysize(input))));
  EXPECT_EQ(QUIC_CRYPTO_TAGS_OUT_OF_ORDER, framer.error());
}

TEST(CryptoFramerTest, ProcessInputTooManyEntries) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0xA0, 0x00,
  };

  EXPECT_FALSE(framer.ProcessInput(StringPiece(AsChars(input),
                                               arraysize(input))));
  EXPECT_EQ(QUIC_CRYPTO_TOO_MANY_ENTRIES, framer.error());
}

TEST(CryptoFramerTest, ProcessInputInvalidLength) {
  test::TestCryptoVisitor visitor;
  CryptoFramer framer;
  framer.set_visitor(&visitor);

  unsigned char input[] = {
    // tag
    0x33, 0x77, 0xAA, 0xFF,
    // num entries
    0x02, 0x00,
    // tag 1
    0x78, 0x56, 0x34, 0x12,
    // tag 2
    0x79, 0x56, 0x34, 0x12,
    // len 1
    0x00, 0x00,
    // len 2
    0x05, 0x00,
  };

  EXPECT_FALSE(framer.ProcessInput(StringPiece(AsChars(input),
                                               arraysize(input))));
  EXPECT_EQ(QUIC_CRYPTO_INVALID_VALUE_LENGTH, framer.error());
}

}  // namespace net
