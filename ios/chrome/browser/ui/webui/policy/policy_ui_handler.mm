// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/policy/policy_ui_handler.h"

#import <UIKit/UIKit.h>
#import <algorithm>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#import "components/enterprise/browser/reporting/common_pref_names.h"
#import "components/policy/core/browser/policy_conversions.h"
#import "components/policy/core/browser/webui/json_generation.h"
#import "components/policy/core/browser/webui/machine_level_user_cloud_policy_status_provider.h"
#import "components/policy/core/browser/webui/policy_webui_constants.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_types.h"
#import "components/policy/core/common/schema.h"
#import "components/policy/core/common/schema_map.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/policy/policy_conversions_client_ios.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/webui/web_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyUIHandler::PolicyUIHandler() = default;

PolicyUIHandler::~PolicyUIHandler() {
  GetPolicyService()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy::SchemaRegistry* registry = ChromeBrowserState::FromWebUIIOS(web_ui())
                                         ->GetPolicyConnector()
                                         ->GetSchemaRegistry();
  registry->RemoveObserver(this);
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
    web::WebUIIOSDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"conflict", IDS_POLICY_LABEL_CONFLICT},
      {"superseding", IDS_POLICY_LABEL_SUPERSEDING},
      {"conflictValue", IDS_POLICY_LABEL_CONFLICT_VALUE},
      {"supersededValue", IDS_POLICY_LABEL_SUPERSEDED_VALUE},
      {"headerLevel", IDS_POLICY_HEADER_LEVEL},
      {"headerName", IDS_POLICY_HEADER_NAME},
      {"headerScope", IDS_POLICY_HEADER_SCOPE},
      {"headerSource", IDS_POLICY_HEADER_SOURCE},
      {"headerStatus", IDS_POLICY_HEADER_STATUS},
      {"headerValue", IDS_POLICY_HEADER_VALUE},
      {"warning", IDS_POLICY_HEADER_WARNING},
      {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
      {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
      {"error", IDS_POLICY_LABEL_ERROR},
      {"deprecated", IDS_POLICY_LABEL_DEPRECATED},
      {"future", IDS_POLICY_LABEL_FUTURE},
      {"info", IDS_POLICY_LABEL_INFO},
      {"ignored", IDS_POLICY_LABEL_IGNORED},
      {"notSpecified", IDS_POLICY_NOT_SPECIFIED},
      {"ok", IDS_POLICY_OK},
      {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
      {"scopeUser", IDS_POLICY_SCOPE_USER},
      {"title", IDS_POLICY_TITLE},
      {"unknown", IDS_POLICY_UNKNOWN},
      {"unset", IDS_POLICY_UNSET},
      {"value", IDS_POLICY_LABEL_VALUE},
      {"sourceDefault", IDS_POLICY_SOURCE_DEFAULT},
      {"loadPoliciesDone", IDS_POLICY_LOAD_POLICIES_DONE},
      {"loadingPolicies", IDS_POLICY_LOADING_POLICIES},
      {"reportUploading", IDS_REPORT_UPLOADING},
      {"reportUploaded", IDS_REPORT_UPLOADED},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddLocalizedStrings(policy::kPolicySources);
  source->UseStringsJs();
}

void PolicyUIHandler::RegisterMessages() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      GetApplicationContext()
          ->GetBrowserPolicyConnector()
          ->machine_level_user_cloud_policy_manager();

  if (manager) {
    policy::BrowserDMTokenStorage* dmTokenStorage =
        policy::BrowserDMTokenStorage::Get();

    machine_status_provider_ =
        std::make_unique<policy::MachineLevelUserCloudPolicyStatusProvider>(
            manager->core(), GetApplicationContext()->GetLocalState(),
            new policy::MachineLevelUserCloudPolicyContext(
                {dmTokenStorage->RetrieveEnrollmentToken(),
                 dmTokenStorage->RetrieveClientId(),
                 enterprise_reporting::kLastUploadSucceededTimestamp}));
    machine_status_provider_observation_.Observe(
        machine_status_provider_.get());
  }

  if (!machine_status_provider_)
    machine_status_provider_ = std::make_unique<policy::PolicyStatusProvider>();

  GetPolicyService()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  browser_state->GetPolicyConnector()->GetSchemaRegistry()->AddObserver(this);

  web_ui()->RegisterMessageCallback(
      "listenPoliciesUpdates",
      base::BindRepeating(&PolicyUIHandler::HandleListenPoliciesUpdates,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::BindRepeating(&PolicyUIHandler::HandleReloadPolicies,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "copyPoliciesJSON",
      base::BindRepeating(&PolicyUIHandler::HandleCopyPoliciesJson,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "uploadReport", base::BindRepeating(&PolicyUIHandler::HandleUploadReport,
                                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPolicyLogs",
      base::BindRepeating(&PolicyUIHandler::HandleGetPolicyLogs,
                          base::Unretained(this)));
}

void PolicyUIHandler::HandleCopyPoliciesJson(const base::Value::List& args) {
  NSString* jsonString = base::SysUTF8ToNSString(GetPoliciesAsJson());
  StoreTextInPasteboard(jsonString);
}

void PolicyUIHandler::HandleUploadReport(const base::Value::List& args) {
  DCHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();
  auto* report_scheduler = GetApplicationContext()
                               ->GetBrowserPolicyConnector()
                               ->chrome_browser_cloud_management_controller()
                               ->report_scheduler();
  if (report_scheduler) {
    report_scheduler->UploadFullReport(
        base::BindOnce(&PolicyUIHandler::OnReportUploaded,
                       weak_factory_.GetWeakPtr(), callback_id));
  } else {
    OnReportUploaded(callback_id);
  }
}

void PolicyUIHandler::HandleGetPolicyLogs(const base::Value::List& args) {
  DCHECK(policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled());
  web_ui()->ResolveJavascriptCallback(
      args[0], policy::PolicyLogger::GetInstance()->GetAsList());
}

std::string PolicyUIHandler::GetPoliciesAsJson() {
  auto client = std::make_unique<PolicyConversionsClientIOS>(
      ChromeBrowserState::FromWebUIIOS(web_ui()));

  return policy::GenerateJson(
      /*policy_values=*/policy::DictionaryPolicyConversions(std::move(client))
          .ToValueDict(),
      GetStatusValue(),
      policy::JsonGenerationParams()
          .with_application_name(l10n_util::GetStringUTF8(IDS_IOS_PRODUCT_NAME))
          .with_channel_name(GetChannelString(GetChannel()))
          .with_processor_variation(l10n_util::GetStringUTF8(
              sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT))
          .with_os_name(version_info::GetOSType()));
}

void PolicyUIHandler::OnSchemaRegistryUpdated(bool has_new_schemas) {
  // Update UI when new schema is added.
  if (has_new_schemas) {
    SendPolicies();
  }
}

void PolicyUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                      const policy::PolicyMap& previous,
                                      const policy::PolicyMap& current) {
  SendPolicies();
}

void PolicyUIHandler::OnPolicyStatusChanged() {
  SendStatus();
}

void PolicyUIHandler::OnReportUploaded(const std::string& callback_id) {
  web_ui()->ResolveJavascriptCallback(base::Value(callback_id),
                                      /*response=*/base::Value());
  SendStatus();
}

base::Value::Dict PolicyUIHandler::GetPolicyNames() const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  policy::SchemaRegistry* registry =
      browser_state->GetPolicyConnector()->GetSchemaRegistry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::Value::List chrome_policy_names;
  policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_namespace);
  for (auto it = chrome_schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    chrome_policy_names.Append(base::Value(it.key()));
  }

  base::Value::Dict chrome_values;
  chrome_values.Set(policy::kNameKey, policy::kChromePoliciesName);
  chrome_values.Set(policy::kPolicyNamesKey, std::move(chrome_policy_names));

  base::Value::Dict names;
  names.Set(policy::kChromePoliciesId, std::move(chrome_values));
  return names;
}

base::Value::Dict PolicyUIHandler::GetPolicyValues() const {
  base::Value::List policy_ids;
  policy_ids.Append(policy::kChromePoliciesId);

  auto client = std::make_unique<PolicyConversionsClientIOS>(
      ChromeBrowserState::FromWebUIIOS(web_ui()));
  base::Value::Dict policy_values =
      policy::ChromePolicyConversions(std::move(client))
          .EnableConvertValues(true)
          .ToValueDict();

  base::Value::Dict dict;
  dict.Set(policy::kPolicyValuesKey, std::move(policy_values));
  dict.Set(policy::kPolicyIdsKey, std::move(policy_ids));
  return dict;
}

void PolicyUIHandler::HandleListenPoliciesUpdates(
    const base::Value::List& args) {
  OnRefreshPoliciesDone();
}

void PolicyUIHandler::HandleReloadPolicies(const base::Value::List& args) {
  GetPolicyService()->RefreshPolicies(base::BindOnce(
      &PolicyUIHandler::OnRefreshPoliciesDone, weak_factory_.GetWeakPtr()));
}

void PolicyUIHandler::SendPolicies() {
  base::Value::Dict names = GetPolicyNames();
  base::Value::Dict values = GetPolicyValues();
  web_ui()->FireWebUIListener("policies-updated", names, values);
}

base::Value::Dict PolicyUIHandler::GetStatusValue() const {
  base::Value::Dict machine_status = machine_status_provider_->GetStatus();

  // Given that it's usual for users to bring their own devices and the fact
  // that device names could expose personal information. We do not show
  // this field in Device Policy Box
  machine_status.Remove(policy::kMachineKey);

  base::Value::Dict status;
  status.Set("machine", std::move(machine_status));
  return status;
}

void PolicyUIHandler::SendStatus() {
  base::Value::Dict status = GetStatusValue();
  web_ui()->FireWebUIListener("status-updated", status);
}

void PolicyUIHandler::OnRefreshPoliciesDone() {
  SendPolicies();
  SendStatus();
}

policy::PolicyService* PolicyUIHandler::GetPolicyService() const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromWebUIIOS(web_ui());
  return browser_state->GetPolicyConnector()->GetPolicyService();
}
