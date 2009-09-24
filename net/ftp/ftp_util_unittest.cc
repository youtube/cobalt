// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_util.h"

#include "base/basictypes.h"
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
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); i++) {
    EXPECT_EQ(kTestCases[i].expected_output,
              net::FtpUtil::VMSPathToUnix(kTestCases[i].input))
        << kTestCases[i].input;
  }
}

}  // namespace
