// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <map>

#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/browser/user_agent_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

namespace {

UserAgentPlatformInfo CreateEmptyPlatformInfo() {
  UserAgentPlatformInfo platform_info;
  platform_info.set_starboard_version("");
  platform_info.set_os_name_and_version("");
  platform_info.set_original_design_manufacturer("");
  platform_info.set_device_type(kSbSystemDeviceTypeUnknown);
  platform_info.set_chipset_model_number("");
  platform_info.set_model_year("");
  platform_info.set_firmware_version("");
  platform_info.set_brand("");
  platform_info.set_model("");
  platform_info.set_aux_field("");
  platform_info.set_connection_type(base::nullopt);
  platform_info.set_javascript_engine_version("");
  platform_info.set_rasterizer_type("");
  platform_info.set_evergreen_version("");
  platform_info.set_cobalt_version("");
  platform_info.set_cobalt_build_version_number("");
  platform_info.set_build_configuration("");
  return platform_info;
}

UserAgentPlatformInfo CreateOnlyOSNameAndVersionPlatformInfo() {
  UserAgentPlatformInfo platform_info = CreateEmptyPlatformInfo();
  platform_info.set_os_name_and_version("GLaDOS 3.11");
  return platform_info;
}

TEST(UserAgentStringFactoryTest, StartsWithMozilla) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_EQ(0, user_agent_string.find("Mozilla/5.0"));
}

TEST(UserAgentStringFactoryTest, ContainsCobalt) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_NE(std::string::npos, user_agent_string.find("Cobalt"));
}

TEST(UserAgentStringFactoryTest, WithOSNameAndVersion) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_NE(std::string::npos, user_agent_string.find("(GLaDOS 3.11)"));
}

TEST(UserAgentStringFactoryTest, WithCobaltVersionAndConfiguration) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_version("16");
  platform_info.set_cobalt_build_version_number("123456");
  platform_info.set_build_configuration("gold");
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str = "Cobalt/16.123456-gold (unlike Gecko)";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

// Look-alike replacements previously used for sanitizing fields
#define COMMA u8"\uFF0C"   // fullwidth comma
#define UNDER u8"\u2E0F"   // paragraphos
#define SLASH u8"\u2215"   // division slash
#define LPAREN u8"\uFF08"  // fullwidth left paren
#define RPAREN u8"\uFF09"  // fullwidth right paren

#define ALPHALOWER "abcdefghijklmnopqrstuvwxyz"
#define ALPHAUPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ALPHA ALPHAUPPER ALPHALOWER
#define DIGIT "0123456789"
#define DIGITREVERSED "9876543210"
#define ALPHADIGIT ALPHA DIGIT
#define TCHAR ALPHADIGIT "!#$%&\'*+-.^_`|~"
#define TCHARORSLASH TCHAR "/"
#define VCHAR_EXCEPTALPHADIGIT "!\"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~"
#define VCHAR_EXCEPTPARENTHESES "!\"#$%&\'*+,-./:;<=>?@[\\]^_`{|}~" ALPHADIGIT
#define VCHAR_EXCEPTPARENTHESESANDCOMMA \
  "!\"#$%&\'*+-./:;<=>?@[\\]^_`{|}~" ALPHADIGIT
#define VCHAR VCHAR_EXCEPTALPHADIGIT ALPHADIGIT
#define VCHARORSPACE " " VCHAR
#define VCHARORSPACE_EXCEPTPARENTHESES " " VCHAR_EXCEPTPARENTHESES
#define VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA \
  " " VCHAR_EXCEPTPARENTHESESANDCOMMA

#define CONTROL                                                  \
  "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F" \
  "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
#define DEL "\x7F"
#define HIGH_ASCII                                                   \
  "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F" \
  "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F" \
  "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF" \
  "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF" \
  "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF" \
  "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF" \
  "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF" \
  "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF"

#define NOT_DIGIT CONTROL VCHAR_EXCEPTALPHADIGIT ALPHA DEL HIGH_ASCII
#define NOT_ALPHADIGIT CONTROL VCHAR_EXCEPTALPHADIGIT DEL HIGH_ASCII
#define NOT_TCHAR CONTROL "\"(),/:;<=>?@[\\]{}" DEL HIGH_ASCII
#define NOT_TCHARORSLASH CONTROL "\"(),:;<=>?@[\\]{}" DEL HIGH_ASCII

#define NOT_VCHARORSPACE CONTROL DEL HIGH_ASCII

TEST(UserAgentStringFactoryTest, SanitizedStarboardVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_starboard_version("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                                      "Baz" NOT_TCHARORSLASH "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedOsNameAndVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_os_name_and_version("Foo()" NOT_VCHARORSPACE
                                        "Bar" VCHARORSPACE_EXCEPTPARENTHESES
                                        "Baz()" NOT_VCHARORSPACE "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(
      std::string::npos,
      user_agent_string.find("FooBar" VCHARORSPACE_EXCEPTPARENTHESES "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedOriginalDesignManufacturer) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_original_design_manufacturer(
      "Foo" NOT_ALPHADIGIT "Bar" ALPHADIGIT "Baz" NOT_ALPHADIGIT "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" ALPHADIGIT "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedChipsetModelNumber) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_chipset_model_number("Foo" NOT_ALPHADIGIT "Bar" ALPHADIGIT
                                         "Baz" NOT_ALPHADIGIT "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" ALPHADIGIT "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedModelYear) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_chipset_model_number("FooBar");
  platform_info.set_model_year(NOT_DIGIT DIGIT NOT_DIGIT DIGITREVERSED);
  platform_info.set_firmware_version("BazQux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar_" DIGIT DIGITREVERSED "/BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedFirmwareVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_firmware_version("Foo" NOT_TCHAR "Bar" TCHAR "Baz" NOT_TCHAR
                                     "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedBrand) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_brand("Foo()," NOT_VCHARORSPACE
                          "Bar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA
                          "Baz()," NOT_VCHARORSPACE "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find(
                "FooBar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedModel) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_model("Foo()," NOT_VCHARORSPACE
                          "Bar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA
                          "Baz()," NOT_VCHARORSPACE "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find(
                "FooBar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedAuxField) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_aux_field("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                              "Baz" NOT_TCHARORSLASH "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedJavascriptEngineVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_javascript_engine_version(
      "Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH "Baz" NOT_TCHARORSLASH "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedRasterizerType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_rasterizer_type("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                                    "Baz" NOT_TCHARORSLASH "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedEvergreenType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_evergreen_version("Foo" NOT_TCHAR "Bar" TCHAR
                                      "Baz" NOT_TCHAR "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedCobaltVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_version("Foo" NOT_TCHAR "Bar" TCHAR "Baz" NOT_TCHAR
                                   "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedCobaltBuildVersionNumber) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_build_version_number("Foo" NOT_TCHAR "Bar" TCHAR
                                                "Baz" NOT_TCHAR "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

TEST(UserAgentStringFactoryTest, SanitizedCobaltBuildConfiguration) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_build_configuration("Foo" NOT_TCHAR "Bar" TCHAR
                                        "Baz" NOT_TCHAR "Qux");
  std::string user_agent_string = CreateUserAgentString(platform_info);
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

TEST(UserAgentStringFactoryTest, WithPlatformInfo) {
  // There are deliberately a variety of underscores, commas, slashes, and
  // parentheses in the strings below to ensure they get sanitized.
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_original_design_manufacturer("Aperture_Science_Innovators");
  platform_info.set_device_type(kSbSystemDeviceTypeOverTheTopBox);
  platform_info.set_chipset_model_number("P-body/Orange_Atlas/Blue");
  platform_info.set_model_year("2013");
  platform_info.set_firmware_version("0,01");
  platform_info.set_brand("Aperture Science (Labs)");
  platform_info.set_model("GLaDOS");
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str =
      "ApertureScienceInnovators_OTT_PbodyOrangeAtlasBlue_2013/001 "
      "(Aperture Science Labs, GLaDOS, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithWiredConnection) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_connection_type(kSbSystemConnectionTypeWired);
  platform_info.set_device_type(kSbSystemDeviceTypeOverTheTopBox);
  std::string user_agent_string = CreateUserAgentString(platform_info);

  EXPECT_NE(std::string::npos, user_agent_string.find(", Wired)"));
}

TEST(UserAgentStringFactoryTest, WithWirelessConnection) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_connection_type(kSbSystemConnectionTypeWireless);
  platform_info.set_device_type(kSbSystemDeviceTypeOverTheTopBox);
  std::string user_agent_string = CreateUserAgentString(platform_info);

  EXPECT_NE(std::string::npos, user_agent_string.find(", Wireless)"));
}

TEST(UserAgentStringFactoryTest, WithOnlyBrandModelAndDeviceType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_device_type(kSbSystemDeviceTypeOverTheTopBox);
  platform_info.set_brand("Aperture Science");
  platform_info.set_model("GLaDOS");
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str =
      ", Unknown_OTT_Unknown_0/Unknown (Aperture Science, GLaDOS, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithStarboardVersion) {
  UserAgentPlatformInfo platform_info = CreateEmptyPlatformInfo();
  platform_info.set_starboard_version("Starboard/6");
  platform_info.set_device_type(kSbSystemDeviceTypeOverTheTopBox);
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str =
      "Starboard/6, Unknown_OTT_Unknown_0/Unknown (Unknown, Unknown, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithJavaScriptVersion) {
  UserAgentPlatformInfo platform_info = CreateEmptyPlatformInfo();
  platform_info.set_javascript_engine_version("V8/6.5.254.28");
  std::string user_agent_string = CreateUserAgentString(platform_info);

  EXPECT_NE(std::string::npos, user_agent_string.find(" V8/6.5.254.28"));
}

TEST(GetUserAgentInputMapTest, DelimitParamsBySemicolon) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      "model_year=2049;starboard_version=Starboard/"
      "13;original_design_manufacturer=foo;device_type=GAME";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"device_type", "GAME"},
      {"model_year", "2049"},
      {"original_design_manufacturer", "foo"},
      {"starboard_version", "Starboard/13"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

TEST(GetUserAgentInputMapTest, HandleSpecialChar) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      "aux_field=foo.bar.baz.qux/"
      "21.2.1.41.0;invalid-field~name=quux(#quuz)";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"aux_field", "foo.bar.baz.qux/21.2.1.41.0"},
      {"invalid-field~name", "quux(#quuz)"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

TEST(GetUserAgentInputMapTest, EscapeSemicolonInValue) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      "os_name_and_version=Foo bar-v7a\\; Baz 7.1.2\\; Qux OS "
      "6.0;model=QUUX";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"model", "QUUX"},
      {"os_name_and_version", "Foo bar-v7a; Baz 7.1.2; Qux OS 6.0"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

TEST(GetUserAgentInputMapTest, OmitEscapeSemicolonInField) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input = "foo//;bar=baz";

  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"bar", "baz"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

TEST(GetUserAgentInputMapTest, HandleEmptyFieldValue) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      "evergreen_type=;device_type=GAME;evergreen_version=";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"device_type", "GAME"},
      {"evergreen_type", ""},
      {"evergreen_version", ""},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

TEST(GetUserAgentInputMapTest, FailSafeWithInvalidInput) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      ";model;aux_field=;=dummy;device_type=GAME;invalid_field";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"aux_field", ""}, {"device_type", "GAME"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}
}  // namespace

}  // namespace browser
}  // namespace cobalt
