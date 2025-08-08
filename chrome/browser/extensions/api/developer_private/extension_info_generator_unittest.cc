// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "components/supervised_user/core/common/features.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace extensions {

using mojom::ManifestLocation;

namespace developer = api::developer_private;

namespace {

const char kAllHostsPermission[] = "*://*/*";

absl::optional<base::Value::Dict> DeserializeJSONTestData(
    const base::FilePath& path,
    std::string* error) {
  JSONFileValueDeserializer deserializer(path);
  std::unique_ptr<base::Value> value = deserializer.Deserialize(nullptr, error);
  if (!value || !value->is_dict()) {
    return absl::nullopt;
  }
  return std::move(*value).TakeDict();
}

// Returns a pointer to the ExtensionInfo for an extension with |id| if it
// is present in |list|.
const developer::ExtensionInfo* GetInfoFromList(
    const ExtensionInfoGenerator::ExtensionInfoList& list,
    const std::string& id) {
  for (const auto& item : list) {
    if (item.id == id)
      return &item;
  }
  return nullptr;
}

// Converts the SiteControls hosts list to a JSON string. This makes test
// validation considerably more concise and readable.
std::string SiteControlsToString(
    const std::vector<developer::SiteControl>& controls) {
  base::Value::List list;
  for (const auto& control : controls) {
    list.Append(control.ToValue());
  }

  std::string json;
  CHECK(base::JSONWriter::Write(list, &json));
  return json;
}

}  // namespace

class ExtensionInfoGeneratorUnitTest : public ExtensionServiceTestWithInstall {
 public:
  ExtensionInfoGeneratorUnitTest() {}

  ExtensionInfoGeneratorUnitTest(const ExtensionInfoGeneratorUnitTest&) =
      delete;
  ExtensionInfoGeneratorUnitTest& operator=(
      const ExtensionInfoGeneratorUnitTest&) = delete;

  ~ExtensionInfoGeneratorUnitTest() override {}

 protected:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeExtensionService(GetExtensionServiceInitParams());
    extension_action_test_util::CreateToolbarModelForProfile(profile());
  }

  // Returns the initialization parameters for the extension service.
  virtual ExtensionServiceInitParams GetExtensionServiceInitParams() {
    return {};
  }

  void OnInfoGenerated(std::unique_ptr<developer::ExtensionInfo>* info_out,
                       ExtensionInfoGenerator::ExtensionInfoList list) {
    EXPECT_EQ(1u, list.size());
    if (!list.empty()) {
      *info_out =
          std::make_unique<developer::ExtensionInfo>(std::move(list[0]));
    }
    std::move(quit_closure_).Run();
  }

  std::unique_ptr<developer::ExtensionInfo> GenerateExtensionInfo(
      const std::string& extension_id) {
    std::unique_ptr<developer::ExtensionInfo> info;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    std::unique_ptr<ExtensionInfoGenerator> generator(
        new ExtensionInfoGenerator(browser_context()));
    generator->CreateExtensionInfo(
        extension_id,
        base::BindOnce(&ExtensionInfoGeneratorUnitTest::OnInfoGenerated,
                       base::Unretained(this), base::Unretained(&info)));
    run_loop.Run();
    return info;
  }

  void OnInfosGenerated(ExtensionInfoGenerator::ExtensionInfoList* out,
                        ExtensionInfoGenerator::ExtensionInfoList list) {
    *out = std::move(list);
    std::move(quit_closure_).Run();
  }

  ExtensionInfoGenerator::ExtensionInfoList GenerateExtensionsInfo() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    ExtensionInfoGenerator generator(browser_context());
    ExtensionInfoGenerator::ExtensionInfoList result;
    generator.CreateExtensionsInfo(
        true, /* include_disabled */
        true, /* include_terminated */
        base::BindOnce(&ExtensionInfoGeneratorUnitTest::OnInfosGenerated,
                       base::Unretained(this), base::Unretained(&result)));
    run_loop.Run();
    return result;
  }

  const scoped_refptr<const Extension> CreateExtension(
      const std::string& name,
      base::Value::List permissions,
      mojom::ManifestLocation location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(base::Value::Dict()
                             .Set("name", name)
                             .Set("description", "an extension")
                             .Set("manifest_version", 2)
                             .Set("version", "1.0.0")
                             .Set("permissions", std::move(permissions)))
            .SetLocation(location)
            .SetID(kId)
            .Build();

    service()->AddExtension(extension.get());
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());

    return extension;
  }

  std::unique_ptr<developer::ExtensionInfo> CreateExtensionInfoFromPath(
      const base::FilePath& extension_path,
      mojom::ManifestLocation location) {
    ChromeTestExtensionLoader loader(browser_context());

    // Unit tests are single process and as such, attempting to wait for an
    // extension renderer process will cause the test to time out.
    loader.set_wait_for_renderers(false);
    loader.set_location(location);
    loader.set_creation_flags(Extension::REQUIRE_KEY);
    scoped_refptr<const Extension> extension =
        loader.LoadExtension(extension_path);
    CHECK(extension.get());

    return GenerateExtensionInfo(extension->id());
  }

  void CompareExpectedAndActualOutput(
      const base::FilePath& extension_path,
      InspectableViewsFinder::ViewList views,
      const base::FilePath& expected_output_path) {
    std::string error;
    absl::optional<base::Value::Dict> expected_output_data =
        DeserializeJSONTestData(expected_output_path, &error);
    ASSERT_TRUE(expected_output_data);
    EXPECT_EQ(std::string(), error);

    // Produce test output.
    std::unique_ptr<developer::ExtensionInfo> info =
        CreateExtensionInfoFromPath(extension_path,
                                    mojom::ManifestLocation::kUnpacked);
    info->views = std::move(views);
    base::Value::Dict actual_output_data = info->ToValue();

    // Compare the outputs.
    // Ignore unknown fields in the actual output data.
    std::string paths_details = " - expected (" +
        expected_output_path.MaybeAsASCII() + ") vs. actual (" +
        extension_path.MaybeAsASCII() + ")";
    std::string expected_string;
    std::string actual_string;
    for (auto field : *expected_output_data) {
      const base::Value& expected_value = field.second;
      base::Value* actual_value =
          actual_output_data.FindByDottedPath(field.first);
      EXPECT_TRUE(actual_value) << field.first + " is missing" + paths_details;
      if (!actual_value)
        continue;
      if (*actual_value != expected_value) {
        base::JSONWriter::Write(expected_value, &expected_string);
        base::JSONWriter::Write(*actual_value, &actual_string);
        EXPECT_EQ(expected_string, actual_string)
            << field.first << paths_details;
      }
    }
  }

 private:
  base::OnceClosure quit_closure_;
};

// Test some of the basic fields.
TEST_F(ExtensionInfoGeneratorUnitTest, BasicInfoTest) {
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId("alpha");
  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", kName)
          .Set("version", kVersion)
          .Set("manifest_version", 3)
          .Set("description", "an extension")
          .Set("host_permissions", base::Value::List()
                                       .Append("file://*/*")
                                       .Append("*://*.google.com/*")
                                       .Append("*://*.example.com/*")
                                       .Append("*://*.foo.bar/*")
                                       .Append("*://*.chromium.org/*"))
          .Set("permissions", base::Value::List().Append("tabs"));
  base::Value::Dict manifest_copy = manifest.Clone();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(ManifestLocation::kUnpacked)
          .SetPath(data_dir())
          .SetID(id)
          .Build();
  service()->AddExtension(extension.get());
  ErrorConsole* error_console = ErrorConsole::Get(profile());
  const GURL kContextUrl("http://example.com");
  error_console->ReportError(std::make_unique<RuntimeError>(
      extension->id(), false, u"source", u"message",
      StackTrace(1, StackFrame(1, 1, u"source", u"function")), kContextUrl,
      logging::LOGGING_ERROR, 1, 1));
  error_console->ReportError(std::make_unique<ManifestError>(
      extension->id(), u"message", "key", std::u16string()));
  error_console->ReportError(std::make_unique<RuntimeError>(
      extension->id(), false, u"source", u"message",
      StackTrace(1, StackFrame(1, 1, u"source", u"function")), kContextUrl,
      logging::LOGGING_WARNING, 1, 1));

  // It's not feasible to validate every field here, because that would be
  // a duplication of the logic in the method itself. Instead, test a handful
  // of fields for sanity.
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info.get());
  EXPECT_EQ(kName, info->name);
  EXPECT_EQ(id, info->id);
  EXPECT_EQ(kVersion, info->version);
  EXPECT_EQ(info->location, developer::LOCATION_UNPACKED);
  ASSERT_TRUE(info->path);
  EXPECT_EQ(data_dir(), base::FilePath::FromUTF8Unsafe(*info->path));
  EXPECT_EQ(api::developer_private::EXTENSION_STATE_ENABLED, info->state);
  EXPECT_EQ(api::developer_private::EXTENSION_TYPE_EXTENSION, info->type);
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
  EXPECT_TRUE(info->incognito_access.is_enabled);
  EXPECT_FALSE(info->incognito_access.is_active);
  EXPECT_TRUE(base::StartsWith(info->icon_url, "data:image/png;base64,"));
  EXPECT_FALSE(*info->pinned_to_toolbar);

  // Strip out the kHostReadWrite permission created by the extension requesting
  // host permissions above; runtime host permissions mean these are always
  // present but not necessarily operative. There should only be one entry,
  // though. This is necessary because the code below wants to assert that every
  // entry in |messages| has a matching entry in
  // |info->permissions.simple_permissions|, and kHostReadWrite is not a simple
  // permission.
  PermissionMessages messages;
  for (const PermissionMessage& message :
       extension->permissions_data()->GetPermissionMessages()) {
    if (!message.permissions().ContainsID(
            extensions::mojom::APIPermissionID::kHostReadWrite)) {
      messages.push_back(message);
    }
  }

  ASSERT_EQ(messages.size(), info->permissions.simple_permissions.size());
  size_t i = 0;
  for (const PermissionMessage& message : messages) {
    const api::developer_private::Permission& info_permission =
        info->permissions.simple_permissions[i];
    EXPECT_EQ(message.message(), base::UTF8ToUTF16(info_permission.message));
    const std::vector<std::u16string>& submessages = message.submessages();
    ASSERT_EQ(submessages.size(), info_permission.submessages.size());
    for (size_t j = 0; j < submessages.size(); ++j) {
      EXPECT_EQ(submessages[j],
                base::UTF8ToUTF16(info_permission.submessages[j]));
    }
    ++i;
  }
  EXPECT_TRUE(info->permissions.runtime_host_permissions);
  EXPECT_TRUE(info->permissions.can_access_site_data);

  ASSERT_EQ(2u, info->runtime_errors.size());
  const api::developer_private::RuntimeError& runtime_error =
      info->runtime_errors[0];
  EXPECT_EQ(extension->id(), runtime_error.extension_id);
  EXPECT_EQ(api::developer_private::ERROR_TYPE_RUNTIME, runtime_error.type);
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_ERROR,
            runtime_error.severity);
  EXPECT_EQ(kContextUrl, GURL(runtime_error.context_url));
  EXPECT_EQ(1u, runtime_error.stack_trace.size());
  ASSERT_EQ(1u, info->manifest_errors.size());
  const api::developer_private::RuntimeError& runtime_error_verbose =
      info->runtime_errors[1];
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_WARN,
            runtime_error_verbose.severity);
  const api::developer_private::ManifestError& manifest_error =
      info->manifest_errors[0];
  EXPECT_EQ(extension->id(), manifest_error.extension_id);

  // Test an extension that isn't unpacked.
  manifest_copy.Set("update_url",
                    "https://clients2.google.com/service/update2/crx");
  id = crx_file::id_util::GenerateId("beta");
  extension = ExtensionBuilder()
                  .SetManifest(std::move(manifest_copy))
                  .SetLocation(ManifestLocation::kExternalPref)
                  .SetID(id)
                  .Build();
  service()->AddExtension(extension.get());
  info = GenerateExtensionInfo(extension->id());
  EXPECT_EQ(developer::LOCATION_THIRD_PARTY, info->location);
  EXPECT_FALSE(info->path);
}

// Tests that the correct location field is returned for an extension that's
// installed by default.
TEST_F(ExtensionInfoGeneratorUnitTest, ExtensionInfoInstalledByDefault) {
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", "installed by default")
          .Set("version", "1.2")
          .Set("manifest_version", 3)
          .Set("update_url", "https://clients2.google.com/service/update2/crx");

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(ManifestLocation::kExternalPref)
          .SetPath(data_dir())
          .SetID(crx_file::id_util::GenerateId("alpha"))
          .AddFlags(Extension::WAS_INSTALLED_BY_DEFAULT)
          .Build();
  service()->AddExtension(extension.get());

  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  EXPECT_EQ(info->location, developer::LOCATION_INSTALLED_BY_DEFAULT);
}

// Tests that the correct location field is returned for an extension that's
// installed by the OEM.
TEST_F(ExtensionInfoGeneratorUnitTest, ExtensionInfoInstalledByOem) {
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  base::Value::Dict manifest =
      base::Value::Dict()
          .Set("name", "installed by OEM")
          .Set("version", "1.2")
          .Set("manifest_version", 3)
          .Set("update_url", "https://clients2.google.com/service/update2/crx");

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(ManifestLocation::kExternalPref)
          .SetPath(data_dir())
          .SetID(crx_file::id_util::GenerateId("alpha"))
          .AddFlags(Extension::WAS_INSTALLED_BY_DEFAULT |
                    Extension::WAS_INSTALLED_BY_OEM)
          .Build();
  service()->AddExtension(extension.get());

  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  EXPECT_EQ(info->location, developer::LOCATION_THIRD_PARTY);
}

// Test three generated json outputs.
TEST_F(ExtensionInfoGeneratorUnitTest, GenerateExtensionsJSONData) {
  // Test Extension1
  base::FilePath extension_path =
      data_dir().AppendASCII("good")
                .AppendASCII("Extensions")
                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                .AppendASCII("1.0.0.0");

  base::FilePath expected_outputs_path =
      data_dir().AppendASCII("api_test")
                .AppendASCII("developer")
                .AppendASCII("generated_output");

  {
    InspectableViewsFinder::ViewList views;
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/bar.html"),
        42, 88, true, false, api::developer_private::VIEW_TYPE_TAB_CONTENTS));
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/dog.html"), 0,
        0, false, true, api::developer_private::VIEW_TYPE_TAB_CONTENTS));

    CompareExpectedAndActualOutput(
        extension_path, std::move(views),
        expected_outputs_path.AppendASCII(
            "behllobkkfkfnphdnhnkndlbkcpglgmj.json"));
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Test Extension2
  extension_path = data_dir()
                       .AppendASCII("good")
                       .AppendASCII("Extensions")
                       .AppendASCII("hpiknbiabeeppbpihjehijgoemciehgk")
                       .AppendASCII("2");

  {
    // It's OK to have duplicate URLs, so long as the IDs are different.
    InspectableViewsFinder::ViewList views;
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://hpiknbiabeeppbpihjehijgoemciehgk/bar.html"),
        42, 88, true, false, api::developer_private::VIEW_TYPE_TAB_CONTENTS));
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://hpiknbiabeeppbpihjehijgoemciehgk/bar.html"), 0,
        0, false, true, api::developer_private::VIEW_TYPE_TAB_CONTENTS));

    CompareExpectedAndActualOutput(
        extension_path, std::move(views),
        expected_outputs_path.AppendASCII(
            "hpiknbiabeeppbpihjehijgoemciehgk.json"));
  }
#endif

  // Test Extension3
  extension_path = data_dir().AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                             .AppendASCII("1.0");
  CompareExpectedAndActualOutput(extension_path,
                                 InspectableViewsFinder::ViewList(),
                                 expected_outputs_path.AppendASCII(
                                     "bjafgdebaacbbbecmhlhpofkepfkgcpa.json"));
}

// Test the safety check display strings
TEST_F(ExtensionInfoGeneratorUnitTest, SafetyCheckStringsTest) {
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kMalware;
    cws_info.unpublished_long_ago = true;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_DISABLED,
            BitMapBlocklistState::BLOCKLISTED_MALWARE);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFETY_CHECK_EXTENSIONS_MALWARE),
              display_strings.detail_string);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_MALWARE),
              display_strings.panel_string);
  }
  {
    // Check edge case where CWSViolationType and BitMapBlocklistState differ.
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kNone;
    cws_info.unpublished_long_ago = true;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_DISABLED,
            BitMapBlocklistState::BLOCKLISTED_MALWARE);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFETY_CHECK_EXTENSIONS_MALWARE),
              display_strings.detail_string);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_MALWARE),
              display_strings.panel_string);
  }
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kNone;
    cws_info.unpublished_long_ago = true;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_DISABLED,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION),
        display_strings.detail_string);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF),
              display_strings.panel_string);
  }
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kPolicy;
    cws_info.unpublished_long_ago = true;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_DISABLED,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION),
        display_strings.detail_string);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF),
              display_strings.panel_string);
  }
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kPolicy;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_ENABLED,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON),
              display_strings.panel_string);
  }
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kPolicy;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_ENABLED,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON),
              display_strings.panel_string);
  }
  {
    CWSInfoService::CWSInfo cws_info;
    cws_info.is_present = true;
    cws_info.violation_type = CWSInfoService::CWSViolationType::kNone;
    cws_info.unpublished_long_ago = true;
    developer::SafetyCheckStrings display_strings =
        ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
            cws_info, developer::EXTENSION_STATE_DISABLED,
            BitMapBlocklistState::NOT_BLOCKLISTED);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFETY_CHECK_EXTENSIONS_UNPUBLISHED),
              display_strings.detail_string);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_UNPUBLISHED_OFF),
              display_strings.panel_string);
    display_strings = ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
        cws_info, developer::EXTENSION_STATE_ENABLED,
        BitMapBlocklistState::NOT_BLOCKLISTED);
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSIONS_SC_UNPUBLISHED_ON),
              display_strings.panel_string);
  }
}

TEST_F(ExtensionInfoGeneratorUnitTest, SafetyCheckEmptyStringTest) {
  CWSInfoService::CWSInfo cws_info;
  cws_info.is_present = false;
  cws_info.violation_type = CWSInfoService::CWSViolationType::kNone;
  cws_info.unpublished_long_ago = false;
  developer::SafetyCheckStrings display_strings;
  display_strings = ExtensionInfoGenerator::CreateSafetyCheckDisplayString(
      cws_info, developer::EXTENSION_STATE_DISABLED,
      BitMapBlocklistState::NOT_BLOCKLISTED);
  EXPECT_EQ(display_strings.detail_string, "");
  EXPECT_EQ(display_strings.panel_string, "");
}

// Tests the generation of the runtime host permissions entries.
TEST_F(ExtensionInfoGeneratorUnitTest, RuntimeHostPermissions) {
  scoped_refptr<const Extension> all_urls_extension = CreateExtension(
      "all_urls", base::Value::List().Append(kAllHostsPermission),
      ManifestLocation::kInternal);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(all_urls_extension->id());

  // The extension should be set to run on all sites.
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_ALL_SITES, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":true,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  // With runtime host permissions, no host permissions are added to
  // |simple_permissions|.
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // Withholding host permissions should result in the extension being set to
  // run on click.
  ScriptingPermissionsModifier permissions_modifier(profile(),
                                                    all_urls_extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  info = GenerateExtensionInfo(all_urls_extension->id());
  runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":false,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // Granting a host permission should set the extension to run on specific
  // sites, and those sites should be in the specific_site_controls.hosts set.
  permissions_modifier.GrantHostPermission(GURL("https://example.com"));
  info = GenerateExtensionInfo(all_urls_extension->id());
  runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"https://example.com/*"},)"
      R"({"granted":false,"host":"*://*/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // An extension that doesn't request any host permissions should not have
  // runtime access controls.
  scoped_refptr<const Extension> no_urls_extension = CreateExtension(
      "no urls", base::Value::List(), ManifestLocation::kInternal);
  info = GenerateExtensionInfo(no_urls_extension->id());
  EXPECT_FALSE(info->permissions.runtime_host_permissions);
  EXPECT_FALSE(info->permissions.can_access_site_data);
}

// Tests that specific_site_controls is correctly populated when permissions
// are granted by the user beyond what the extension originally requested in the
// manifest.
TEST_F(ExtensionInfoGeneratorUnitTest,
       RuntimeHostPermissionsBeyondRequestedScope) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension", base::Value::List().Append("http://*/*"),
                      ManifestLocation::kInternal);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  // Withhold permissions, and grant *://chromium.org/*.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  URLPattern all_chromium(Extension::kValidHostPermissionSchemes,
                          "*://chromium.org/*");
  PermissionSet all_chromium_set(APIPermissionSet(), ManifestPermissionSet(),
                                 URLPatternSet({all_chromium}),
                                 URLPatternSet({all_chromium}));
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, all_chromium_set);

  // The extension should only be granted http://chromium.org/* (since that's
  // the intersection with what it requested).
  URLPattern http_chromium(Extension::kValidHostPermissionSchemes,
                           "http://chromium.org/*");
  EXPECT_EQ(PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                          URLPatternSet({http_chromium}), URLPatternSet()),
            extension->permissions_data()->active_permissions());

  // The generated info should use the entirety of the granted permission,
  // which is *://chromium.org/*.
  info = GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"*://chromium.org/*"},)"
      R"({"granted":false,"host":"http://*/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
}

// Tests that specific_site_controls is correctly populated when the extension
// requests access to specific hosts.
TEST_F(ExtensionInfoGeneratorUnitTest, RuntimeHostPermissionsSpecificHosts) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension",
                      base::Value::List()
                          .Append("https://example.com/*")
                          .Append("https://chromium.org/*"),
                      ManifestLocation::kInternal);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  // Withhold permissions, and grant *://chromium.org/*.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  URLPattern all_chromium(Extension::kValidHostPermissionSchemes,
                          "https://chromium.org/*");
  PermissionSet all_chromium_set(APIPermissionSet(), ManifestPermissionSet(),
                                 URLPatternSet({all_chromium}),
                                 URLPatternSet({all_chromium}));
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, all_chromium_set);

  // The generated info should use the entirety of the granted permission,
  // which is *://chromium.org/*.
  info = GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"https://chromium.org/*"},)"
      R"({"granted":false,"host":"https://example.com/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_FALSE(runtime_hosts->has_all_hosts);
}

// Tests that requesting all_url style permissions as a runtime granted pattern
// correctly is treated as having access to all sites.
TEST_F(ExtensionInfoGeneratorUnitTest, RuntimeHostPermissionsAllURLs) {
  scoped_refptr<const Extension> all_urls_extension = CreateExtension(
      "all_urls", base::Value::List().Append(kAllHostsPermission),
      ManifestLocation::kInternal);

  // Withholding host permissions should result in the extension being set to
  // run on click.
  ScriptingPermissionsModifier permissions_modifier(profile(),
                                                    all_urls_extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(all_urls_extension->id());
  const developer::RuntimeHostPermissions* runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":false,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));

  // Grant the requested pattern ("*://*/*").
  URLPattern all_url(Extension::kValidHostPermissionSchemes,
                     kAllHostsPermission);
  PermissionSet all_url_set(APIPermissionSet(), ManifestPermissionSet(),
                            URLPatternSet({all_url}), URLPatternSet({all_url}));
  PermissionsUpdater(profile()).GrantRuntimePermissions(
      *all_urls_extension, all_url_set, base::DoNothing());

  // Now the extension should look like it has access to all hosts, while still
  // also counting as having permission withholding enabled.
  info = GenerateExtensionInfo(all_urls_extension->id());
  runtime_hosts =
      base::OptionalToPtr(info->permissions.runtime_host_permissions);
  EXPECT_EQ(developer::HOST_ACCESS_ON_ALL_SITES, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":true,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));
}

// Tests the population of withheld runtime hosts when they overlap with granted
// patterns.
TEST_F(ExtensionInfoGeneratorUnitTest, WithheldUrlsOverlapping) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension",
                      base::Value::List()
                          .Append("*://example.com/*")
                          .Append("https://chromium.org/*"),
                      ManifestLocation::kInternal);
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // Initially, no hosts are granted.
    EXPECT_EQ(
        R"([{"granted":false,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant http://example.com, which is a subset of the requested host pattern
  // (*://example.com).
  modifier.GrantHostPermission(GURL("http://example.com/"));
  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // We should display that http://example.com is granted, but *://example.com
    // is still requested. This is technically correct.
    // TODO(devlin): This is an edge case, so it's okay for it to be a little
    // rough (as long as it's not incorrect), but it would be nice to polish it
    // out. Ideally, for extensions requesting specific hosts, we'd only allow
    // granting/revoking specific patterns (e.g., all example.com sites).
    EXPECT_EQ(
        R"([{"granted":true,"host":"http://example.com/*"},)"
        R"({"granted":false,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant the requested pattern ("*://example.com/*").
  {
    URLPattern example_com(Extension::kValidHostPermissionSchemes,
                           "*://example.com/*");
    PermissionSet example_com_set(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({example_com}),
                                  URLPatternSet({example_com}));
    PermissionsUpdater(profile()).GrantRuntimePermissions(
        *extension, example_com_set, base::DoNothing());
  }

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // The http://example.com/* pattern should be omitted, since it's consumed
    // by the *://example.com/* pattern.
    EXPECT_EQ(
        R"([{"granted":true,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant permission beyond what was requested (*://*.example.com, when
  // subdomains weren't in the extension manifest).
  {
    URLPattern example_com(Extension::kValidHostPermissionSchemes,
                           "*://*.example.com/*");
    PermissionSet example_com_set(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({example_com}),
                                  URLPatternSet({example_com}));
    PermissionsUpdater(profile()).GrantRuntimePermissions(
        *extension, example_com_set, base::DoNothing());
  }

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // The full granted pattern should be visible.
    EXPECT_EQ(
        R"([{"granted":true,"host":"*://*.example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }
}

// Tests the population of withheld runtime hosts when they overlap with granted
// patterns.
TEST_F(ExtensionInfoGeneratorUnitTest,
       WithheldUrlsOverlappingWithContentScript) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermissions({"*://example.com/*", "*://chromium.org/*"})
          .AddContentScript("script.js", {"*://example.com/foo"})
          .Build();
  {
    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());
  }

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // Initially, no hosts are granted.
    EXPECT_EQ(
        R"([{"granted":false,"host":"*://chromium.org/*"},)"
        R"({"granted":false,"host":"*://example.com/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK,
              info->permissions.runtime_host_permissions->host_access);
  }
}

// Tests that file:// access checkbox shows up for extensions with activeTab
// permission. See crbug.com/850643.
TEST_F(ExtensionInfoGeneratorUnitTest, ActiveTabFileUrls) {
  scoped_refptr<const Extension> extension =
      CreateExtension("activeTab", base::Value::List().Append("activeTab"),
                      ManifestLocation::kInternal);
  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
}

// Test that `permissions.can_access_site_data` is set to true for extensions
// with API permissions that can access site data, without specifying host
// permissions.
TEST_F(ExtensionInfoGeneratorUnitTest,
       CanAccessSiteDataWithoutHostPermissions) {
  scoped_refptr<const Extension> active_tab_extension =
      CreateExtension("activeTab", base::Value::List().Append("activeTab"),
                      ManifestLocation::kInternal);
  scoped_refptr<const Extension> debugger_extension =
      CreateExtension("activeTab", base::Value::List().Append("debugger"),
                      ManifestLocation::kInternal);

  std::unique_ptr<developer::ExtensionInfo> active_tab_info =
      GenerateExtensionInfo(active_tab_extension->id());
  std::unique_ptr<developer::ExtensionInfo> debugger_info =
      GenerateExtensionInfo(debugger_extension->id());

  EXPECT_TRUE(active_tab_info->permissions.can_access_site_data);
  EXPECT_TRUE(debugger_info->permissions.can_access_site_data);
}

// Tests that blocklisted extensions are returned by the ExtensionInfoGenerator.
TEST_F(ExtensionInfoGeneratorUnitTest, Blocklisted) {
  const scoped_refptr<const Extension> extension1 = CreateExtension(
      "test1", base::Value::List(), ManifestLocation::kInternal);
  const scoped_refptr<const Extension> extension2 = CreateExtension(
      "test2", base::Value::List(), ManifestLocation::kInternal);

  std::string id1 = extension1->id();
  std::string id2 = extension2->id();
  ASSERT_NE(id1, id2);

  ExtensionInfoGenerator::ExtensionInfoList info_list =
      GenerateExtensionsInfo();
  const developer::ExtensionInfo* info1 = GetInfoFromList(info_list, id1);
  const developer::ExtensionInfo* info2 = GetInfoFromList(info_list, id2);
  ASSERT_NE(nullptr, info1);
  ASSERT_NE(nullptr, info2);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info1->state);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info2->state);

  service()->BlocklistExtensionForTest(id1);

  info_list = GenerateExtensionsInfo();
  info1 = GetInfoFromList(info_list, id1);
  info2 = GetInfoFromList(info_list, id2);
  ASSERT_NE(nullptr, info1);
  ASSERT_NE(nullptr, info2);
  EXPECT_EQ(developer::EXTENSION_STATE_BLACKLISTED, info1->state);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info2->state);

  // Verify getExtensionInfo() returns data on blocklisted extensions.
  auto info3 = GenerateExtensionInfo(id1);
  ASSERT_NE(nullptr, info3);
  EXPECT_EQ(developer::EXTENSION_STATE_BLACKLISTED, info3->state);
}

// Test generating extension action commands properly.
TEST_F(ExtensionInfoGeneratorUnitTest, ExtensionActionCommands) {
  struct {
    const char* name;
    const char* command_key;
    ActionInfo::Type action_type;
    const int manifest_version;
  } test_cases[] = {
      {"browser action", "_execute_browser_action", ActionInfo::TYPE_BROWSER,
       2},
      {"page action", "_execute_page_action", ActionInfo::TYPE_PAGE, 2},
      {"action", "_execute_action", ActionInfo::TYPE_ACTION, 3},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    base::Value::Dict command_dict =
        base::Value::Dict()
            .Set("suggested_key",
                 base::Value::Dict().Set("default", "Ctrl+Shift+P"))
            .Set("description", "Execute!");
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetAction(test_case.action_type)
            .SetManifestKey("commands",
                            base::Value::Dict().Set(test_case.command_key,
                                                    std::move(command_dict)))
            .SetManifestVersion(test_case.manifest_version)
            .Build();
    service()->AddExtension(extension.get());
    auto info = GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info);
    ASSERT_EQ(1u, info->commands.size());
    EXPECT_EQ(test_case.command_key, info->commands[0].name);
    EXPECT_TRUE(info->commands[0].is_extension_action);
  }
}

// Tests that the parent_disabled_permissions disable reason is never set for
// regular users. Prevents a regression to crbug/1100395.
TEST_F(ExtensionInfoGeneratorUnitTest,
       NoParentDisabledPermissionsForRegularUsers) {
  // Preconditions.
  ASSERT_FALSE(profile()->IsChild());

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as |extension| will be destroyed during updating.
  std::string extension_id = extension->id();

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled pending approval for permission increases.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  // Due to a permissions increase, prefs will contain escalation information.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension_id);

  // Verify that the kite icon error tooltip doesn't appear for regular users.
  EXPECT_FALSE(info->disable_reasons.parent_disabled_permissions);
}

// Test that the generator returns if the extension can be pinned to the toolbar
// and if it can, whether or not it's pinned.
TEST_F(ExtensionInfoGeneratorUnitTest, IsPinnedToToolbar) {
  // By default, the extension is not pinned to the toolbar but can be.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test1", base::Value::List(), ManifestLocation::kInternal);
  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  EXPECT_FALSE(*info->pinned_to_toolbar);

  // Pin the extension to the toolbar and test that this is reflected in the
  // generated info.
  ToolbarActionsModel* toolbar_actions_model =
      ToolbarActionsModel::Get(profile());
  toolbar_actions_model->SetActionVisibility(extension->id(), true);
  info = GenerateExtensionInfo(extension->id());
  EXPECT_TRUE(*info->pinned_to_toolbar);

  // Disable the extension. Since disabled extensions have no action, the
  // `pinned_to_toolbar` field should not exist.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  info = GenerateExtensionInfo(extension->id());
  EXPECT_FALSE(info->pinned_to_toolbar.has_value());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
// Tests for supervised users (child accounts). Supervised users are not allowed
// to install apps or extensions unless their parent approves.
class ExtensionInfoGeneratorUnitTestSupervised
    : public ExtensionInfoGeneratorUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionInfoGeneratorUnitTestSupervised() = default;
  ~ExtensionInfoGeneratorUnitTestSupervised() override = default;

  // ExtensionInfoGeneratorUnitTest:
  ExtensionServiceInitParams GetExtensionServiceInitParams() override {
    ExtensionServiceInitParams params =
        ExtensionInfoGeneratorUnitTest::GetExtensionServiceInitParams();
    params.profile_is_supervised = true;
    return params;
  }

  void SetUp() override {
    ExtensionInfoGeneratorUnitTest::SetUp();

    // Set up custodians (parents) for the child.
    supervised_user_test_util::AddCustodians(profile());

    // Set the pref to allow the child to request extension install.
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  }

  void TearDown() override {
    ExtensionInfoGeneratorUnitTest::TearDown();
  }

  bool AreExtensionPermissionsEnabled() { return GetParam(); }
};

// Tests that when an extension is disabled pending permission updates, and the
// parent has turned off the "Permissions for sites, apps and extensions"
// toggle, then supervised users will see a kite error icon with a tooltip.
TEST_P(ExtensionInfoGeneratorUnitTestSupervised,
       ParentDisabledPermissionsForSupervisedUsers) {
  // Extension permissions for supervised users is already enabled on ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list;
  if (AreExtensionPermissionsEnabled()) {
    feature_list_.InitAndEnableFeature(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  } else {
    feature_list_.InitAndDisableFeature(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
  }
#endif
  ASSERT_TRUE(profile()->IsChild());

  std::unique_ptr<SupervisedUserExtensionsDelegate>
      supervised_user_extensions_delegate =
          std::make_unique<SupervisedUserExtensionsDelegateImpl>(profile());
  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  base::FilePath path = base_path.AppendASCII("v1");

  // When extension permissions are enabled the extensions will be installed
  // but disabled until custodian approvals are performed.
  // When extension permissions are disabled the extensions will be installed
  // and enabled.
  InstallState install_state =
      AreExtensionPermissionsEnabled() ? INSTALL_WITHOUT_LOAD : INSTALL_NEW;
  const Extension* extension = PackAndInstallCRX(path, pem_path, install_state);
  ASSERT_TRUE(extension);
  if (AreExtensionPermissionsEnabled()) {
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  } else {
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  }

  // Save the id, as |extension| will be destroyed during updating.
  std::string extension_id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  if (AreExtensionPermissionsEnabled()) {
    EXPECT_TRUE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

    // Simulate parent approval for the extension installation.
    supervised_user_extensions_delegate->AddExtensionApproval(*extension);
  } else {
    EXPECT_FALSE(prefs->IsExtensionDisabled(extension_id));
  }

  // The extension should be enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled pending approval for permission increases.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Simulate the parent disallowing the child from approving permission
  // updates. If the extension permissions are disabled, this had no effect.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  // The extension should be disabled only if the extension permissions are
  // enabled.
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension_id);
  bool is_exention_disabled = AreExtensionPermissionsEnabled();
  EXPECT_EQ(info->disable_reasons.parent_disabled_permissions,
            is_exention_disabled);
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionsPermissionsForSupervisedUsersOnDesktopFeature,
    ExtensionInfoGeneratorUnitTestSupervised,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    testing::Bool()
#else
    // For ChromeOS the extension permissions are on by default.
    testing::Values(true)
#endif
);

#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
}  // namespace extensions
