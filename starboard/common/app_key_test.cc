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

#include <string>

#include "starboard/common/app_key_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

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
        .url = "http://user:pass@foo:21/bar;par?b#c",
        .extracted = "http://foo:21/bar;par",
        .encoded = "aHR0cDovL2ZvbzoyMS9iYXI7cGFy",
    },
    {
        .url = "http:foo.com",
        .extracted = "http:foo.com",
        .encoded = "aHR0cDpmb28uY29t",
    },
    {
        .url = "\t   :foo.com   \n",
        .extracted = "\t   :foo.com   \n",
        .encoded = "CSAgIDpmb28uY29tICAgCg==",
    },
    {
        .url = " foo.com  ",
        .extracted = " foo.com  ",
        .encoded = "IGZvby5jb20gIA==",
    },
    {
        .url = "a:\t foo.com",
        .extracted = "a:\t foo.com",
        .encoded = "YToJIGZvby5jb20=",
    },
    {
        .url = "http://f:21/ b ? d # e ",
        .extracted = "http://f:21/ b ",
        .encoded = "aHR0cDovL2Y6MjEvIGIg",
    },
    {
        .url = "http://f:/c",
        .extracted = "http://f:/c",
        .encoded = "aHR0cDovL2Y6L2M=",
    },
    {
        .url = "http://f:0/c",
        .extracted = "http://f:0/c",
        .encoded = "aHR0cDovL2Y6MC9j",
    },
    {
        .url = "http://f:00000000000000/c",
        .extracted = "http://f:00000000000000/c",
        .encoded = "aHR0cDovL2Y6MDAwMDAwMDAwMDAwMDAvYw==",
    },
    {
        .url = "http://f:00000000000000000000080/c",
        .extracted = "http://f:00000000000000000000080/c",
        .encoded = "aHR0cDovL2Y6MDAwMDAwMDAwMDAwMDAwMDAwMDAwODAvYw==",
    },
    {
        .url = "http://f:b/c",
        .extracted = "http://f:b/c",
        .encoded = "aHR0cDovL2Y6Yi9j",
    },
    {
        .url = "http://f: /c",
        .extracted = "http://f: /c",
        .encoded = "aHR0cDovL2Y6IC9j",
    },
    {
        .url = "http://f:\n/c",
        .extracted = "http://f:\n/c",
        .encoded = "aHR0cDovL2Y6Ci9j",
    },
    {
        .url = "http://f:fifty-two/c",
        .extracted = "http://f:fifty-two/c",
        .encoded = "aHR0cDovL2Y6ZmlmdHktdHdvL2M=",
    },
    {
        .url = "http://f:999999/c",
        .extracted = "http://f:999999/c",
        .encoded = "aHR0cDovL2Y6OTk5OTk5L2M=",
    },
    {
        .url = "http://f: 21 / b ? d # e ",
        .extracted = "http://f: 21 / b ",
        .encoded = "aHR0cDovL2Y6IDIxIC8gYiA=",
    },
    {
        .url = "",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "  \t",
        .extracted = "  \t",
        .encoded = "ICAJ",
    },
    {
        .url = ":foo.com/",
        .extracted = ":foo.com/",
        .encoded = "OmZvby5jb20v",
    },
    {
        .url = ":foo.com\\",
        .extracted = ":foo.com\\",
        .encoded = "OmZvby5jb21c",
    },
    {
        .url = ":",
        .extracted = ":",
        .encoded = "Og==",
    },
    {
        .url = ":a",
        .extracted = ":a",
        .encoded = "OmE=",
    },
    {
        .url = ":/",
        .extracted = ":/",
        .encoded = "Oi8=",
    },
    {
        .url = ":\\",
        .extracted = ":\\",
        .encoded = "Olw=",
    },
    {
        .url = ":#",
        .extracted = ":",
        .encoded = "Og==",
    },
    {
        .url = "#",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "#/",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "#\\",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "#;?",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "?",
        .extracted = "",
        .encoded = "",
    },
    {
        .url = "/",
        .extracted = "/",
        .encoded = "Lw==",
    },
    {
        .url = ":23",
        .extracted = ":23",
        .encoded = "OjIz",
    },
    {
        .url = "/:23",
        .extracted = "/:23",
        .encoded = "LzoyMw==",
    },
    {
        .url = "//",
        .extracted = "//",
        .encoded = "Ly8=",
    },
    {
        .url = "::",
        .extracted = "::",
        .encoded = "Ojo=",
    },
    {
        .url = "::23",
        .extracted = "::23",
        .encoded = "OjoyMw==",
    },
    {
        .url = "foo://",
        .extracted = "foo://",
        .encoded = "Zm9vOi8v",
    },
    {
        .url = "http://a:b@c:29/d",
        .extracted = "http://c:29/d",
        .encoded = "aHR0cDovL2M6MjkvZA==",
    },
    {
        .url = "http::@c:29",
        .extracted = "http::@c:29",
        .encoded = "aHR0cDo6QGM6Mjk=",
    },
    {
        .url = "http://&a:foo(b]c@d:2/",
        .extracted = "http://d:2/",
        .encoded = "aHR0cDovL2Q6Mi8=",
    },
    {
        .url = "http://::@c@d:2",
        .extracted = "http://c@d:2",
        .encoded = "aHR0cDovL2NAZDoy",
    },
    {
        .url = "http://foo.com:b@d/",
        .extracted = "http://d/",
        .encoded = "aHR0cDovL2Qv",
    },
    {
        .url = "http://foo.com/\\@",
        .extracted = "http://",
        .encoded = "aHR0cDovLw==",
    },
    {
        .url = "http:\\\\foo.com\\",
        .extracted = "http:\\\\foo.com\\",
        .encoded = "aHR0cDpcXGZvby5jb21c",
    },
    {
        .url = "http:\\\\a\\b:c\\d@foo.com\\",
        .extracted = "http:\\\\a\\b:c\\d@foo.com\\",
        .encoded = "aHR0cDpcXGFcYjpjXGRAZm9vLmNvbVw=",
    },
    {
        .url = "foo:/",
        .extracted = "foo:/",
        .encoded = "Zm9vOi8=",
    },
    {
        .url = "foo:/bar.com/",
        .extracted = "foo:/bar.com/",
        .encoded = "Zm9vOi9iYXIuY29tLw==",
    },
    {
        .url = "foo://///////",
        .extracted = "foo://///////",
        .encoded = "Zm9vOi8vLy8vLy8vLw==",
    },
    {
        .url = "foo://///////bar.com/",
        .extracted = "foo://///////bar.com/",
        .encoded = "Zm9vOi8vLy8vLy8vL2Jhci5jb20v",
    },
    {
        .url = "foo:////://///",
        .extracted = "foo:////://///",
        .encoded = "Zm9vOi8vLy86Ly8vLy8=",
    },
    {
        .url = "c:/foo",
        .extracted = "c:/foo",
        .encoded = "YzovZm9v",
    },
    {
        .url = "//foo/bar",
        .extracted = "//foo/bar",
        .encoded = "Ly9mb28vYmFy",
    },
    {
        .url = "http://foo/path;a??e#f#g",
        .extracted = "http://foo/path;a",
        .encoded = "aHR0cDovL2Zvby9wYXRoO2E=",
    },
    {
        .url = "http://foo/abcd?efgh?ijkl",
        .extracted = "http://foo/abcd",
        .encoded = "aHR0cDovL2Zvby9hYmNk",
    },
    {
        .url = "http://foo/abcd#foo?bar",
        .extracted = "http://foo/abcd",
        .encoded = "aHR0cDovL2Zvby9hYmNk",
    },
    {
        .url = "[61:24:74]:98",
        .extracted = "[61:24:74]:98",
        .encoded = "WzYxOjI0Ojc0XTo5OA==",
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

}  // namespace starboard
