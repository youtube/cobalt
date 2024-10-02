// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tag.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using updater::tagging::AppArgs;
using updater::tagging::ErrorCode;
using updater::tagging::TagArgs;

// Builder pattern helper to construct the TagArgs struct.
class TagArgsBuilder {
 public:
  TagArgs Build() { return std::move(inner_); }

  TagArgsBuilder& WithBundleName(const std::string& bundle_name) {
    this->inner_.bundle_name = bundle_name;
    return *this;
  }
  TagArgsBuilder& WithInstallationId(const std::string& installation_id) {
    this->inner_.installation_id = installation_id;
    return *this;
  }
  TagArgsBuilder& WithBrandCode(const std::string& brand_code) {
    this->inner_.brand_code = brand_code;
    return *this;
  }
  TagArgsBuilder& WithClientId(const std::string& client_id) {
    this->inner_.client_id = client_id;
    return *this;
  }
  TagArgsBuilder& WithExperimentLabels(const std::string& experiment_labels) {
    this->inner_.experiment_labels = experiment_labels;
    return *this;
  }
  TagArgsBuilder& WithReferralId(const std::string& referral_id) {
    this->inner_.referral_id = referral_id;
    return *this;
  }
  TagArgsBuilder& WithLanguage(const std::string& language) {
    this->inner_.language = language;
    return *this;
  }
  TagArgsBuilder& WithFlighting(bool flighting) {
    this->inner_.flighting = flighting;
    return *this;
  }
  TagArgsBuilder& WithBrowserType(TagArgs::BrowserType browser_type) {
    this->inner_.browser_type = browser_type;
    return *this;
  }
  TagArgsBuilder& WithUsageStatsEnable(bool usage_stats_enable) {
    this->inner_.usage_stats_enable = usage_stats_enable;
    return *this;
  }
  TagArgsBuilder& WithApp(AppArgs app) {
    this->inner_.apps.push_back(std::move(app));
    return *this;
  }

 private:
  TagArgs inner_;
};

// Builder pattern helper to construct the AppArgs struct.
class AppArgsBuilder {
 public:
  explicit AppArgsBuilder(const std::string& app_id) : inner_(app_id) {}

  AppArgs Build() { return std::move(inner_); }

  AppArgsBuilder& WithAppName(const std::string& app_name) {
    this->inner_.app_name = app_name;
    return *this;
  }
  AppArgsBuilder& WithNeedsAdmin(AppArgs::NeedsAdmin needs_admin) {
    this->inner_.needs_admin = needs_admin;
    return *this;
  }
  AppArgsBuilder& WithAp(const std::string& ap) {
    this->inner_.ap = ap;
    return *this;
  }
  AppArgsBuilder& WithEncodedInstallerData(
      const std::string& encoded_installer_data) {
    this->inner_.encoded_installer_data = encoded_installer_data;
    return *this;
  }
  AppArgsBuilder& WithInstallDataIndex(const std::string& install_data_index) {
    this->inner_.install_data_index = install_data_index;
    return *this;
  }
  AppArgsBuilder& WithExperimentLabels(const std::string& experiment_labels) {
    this->inner_.experiment_labels = experiment_labels;
    return *this;
  }
  AppArgsBuilder& WithUntrustedData(const std::string& untrusted_data) {
    this->inner_.untrusted_data = untrusted_data;
    return *this;
  }

 private:
  AppArgs inner_;
};

void VerifyTagParseSuccess(
    base::StringPiece tag,
    absl::optional<base::StringPiece> app_installer_data_args,
    const TagArgs& expected) {
  TagArgs actual;
  ASSERT_EQ(ErrorCode::kSuccess, Parse(tag, app_installer_data_args, &actual));

  EXPECT_EQ(expected.bundle_name, actual.bundle_name);
  EXPECT_EQ(expected.installation_id, actual.installation_id);
  EXPECT_EQ(expected.brand_code, actual.brand_code);
  EXPECT_EQ(expected.client_id, actual.client_id);
  EXPECT_EQ(expected.experiment_labels, actual.experiment_labels);
  EXPECT_EQ(expected.referral_id, actual.referral_id);
  EXPECT_EQ(expected.language, actual.language);
  EXPECT_EQ(expected.browser_type, actual.browser_type);
  EXPECT_EQ(expected.usage_stats_enable, actual.usage_stats_enable);

  EXPECT_EQ(expected.apps.size(), actual.apps.size());
  for (size_t i = 0; i < actual.apps.size(); ++i) {
    const AppArgs& app_expected = expected.apps[i];
    const AppArgs& app_actual = actual.apps[i];

    EXPECT_EQ(app_expected.app_id, app_actual.app_id);
    EXPECT_EQ(app_expected.app_name, app_actual.app_name);
    EXPECT_EQ(app_expected.needs_admin, app_actual.needs_admin);
    EXPECT_EQ(app_expected.ap, app_actual.ap);
    EXPECT_EQ(app_expected.encoded_installer_data,
              app_actual.encoded_installer_data);
    EXPECT_EQ(app_expected.install_data_index, app_actual.install_data_index);
    EXPECT_EQ(app_expected.experiment_labels, app_actual.experiment_labels);
  }
}

void VerifyTagParseFail(
    base::StringPiece tag,
    absl::optional<base::StringPiece> app_installer_data_args,
    ErrorCode expected) {
  TagArgs args;
  ASSERT_EQ(expected, Parse(tag, app_installer_data_args, &args));
}

}  // namespace

namespace updater {

using tagging::AppArgs;
using tagging::ErrorCode;
using tagging::Parse;
using tagging::TagArgs;

TEST(TagParserTest, InvalidValueNameIsSupersetOfValidName) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname1=Hello",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, AppNameSpaceForValue) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= ",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, AppNameEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%20",
      absl::nullopt, ErrorCode::kApp_AppNameCannotBeWhitespace);
}

TEST(TagParserTest, AppNameValid) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Test",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName("Test")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("Test")
                       .Build())
          .Build());
}

// This must work because the enterprise MSI code assumes spaces are allowed.
TEST(TagParserTest, AppNameWithSpace) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=Test App",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName("Test App")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("Test App")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithSpaceAtEnd) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= T Ap p ",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithEncodedSpacesAtEnd) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%20T%20Ap%20p%20",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameWithMultipleSpaces) {
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname= T Ap p",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName("T Ap p")
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName("T Ap p")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameUnicode) {
  std::string non_ascii_name = "रहा";
  VerifyTagParseSuccess(
      "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&"
      "appname=%E0%A4%B0%E0%A4%B9%E0%A4%BE",
      absl::nullopt,
      TagArgsBuilder()
          .WithBundleName(non_ascii_name)
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName(non_ascii_name)
                       .Build())
          .Build());
}

TEST(TagParserTest, AppNameUnicode2) {
  std::string non_ascii_name = "स्थापित कर रहा है।";
  std::string escaped(
      "%E0%A4%B8%E0%A5%8D%E0%A4%A5%E0%A4%BE%E0%A4%AA%E0%A4%BF"
      "%E0%A4%A4%20%E0%A4%95%E0%A4%B0%20%E0%A4%B0%E0%A4%B9%E0"
      "%A4%BE%20%E0%A4%B9%E0%A5%88%E0%A5%A4");

  std::stringstream tag;
  tag << "appguid=D0324988-DA8A-49e5-BCE5-925FCD04EAB7&";
  tag << "appname=" << escaped;
  VerifyTagParseSuccess(
      tag.str(), absl::nullopt,
      TagArgsBuilder()
          .WithBundleName(non_ascii_name)
          .WithApp(AppArgsBuilder("d0324988-da8a-49e5-bce5-925fcd04eab7")
                       .WithAppName(non_ascii_name)
                       .Build())
          .Build());
}

TEST(TagParserTest, AppIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B", absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, AppIdNotASCII) {
  VerifyTagParseFail("appguid=रहा", absl::nullopt,
                     ErrorCode::kApp_AppIdIsNotValid);
}

// Most tests here do not reflect this, but appids can be non-GUID ASCII
// strings.
TEST(TagParserTest, AppIdNotAGuid) {
  VerifyTagParseSuccess(
      "appguid=non-guid-id", absl::nullopt,
      TagArgsBuilder().WithApp(AppArgsBuilder("non-guid-id").Build()).Build());
}

TEST(TagParserTest, AppIdCaseInsensitive) {
  VerifyTagParseSuccess(
      "appguid=ShouldBeCaseInsensitive", absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("shouldbecaseinsensitive").Build())
          .Build());
}

TEST(TagParserTest, NeedsAdminInvalid) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=Hello",
      absl::nullopt, ErrorCode::kApp_NeedsAdminValueIsInvalid);
}

TEST(TagParserTest, NeedsAdminSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin= ",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NeedsAdminTrueUpperCaseT) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=True",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kYes)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsAdminTrueLowerCaseT) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=true",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kYes)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsFalseUpperCaseF) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=False",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kNo)
                       .Build())
          .Build());
}

TEST(TagParserTest, NeedsAdminFalseLowerCaseF) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "needsadmin=false",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kNo)
                       .Build())
          .Build());
}

//
// Test the handling of the contents of the extra arguments.
//

TEST(TagParserTest, AssignmentOnly) {
  VerifyTagParseFail("=", absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment1) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1=",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, ExtraAssignment2) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=usagestats=1",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment3) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&=",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ExtraAssignment4) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=&usagestats=1",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, ValueWithoutName) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "=hello",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

// Also tests ending extra arguments with '='.
TEST(TagParserTest, NameWithoutValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NameWithoutValueBeforeNextArgument) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=&client=hello",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, NameWithoutArgumentSeparatorAfterIntValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1client=hello",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, NameWithoutArgumentSeparatorAfterStringValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=yesclient=hello",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, TagHasDoubleAmpersand) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&&client=hello",
      {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=foobar"},
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithEncodedInstallerData("foobar")
                       .Build())
          .WithUsageStatsEnable(true)
          .WithClientId("hello")
          .Build());
}

TEST(TagParserTest, TagAmpersandOnly) {
  VerifyTagParseSuccess("&", absl::nullopt, TagArgsBuilder().Build());
}

TEST(TagParserTest, TagBeginsInAmpersand) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, TagEndsInAmpersand) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhitespaceOnly) {
  for (const auto* whitespace : {"", " ", "\t", "\r", "\n", "\r\n"}) {
    VerifyTagParseSuccess(whitespace, absl::nullopt, TagArgsBuilder().Build());
  }
}

//
// Test the parsing of the extra command and its arguments into a string.
//

TEST(TagParserTest, OneValidAttribute) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, TwoValidAttributes) {
  VerifyTagParseSuccess(
      "&appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1&client=hello",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .WithClientId("hello")
          .Build());
}

TEST(TagParserTest, TagHasSwitchInTheMiddle) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1/other_value=9",
      absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasDoubleQuoteInTheMiddle) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\"/other_value=9",
      absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasDoubleQuoteInTheMiddleAndNoForwardSlash) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\"other_value=9",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, TagHasSpaceAndForwardSlashBeforeQuote) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1 /other_value=9",
      absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, TagHasForwardSlashBeforeQuote) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1/other_value=9",
      absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, AttributeSpecifiedTwice) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1\" \"client=10",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, WhiteSpaceBeforeArgs1) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      " usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs2) {
  VerifyTagParseSuccess(
      "\tappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs3) {
  VerifyTagParseSuccess(
      "\rappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs4) {
  VerifyTagParseSuccess(
      "\nappguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B"
      "&usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, WhiteSpaceBeforeArgs5) {
  VerifyTagParseSuccess("\r\nusagestats=1", absl::nullopt,
                        TagArgsBuilder().WithUsageStatsEnable(true).Build());
}

TEST(TagParserTest, ForwardSlash1) {
  VerifyTagParseFail("/", absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, ForwardSlash2) {
  VerifyTagParseFail("/ appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, BackwardSlash1) {
  VerifyTagParseFail("\\", absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, BackwardSlash2) {
  VerifyTagParseFail("\\appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, BackwardSlash3) {
  VerifyTagParseFail("\\ appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B",
                     absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, AppArgsMustHaveValue) {
  for (const auto* tag :
       {"appguid", "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&ap",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&experiments",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&appname",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&needsadmin",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&installdataindex",
        "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&untrusteddata"}) {
    VerifyTagParseFail(tag, absl::nullopt, ErrorCode::kAttributeMustHaveValue);
  }
}

//
// Test specific extra commands.
//

TEST(TagParserTest, UsageStatsOutsideExtraCommand) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "/usagestats",
      absl::nullopt, ErrorCode::kTagIsInvalid);
}

TEST(TagParserTest, UsageStatsOn) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=1",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(true)
          .Build());
}

TEST(TagParserTest, UsageStatsOff) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=0",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithUsageStatsEnable(false)
          .Build());
}

TEST(TagParserTest, UsageStatsNone) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=2",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, UsageStatsInvalidPositiveValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=3",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, UsageStatsInvalidNegativeValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=-1",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, UsageStatsValueIsString) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "usagestats=true",
      absl::nullopt, ErrorCode::kGlobal_UsageStatsValueIsInvalid);
}

TEST(TagParserTest, BundleNameValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename=Google%20Bundle",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBundleName("Google Bundle")

          .Build());
}

TEST(TagParserTest, BundleNameSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename= ",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, BundleNameEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "bundlename=%20",
      absl::nullopt, ErrorCode::kGlobal_BundleNameCannotBeWhitespace);
}

TEST(TagParserTest, BundleNameNotPresentButAppNameIs) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=Google%20Chrome",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAppName("Google Chrome")
                       .Build())
          .WithBundleName("Google Chrome")
          .Build());
}
TEST(TagParserTest, BundleNameNorAppNamePresent) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&", absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .Build());
}

TEST(TagParserTest, BundleNameNotPresentAndNoApp) {
  VerifyTagParseSuccess(
      "browser=0", absl::nullopt,
      TagArgsBuilder().WithBrowserType(TagArgs::BrowserType::kUnknown).Build());
}

TEST(TagParserTest, InstallationIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithInstallationId("98CEC468-9429-4984-AEDE-4F53C6A14869")
          .Build());
}

TEST(TagParserTest, InstallationIdContainsNonASCII) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "iid=रहा",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithInstallationId("रहा")
          .Build());
}

TEST(TagParserTest, BrandCodeValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "brand=GOOG",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrandCode("GOOG")
          .Build());
}

TEST(TagParserTest, ClientIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "client=some_partner",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithClientId("some_partner")
          .Build());
}

TEST(TagParserTest, UpdaterExperimentIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments=experiment%3DgroupA%7Cexpir",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithExperimentLabels("experiment=groupA|expir")
          .Build());
}

TEST(TagParserTest, UpdaterExperimentIdSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments= ",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, UpdaterExperimentIdEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "omahaexperiments=%20",
      absl::nullopt, ErrorCode::kGlobal_ExperimentLabelsCannotBeWhitespace);
}

TEST(TagParserTest, AppExperimentIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments=experiment%3DgroupA%7Cexpir",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithExperimentLabels("experiment=groupA|expir")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppExperimentIdSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments= ",
      absl::nullopt, ErrorCode::kAttributeMustHaveValue);
}

TEST(TagParserTest, AppExperimentIdEncodedSpaceForValue) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "experiments=%20",
      absl::nullopt, ErrorCode::kApp_ExperimentLabelsCannotBeWhitespace);
}

TEST(TagParserTest, ReferralIdValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "referral=ABCD123",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithReferralId("ABCD123")
          .Build());
}

TEST(TagParserTest, ApValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "ap=developer",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAp("developer")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppInstallerDataArgsValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
      {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=%E0%A4foobar"},
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithEncodedInstallerData("%E0%A4foobar")
                       .Build())
          .Build());
}

TEST(TagParserTest, AppInstallerDataArgsInvalidAppId) {
  VerifyTagParseFail("appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
                     {"appguid=E135384F-85A2-4328-B07D-2CF70313D505&"
                      "installerdata=%E0%A4foobar"},
                     ErrorCode::kAppInstallerData_AppIdNotFound);
}

TEST(TagParserTest, AppInstallerDataArgsInvalidAttribute) {
  VerifyTagParseFail("appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&",
                     {"appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
                      "needsadmin=true&"},
                     ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, InstallerDataNotAllowedInTag) {
  VerifyTagParseFail(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp2&"
      "needsadmin=true&"
      "installerdata=Hello%20World",
      absl::nullopt, ErrorCode::kUnrecognizedName);
}

TEST(TagParserTest, InstallDataIndexValid) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "installdataindex=foobar",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithInstallDataIndex("foobar")
                       .Build())
          .Build());
}

TEST(TagParserTest, BrowserTypeValid) {
  std::tuple<base::StringPiece, TagArgs::BrowserType>
      pairs[static_cast<int>(TagArgs::BrowserType::kMax)] = {
          {"0", TagArgs::BrowserType::kUnknown},
          {"1", TagArgs::BrowserType::kDefault},
          {"2", TagArgs::BrowserType::kInternetExplorer},
          {"3", TagArgs::BrowserType::kFirefox},
          {"4", TagArgs::BrowserType::kChrome},
      };

  for (const auto& pair : pairs) {
    std::stringstream tag;
    tag << "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&";
    tag << "browser=" << std::get<0>(pair);
    VerifyTagParseSuccess(
        tag.str(), absl::nullopt,
        TagArgsBuilder()
            .WithApp(
                AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
            .WithBrowserType(std::get<1>(pair))
            .Build());
  }
}

TEST(TagParserTest, BrowserTypeInvalid) {
  EXPECT_EQ(5, int(TagArgs::BrowserType::kMax))
      << "Browser type may have been added. Update the BrowserTypeValid test "
         "and change browser values in tag strings below.";

  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "browser=5",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrowserType(TagArgs::BrowserType::kUnknown)
          .Build());

  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "browser=9",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithBrowserType(TagArgs::BrowserType::kUnknown)
          .Build());
}

TEST(TagParserTest, ValidLang) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "lang=en",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithLanguage("en")
          .Build());
}

// Language must be passed even if not supported. See http://b/1336966.
TEST(TagParserTest, UnsupportedLang) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "lang=foobar",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(
              AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b").Build())
          .WithLanguage("foobar")
          .Build());
}

TEST(TagParserTest, AppNameSpecifiedTwice) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "appname=TestApp2&",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B")
                       .WithAppName("TestApp2")
                       .Build())
          .WithBundleName("TestApp2")
          .Build());
}

TEST(TagParserTest, CaseInsensitiveAttributeNames) {
  VerifyTagParseSuccess(
      "APPguID=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "APPNAME=TestApp&",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("8617EE50-F91C-4DC1-B937-0969EEF59B0B")
                       .WithAppName("TestApp")
                       .Build())
          .WithBundleName("TestApp")
          .Build());
}

TEST(TagParserTest, BracesEncoding) {
  VerifyTagParseSuccess(
      "appguid=%7B8617EE50-F91C-4DC1-B937-0969EEF59B0B%7D&"
      "appname=TestApp&",
      absl::nullopt,
      TagArgsBuilder()
          .WithApp(AppArgsBuilder("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}")
                       .WithAppName("TestApp")
                       .Build())
          .WithBundleName("TestApp")
          .Build());
}

//
// Test multiple applications in the extra arguments
//
TEST(TagParserTestMultipleEntries, TestNotStartingWithAppId) {
  VerifyTagParseFail(
      "appname=TestApp&"
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "appname=false&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap&"
      "usagestats=1&"
      "browser=2&",
      absl::nullopt, ErrorCode::kApp_AppIdNotSpecified);
}

// This also tests that the last occurrence of a global extra arg is the one
// that is saved.
TEST(TagParserTestMultipleEntries, ThreeApplications) {
  VerifyTagParseSuccess(
      "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
      "appname=TestApp&"
      "needsadmin=false&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap&"
      "usagestats=1&"
      "browser=2&"
      "brand=GOOG&"
      "client=_some_client&"
      "experiments=_experiment_a&"
      "untrusteddata=ABCDEFG&"
      "referral=A123456789&"
      "appguid=5E46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp2&"
      "needsadmin=true&"
      "experiments=_experiment_b&"
      "untrusteddata=1234567&"
      "iid=98CEC468-9429-4984-AEDE-4F53C6A14869&"
      "ap=test_ap2&"
      "usagestats=0&"
      "browser=3&"
      "brand=g00g&"
      "client=_different_client&"
      "appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
      "appname=TestApp3&"
      "needsadmin=prefers&",
      {"appguid=5F46DE36-737D-4271-91C1-C062F9FE21D9&"
       "installerdata=installerdata_app3&"
       "appguid=8617EE50-F91C-4DC1-B937-0969EEF59B0B&"
       "installerdata=installerdata_app1"},
      TagArgsBuilder()
          .WithBundleName("TestApp")
          .WithInstallationId("98CEC468-9429-4984-AEDE-4F53C6A14869")
          .WithBrandCode("g00g")
          .WithClientId("_different_client")
          .WithReferralId("A123456789")
          .WithBrowserType(TagArgs::BrowserType::kFirefox)
          .WithUsageStatsEnable(false)
          .WithApp(AppArgsBuilder("8617ee50-f91c-4dc1-b937-0969eef59b0b")
                       .WithAppName("TestApp")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kNo)
                       .WithAp("test_ap")
                       .WithEncodedInstallerData("installerdata_app1")
                       .WithExperimentLabels("_experiment_a")
                       .WithUntrustedData("A=3&Message=Hello,%20World!")
                       .Build())
          .WithApp(AppArgsBuilder("5e46de36-737d-4271-91c1-c062f9fe21d9")
                       .WithAppName("TestApp2")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kYes)
                       .WithAp("test_ap2")
                       .WithExperimentLabels("_experiment_b")
                       .WithUntrustedData("X=5")
                       .Build())
          .WithApp(AppArgsBuilder("5f46de36-737d-4271-91c1-c062f9fe21d9")
                       .WithAppName("TestApp3")
                       .WithEncodedInstallerData("installerdata_app3")
                       .WithNeedsAdmin(AppArgs::NeedsAdmin::kPrefers)
                       .Build())
          .Build());
}

}  // namespace updater
