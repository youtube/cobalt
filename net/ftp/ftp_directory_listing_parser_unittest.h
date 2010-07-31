// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_
#define NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_
#pragma once

#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class FtpDirectoryListingParserTest : public testing::Test {
 public:
  struct SingleLineTestData {
    const char* input;
    FtpDirectoryListingEntry::Type type;
    const char* filename;
    int64 size;
    int year;
    int month;
    int day_of_month;
    int hour;
    int minute;
  };

 protected:
  FtpDirectoryListingParserTest() {}

  void RunSingleLineTestCase(FtpDirectoryListingParser* parser,
                             const SingleLineTestData& test_case) {
    ASSERT_TRUE(parser->ConsumeLine(UTF8ToUTF16(test_case.input)));
    ASSERT_TRUE(parser->EntryAvailable());
    FtpDirectoryListingEntry entry = parser->PopEntry();
    EXPECT_EQ(test_case.type, entry.type);
    EXPECT_EQ(UTF8ToUTF16(test_case.filename), entry.name);
    EXPECT_EQ(test_case.size, entry.size);

    base::Time::Exploded time_exploded;
    entry.last_modified.LocalExplode(&time_exploded);
    EXPECT_EQ(test_case.year, time_exploded.year);
    EXPECT_EQ(test_case.month, time_exploded.month);
    EXPECT_EQ(test_case.day_of_month, time_exploded.day_of_month);
    EXPECT_EQ(test_case.hour, time_exploded.hour);
    EXPECT_EQ(test_case.minute, time_exploded.minute);
    EXPECT_EQ(0, time_exploded.second);
    EXPECT_EQ(0, time_exploded.millisecond);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpDirectoryListingParserTest);
};

}  // namespace net

#endif  // NET_FTP_FTP_DIRECTORY_LISTING_PARSER_UNITTEST_H_

