// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(FtpUtilTest, UnixFilePathToVMS) {
  const struct {
    const char* input;
    const char* expected_output;
  } kTestCases[] = {
    { "",           ""            },
    { "/",          "[]"          },
    { "/a",         "a"           },
    { "/a/b",       "a:[000000]b" },
    { "/a/b/c",     "a:[b]c"      },
    { "/a/b/c/d",   "a:[b.c]d"    },
    { "/a/b/c/d/e", "a:[b.c.d]e"  },
    { "a",          "a"           },
    { "a/b",        "[.a]b"       },
    { "a/b/c",      "[.a.b]c"     },
    { "a/b/c/d",    "[.a.b.c]d"   },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    EXPECT_EQ(kTestCases[i].expected_output,
              net::FtpUtil::UnixFilePathToVMS(kTestCases[i].input))
        << kTestCases[i].input;
  }
}

TEST(FtpUtilTest, UnixDirectoryPathToVMS) {
  const struct {
    const char* input;
    const char* expected_output;
  } kTestCases[] = {
    { "",            ""            },
    { "/",           ""            },
    { "/a",          "a:[000000]"  },
    { "/a/",         "a:[000000]"  },
    { "/a/b",        "a:[b]"       },
    { "/a/b/",       "a:[b]"       },
    { "/a/b/c",      "a:[b.c]"     },
    { "/a/b/c/",     "a:[b.c]"     },
    { "/a/b/c/d",    "a:[b.c.d]"   },
    { "/a/b/c/d/",   "a:[b.c.d]"   },
    { "/a/b/c/d/e",  "a:[b.c.d.e]" },
    { "/a/b/c/d/e/", "a:[b.c.d.e]" },
    { "a",           "[.a]"        },
    { "a/",          "[.a]"        },
    { "a/b",         "[.a.b]"      },
    { "a/b/",        "[.a.b]"      },
    { "a/b/c",       "[.a.b.c]"    },
    { "a/b/c/",      "[.a.b.c]"    },
    { "a/b/c/d",     "[.a.b.c.d]"  },
    { "a/b/c/d/",    "[.a.b.c.d]"  },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    EXPECT_EQ(kTestCases[i].expected_output,
              net::FtpUtil::UnixDirectoryPathToVMS(kTestCases[i].input))
        << kTestCases[i].input;
  }
}

TEST(FtpUtilTest, VMSPathToUnix) {
  const struct {
    const char* input;
    const char* expected_output;
  } kTestCases[] = {
    { "",            "."          },
    { "[]",          "/"          },
    { "a",           "/a"         },
    { "a:[000000]",  "/a"         },
    { "a:[000000]b", "/a/b"       },
    { "a:[b]",       "/a/b"       },
    { "a:[b]c",      "/a/b/c"     },
    { "a:[b.c]",     "/a/b/c"     },
    { "a:[b.c]d",    "/a/b/c/d"   },
    { "a:[b.c.d]",   "/a/b/c/d"   },
    { "a:[b.c.d]e",  "/a/b/c/d/e" },
    { "a:[b.c.d.e]", "/a/b/c/d/e" },
    { "[.a]",        "a"          },
    { "[.a]b",       "a/b"        },
    { "[.a.b]",      "a/b"        },
    { "[.a.b]c",     "a/b/c"      },
    { "[.a.b.c]",    "a/b/c"      },
    { "[.a.b.c]d",   "a/b/c/d"    },
    { "[.a.b.c.d]",  "a/b/c/d"    },
    { "[.",          ""           },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    EXPECT_EQ(kTestCases[i].expected_output,
              net::FtpUtil::VMSPathToUnix(kTestCases[i].input))
        << kTestCases[i].input;
  }
}

TEST(FtpUtilTest, LsDateListingToTime) {
  base::Time mock_current_time;
  ASSERT_TRUE(base::Time::FromString(L"Tue, 15 Nov 1994 12:45:26 GMT",
                                     &mock_current_time));

  const struct {
    // Input.
    const char* month;
    const char* day;
    const char* rest;

    // Expected output.
    int expected_year;
    int expected_month;
    int expected_day_of_month;
    int expected_hour;
    int expected_minute;
  } kTestCases[] = {
    { "Nov", "01", "2007", 2007, 11, 1, 0, 0 },
    { "Jul", "25", "13:37", 1994, 7, 25, 13, 37 },

    // Test date listings in German, we should support them for FTP servers
    // giving localized listings.
    { "M\xc3\xa4r", "13", "2009", 2009, 3, 13, 0, 0 },
    { "Mai", "1", "10:10", 1994, 5, 1, 10, 10 },
    { "Okt", "14", "21:18", 1994, 10, 14, 21, 18 },
    { "Dez", "25", "2008", 2008, 12, 25, 0, 0 },

    // Test current year detection.
    { "Nov", "01", "12:00", 1994, 11, 1, 12, 0 },
    { "Nov", "15", "12:00", 1994, 11, 15, 12, 0 },
    { "Nov", "16", "12:00", 1993, 11, 16, 12, 0 },
    { "Jan", "01", "08:30", 1994, 1, 1, 8, 30 },
    { "Sep", "02", "09:00", 1994, 9, 2, 9, 0 },
    { "Dec", "06", "21:00", 1993, 12, 6, 21, 0 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s %s %s", i,
                                    kTestCases[i].month, kTestCases[i].day,
                                    kTestCases[i].rest));

    base::Time time;
    ASSERT_TRUE(net::FtpUtil::LsDateListingToTime(
        UTF8ToUTF16(kTestCases[i].month), UTF8ToUTF16(kTestCases[i].day),
        UTF8ToUTF16(kTestCases[i].rest), mock_current_time, &time));

    base::Time::Exploded time_exploded;
    time.LocalExplode(&time_exploded);
    EXPECT_EQ(kTestCases[i].expected_year, time_exploded.year);
    EXPECT_EQ(kTestCases[i].expected_month, time_exploded.month);
    EXPECT_EQ(kTestCases[i].expected_day_of_month, time_exploded.day_of_month);
    EXPECT_EQ(kTestCases[i].expected_hour, time_exploded.hour);
    EXPECT_EQ(kTestCases[i].expected_minute, time_exploded.minute);
    EXPECT_EQ(0, time_exploded.second);
    EXPECT_EQ(0, time_exploded.millisecond);
  }
}

TEST(FtpUtilTest, GetStringPartAfterColumns) {
  const struct {
    const char* text;
    int column;
    const char* expected_result;
  } kTestCases[] = {
    { "", 0, "" },
    { "", 1, "" },
    { "foo abc", 0, "foo abc" },
    { "foo abc", 1, "abc" },
    { "  foo   abc", 0, "foo   abc" },
    { "  foo   abc", 1, "abc" },
    { "  foo   abc", 2, "" },
    { "  foo   abc ", 0, "foo   abc" },
    { "  foo   abc ", 1, "abc" },
    { "  foo   abc ", 2, "" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s %d", i,
                                    kTestCases[i].text, kTestCases[i].column));

    EXPECT_EQ(ASCIIToUTF16(kTestCases[i].expected_result),
              net::FtpUtil::GetStringPartAfterColumns(
                  ASCIIToUTF16(kTestCases[i].text), kTestCases[i].column));
  }
}

}  // namespace
