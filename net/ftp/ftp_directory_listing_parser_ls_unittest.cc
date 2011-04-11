// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_directory_listing_parser_unittest.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "net/ftp/ftp_directory_listing_parser_ls.h"

namespace net {

namespace {

typedef FtpDirectoryListingParserTest FtpDirectoryListingParserLsTest;

TEST_F(FtpDirectoryListingParserLsTest, Good) {
  const struct SingleLineTestData good_cases[] = {
    { "-rw-r--r--    1 ftp      ftp           528 Nov 01  2007 README",
      FtpDirectoryListingEntry::FILE, "README", 528,
      2007, 11, 1, 0, 0 },
    { "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 directory",
      FtpDirectoryListingEntry::DIRECTORY, "directory", -1,
      1994, 5, 15, 18, 11 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "lrwxrwxrwx 1 0  0 3 Oct 12 13:37 mirror -> pub",
      FtpDirectoryListingEntry::SYMLINK, "mirror", -1,
      1994, 10, 12, 13, 37 },
    { "drwxrwsr-x    4 501      501          4096 Feb 20  2007 pub",
      FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2007, 2, 20, 0, 0 },
    { "drwxr-xr-x   4 (?)      (?)          4096 Apr  8  2007 jigdo",
      FtpDirectoryListingEntry::DIRECTORY, "jigdo", -1,
      2007, 4, 8, 0, 0 },
    { "drwx-wx-wt  2 root  wheel  512 Jul  1 02:15 incoming",
      FtpDirectoryListingEntry::DIRECTORY, "incoming", -1,
      1994, 7, 1, 2, 15 },
    { "-rw-r--r-- 1 2 3 3447432 May 18  2009 Foo - Manual.pdf",
      FtpDirectoryListingEntry::FILE, "Foo - Manual.pdf", 3447432,
      2009, 5, 18, 0, 0 },
    { "d-wx-wx-wt+  4 ftp      989          512 Dec  8 15:54 incoming",
      FtpDirectoryListingEntry::DIRECTORY, "incoming", -1,
      1993, 12, 8, 15, 54 },
    { "drwxrwxrwx   1 owner    group               1024 Sep 13  0:30 audio",
      FtpDirectoryListingEntry::DIRECTORY, "audio", -1,
      1994, 9, 13, 0, 30 },
    { "lrwxrwxrwx 1 0  0 26 Sep 18 2008 pub",
      FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },

    // Tests for the wu-ftpd variant:
    { "drwxr-xr-x   2 sys          512 Mar 27  2009 pub",
      FtpDirectoryListingEntry::DIRECTORY, "pub", -1,
      2009, 3, 27, 0, 0 },
    { "lrwxrwxrwx 0  0 26 Sep 18 2008 pub -> vol/1/.CLUSTER/var_ftp/pub",
      FtpDirectoryListingEntry::SYMLINK, "pub", -1,
      2008, 9, 18, 0, 0 },
    { "drwxr-xr-x   (?)      (?)          4096 Apr  8  2007 jigdo",
      FtpDirectoryListingEntry::DIRECTORY, "jigdo", -1,
      2007, 4, 8, 0, 0 },
    { "-rw-r--r-- 2 3 3447432 May 18  2009 Foo - Manual.pdf",
      FtpDirectoryListingEntry::FILE, "Foo - Manual.pdf", 3447432,
      2009, 5, 18, 0, 0 },

    // Tests for "ls -l" style listings sent by an OS/2 server (FtpServer):
    { "-r--r--r--  1 ftp      -A---       13274 Mar  1  2006 UpTime.exe",
      FtpDirectoryListingEntry::FILE, "UpTime.exe", 13274,
      2006, 3, 1, 0, 0 },
    { "dr--r--r--  1 ftp      -----           0 Nov 17 17:08 kernels",
      FtpDirectoryListingEntry::DIRECTORY, "kernels", -1,
      1993, 11, 17, 17, 8 },

    // Tests for "ls -l" style listing sent by Xplain FTP Server.
    { "drwxr-xr-x               folder        0 Jul 17  2006 online",
      FtpDirectoryListingEntry::DIRECTORY, "online", -1,
      2006, 7, 17, 0, 0 },

    // Tests for "ls -l" style listing with owning group name
    // not separated from file size (http://crbug.com/58963).
    { "-rw-r--r-- 1 ftpadmin ftpadmin125435904 Apr  9  2008 .pureftpd-upload",
      FtpDirectoryListingEntry::FILE, ".pureftpd-upload", 0,
      2008, 4, 9, 0, 0 },

    // Tests for "ls -l" style listing with number of links
    // not separated from permission listing (http://crbug.com/70394).
    { "drwxr-xr-x1732 266      111        90112 Jun 21  2001 .rda_2",
      FtpDirectoryListingEntry::DIRECTORY, ".rda_2", -1,
      2001, 6, 21, 0, 0 },

    // Tests for "ls -l" style listing with group name containing spaces.
    { "drwxrwxr-x   3 %%%%     Domain Users     4096 Dec  9  2009 %%%%%",
      net::FtpDirectoryListingEntry::DIRECTORY, "%%%%%", -1,
      2009, 12, 9, 0, 0 },

    // Tests for "ls -l" style listing in Russian locale (note the swapped
    // parts order: the day of month is the first, before month).
    { "-rwxrwxr-x 1 ftp ftp 123 23 \xd0\xbc\xd0\xb0\xd0\xb9 2011 test",
      net::FtpDirectoryListingEntry::FILE, "test", 123,
      2011, 5, 23, 0, 0 },
    { "drwxrwxr-x 1 ftp ftp 4096 19 \xd0\xbe\xd0\xba\xd1\x82 2011 dir",
      net::FtpDirectoryListingEntry::DIRECTORY, "dir", -1,
      2011, 10, 19, 0, 0 },
  };
  for (size_t i = 0; i < arraysize(good_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    good_cases[i].input));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingLs(
        GetSingleLineTestCase(good_cases[i].input),
        GetMockCurrentTime(),
        &entries));
    VerifySingleLineTestCase(good_cases[i], entries);
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

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_TRUE(ParseFtpDirectoryListingLs(
                    GetSingleLineTestCase(ignored_cases[i]),
                    GetMockCurrentTime(),
                    &entries));
    EXPECT_EQ(0U, entries.size());
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
    "drwxrwxrwx   1 owner    group               1024 Sep 13  0:3 audio",

    "-qqqqqqqqq+  2 sys          512 Mar 27  2009 pub",
  };
  for (size_t i = 0; i < arraysize(bad_cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    bad_cases[i]));

    std::vector<FtpDirectoryListingEntry> entries;
    EXPECT_FALSE(ParseFtpDirectoryListingLs(GetSingleLineTestCase(bad_cases[i]),
                                            GetMockCurrentTime(),
                                            &entries));
  }
}

}  // namespace

}  // namespace net
