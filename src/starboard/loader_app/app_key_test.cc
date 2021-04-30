// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/loader_app/app_key_internal.h"

#include <string>

#include "starboard/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace loader_app {

typedef struct URLWithExtractedAndEncoded {
  // The initial URL to be handled.
  const char* url;

  // The application key extracted from |url|.
  const char* extracted;

  // The base64 encoded |extracted|.
  const char* encoded;
} URLWithExtractedAndEncoded;

// The URLs below were taken from url/url_parse_unittest.cc.
const URLWithExtractedAndEncoded kURLWithExtractedAndEncoded[] = {
  {
    "http://user:pass@foo:21/bar;par?b#c",
    "http://foo:21/bar;par",
    "aHR0cDovL2ZvbzoyMS9iYXI7cGFy",
  },
  {
    "http:foo.com",
    "http:foo.com",
    "aHR0cDpmb28uY29t",
  },
  {
    "\t   :foo.com   \n",
    "\t   :foo.com   \n",
    "CSAgIDpmb28uY29tICAgCg==",
  },
  {
    " foo.com  ",
    " foo.com  ",
    "IGZvby5jb20gIA==",
  },
  {
    "a:\t foo.com",
    "a:\t foo.com",
    "YToJIGZvby5jb20=",
  },
  {
    "http://f:21/ b ? d # e ",
    "http://f:21/ b ",
    "aHR0cDovL2Y6MjEvIGIg",
  },
  {
    "http://f:/c",
    "http://f:/c",
    "aHR0cDovL2Y6L2M=",
  },
  {
    "http://f:0/c",
    "http://f:0/c",
    "aHR0cDovL2Y6MC9j",
  },
  {
    "http://f:00000000000000/c",
    "http://f:00000000000000/c",
    "aHR0cDovL2Y6MDAwMDAwMDAwMDAwMDAvYw==",
  },
  {
    "http://f:00000000000000000000080/c",
    "http://f:00000000000000000000080/c",
    "aHR0cDovL2Y6MDAwMDAwMDAwMDAwMDAwMDAwMDAwODAvYw==",
  },
  {
    "http://f:b/c",
    "http://f:b/c",
    "aHR0cDovL2Y6Yi9j",
  },
  {
    "http://f: /c",
    "http://f: /c",
    "aHR0cDovL2Y6IC9j",
  },
  {
    "http://f:\n/c",
    "http://f:\n/c",
    "aHR0cDovL2Y6Ci9j",
  },
  {
    "http://f:fifty-two/c",
    "http://f:fifty-two/c",
    "aHR0cDovL2Y6ZmlmdHktdHdvL2M=",
  },
  {
    "http://f:999999/c",
    "http://f:999999/c",
    "aHR0cDovL2Y6OTk5OTk5L2M=",
  },
  {
    "http://f: 21 / b ? d # e ",
    "http://f: 21 / b ",
    "aHR0cDovL2Y6IDIxIC8gYiA=",
  },
  {
    "",
    "",
    "",
  },
  {
    "  \t",
    "  \t",
    "ICAJ",
  },
  {
    ":foo.com/",
    ":foo.com/",
    "OmZvby5jb20v",
  },
  {
    ":foo.com\\",
    ":foo.com\\",
    "OmZvby5jb21c",
  },
  {
    ":",
    ":",
    "Og==",
  },
  {
    ":a",
    ":a",
    "OmE=",
  },
  {
    ":/",
    ":/",
    "Oi8=",
  },
  {
    ":\\",
    ":\\",
    "Olw=",
  },
  {
    ":#",
    ":",
    "Og==",
  },
  {
    "#",
    "",
    "",
  },
  {
    "#/",
    "",
    "",
  },
  {
    "#\\",
    "",
    "",
  },
  {
    "#;?",
    "",
    "",
  },
  {
    "?",
    "",
    "",
  },
  {
    "/",
    "/",
    "Lw==",
  },
  {
    ":23",
    ":23",
    "OjIz",
  },
  {
    "/:23",
    "/:23",
    "LzoyMw==",
  },
  {
    "//",
    "//",
    "Ly8=",
  },
  {
    "::",
    "::",
    "Ojo=",
  },
  {
    "::23",
    "::23",
    "OjoyMw==",
  },
  {
    "foo://",
    "foo://",
    "Zm9vOi8v",
  },
  {
    "http://a:b@c:29/d",
    "http://c:29/d",
    "aHR0cDovL2M6MjkvZA==",
  },
  {
    "http::@c:29",
    "http::@c:29",
    "aHR0cDo6QGM6Mjk=",
  },
  {
    "http://&a:foo(b]c@d:2/",
    "http://d:2/",
    "aHR0cDovL2Q6Mi8=",
  },
  {
    "http://::@c@d:2",
    "http://c@d:2",
    "aHR0cDovL2NAZDoy",
  },
  {
    "http://foo.com:b@d/",
    "http://d/",
    "aHR0cDovL2Qv",
  },
  {
    "http://foo.com/\\@",
    "http://",
    "aHR0cDovLw==",
  },
  {
    "http:\\\\foo.com\\",
    "http:\\\\foo.com\\",
    "aHR0cDpcXGZvby5jb21c",
  },
  {
    "http:\\\\a\\b:c\\d@foo.com\\",
    "http:\\\\a\\b:c\\d@foo.com\\",
    "aHR0cDpcXGFcYjpjXGRAZm9vLmNvbVw=",
  },
  {
    "foo:/",
    "foo:/",
    "Zm9vOi8=",
  },
  {
    "foo:/bar.com/",
    "foo:/bar.com/",
    "Zm9vOi9iYXIuY29tLw==",
  },
  {
    "foo://///////",
    "foo://///////",
    "Zm9vOi8vLy8vLy8vLw==",
  },
  {
    "foo://///////bar.com/",
    "foo://///////bar.com/",
    "Zm9vOi8vLy8vLy8vL2Jhci5jb20v",
  },
  {
    "foo:////://///",
    "foo:////://///",
    "Zm9vOi8vLy86Ly8vLy8=",
  },
  {
    "c:/foo",
    "c:/foo",
    "YzovZm9v",
  },
  {
    "//foo/bar",
    "//foo/bar",
    "Ly9mb28vYmFy",
  },
  {
    "http://foo/path;a??e#f#g",
    "http://foo/path;a",
    "aHR0cDovL2Zvby9wYXRoO2E=",
  },
  {
    "http://foo/abcd?efgh?ijkl",
    "http://foo/abcd",
    "aHR0cDovL2Zvby9hYmNk",
  },
  {
    "http://foo/abcd#foo?bar",
    "http://foo/abcd",
    "aHR0cDovL2Zvby9hYmNk",
  },
  {
    "[61:24:74]:98",
    "[61:24:74]:98",
    "WzYxOjI0Ojc0XTo5OA==",
  },
  {
    "http://[61:27]:98",
    "http://[61:27]:98",
    "aHR0cDovL1s2MToyN106OTg=",
  },
  {
    "http:[61:27]/:foo",
    "http:[61:27]/:foo",
    "aHR0cDpbNjE6MjddLzpmb28=",
  },
  {
    "http://[1::2]:3:4",
    "http://[1::2]:3:4",
    "aHR0cDovL1sxOjoyXTozOjQ=",
  },
  {
    "http://2001::1",
    "http://2001::1",
    "aHR0cDovLzIwMDE6OjE=",
  },
  {
    "http://[2001::1",
    "http://[2001::1",
    "aHR0cDovL1syMDAxOjox",
  },
  {
    "http://2001::1]",
    "http://2001::1]",
    "aHR0cDovLzIwMDE6OjFd",
  },
  {
    "http://2001::1]:80",
    "http://2001::1]:80",
    "aHR0cDovLzIwMDE6OjFdOjgw",
  },
  {
    "http://[2001::1]",
    "http://[2001::1]",
    "aHR0cDovL1syMDAxOjoxXQ==",
  },
  {
    "http://[2001::1]:80",
    "http://[2001::1]:80",
    "aHR0cDovL1syMDAxOjoxXTo4MA==",
  },
  {
    "http://[[::]]",
    "http://[[::]]",
    "aHR0cDovL1tbOjpdXQ==",
  },
  {
    "file:server",
    "file:server",
    "ZmlsZTpzZXJ2ZXI=",
  },
  {
    "  file: server  \t",
    "  file: server  \t",
    "ICBmaWxlOiBzZXJ2ZXIgIAk=",
  },
  {
    "FiLe:c|",
    "FiLe:c|",
    "RmlMZTpjfA==",
  },
  {
    "FILE:/\\\\/server/file",
    "FILE:/\\\\/server/file",
    "RklMRTovXFwvc2VydmVyL2ZpbGU=",
  },
  {
    "file://server/",
    "file://server/",
    "ZmlsZTovL3NlcnZlci8=",
  },
  {
    "file://localhost/c:/",
    "file://localhost/c:/",
    "ZmlsZTovL2xvY2FsaG9zdC9jOi8=",
  },
  {
    "file://127.0.0.1/c|\\",
    "file://127.0.0.1/c|\\",
    "ZmlsZTovLzEyNy4wLjAuMS9jfFw=",
  },
  {
    "file:/",
    "file:/",
    "ZmlsZTov",
  },
  {
    "file:",
    "file:",
    "ZmlsZTo=",
  },
  {
    "file:c:\\fo\\b",
    "file:c:\\fo\\b",
    "ZmlsZTpjOlxmb1xi",
  },
  {
    "file:/c:\\foo/bar",
    "file:/c:\\foo/bar",
    "ZmlsZTovYzpcZm9vL2Jhcg==",
  },
  {
    "file://c:/f\\b",
    "file://c:/f\\b",
    "ZmlsZTovL2M6L2ZcYg==",
  },
  {
    "file:///C:/foo",
    "file:///C:/foo",
    "ZmlsZTovLy9DOi9mb28=",
  },
  {
    "file://///\\/\\/c:\\f\\b",
    "file://///\\/\\/c:\\f\\b",
    "ZmlsZTovLy8vL1wvXC9jOlxmXGI=",
  },
  {
    "file:server/file",
    "file:server/file",
    "ZmlsZTpzZXJ2ZXIvZmlsZQ==",
  },
  {
    "file:/server/file",
    "file:/server/file",
    "ZmlsZTovc2VydmVyL2ZpbGU=",
  },
  {
    "file://server/file",
    "file://server/file",
    "ZmlsZTovL3NlcnZlci9maWxl",
  },
  {
    "file:///server/file",
    "file:///server/file",
    "ZmlsZTovLy9zZXJ2ZXIvZmlsZQ==",
  },
  {
    "file://\\server/file",
    "file://\\server/file",
    "ZmlsZTovL1xzZXJ2ZXIvZmlsZQ==",
  },
  {
    "file:////server/file",
    "file:////server/file",
    "ZmlsZTovLy8vc2VydmVyL2ZpbGU=",
  },
  {
    "file:///C:/foo.html?#",
    "file:///C:/foo.html",
    "ZmlsZTovLy9DOi9mb28uaHRtbA==",
  },
  {
    "file:///C:/foo.html?query=yes#ref",
    "file:///C:/foo.html",
    "ZmlsZTovLy9DOi9mb28uaHRtbA==",
  },
  {
    "file:",
    "file:",
    "ZmlsZTo=",
  },
  {
    "file:path",
    "file:path",
    "ZmlsZTpwYXRo",
  },
  {
    "file:path/",
    "file:path/",
    "ZmlsZTpwYXRoLw==",
  },
  {
    "file:path/f.txt",
    "file:path/f.txt",
    "ZmlsZTpwYXRoL2YudHh0",
  },
  {
    "file:/",
    "file:/",
    "ZmlsZTov",
  },
  {
    "file:/path",
    "file:/path",
    "ZmlsZTovcGF0aA==",
  },
  {
    "file:/path/",
    "file:/path/",
    "ZmlsZTovcGF0aC8=",
  },
  {
    "file:/path/f.txt",
    "file:/path/f.txt",
    "ZmlsZTovcGF0aC9mLnR4dA==",
  },
  {
    "file://",
    "file://",
    "ZmlsZTovLw==",
  },
  {
    "file://server",
    "file://server",
    "ZmlsZTovL3NlcnZlcg==",
  },
  {
    "file://server/",
    "file://server/",
    "ZmlsZTovL3NlcnZlci8=",
  },
  {
    "file://server/f.txt",
    "file://server/f.txt",
    "ZmlsZTovL3NlcnZlci9mLnR4dA==",
  },
  {
    "file:///",
    "file:///",
    "ZmlsZTovLy8=",
  },
  {
    "file:///path",
    "file:///path",
    "ZmlsZTovLy9wYXRo",
  },
  {
    "file:///path/",
    "file:///path/",
    "ZmlsZTovLy9wYXRoLw==",
  },
  {
    "file:///path/f.txt",
    "file:///path/f.txt",
    "ZmlsZTovLy9wYXRoL2YudHh0",
  },
  {
    "file:////",
    "file:////",
    "ZmlsZTovLy8v",
  },
  {
    "file:////path",
    "file:////path",
    "ZmlsZTovLy8vcGF0aA==",
  },
  {
    "file:////path/",
    "file:////path/",
    "ZmlsZTovLy8vcGF0aC8=",
  },
  {
    "file:////path/f.txt",
    "file:////path/f.txt",
    "ZmlsZTovLy8vcGF0aC9mLnR4dA==",
  },
  {
    "path/f.txt",
    "path/f.txt",
    "cGF0aC9mLnR4dA==",
  },
  {
    "path:80/f.txt",
    "path:80/f.txt",
    "cGF0aDo4MC9mLnR4dA==",
  },
  {
    "path/f.txt:80",
    "path/f.txt:80",
    "cGF0aC9mLnR4dDo4MA==",
  },
  {
    "/path/f.txt",
    "/path/f.txt",
    "L3BhdGgvZi50eHQ=",
  },
  {
    "/path:80/f.txt",
    "/path:80/f.txt",
    "L3BhdGg6ODAvZi50eHQ=",
  },
  {
    "/path/f.txt:80",
    "/path/f.txt:80",
    "L3BhdGgvZi50eHQ6ODA=",
  },
  {
    "//server/f.txt",
    "//server/f.txt",
    "Ly9zZXJ2ZXIvZi50eHQ=",
  },
  {
    "//server:80/f.txt",
    "//server:80/f.txt",
    "Ly9zZXJ2ZXI6ODAvZi50eHQ=",
  },
  {
    "//server/f.txt:80",
    "//server/f.txt:80",
    "Ly9zZXJ2ZXIvZi50eHQ6ODA=",
  },
  {
    "///path/f.txt",
    "///path/f.txt",
    "Ly8vcGF0aC9mLnR4dA==",
  },
  {
    "///path:80/f.txt",
    "///path:80/f.txt",
    "Ly8vcGF0aDo4MC9mLnR4dA==",
  },
  {
    "///path/f.txt:80",
    "///path/f.txt:80",
    "Ly8vcGF0aC9mLnR4dDo4MA==",
  },
  {
    "////path/f.txt",
    "////path/f.txt",
    "Ly8vL3BhdGgvZi50eHQ=",
  },
  {
    "////path:80/f.txt",
    "////path:80/f.txt",
    "Ly8vL3BhdGg6ODAvZi50eHQ=",
  },
  {
    "////path/f.txt:80",
    "////path/f.txt:80",
    "Ly8vL3BhdGgvZi50eHQ6ODA=",
  },
  {
    "file:///foo.html?#",
    "file:///foo.html",
    "ZmlsZTovLy9mb28uaHRtbA==",
  },
  {
    "file:///foo.html?q=y#ref",
    "file:///foo.html",
    "ZmlsZTovLy9mb28uaHRtbA==",
  },
};

TEST(AppKeyTest, SunnyDayExtractAppKey) {
  for (const URLWithExtractedAndEncoded& expected :
       kURLWithExtractedAndEncoded) {
    std::string actual = ExtractAppKey(expected.url);
    EXPECT_EQ(std::string(expected.extracted), actual);

    actual = EncodeAppKey(actual);
    EXPECT_EQ(std::string(expected.encoded), actual);
  }
}

TEST(AppKeyTest, SunnyDayExtractAppKeySanitizesResult) {
  // This magic is 11111011 11110000 in binary and thus begins with '+' and '/'
  // when base64 encoded.
  uint8_t magic[3] = {0xFB, 0xF0, 0x00};

  const std::string actual =
      ExtractAppKey(reinterpret_cast<const char*>(magic));

  // Check that the '+' and '/' characters were replaced with '-' and '_'.
  EXPECT_EQ("\xFB\xF0", actual);
  EXPECT_EQ("-_A=", EncodeAppKey(actual));
}

}  // namespace loader_app
}  // namespace starboard
