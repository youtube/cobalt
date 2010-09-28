// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser_netware.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserNetwareTest;

TEST_F(FtpDirectoryListingParserNetwareTest, Good) {
  base::Time mock_current_time;
  ASSERT_TRUE(base::Time::FromString(L"Tue, 15 Nov 1994 12:45:26 GMT",
                                     &mock_current_time));

  const struct SingleLineTestData good_cases[] = {
    { "d [RWCEAFMS] ftpadmin 512 Jan 29  2004 pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2004, 1, 29, 0, 0 },
    { "- [RW------] ftpadmin 123 Nov 11  18:25 afile",
      net::FtpDirectoryListingEntry::FILE, "afile", 123,
      1994, 11, 11, 18, 25 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    net::FtpDirectoryListingParserNetware parser(mock_current_time);
    // The parser requires a "total n" like before accepting regular input.
    ASSERT_TRUE(parser.ConsumeLine(UTF8ToUTF16("total 1")));
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserNetwareTest, Bad) {
  base::Time mock_current_time;
  ASSERT_TRUE(base::Time::FromString(L"Tue, 15 Nov 1994 12:45:26 GMT",
                                     &mock_current_time));

  const char* bad_cases[] = {
    " foo",
    "garbage",
    "d [] ftpadmin 512 Jan 29  2004 pub",
    "d [XGARBAGE] ftpadmin 512 Jan 29  2004 pub",
    "d [RWCEAFMS] 512 Jan 29  2004 pub",
    "d [RWCEAFMS] ftpadmin -1 Jan 29  2004 pub",
    "l [RW------] ftpadmin 512 Jan 29  2004 pub",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserNetware parser(mock_current_time);
    // The parser requires a "total n" like before accepting regular input.
    ASSERT_TRUE(parser.ConsumeLine(UTF8ToUTF16("total 1")));
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
