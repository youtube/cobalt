// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_hub_handler.h"
#include <memory>

#include "base/check.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/safety_hub/extensions_result.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service_factory.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/permissions/constants.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

using safety_hub::SafetyHubCardState;

namespace {
// Key of the expiration time in the |UnusedSitePermissions| object. Indicates
// the time after which the associated origin and permissions are no longer
// shown in the UI.
constexpr char kExpirationKey[] = "expiration";
// Key of the lifetime in the |UnusedSitePermissions| object.
constexpr char kLifetimeKey[] = "lifetime";

// Get values from |UnusedSitePermission| object in
// safety_hub_browser_proxy.ts.
std::tuple<url::Origin,
           std::set<ContentSettingsType>,
           content_settings::ContentSettingConstraints>
GetUnusedSitePermissionsFromDict(
    const base::Value::Dict& unused_site_permissions) {
  const std::string* origin_str =
      unused_site_permissions.FindString(site_settings::kOrigin);
  CHECK(origin_str);
  const auto url = GURL(*origin_str);
  CHECK(url.is_valid());
  const url::Origin origin = url::Origin::Create(url);

  const base::Value::List* permissions =
      unused_site_permissions.FindList(site_settings::kPermissions);
  CHECK(permissions);
  std::set<ContentSettingsType> permission_types;
  for (const auto& permission : *permissions) {
    CHECK(permission.is_string());
    const std::string& type_string = permission.GetString();
    ContentSettingsType type =
        site_settings::ContentSettingsTypeFromGroupName(type_string);
    CHECK(type != ContentSettingsType::DEFAULT)
        << type_string << " is not expected to have a UI representation.";
    permission_types.insert(type);
  }

  const base::Value* js_expiration =
      unused_site_permissions.Find(kExpirationKey);
  CHECK(js_expiration);
  base::Time expiration = base::ValueToTime(js_expiration).value();

  const base::Value* js_lifetime = unused_site_permissions.Find(kLifetimeKey);
  // Users may edit the stored fields directly, so we cannot assume their
  // presence and validity.
  base::TimeDelta lifetime = content_settings::RuleMetaData::ComputeLifetime(
      /*lifetime=*/
      base::ValueToTimeDelta(js_lifetime).value_or(base::TimeDelta()),
      /*expiration=*/expiration);

  content_settings::ContentSettingConstraints constraints =
      content_settings::ContentSettingConstraints(expiration - lifetime);
  constraints.set_lifetime(lifetime);

  return std::make_tuple(origin, permission_types, constraints);
}

// Returns the state of Safe Browsing setting.
SafeBrowsingState GetSafeBrowsingState(PrefService* pref_service) {
  // TODO(crbug.com/1443466): Use SafeBrowsingResult from Safety Hub instead.
  if (safe_browsing::IsEnhancedProtectionEnabled(*pref_service))
    return SafeBrowsingState::kEnabledEnhanced;
  if (safe_browsing::IsSafeBrowsingEnabled(*pref_service))
    return SafeBrowsingState::kEnabledStandard;
  if (safe_browsing::IsSafeBrowsingPolicyManaged(*pref_service))
    return SafeBrowsingState::kDisabledByAdmin;
  if (safe_browsing::IsSafeBrowsingExtensionControlled(*pref_service))
    return SafeBrowsingState::kDisabledByExtension;
  return SafeBrowsingState::kDisabledByUser;
}

base::Value::Dict CardDataToValue(int header_id,
                                  int subheader_id,
                                  SafetyHubCardState card_state) {
  base::Value::Dict sb_card_info;

  sb_card_info.Set(safety_hub::kCardHeaderKey,
                   l10n_util::GetStringUTF16(header_id));
  sb_card_info.Set(safety_hub::kCardSubheaderKey,
                   l10n_util::GetStringUTF16(subheader_id));
  sb_card_info.Set(safety_hub::kCardStateKey, static_cast<int>(card_state));

  return sb_card_info;
}

// Returns true if the card dict indicates there is something actionable for the
// user.
bool CardHasRecommendations(base::Value::Dict card_data) {
  absl::optional<int> state = card_data.FindInt(safety_hub::kCardStateKey);
  CHECK(state.has_value());
  SafetyHubCardState card_state =
      static_cast<SafetyHubCardState>(state.value());

  return card_state == SafetyHubCardState::kWarning ||
         card_state == SafetyHubCardState::kWeak;
}

void AppendModuleNameToString(std::u16string& str,
                              int uppercase_id,
                              int lowercase_id = 0) {
  if (str.empty()) {
    str.append(l10n_util::GetStringUTF16(uppercase_id));
    return;
  }

  if (lowercase_id == 0) {
    lowercase_id = uppercase_id;
  }

  str.append(
      l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_HUB_MODULE_NAME_SEPARATOR));
  str.append(u" ");
  str.append(l10n_util::GetStringUTF16(lowercase_id));
}
}  // namespace

SafetyHubHandler::SafetyHubHandler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}
SafetyHubHandler::~SafetyHubHandler() = default;

// static
std::unique_ptr<SafetyHubHandler> SafetyHubHandler::GetForProfile(
    Profile* profile) {
  return std::make_unique<SafetyHubHandler>(profile);
}

void SafetyHubHandler::HandleGetRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  base::Value::List result = PopulateUnusedSitePermissionsData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SafetyHubHandler::HandleAllowPermissionsAgainForUnusedSite(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_string());
  const std::string& origin_str = args[0].GetString();

  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  url::Origin origin = url::Origin::Create(GURL(origin_str));

  service->RegrantPermissionsForOrigin(origin);
  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAllowPermissionsAgainForUnusedSite(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_dict());

  auto [origin, permissions, constraints] =
      GetUnusedSitePermissionsFromDict(args[0].GetDict());
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  service->UndoRegrantPermissionsForOrigin(permissions, constraints, origin);

  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleAcknowledgeRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);
  service->ClearRevokedPermissionsList();

  SendUnusedSitePermissionsReviewList();
}

void SafetyHubHandler::HandleUndoAcknowledgeRevokedUnusedSitePermissionsList(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_list());

  const base::Value::List& unused_site_permissions_list = args[0].GetList();
  UnusedSitePermissionsService* service =
      UnusedSitePermissionsServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& unused_site_permissions_js : unused_site_permissions_list) {
    CHECK(unused_site_permissions_js.is_dict());
    auto [origin, permissions, constraints] =
        GetUnusedSitePermissionsFromDict(unused_site_permissions_js.GetDict());

    service->StorePermissionInRevokedPermissionSetting(permissions, constraints,
                                                       origin);
  }

  SendUnusedSitePermissionsReviewList();
}

base::Value::List SafetyHubHandler::PopulateUnusedSitePermissionsData() {
  base::Value::List result;
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kSafetyCheckUnusedSitePermissions)) {
    return result;
  }

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& revoked_permissions : hcsm->GetSettingsForOneType(
           ContentSettingsType::REVOKED_UNUSED_SITE_PERMISSIONS)) {
    base::Value::Dict revoked_permission_value;
    revoked_permission_value.Set(
        site_settings::kOrigin, revoked_permissions.primary_pattern.ToString());
    const base::Value& stored_value = revoked_permissions.setting_value;
    DCHECK(stored_value.is_dict());

    // The revoked permissions list should be reachable by given key.
    DCHECK(stored_value.GetDict().FindList(permissions::kRevokedKey));

    auto type_list =
        stored_value.GetDict().FindList(permissions::kRevokedKey)->Clone();
    base::Value::List permissions_value_list;
    for (base::Value& type : type_list) {
      base::StringPiece permission_str =
          site_settings::ContentSettingsTypeToGroupName(
              static_cast<ContentSettingsType>(type.GetInt()));
      if (!permission_str.empty()) {
        permissions_value_list.Append(permission_str);
      }
    }

    // Some permissions have no readable name, although Safety Hub revokes them.
    // To prevent crashes, if there is no permission to be shown in the UI, the
    // origin will not be added to the revoked permissions list.
    // TODO(crbug.com/1459305): Remove this after adding check for
    // ContentSettingsTypeToGroupName.
    if (permissions_value_list.empty()) {
      continue;
    }

    revoked_permission_value.Set(
        site_settings::kPermissions,
        base::Value(std::move(permissions_value_list)));

    revoked_permission_value.Set(
        kExpirationKey,
        base::TimeToValue(revoked_permissions.metadata.expiration()));

    revoked_permission_value.Set(
        kLifetimeKey,
        base::TimeDeltaToValue(revoked_permissions.metadata.lifetime()));

    result.Append(std::move(revoked_permission_value));
  }
  return result;
}

void SafetyHubHandler::HandleGetNotificationPermissionReviewList(
    const base::Value::List& args) {
  AllowJavascript();

  const base::Value& callback_id = args[0];

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  DCHECK(service);

  if (!service) {
    RejectJavascriptCallback(callback_id, base::Value());
  }

  base::Value::List result =
      service->PopulateNotificationPermissionReviewData();

  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void SafetyHubHandler::HandleIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->AddPatternToNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleResetNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_DEFAULT);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleDismissActiveMenuNotification(
    const base::Value::List& args) {
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile_)
      ->DismissActiveNotification();
}

void SafetyHubHandler::HandleDismissPasswordMenuNotification(
    const base::Value::List& args) {
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile_)
      ->DismissPasswordNotification();
}

void SafetyHubHandler::HandleBlockNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_BLOCK);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleAllowNotificationPermissionForOrigins(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  for (const auto& origin : origins) {
    map->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString(origin.GetString()),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        CONTENT_SETTING_ALLOW);
  }

  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleUndoIgnoreOriginsForNotificationPermissionReview(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value::List& origins = args[0].GetList();
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);

  for (const auto& origin : origins) {
    const ContentSettingsPattern& primary_pattern =
        ContentSettingsPattern::FromString(origin.GetString());
    service->RemovePatternFromNotificationPermissionReviewBlocklist(
        primary_pattern, ContentSettingsPattern::Wildcard());
  }
  SendNotificationPermissionReviewList();
}

void SafetyHubHandler::HandleGetSafeBrowsingCardData(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSafeBrowsingCardData());
}

base::Value::Dict SafetyHubHandler::GetSafeBrowsingCardData() {
  SafeBrowsingState result = GetSafeBrowsingState(profile_->GetPrefs());

  base::Value::Dict sb_card_info;

  switch (result) {
    case SafeBrowsingState::kEnabledEnhanced:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_ENHANCED_SUBHEADER,
                          SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kEnabledStandard:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_ON_STANDARD_SUBHEADER,
                          SafetyHubCardState::kSafe);
      break;
    case SafeBrowsingState::kDisabledByAdmin:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_MANAGED_SUBHEADER,
                          SafetyHubCardState::kInfo);
      break;
    case SafeBrowsingState::kDisabledByExtension:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_EXTENSION_SUBHEADER,
                          SafetyHubCardState::kInfo);
      break;
    default:
      sb_card_info =
          CardDataToValue(IDS_SETTINGS_SAFETY_HUB_SB_OFF_HEADER,
                          IDS_SETTINGS_SAFETY_HUB_SB_OFF_USER_SUBHEADER,
                          SafetyHubCardState::kWarning);
  }
  return sb_card_info;
}

void SafetyHubHandler::HandleGetPasswordCardData(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, base::Value(GetPasswordCardData()));
}

base::Value::Dict SafetyHubHandler::GetPasswordCardData() {
  PasswordStatusCheckService* service =
      PasswordStatusCheckServiceFactory::GetForProfile(profile_);
  CHECK(service);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  bool signed_in = identity_manager && identity_manager->HasPrimaryAccount(
                                           signin::ConsentLevel::kSignin);

  return service->GetPasswordCardData(signed_in);
}

void SafetyHubHandler::HandleGetVersionCardData(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, base::Value(GetVersionCardData()));
}

base::Value::Dict SafetyHubHandler::GetVersionCardData() {
  base::Value::Dict result;
  switch (g_browser_process->GetBuildState()->update_type()) {
    case BuildState::UpdateType::kNone:
      result.Set(safety_hub::kCardHeaderKey,
                 l10n_util::GetStringUTF16(
                     IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_UPDATED));
      result.Set(safety_hub::kCardSubheaderKey,
                 VersionUI::GetAnnotatedVersionStringForUi());
      result.Set(safety_hub::kCardStateKey,
                 static_cast<int>(SafetyHubCardState::kSafe));
      break;
    case BuildState::UpdateType::kNormalUpdate:
    // kEnterpriseRollback and kChannelSwitchRollback are fairly rare state,
    // they will be handled same as there is waiting updates.
    case BuildState::UpdateType::kEnterpriseRollback:
    case BuildState::UpdateType::kChannelSwitchRollback:
      result = CardDataToValue(
          IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_HEADER_RESTART,
          IDS_SETTINGS_SAFETY_HUB_VERSION_CARD_SUBHEADER_RESTART,
          SafetyHubCardState::kWarning);
  }
  return result;
}

void SafetyHubHandler::HandleGetSafetyHubHasRecommendations(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  bool has_recommendations = !GetSafetyHubModulesWithRecommendations().empty();

  ResolveJavascriptCallback(callback_id, has_recommendations);
}

void SafetyHubHandler::HandleGetSafetyHubEntryPointSubheader(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  std::set<SafetyHubModule> modules = GetSafetyHubModulesWithRecommendations();

  // If there is no module that needs attention, a static string will be used
  // for the subheader.
  if (modules.empty()) {
    ResolveJavascriptCallback(
        callback_id, base::Value(l10n_util::GetStringUTF16(
                         IDS_SETTINGS_SAFETY_HUB_ENTRY_POINT_NOTHING_TO_DO)));
    return;
  }

  // Modules in subheader should be added in the following order: Passwords,
  // Version, Safe Browsing, Extensions, Notifications, Permissions.
  std::u16string subheader = u"";

  if (modules.contains(SafetyHubModule::kPasswords)) {
    AppendModuleNameToString(subheader,
                             IDS_SETTINGS_SAFETY_HUB_PASSWORDS_MODULE_NAME);
  }

  if (modules.contains(SafetyHubModule::kVersion)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_VERSION_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kSafeBrowsing)) {
    AppendModuleNameToString(subheader,
                             IDS_SETTINGS_SAFETY_HUB_SAFE_BROWSING_MODULE_NAME);
  }

  if (modules.contains(SafetyHubModule::kExtensions)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_EXTENSIONS_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kNotifications)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_NOTIFICATIONS_MODULE_LOWERCASE_NAME);
  }

  if (modules.contains(SafetyHubModule::kUnusedSitePermissions)) {
    AppendModuleNameToString(
        subheader, IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_UPPERCASE_NAME,
        IDS_SETTINGS_SAFETY_HUB_PERMISSIONS_MODULE_LOWERCASE_NAME);
  }

  ResolveJavascriptCallback(callback_id, base::Value(subheader));
}

std::set<SafetyHubHandler::SafetyHubModule>
SafetyHubHandler::GetSafetyHubModulesWithRecommendations() {
  std::set<SafetyHubModule> modules;

  // Passwords module
  if (CardHasRecommendations(GetPasswordCardData())) {
    modules.insert(SafetyHubModule::kPasswords);
  }
  // Version module
  if (CardHasRecommendations(GetVersionCardData())) {
    modules.insert(SafetyHubModule::kVersion);
  }
  // SafeBrowsing module
  if (CardHasRecommendations(GetSafeBrowsingCardData())) {
    modules.insert(SafetyHubModule::kSafeBrowsing);
  }
  // Extensions module
  if (GetNumberOfExtensionsThatNeedReview() > 0) {
    modules.insert(SafetyHubModule::kExtensions);
  }
  // Notifications module
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  CHECK(service);
  if (!service->PopulateNotificationPermissionReviewData().empty()) {
    modules.insert(SafetyHubModule::kNotifications);
  }
  // Unused site permission module
  if (!PopulateUnusedSitePermissionsData().empty()) {
    modules.insert(SafetyHubModule::kUnusedSitePermissions);
  }

  return modules;
}

void SafetyHubHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "allowPermissionsAgainForUnusedSite",
      base::BindRepeating(
          &SafetyHubHandler::HandleAllowPermissionsAgainForUnusedSite,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoAllowPermissionsAgainForUnusedSite",
      base::BindRepeating(
          &SafetyHubHandler::HandleUndoAllowPermissionsAgainForUnusedSite,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "acknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::HandleAcknowledgeRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoAcknowledgeRevokedUnusedSitePermissionsList",
      base::BindRepeating(
          &SafetyHubHandler::
              HandleUndoAcknowledgeRevokedUnusedSitePermissionsList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getNotificationPermissionReview",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetNotificationPermissionReviewList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "ignoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleIgnoreOriginsForNotificationPermissionReview,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleResetNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dismissActiveMenuNotification",
      base::BindRepeating(
          &SafetyHubHandler::HandleDismissActiveMenuNotification,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "dismissSafetyHubPasswordMenuNotification",
      base::BindRepeating(
          &SafetyHubHandler::HandleDismissPasswordMenuNotification,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "blockNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleBlockNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "allowNotificationPermissionForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::HandleAllowNotificationPermissionForOrigins,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "undoIgnoreNotificationPermissionReviewForOrigins",
      base::BindRepeating(
          &SafetyHubHandler::
              HandleUndoIgnoreOriginsForNotificationPermissionReview,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSafeBrowsingCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetSafeBrowsingCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPasswordCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetPasswordCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getVersionCardData",
      base::BindRepeating(&SafetyHubHandler::HandleGetVersionCardData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSafetyHubHasRecommendations",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetSafetyHubHasRecommendations,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSafetyHubEntryPointSubheader",
      base::BindRepeating(
          &SafetyHubHandler::HandleGetSafetyHubEntryPointSubheader,
          base::Unretained(this)));
}

void SafetyHubHandler::SendUnusedSitePermissionsReviewList() {
  // Notify observers that the unused site permission review list could have
  // changed. Note that the list is not guaranteed to have changed. In places
  // where determining whether the list has changed is cause for performance
  // concerns, an unchanged list may be sent.
  FireWebUIListener("unused-permission-review-list-maybe-changed",
                    PopulateUnusedSitePermissionsData());
}

void SafetyHubHandler::SendNotificationPermissionReviewList() {
  NotificationPermissionsReviewService* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  // Notify observers that the permission review list could have changed. Note
  // that the list is not guaranteed to have changed.
  FireWebUIListener(
      site_settings::kNotificationPermissionsReviewListMaybeChangedEvent,
      service->PopulateNotificationPermissionReviewData());
}

int SafetyHubHandler::GetNumberOfExtensionsThatNeedReview() {
  extensions::CWSInfoService* cws_info_service =
      extensions::CWSInfoServiceFactory::GetForProfile(profile_);
  absl::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(cws_info_service, profile_, false);
  if (!sh_result.has_value()) {
    return 0;
  }

  auto* result = static_cast<SafetyHubExtensionsResult*>(sh_result->get());

  return result->GetNumTriggeringExtensions();
}

void SafetyHubHandler::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void SafetyHubHandler::OnJavascriptAllowed() {}

void SafetyHubHandler::OnJavascriptDisallowed() {}
