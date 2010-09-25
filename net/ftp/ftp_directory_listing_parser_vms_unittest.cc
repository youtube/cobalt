// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/ftp/ftp_directory_listing_parser_vms.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserVmsTest;

TEST_F(FtpDirectoryListingParserVmsTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "README.TXT;4  2  18-APR-2000 10:40:39.90",
      net::FtpDirectoryListingEntry::FILE, "readme.txt", 1024,
      2000, 4, 18, 10, 40 },
    { ".WELCOME;1    2  13-FEB-2002 23:32:40.47",
      net::FtpDirectoryListingEntry::FILE, ".welcome", 1024,
      2002, 2, 13, 23, 32 },
    { "FILE.;1    2  13-FEB-2002 23:32:40.47",
      net::FtpDirectoryListingEntry::FILE, "file.", 1024,
      2002, 2, 13, 23, 32 },
    { "EXAMPLE.TXT;1  1   4-NOV-2009 06:02 [JOHNDOE] (RWED,RWED,,)",
      net::FtpDirectoryListingEntry::FILE, "example.txt", 512,
      2009, 11, 4, 6, 2 },
    { "ANNOUNCE.TXT;2 1/16 12-MAR-2005 08:44:57 [SYSTEM] (RWED,RWED,RE,RE)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "TEST.DIR;1 1 4-MAR-1999 22:14:34 [UCX$NOBO,ANONYMOUS] (RWE,RWE,RWE,RWE)",
      net::FtpDirectoryListingEntry::DIRECTORY, "test", -1,
      1999, 3, 4, 22, 14 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (,,,)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (R,RW,RWD,RE)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
    { "ANNOUNCE.TXT;2 1 12-MAR-2005 08:44:57 [X] (ED,RED,WD,WED)",
      net::FtpDirectoryListingEntry::FILE, "announce.txt", 512,
      2005, 3, 12, 8, 44 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    net::FtpDirectoryListingParserVms parser;
    ASSERT_TRUE(
        parser.ConsumeLine(ASCIIToUTF16("Directory ANONYMOUS_ROOT:[000000]")));
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserVmsTest, Bad) {
  const char* bad_cases[] = {
    "Directory ROOT|garbage",

    // Missing file version number.
    "Directory ROOT|README.TXT 2 18-APR-2000 10:40:39",

    // Missing extension.
    "Directory ROOT|README;1 2 18-APR-2000 10:40:39",

    // Malformed file size.
    "Directory ROOT|README.TXT;1 garbage 18-APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 -2 18-APR-2000 10:40:39",

    // Malformed date.
    "Directory ROOT|README.TXT;1 2 APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 2 -18-APR-2000 10:40:39",
    "Directory ROOT|README.TXT;1 2 18-APR 10:40:39",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40.25",
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40.25.25",

    // Empty line inside the listing.
    "Directory ROOT|README.TXT;1 2 18-APR-2000 10:40:42"
    "||README.TXT;1 2 18-APR-2000 10:40:42",

    // Data after footer.
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|",
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|garbage",
    "Directory ROOT|README.TXT;4 2 18-APR-2000 10:40:39"
    "||Total of 1 file|Total of 1 file",

    // Malformed security information.
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 (RWED,RWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM]",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 (SYSTEM) (RWED,RWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [SYSTEM] [RWED,RWED,RE,RE]",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,RE,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWEDRWED,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,DEWR,RE,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RWED,Q,RE)",
    "Directory ROOT|X.TXT;2 1 12-MAR-2005 08:44:57 [X] (RWED,RRWWEEDD,RE,RE)",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i, bad_cases[i]));

    std::vector<std::string> lines;
    SplitString(bad_cases[i], '|', &lines);
    net::FtpDirectoryListingParserVms parser;
    bool failed = false;
    for (std::vector<std::string>::const_iterator i = lines.begin();
         i != lines.end(); ++i) {
      if (!parser.ConsumeLine(UTF8ToUTF16(*i))) {
        failed = true;
        break;
      }
    }
    EXPECT_TRUE(failed);
  }
}

}  // namespace
