// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserLsTest;

TEST_F(FtpDirectoryListingParserLsTest, Good) {
  base::Time::Exploded now_exploded;
  base::Time::Now().LocalExplode(&now_exploded);

  const struct SingleLineTestData good_cases[] = {
    { "-rw-r--r--    1 ftp      ftp           528 Nov 01  2007 README",
      net::FtpDirectoryListingEntry::FILE, "README", 528,
      2007, 11, 1, 0, 0 },
    { "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 directory",
      net::FtpDirectoryListingEntry::DIRECTORY, "directory", -1,
      now_exploded.year, 5, 15, 18, 11 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      net::FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "lrwxrwxrwx 1 0  0 3 Oct 12 13:37 mirror -> pub",
      net::FtpDirectoryListingEntry::SYMLINK, "mirror", -1,
      now_exploded.year, 10, 12, 13, 37 },
    { "drwxrwsr-x    4 501      501          4096 Feb 20  2007 pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2007, 2, 20, 0, 0 },
    { "drwxr-xr-x   4 (?)      (?)          4096 Apr  8  2007 jigdo",
      net::FtpDirectoryListingEntry::DIRECTORY, "jigdo", -1,
      2007, 4, 8, 0, 0 },
    { "drwx-wx-wt  2 root  wheel  512 Jul  1 02:15 incoming",
      net::FtpDirectoryListingEntry::DIRECTORY, "incoming", -1,
      now_exploded.year, 7, 1, 2, 15 },
    { "-rw-r--r-- 1 2 3 3447432 May 18  2009 Foo - Manual.pdf",
      net::FtpDirectoryListingEntry::FILE, "Foo - Manual.pdf", 3447432,
      2009, 5, 18, 0, 0 },

    // Tests for the wu-ftpd variant:
    { "drwxr-xr-x   2 sys          512 Mar 27  2009 pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2009, 3, 27, 0, 0 },
    { "lrwxrwxrwx 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      net::FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "drwxr-xr-x   (?)      (?)          4096 Apr  8  2007 jigdo",
      net::FtpDirectoryListingEntry::DIRECTORY, "jigdo", -1,
      2007, 4, 8, 0, 0 },
    { "-rw-r--r-- 2 3 3447432 May 18  2009 Foo - Manual.pdf",
      net::FtpDirectoryListingEntry::FILE, "Foo - Manual.pdf", 3447432,
      2009, 5, 18, 0, 0 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "]: %s", i, good_cases[i].input));

    net::FtpDirectoryListingParserLs parser;
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserLsTest, Bad) {
  const char* bad_cases[] = {
    "garbage",
    "-rw-r--r-- ftp ftp",
    "-rw-r--rgb ftp ftp 528 Nov 01 2007 README",
    "-rw-rgbr-- ftp ftp 528 Nov 01 2007 README",
    "qrwwr--r-- ftp ftp 528 Nov 01 2007 README",
    "-rw-r--r-- ftp ftp -528 Nov 01 2007 README",
    "-rw-r--r-- ftp ftp 528 Foo 01 2007 README",
    "-rw-r--r-- 1 ftp ftp",
    "-rw-r--rgb 1 ftp ftp 528 Nov 01 2007 README",
    "-rw-rgbr-- 1 ftp ftp 528 Nov 01 2007 README",
    "qrwwr--r-- 1 ftp ftp 528 Nov 01 2007 README",
    "-rw-r--r-- 1 ftp ftp -528 Nov 01 2007 README",
    "-rw-r--r-- 1 ftp ftp 528 Foo 01 2007 README",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserLs parser;
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
