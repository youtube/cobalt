// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(DnsConfigServiceWinTest, ParseDomain) {
  const struct TestCase {
    const wchar_t* input;
    const char* output[4];  // NULL-terminated, empty if expected false
  } cases[] = {
    { L"chromium.org", { "chromium.org", NULL } },
    { L"chromium.org,org", { "chromium.org", "org", NULL } },
    // Empty suffixes terminate the list
    { L"crbug.com,com,,org", { "crbug.com", "com", NULL } },
    // IDN are converted to punycode
    { L"\u017c\xf3\u0142ta.pi\u0119\u015b\u0107.pl,pl",
        { "xn--ta-4ja03asj.xn--pi-wla5e0q.pl", "pl", NULL } },
    // Empty search list is invalid
    { L"", { NULL } },
    { L",,", { NULL } },
    { NULL, { NULL } },
  };

  std::vector<std::string> actual_output, expected_output;
  for (const TestCase* t = cases; t->input; ++t) {
    actual_output.clear();
    actual_output.push_back("UNSET");
    expected_output.clear();
    for (const char* const* output = t->output; *output; ++output) {
      expected_output.push_back(*output);
    }
    bool result = ParseSearchList(t->input, &actual_output);
    if (!expected_output.empty()) {
      EXPECT_TRUE(result);
      EXPECT_EQ(expected_output, actual_output);
    } else {
      EXPECT_FALSE(result) << "Unexpected parse success on " << t->input;
      expected_output.push_back("UNSET");
      EXPECT_EQ(expected_output, actual_output);
    }
  }
}

}  // namespace

}  // namespace net

