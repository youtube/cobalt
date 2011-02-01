// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"

namespace {

typedef net::FtpDirectoryListingParserTest FtpDirectoryListingParserLsTest;

TEST_F(FtpDirectoryListingParserLsTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "-rw-r--r--    1 ftp      ftp           528 Nov 01  2007 README",
      net::FtpDirectoryListingEntry::FILE, "README", 528,
      2007, 11, 1, 0, 0 },
    { "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 directory",
      net::FtpDirectoryListingEntry::DIRECTORY, "directory", -1,
      1994, 5, 15, 18, 11 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      net::FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "lrwxrwxrwx 1 0  0 3 Oct 12 13:37 mirror -> pub",
      net::FtpDirectoryListingEntry::SYMLINK, "mirror", -1,
      1994, 10, 12, 13, 37 },
    { "drwxrwsr-x    4 501      501          4096 Feb 20  2007 pub",
      net::FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2007, 2, 20, 0, 0 },
    { "drwxr-xr-x   4 (?)      (?)          4096 Apr  8  2007 jigdo",
      net::FtpDirectoryListingEntry::DIRECTORY, "jigdo", -1,
      2007, 4, 8, 0, 0 },
    { "drwx-wx-wt  2 root  wheel  512 Jul  1 02:15 incoming",
      net::FtpDirectoryListingEntry::DIRECTORY, "incoming", -1,
      1994, 7, 1, 2, 15 },
    { "-rw-r--r-- 1 2 3 3447432 May 18  2009 Foo - Manual.pdf",
      net::FtpDirectoryListingEntry::FILE, "Foo - Manual.pdf", 3447432,
      2009, 5, 18, 0, 0 },
    { "d-wx-wx-wt+  4 ftp      989          512 Dec  8 15:54 incoming",
      net::FtpDirectoryListingEntry::DIRECTORY, "incoming", -1,
      1993, 12, 8, 15, 54 },
    { "drwxrwxrwx   1 owner    group               0 Sep 13  0:30 audio",
      net::FtpDirectoryListingEntry::DIRECTORY, "audio", -1,
      1994, 9, 13, 0, 30 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub",
      net::FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },

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

    // Tests for "ls -l" style listings sent by an OS/2 server (FtpServer):
    { "-r--r--r--  1 ftp      -A---       13274 Mar  1  2006 UpTime.exe",
      net::FtpDirectoryListingEntry::FILE, "UpTime.exe", 13274,
      2006, 3, 1, 0, 0 },
    { "dr--r--r--  1 ftp      -----           0 Nov 17 17:08 kernels",
      net::FtpDirectoryListingEntry::DIRECTORY, "kernels", -1,
      1993, 11, 17, 17, 8 },

    // Tests for "ls -l" style listing sent by Xplain FTP Server.
    { "drwxr-xr-x               folder        0 Jul 17  2006 online",
      net::FtpDirectoryListingEntry::DIRECTORY, "online", -1,
      2006, 7, 17, 0, 0 },

    // Tests for "ls -l" style listing with owning group name
    // not separated from file size (http://crbug.com/58963).
    { "-rw-r--r-- 1 ftpadmin ftpadmin125435904 Apr  9  2008 .pureftpd-upload",
      net::FtpDirectoryListingEntry::FILE, ".pureftpd-upload", 0,
      2008, 4, 9, 0, 0 },

    // Tests for "ls -l" style listing with number of links
    // not separated from permission listing (http://crbug.com/70394).
    { "drwxr-xr-x1732 266      111        90112 Jun 21  2001 .rda_2",
      net::FtpDirectoryListingEntry::DIRECTORY, ".rda_2", -1,
      2001, 6, 21, 0, 0 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    net::FtpDirectoryListingParserLs parser(GetMockCurrentTime());
    RunSingleLineTestCase(&parser, good_cases[i]);
  }
}

TEST_F(FtpDirectoryListingParserLsTest, Ignored) {
  const char* ignored_cases[] = {
    "drwxr-xr-x 2 0 0 4096 Mar 18  2007  ",  // http://crbug.com/60065

    "ftpd: .: Permission denied",
    "ftpd-BSD: .: Permission denied",

    // Tests important for security: verify that after we detect the column
    // offset we don't try to access invalid memory on malformed input.
    "drwxr-xr-x 3 ftp ftp 4096 May 15 18:11",
    "drwxr-xr-x 3 ftp     4096 May 15 18:11",
    "drwxr-xr-x   folder     0 May 15 18:11",
  };
  for (size_t i = 0; i < arraysize(ignored_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    ignored_cases[i]));

    net::FtpDirectoryListingParserLs parser(GetMockCurrentTime());
    EXPECT_TRUE(parser.ConsumeLine(UTF8ToUTF16(ignored_cases[i])));
    EXPECT_FALSE(parser.EntryAvailable());
    EXPECT_TRUE(parser.OnEndOfInput());
    EXPECT_FALSE(parser.EntryAvailable());
  }
}

TEST_F(FtpDirectoryListingParserLsTest, Bad) {
  const char* bad_cases[] = {
    " foo",
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
    "drwxrwxrwx   1 owner    group               0 Sep 13  0:3 audio",

    "-qqqqqqqqq+  2 sys          512 Mar 27  2009 pub",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    net::FtpDirectoryListingParserLs parser(GetMockCurrentTime());
    EXPECT_FALSE(parser.ConsumeLine(UTF8ToUTF16(bad_cases[i]))) << bad_cases[i];
  }
}

}  // namespace
