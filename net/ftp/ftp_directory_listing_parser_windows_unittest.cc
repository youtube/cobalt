// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "net/ftp/ftp_directory_listing_parser_windows.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserWindowsTest;

TEST_F(FtpDirectoryListingParserWindowsTest, Good) {
  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);

  const struct SingleLineTestData good_cases[] = {
    { "11-02-09  05:32PM       <DIR>          NT",
      net::FtpDirectoryListingEntry::DIRECTORY, "NT", -1,
      2009, 11, 2, 17, 32 },
    { "01-06-09  02:42PM                  458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2009, 1, 6, 14, 42 },
    { "01-06-09  02:42AM                  1 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 1,
      2009, 1, 6, 2, 42 },
    { "01-06-01  02:42AM                  458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      2001, 1, 6, 2, 42 },
    { "01-06-00  02:42AM                  458 Corner1.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner1.txt", 458,
      2000, 1, 6, 2, 42 },
    { "01-06-99  02:42AM                  458 Corner2.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner2.txt", 458,
      1999, 1, 6, 2, 42 },
    { "01-06-80  02:42AM                  458 Corner3.txt",
      net::FtpDirectoryListingEntry::FILE, "Corner3.txt", 458,
      1980, 1, 6, 2, 42 },
#if !defined(OS_LINUX)
    // TODO(phajdan.jr): Re-enable when 2038-year problem is fixed on Linux.
    { "01-06-79  02:42AM                  458 Corner4",
      net::FtpDirectoryListingEntry::FILE, "Corner4", 458,
      2079, 1, 6, 2, 42 },
#endif  // !defined (OS_LINUX)
    { "01-06-1979  02:42AM                458 Readme.txt",
      net::FtpDirectoryListingEntry::FILE, "Readme.txt", 458,
      1979, 1, 6, 2, 42 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, good_cases[i].input));

    net::FtpDirectoryListingParserWindows parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserWindowsTest, Bad) {
  const char* bad_cases[] = {
    "",
    "garbage",
    "11-02-09  05:32PM       <GARBAGE>      NT",
    "11-02-09  05:32         <DIR>          NT",
    "11-FEB-09 05:32PM       <DIR>          NT",
    "11-02     05:32PM       <DIR>          NT",
    "11-02-09  05:32PM                 -1   NT",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserWindows parser;
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
