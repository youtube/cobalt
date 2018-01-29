// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/network/user_agent_string_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {

class UserAgentBindingsTest : public BindingsTestBase {
 public:
  bool EvaluateRegex(const std::string& regex, const std::string& str,
                     std::string* result) {
    std::stringstream ss;
    ss << "/" << regex << "/g.exec('" << str << "')";
    bool ok = EvaluateScript(ss.str(), result);
    return ok && (result->compare("null") != 0);
  }

  bool RegexMatches(const std::string& regex, const std::string& str) {
    std::string result;
    return EvaluateRegex(regex, str, &result);
  }

  // This matches a device.py file in Google's internal code repository.
  std::vector<std::string> GenUserAgentRegexes() {
    std::vector<std::string> output;
    output.reserve(14);
    const char ua_field_chars[] = "-.A-Za-z0-9\\\\,_/()";

    std::stringstream ss;
    // Note that ?P<NAME> is used by python for named capture groups. Javascript
    // does not support named capture groups, so they are commented out.
    // Additionally, all model names have been replaced with more general regexes.
    // YTTV 2016: Operator_DeviceType_Chipset/Firmware (Brand, Model, Connection)
    ss << "("/*?P<device>*/"("/*?P<operator>*/"[" << ua_field_chars << "'/]*)_"
       << "("/*?P<type>*/"[" << ua_field_chars << "/]+)_"
       << "("/*?P<chipset>*/"[" << ua_field_chars << "/_]+)) ?/ ?"
       << "[" << ua_field_chars << "_]* "
       << "\\(("/*?P<brand>*/"[" << ua_field_chars << "/_ ]+), ?("/*?P<model>*/"[^,]*), "
       << "?[WIREDLSwiredls\\\\/]*\\)";
    output.push_back(ss.str());
    // TTV 2013: Device/Firmware (Brand, Model, Connection)
    output.push_back("("/*?P<device>*/"[-_.A-Za-z0-9\\/]+) ?/ ?[-_.A-Za-z0-9\\]* ");
    output.push_back("\\(("/*?P<brand>*/"[-_.A-Za-z0-9\\\\/ ]+), ?("/*?P<model>*/"[^,]*), ");
    output.push_back("?[WIREDLSwiredls\\\\/]*\\)");
    // YTTV 2012: Vendor Device/Firmware (Model, SKU, Lang, Country)
    output.push_back("("/*?P<brand>*/"[-_.A-Za-z0-9\\\\/]+) ");
    output.push_back("("/*?P<device>*/"[-_.A-Za-z0-9\\\\]+)/[-_.A-Za-z0-9\\\\/]* \\(("/*?P<model>*/"[^,]*), ");
    output.push_back("?[-_.A-Za-z0-9\\\\/]*,[^,]+,[^)]+\\)");
    // (REMOVED) Spec:
    output.push_back("("/*?P<device>*/"[a-zA-Z]+)/[0-9.]+ \\([^;]*; ?("/*?P<brand>*/"[^;]*); ");
    output.push_back("?("/*?P<model>*/"[^;]*);[^;]*;[^;]*;[^)]*\\)");
    // (REMOVED) 2012:
    output.push_back("("/*?P<brand>*/"[G-L]+) Browser/[0-9.]+\\([^;]*; [E-L]+; ");
    output.push_back("?("/*?P<model>*/"[^;]*);[^;]*;[^;]*;\\); [G-L]+ ("/*?P<device>*/"[-_.A-Za-z0-9\\\\/]+) ");
    // (REMOVED)
    // Note: device is more of a firmware version, but model should feasibly always match.
    output.push_back("("/*?P<brand>*/"[a-zA-Z]+)/("/*?P<model>*/"\\S+) \\(("/*?P<device>*/"[^)]+)\\)");
    // (REMOVED)
    output.push_back("[.A-Za-z0-9\\\\/]+ [a-zA-Z]+/[.0-9]+ ("/*?P<brand>*/"[a-zA-Z]+)[T-V]+/[.0-9]+ ");
    output.push_back("model/("/*?P<model>*/"\\S+) ?[-_.A-Za-z0-9\\\\/]* build/[A-Za-z0-9]+");
    return output;
  }
};

// Simple test to make sure that simple regex's work on this platform.
TEST_F(UserAgentBindingsTest, RegexSanity) {
  ASSERT_TRUE(RegexMatches("b+", "b"));
  ASSERT_TRUE(RegexMatches("b+", "bb"));
  ASSERT_TRUE(RegexMatches("b+", "bbb"));
  ASSERT_TRUE(RegexMatches("[ab]+", "ab"));
  ASSERT_FALSE(RegexMatches("[ab]+", "c"));
  ASSERT_FALSE(RegexMatches("abc", "c"));
}

// Tests that the platform user agent string will be accepted by one of the
// user agent regexes.
TEST_F(UserAgentBindingsTest, UserAgent) {
  using cobalt::network::UserAgentStringFactory;

  std::string user_agent_string =
      UserAgentStringFactory::ForCurrentPlatform()->CreateUserAgentString();

  std::vector<std::string> regexes = GenUserAgentRegexes();

  bool passes = false;
  for (const std::string& regex : regexes) {
    passes = RegexMatches(regex, user_agent_string);
    if (passes) {
      break;
    }
  }
  EXPECT_TRUE(passes);
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
