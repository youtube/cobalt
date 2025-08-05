// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/user_agent/user_agent_platform_info.h"

#include <map>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

namespace {

UserAgentPlatformInfo CreateEmptyPlatformInfo() {
  UserAgentPlatformInfo platform_info;
  platform_info.set_starboard_version("");
  platform_info.set_os_name_and_version("");
  platform_info.set_original_design_manufacturer("");
  platform_info.set_device_type("UNKNOWN");
  platform_info.set_chipset_model_number("");
  platform_info.set_model_year("");
  platform_info.set_firmware_version("");
  platform_info.set_brand("");
  platform_info.set_model("");
  platform_info.set_aux_field("");
  platform_info.set_javascript_engine_version("");
  platform_info.set_rasterizer_type("");
  platform_info.set_evergreen_version("");
  platform_info.set_evergreen_type("");
  platform_info.set_evergreen_file_type("");
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

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_StartsWithMozilla DISABLED_StartsWithMozilla
#else
#define MAYBE_StartsWithMozilla StartsWithMozilla
#endif
TEST(UserAgentStringTest, MAYBE_StartsWithMozilla) {
  std::string user_agent_string =
      CreateOnlyOSNameAndVersionPlatformInfo().ToString();
  EXPECT_EQ(0UL, user_agent_string.find("Mozilla/5.0"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ContainsCobalt DISABLED_ContainsCobalt
#else
#define MAYBE_ContainsCobalt ContainsCobalt
#endif
TEST(UserAgentStringTest, MAYBE_ContainsCobalt) {
  std::string user_agent_string =
      CreateOnlyOSNameAndVersionPlatformInfo().ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("Cobalt"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithOSNameAndVersion DISABLED_WithOSNameAndVersion
#else
#define MAYBE_WithOSNameAndVersion WithOSNameAndVersion
#endif
TEST(UserAgentStringTest, MAYBE_WithOSNameAndVersion) {
  std::string user_agent_string =
      CreateOnlyOSNameAndVersionPlatformInfo().ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("(GLaDOS 3.11)"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithCobaltVersionAndConfiguration \
  DISABLED_WithCobaltVersionAndConfiguration
#else
#define MAYBE_WithCobaltVersionAndConfiguration \
  WithCobaltVersionAndConfiguration
#endif
TEST(UserAgentStringTest, MAYBE_WithCobaltVersionAndConfiguration) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_version("16");
  platform_info.set_cobalt_build_version_number("123456");
  platform_info.set_build_configuration("gold");
  std::string user_agent_string = platform_info.ToString();

  const std::string tv_info_str = "Cobalt/16.123456-gold (unlike Gecko)";
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
#define TCHAR ALPHADIGIT "!#$%&'*+-.^_`|~"
#define TCHARORSLASH TCHAR "/"
#define VCHAR_EXCEPTALPHADIGIT "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
#define VCHAR_EXCEPTPARENTHESES "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" ALPHADIGIT
#define VCHAR_EXCEPTPARENTHESESANDCOMMA \
  "!\"#$%&'()*+-./:;<=>?@[\\]^_`{|}~" ALPHADIGIT
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
#define NOT_TCHARORSLASH CONTROL "\"(),/:;<=>?@[\\]{}" DEL HIGH_ASCII

#define NOT_VCHARORSPACE CONTROL DEL HIGH_ASCII

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedStarboardVersion DISABLED_SanitizedStarboardVersion
#else
#define MAYBE_SanitizedStarboardVersion SanitizedStarboardVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedStarboardVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_starboard_version("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                                      "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedOsNameAndVersion DISABLED_SanitizedOsNameAndVersion
#else
#define MAYBE_SanitizedOsNameAndVersion SanitizedOsNameAndVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedOsNameAndVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_os_name_and_version("Foo()" NOT_VCHARORSPACE
                                        "Bar" VCHARORSPACE_EXCEPTPARENTHESES
                                        "Baz()" NOT_VCHARORSPACE "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(
      std::string::npos,
      user_agent_string.find("FooBar" VCHARORSPACE_EXCEPTPARENTHESES "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedOriginalDesignManufacturer \
  DISABLED_SanitizedOriginalDesignManufacturer
#else
#define MAYBE_SanitizedOriginalDesignManufacturer \
  SanitizedOriginalDesignManufacturer
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedOriginalDesignManufacturer) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_original_design_manufacturer(
      "Foo" NOT_ALPHADIGIT "Bar" ALPHADIGIT "Baz" NOT_ALPHADIGIT "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" ALPHADIGIT "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedChipsetModelNumber DISABLED_SanitizedChipsetModelNumber
#else
#define MAYBE_SanitizedChipsetModelNumber SanitizedChipsetModelNumber
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedChipsetModelNumber) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_chipset_model_number("Foo" NOT_ALPHADIGIT "Bar" ALPHADIGIT
                                         "Baz" NOT_ALPHADIGIT "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" ALPHADIGIT "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedModelYear DISABLED_SanitizedModelYear
#else
#define MAYBE_SanitizedModelYear SanitizedModelYear
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedModelYear) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_chipset_model_number("FooBar");
  platform_info.set_model_year(NOT_DIGIT DIGIT NOT_DIGIT DIGITREVERSED);
  platform_info.set_firmware_version("BazQux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar_" DIGIT DIGITREVERSED "/BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedFirmwareVersion DISABLED_SanitizedFirmwareVersion
#else
#define MAYBE_SanitizedFirmwareVersion SanitizedFirmwareVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedFirmwareVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_firmware_version("Foo" NOT_TCHAR "Bar" TCHAR "Baz" NOT_TCHAR
                                     "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedBrand DISABLED_SanitizedBrand
#else
#define MAYBE_SanitizedBrand SanitizedBrand
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedBrand) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_brand("Foo()," NOT_VCHARORSPACE
                          "Bar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA
                          "Baz()," NOT_VCHARORSPACE "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find(
                "FooBar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedModel DISABLED_SanitizedModel
#else
#define MAYBE_SanitizedModel SanitizedModel
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedModel) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_model("Foo()," NOT_VCHARORSPACE
                          "Bar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA
                          "Baz()," NOT_VCHARORSPACE "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find(
                "FooBar" VCHARORSPACE_EXCEPTPARENTHESESANDCOMMA "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedAuxField DISABLED_SanitizedAuxField
#else
#define MAYBE_SanitizedAuxField SanitizedAuxField
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedAuxField) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_aux_field("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                              "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedJavascriptEngineVersion \
  DISABLED_SanitizedJavascriptEngineVersion
#else
#define MAYBE_SanitizedJavascriptEngineVersion SanitizedJavascriptEngineVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedJavascriptEngineVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_javascript_engine_version(
      "Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedRasterizerType DISABLED_SanitizedRasterizerType
#else
#define MAYBE_SanitizedRasterizerType SanitizedRasterizerType
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedRasterizerType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_rasterizer_type("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                                    "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedEvergreenVersion DISABLED_SanitizedEvergreenVersion
#else
#define MAYBE_SanitizedEvergreenVersion SanitizedEvergreenVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedEvergreenVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_evergreen_version("Foo" NOT_TCHAR "Bar" TCHAR
                                      "Baz" NOT_TCHAR "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedEvergreenType DISABLED_SanitizedEvergreenType
#else
#define MAYBE_SanitizedEvergreenType SanitizedEvergreenType
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedEvergreenType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_evergreen_type("Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH
                                   "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedEvergreenFileType DISABLED_SanitizedEvergreenFileType
#else
#define MAYBE_SanitizedEvergreenFileType SanitizedEvergreenFileType
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedEvergreenFileType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_evergreen_file_type(
      "Foo" NOT_TCHARORSLASH "Bar" TCHARORSLASH "Baz" NOT_TCHARORSLASH "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("FooBar" TCHARORSLASH "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedCobaltVersion DISABLED_SanitizedCobaltVersion
#else
#define MAYBE_SanitizedCobaltVersion SanitizedCobaltVersion
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedCobaltVersion) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_version("Foo" NOT_TCHAR "Bar" TCHAR "Baz" NOT_TCHAR
                                   "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedCobaltBuildVersionNumber \
  DISABLED_SanitizedCobaltBuildVersionNumber
#else
#define MAYBE_SanitizedCobaltBuildVersionNumber \
  SanitizedCobaltBuildVersionNumber
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedCobaltBuildVersionNumber) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_cobalt_build_version_number("Foo" NOT_TCHAR "Bar" TCHAR
                                                "Baz" NOT_TCHAR "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SanitizedCobaltBuildConfiguration \
  DISABLED_SanitizedCobaltBuildConfiguration
#else
#define MAYBE_SanitizedCobaltBuildConfiguration \
  SanitizedCobaltBuildConfiguration
#endif
TEST(UserAgentStringTest, MAYBE_SanitizedCobaltBuildConfiguration) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_build_configuration("Foo" NOT_TCHAR "Bar" TCHAR
                                        "Baz" NOT_TCHAR "Qux");
  const std::string user_agent_string = platform_info.ToString();
  EXPECT_NE(std::string::npos, user_agent_string.find("FooBar" TCHAR "BazQux"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithPlatformInfo DISABLED_WithPlatformInfo
#else
#define MAYBE_WithPlatformInfo WithPlatformInfo
#endif
TEST(UserAgentStringTest, MAYBE_WithPlatformInfo) {
  // There are deliberately a variety of underscores, commas, slashes, and
  // parentheses in the strings below to ensure they get sanitized.
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_original_design_manufacturer("Aperture_Science_Innovators");
  platform_info.set_device_type("OTT");
  platform_info.set_chipset_model_number("P-body/Orange_Atlas/Blue");
  platform_info.set_model_year("2013");
  platform_info.set_firmware_version("0,01");
  platform_info.set_brand("Aperture Science (Labs)");
  platform_info.set_model("GLaDOS");
  const std::string user_agent_string = platform_info.ToString();

  const std::string tv_info_str =
      "ApertureScienceInnovators_OTT_PbodyOrangeAtlasBlue_2013/001 "
      "(Aperture Science Labs, GLaDOS)";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithOnlyBrandModelAndDeviceType \
  DISABLED_WithOnlyBrandModelAndDeviceType
#else
#define MAYBE_WithOnlyBrandModelAndDeviceType WithOnlyBrandModelAndDeviceType
#endif
TEST(UserAgentStringTest, MAYBE_WithOnlyBrandModelAndDeviceType) {
  UserAgentPlatformInfo platform_info =
      CreateOnlyOSNameAndVersionPlatformInfo();
  platform_info.set_device_type("OTT");
  platform_info.set_brand("Aperture Science");
  platform_info.set_model("GLaDOS");
  const std::string user_agent_string = platform_info.ToString();

  const std::string tv_info_str =
      ", Unknown_OTT_Unknown_0/Unknown (Aperture Science, GLaDOS)";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithStarboardVersion DISABLED_WithStarboardVersion
#else
#define MAYBE_WithStarboardVersion WithStarboardVersion
#endif
TEST(UserAgentStringTest, MAYBE_WithStarboardVersion) {
  UserAgentPlatformInfo platform_info = CreateEmptyPlatformInfo();
  platform_info.set_starboard_version("Starboard/6");
  platform_info.set_device_type("OTT");
  const std::string user_agent_string = platform_info.ToString();

  const std::string tv_info_str =
      "Starboard/6, Unknown_OTT_Unknown_0/Unknown (Unknown, Unknown)";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_WithJavaScriptVersion DISABLED_WithJavaScriptVersion
#else
#define MAYBE_WithJavaScriptVersion WithJavaScriptVersion
#endif
TEST(UserAgentStringTest, MAYBE_WithJavaScriptVersion) {
  UserAgentPlatformInfo platform_info = CreateEmptyPlatformInfo();
  platform_info.set_javascript_engine_version("V8/6.5.254.28");
  const std::string user_agent_string = platform_info.ToString();

  EXPECT_NE(std::string::npos, user_agent_string.find(" V8/6.5.254.28"));
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_DelimitParamsBySemicolon DISABLED_DelimitParamsBySemicolon
#else
#define MAYBE_DelimitParamsBySemicolon DelimitParamsBySemicolon
#endif
TEST(GetUserAgentInputMapTest, MAYBE_DelimitParamsBySemicolon) {
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

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_HandleSpecialChar DISABLED_HandleSpecialChar
#else
#define MAYBE_HandleSpecialChar HandleSpecialChar
#endif
TEST(GetUserAgentInputMapTest, MAYBE_HandleSpecialChar) {
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

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EscapeSemicolonInValue DISABLED_EscapeSemicolonInValue
#else
#define MAYBE_EscapeSemicolonInValue EscapeSemicolonInValue
#endif
TEST(GetUserAgentInputMapTest, MAYBE_EscapeSemicolonInValue) {
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

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_OmitEscapeSemicolonInField DISABLED_OmitEscapeSemicolonInField
#else
#define MAYBE_OmitEscapeSemicolonInField OmitEscapeSemicolonInField
#endif
TEST(GetUserAgentInputMapTest, MAYBE_OmitEscapeSemicolonInField) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input = "foo//;bar=baz";

  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"bar", "baz"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_HandleEmptyFieldValue DISABLED_HandleEmptyFieldValue
#else
#define MAYBE_HandleEmptyFieldValue HandleEmptyFieldValue
#endif
TEST(GetUserAgentInputMapTest, MAYBE_HandleEmptyFieldValue) {
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

// TODO(b/436368441): Investigate this test failure.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FailSafeWithInvalidInput DISABLED_FailSafeWithInvalidInput
#else
#define MAYBE_FailSafeWithInvalidInput FailSafeWithInvalidInput
#endif
TEST(GetUserAgentInputMapTest, MAYBE_FailSafeWithInvalidInput) {
  std::map<std::string, std::string> user_agent_input_map;
  const std::string user_agent_input =
      ";model;aux_field=;=dummy;device_type=GAME;invalid_field";
  GetUserAgentInputMap(user_agent_input, user_agent_input_map);

  std::map<std::string, std::string> expected_user_agent_input_map{
      {"aux_field", ""},
      {"device_type", "GAME"},
  };
  EXPECT_TRUE(user_agent_input_map == expected_user_agent_input_map);
}
}  // namespace

}  // namespace cobalt
