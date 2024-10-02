// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/database/database_identifier.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::DatabaseIdentifier;

namespace content {
namespace {

TEST(DatabaseIdentifierTest, CreateIdentifierFromOrigin) {
  struct OriginTestCase {
    std::string origin;
    std::string expectedIdentifier;
  } cases[] = {
    {"http://google.com", "http_google.com_0"},
    {"http://google.com:80", "http_google.com_0"},
    {"https://www.google.com", "https_www.google.com_0"},
    {"https://www.google.com:443", "https_www.google.com_0"},
    {"http://foo_bar_baz.org", "http_foo_bar_baz.org_0"},
    {"http://nondefaultport.net:8001", "http_nondefaultport.net_8001"},
    {"http://invalidportnumber.org:70000", "__0"},
    {"http://invalidportnumber.org:-6", "__0"},
    {"http://%E2%98%83.unicode.com", "http_xn--n3h.unicode.com_0"},
    {"http://\xe2\x98\x83.unicode.com", "http_xn--n3h.unicode.com_0"},
    {"http://\xf0\x9f\x92\xa9.unicode.com", "http_xn--ls8h.unicode.com_0"},
    {"file:///", "file__0"},
    {"data:", "__0"},
    {"about:blank", "__0"},
    {"non-standard://foobar.com", "__0"},
    {"http://[::1]:8080", "http_[__1]_8080"},
    {"http://[3ffe:2a00:100:7031::1]", "http_[3ffe_2a00_100_7031__1]_0"},
    {"http://[::ffff:8190:3426]", "http_[__ffff_8190_3426]_0"},
  };

  for (const auto& test_case : cases) {
    GURL origin_url(test_case.origin);
    url::Origin origin = url::Origin::Create(origin_url);

    DatabaseIdentifier identifier_from_url =
        DatabaseIdentifier::CreateFromOrigin(origin_url);
    EXPECT_EQ(test_case.expectedIdentifier, identifier_from_url.ToString())
        << "test case " << test_case.origin;

    DatabaseIdentifier identifier_from_origin =
        DatabaseIdentifier::CreateFromOrigin(origin_url);
    EXPECT_EQ(test_case.expectedIdentifier, identifier_from_origin.ToString())
        << "test case " << test_case.origin;
  }
}

// This tests the encoding of a hostname including every character in the range
// [\x1f, \x80].
TEST(DatabaseIdentifierTest, CreateIdentifierAllHostChars) {
  struct Case {
    std::string hostname;
    std::string expected;
    bool shouldRoundTrip;
  } cases[] = {
    {"x\x1Fx", "__0", false},
    {"x\x20x", "http_x%20x_0", false},
    {"x\x21x", "http_x%21x_0", false},
    {"x\x22x", "http_x%22x_0", false},
    {"x\x23x", "http_x_0", false},  // 'x#x', the # and following are ignored.
    {"x\x24x", "http_x%24x_0", false},
    {"x\x25x", "__0", false},
    {"x\x26x", "http_x%26x_0", false},
    {"x\x27x", "http_x%27x_0", false},
    {"x\x28x", "http_x%28x_0", false},
    {"x\x29x", "http_x%29x_0", false},
    {"x\x2ax", "http_x%2ax_0", false},
    {"x\x2bx", "http_x+x_0", false},
    {"x\x2cx", "http_x%2cx_0", false},
    {"x\x2dx", "http_x-x_0", true},
    {"x\x2ex", "http_x.x_0", true},
    {"x\x2fx", "http_x_0", false},  // 'x/x', the / and following are ignored.
    {"x\x30x", "http_x0x_0", true},
    {"x\x31x", "http_x1x_0", true},
    {"x\x32x", "http_x2x_0", true},
    {"x\x33x", "http_x3x_0", true},
    {"x\x34x", "http_x4x_0", true},
    {"x\x35x", "http_x5x_0", true},
    {"x\x36x", "http_x6x_0", true},
    {"x\x37x", "http_x7x_0", true},
    {"x\x38x", "http_x8x_0", true},
    {"x\x39x", "http_x9x_0", true},
    {"x\x3ax", "__0", false},
    {"x\x3bx", "__0", false},
    {"x\x3cx", "http_x%3cx_0", false},
    {"x\x3dx", "http_x%3dx_0", false},
    {"x\x3ex", "http_x%3ex_0", false},
    {"x\x3fx", "http_x_0", false},  // 'x?x', the ? and following are ignored.
    {"x\x40x", "http_x_0", false},  // 'x@x', the @ and following are ignored.
    {"x\x41x", "http_xax_0", true},
    {"x\x42x", "http_xbx_0", true},
    {"x\x43x", "http_xcx_0", true},
    {"x\x44x", "http_xdx_0", true},
    {"x\x45x", "http_xex_0", true},
    {"x\x46x", "http_xfx_0", true},
    {"x\x47x", "http_xgx_0", true},
    {"x\x48x", "http_xhx_0", true},
    {"x\x49x", "http_xix_0", true},
    {"x\x4ax", "http_xjx_0", true},
    {"x\x4bx", "http_xkx_0", true},
    {"x\x4cx", "http_xlx_0", true},
    {"x\x4dx", "http_xmx_0", true},
    {"x\x4ex", "http_xnx_0", true},
    {"x\x4fx", "http_xox_0", true},
    {"x\x50x", "http_xpx_0", true},
    {"x\x51x", "http_xqx_0", true},
    {"x\x52x", "http_xrx_0", true},
    {"x\x53x", "http_xsx_0", true},
    {"x\x54x", "http_xtx_0", true},
    {"x\x55x", "http_xux_0", true},
    {"x\x56x", "http_xvx_0", true},
    {"x\x57x", "http_xwx_0", true},
    {"x\x58x", "http_xxx_0", true},
    {"x\x59x", "http_xyx_0", true},
    {"x\x5ax", "http_xzx_0", true},
    {"x\x5bx", "__0", false},
    {"x\x5cx", "http_x_0", false},  // "x\x", the \ and following are ignored.
    {"x\x5dx", "__0", false},
    {"x\x5ex", "__0", false},
    {"x\x5fx", "http_x_x_0", true},
    {"x\x60x", "http_x%60x_0", false},
    {"x\x61x", "http_xax_0", true},
    {"x\x62x", "http_xbx_0", true},
    {"x\x63x", "http_xcx_0", true},
    {"x\x64x", "http_xdx_0", true},
    {"x\x65x", "http_xex_0", true},
    {"x\x66x", "http_xfx_0", true},
    {"x\x67x", "http_xgx_0", true},
    {"x\x68x", "http_xhx_0", true},
    {"x\x69x", "http_xix_0", true},
    {"x\x6ax", "http_xjx_0", true},
    {"x\x6bx", "http_xkx_0", true},
    {"x\x6cx", "http_xlx_0", true},
    {"x\x6dx", "http_xmx_0", true},
    {"x\x6ex", "http_xnx_0", true},
    {"x\x6fx", "http_xox_0", true},
    {"x\x70x", "http_xpx_0", true},
    {"x\x71x", "http_xqx_0", true},
    {"x\x72x", "http_xrx_0", true},
    {"x\x73x", "http_xsx_0", true},
    {"x\x74x", "http_xtx_0", true},
    {"x\x75x", "http_xux_0", true},
    {"x\x76x", "http_xvx_0", true},
    {"x\x77x", "http_xwx_0", true},
    {"x\x78x", "http_xxx_0", true},
    {"x\x79x", "http_xyx_0", true},
    {"x\x7ax", "http_xzx_0", true},
    {"x\x7bx", "http_x%7bx_0", false},
    {"x\x7cx", "http_x%7cx_0", false},
    {"x\x7dx", "http_x%7dx_0", false},
    {"x\x7ex", "__0", false},
    {"x\x7fx", "__0", false},
    {"x\x80x", "__0", false},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    GURL origin_url("http://" + cases[i].hostname);
    url::Origin origin = url::Origin::Create(origin_url);
    DatabaseIdentifier identifier_from_url =
        DatabaseIdentifier::CreateFromOrigin(origin_url);
    DatabaseIdentifier identifier_from_origin =
        DatabaseIdentifier::CreateFromOrigin(origin);
    EXPECT_EQ(cases[i].expected, identifier_from_url.ToString())
        << "test case " << i << " :\"" << cases[i].hostname << "\"";
    EXPECT_EQ(cases[i].expected, identifier_from_origin.ToString())
        << "test case " << i << " :\"" << cases[i].hostname << "\"";
    EXPECT_EQ(identifier_from_url.ToString(),
              identifier_from_origin.ToString());
    if (cases[i].shouldRoundTrip) {
      DatabaseIdentifier parsed_identifier_from_url =
          DatabaseIdentifier::Parse(identifier_from_url.ToString());
      EXPECT_EQ(identifier_from_url.ToString(),
                parsed_identifier_from_url.ToString())
          << "test case " << i << " :\"" << cases[i].hostname << "\"";
      DatabaseIdentifier parsed_identifier_from_origin =
          DatabaseIdentifier::Parse(identifier_from_origin.ToString());
      EXPECT_EQ(identifier_from_origin.ToString(),
                parsed_identifier_from_origin.ToString())
          << "test case " << i << " :\"" << cases[i].hostname << "\"";
      EXPECT_EQ(parsed_identifier_from_url.ToString(),
                parsed_identifier_from_origin.ToString());
    }
  }
}

TEST(DatabaseIdentifierTest, ExtractOriginDataFromIdentifier) {
  struct IdentifierTestCase {
    std::string str;
    std::string expected_scheme;
    std::string expected_host;
    int expected_port;
    GURL expected_origin;
    bool expected_unique;
  };

  IdentifierTestCase valid_cases[] = {
    {"http_google.com_0",
     "http", "google.com", 0, GURL("http://google.com"), false},
    {"https_google.com_0",
     "https", "google.com", 0, GURL("https://google.com"), false},
    {"ftp_google.com_0",
     "ftp", "google.com", 0, GURL("ftp://google.com"), false},
    {"unknown_google.com_0",
     "unknown", "", 0, GURL("unknown://"), false},
    {"http_nondefaultport.net_8001",
     "http", "nondefaultport.net", 8001,
     GURL("http://nondefaultport.net:8001"), false},
    {"file__0",
     "", "", 0, GURL("file:///"), true},
    {"__0",
     "", "", 0, GURL(), true},
    {"http_foo_bar_baz.org_0",
     "http", "foo_bar_baz.org", 0, GURL("http://foo_bar_baz.org"), false},
    {"http_xn--n3h.unicode.com_0",
     "http", "xn--n3h.unicode.com", 0,
      GURL("http://xn--n3h.unicode.com"), false},
    {"http_dot.com_0", "http", "dot.com", 0, GURL("http://dot.com"), false},
    {"http_escaped%3Dfun.com_0", "http", "escaped%3dfun.com", 0,
      GURL("http://escaped%3dfun.com"), false},
    {"http_[__1]_8080",
     "http", "[::1]", 8080, GURL("http://[::1]:8080"), false},
    {"http_[3ffe_2a00_100_7031__1]_0",
     "http", "[3ffe:2a00:100:7031::1]", 0,
      GURL("http://[3ffe:2a00:100:7031::1]"), false},
    {"http_[__ffff_8190_3426]_0",
     "http", "[::ffff:8190:3426]", 0, GURL("http://[::ffff:8190:3426]"), false},
  };

  for (const auto& valid_case : valid_cases) {
    DatabaseIdentifier identifier = DatabaseIdentifier::Parse(valid_case.str);
    EXPECT_EQ(valid_case.expected_scheme, identifier.scheme())
        << "test case " << valid_case.str;
    EXPECT_EQ(valid_case.expected_host, identifier.hostname())
        << "test case " << valid_case.str;
    EXPECT_EQ(valid_case.expected_port, identifier.port())
        << "test case " << valid_case.str;
    EXPECT_EQ(valid_case.expected_origin, identifier.ToOrigin())
        << "test case " << valid_case.str;
    EXPECT_EQ(valid_case.expected_unique, identifier.is_unique())
        << "test case " << valid_case.str;
  }

  std::string bogus_components[] = {
    "", "_", "__", std::string("\x00", 1), std::string("http_\x00_0", 8),
    "ht\x7ctp_badscheme.com_0", "http_unescaped_percent_%.com_0",
    "http_port_too_big.net_75000", "http_port_too_small.net_-25",
    "http_shouldbeescaped\x7c.com_0", "http_latin1\x8a.org_8001",
    "http_\xe2\x98\x83.unicode.com_0",
    "http_dot%252ecom_0",
    "HtTp_NonCanonicalRepresenTation_0",
    "http_non_ascii.\xa1.com_0",
    "http_not_canonical_escape%3d_0",
    "http_bytes_after_port_0abcd",
  };

  for (const auto& bogus_component : bogus_components) {
    DatabaseIdentifier identifier = DatabaseIdentifier::Parse(bogus_component);
    EXPECT_EQ("__0", identifier.ToString()) << "test case " << bogus_component;
    EXPECT_EQ(GURL("null"), identifier.ToOrigin())
        << "test case " << bogus_component;
    EXPECT_EQ(true, identifier.is_unique()) << "test case " << bogus_component;
  }
}

static GURL GURLToAndFromOriginIdentifier(const GURL& origin_url) {
  std::string id = storage::GetIdentifierFromOrigin(origin_url);
  return storage::GetOriginURLFromIdentifier(id);
}

static url::Origin OriginToAndFromOriginIdentifier(const url::Origin& origin) {
  std::string id = storage::GetIdentifierFromOrigin(origin);
  return storage::GetOriginFromIdentifier(id);
}

static void TestValidOriginIdentifier(bool expected_result,
                                      const std::string& id) {
  EXPECT_EQ(expected_result, storage::IsValidOriginIdentifier(id));
}

TEST(DatabaseIdentifierTest, OriginIdentifiers) {
  const GURL kFileOriginURL(GURL("file:///").DeprecatedGetOriginAsURL());
  const GURL kHttpOriginURL(GURL("http://bar/").DeprecatedGetOriginAsURL());
  const url::Origin kFileOrigin = url::Origin::Create(kFileOriginURL);
  const url::Origin kHttpOrigin = url::Origin::Create(kHttpOriginURL);

  EXPECT_EQ(kFileOriginURL, GURLToAndFromOriginIdentifier(kFileOriginURL));
  EXPECT_EQ(kHttpOriginURL, GURLToAndFromOriginIdentifier(kHttpOriginURL));
  EXPECT_EQ(kFileOrigin, OriginToAndFromOriginIdentifier(kFileOrigin));
  EXPECT_EQ(kHttpOrigin, OriginToAndFromOriginIdentifier(kHttpOrigin));
}

TEST(DatabaseIdentifierTest, IsValidOriginIdentifier) {
  TestValidOriginIdentifier(true,  "http_bar_0");
  TestValidOriginIdentifier(false,  "");
  TestValidOriginIdentifier(false, "bad..id");
  TestValidOriginIdentifier(false, "bad/id");
  TestValidOriginIdentifier(false, "bad\\id");
  TestValidOriginIdentifier(false, "http_bad:0_2");
  TestValidOriginIdentifier(false, std::string("bad\0id", 6));
}

}  // namespace
}  // namespace content
