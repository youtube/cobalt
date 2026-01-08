#include "third_party/blink/renderer/core/frame/csp/local_ip.h"

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

struct TestCase {
  const char* ip_str;
  bool expected_result;
};

class IsIPInPrivateRangeTest : public testing::TestWithParam<TestCase> {};

TEST_P(IsIPInPrivateRangeTest, CheckRanges) {
  const TestCase& test_case = GetParam();
  EXPECT_EQ(test_case.expected_result, IsIPInPrivateRange(test_case.ip_str))
      << "Failed for IP: " << test_case.ip_str;
}

const TestCase kTestCases[] = {
    // --- IPv4 RFC 1918 ---
    {"10.0.0.1", true},
    {"10.255.255.255", true},
    {"172.16.0.0", true},
    {"172.31.255.255", true},
    {"192.168.1.1", true},
    {"8.8.8.8", false},          // Public
    {"172.15.255.255", false},   // Just outside range
    {"172.32.0.0", false},       // Just outside range

    // --- IPv6 ULA (fc00::/7) ---
    {"[fd00::1]", true},
    {"[fc00::1]", true},
    {"[2001:db8::1]", false},      // Global Unicast

    // --- 6to4 (2002::/16) ---
    // 2002:0a00:0001:: -> 10.0.0.1 (Private)
    {"[2002:0a00:0001::]", true},
    // 2002:0808:0808:: -> 8.8.8.8 (Public)
    {"[2002:0808:0808::]", false},

    // --- IPv4-Mapped IPv6 ---
    {"[::ffff:192.168.0.1]", true},
    {"[::ffff:1.1.1.1]", false},

    // --- Invalid Inputs ---
    {"not_an_ip", false},
    {"256.256.256.256", false},
    {"", false},
};

INSTANTIATE_TEST_SUITE_P(All,
                         IsIPInPrivateRangeTest,
                         testing::ValuesIn(kTestCases));

}  // namespace
