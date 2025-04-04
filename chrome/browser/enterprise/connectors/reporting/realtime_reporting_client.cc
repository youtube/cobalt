// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kPolicyClientDescription[] = "any";
#else
const char kChromeBrowserCloudManagementClientDescription[] =
    "a machine-level user";
#endif
const char kProfilePolicyClientDescription[] = "a profile-level user";

bool IsClientValid(const std::string& dm_token,
                   policy::CloudPolicyClient* client) {
  return client && client->dm_token() == dm_token;
}

}  // namespace

namespace enterprise_connectors {

const char RealtimeReportingClient::kKeyProfileIdentifier[] =
    "profileIdentifier";

RealtimeReportingClient::RealtimeReportingClient(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = extensions::EventRouter::Get(context_);
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
}

RealtimeReportingClient::~RealtimeReportingClient() {
  if (browser_client_)
    browser_client_->RemoveObserver(this);
  if (profile_client_)
    profile_client_->RemoveObserver(this);
}

// static
std::string RealtimeReportingClient::GetBaseName(const std::string& filename) {
  base::FilePath::StringType os_filename;
#if BUILDFLAG(IS_WIN)
  os_filename = base::UTF8ToWide(filename);
#else
  os_filename = filename;
#endif
  return base::FilePath(os_filename).BaseName().AsUTF8Unsafe();
}

// static
bool RealtimeReportingClient::ShouldInitRealtimeReportingClient() {
  if (profiles::IsPublicSession() &&
      !base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabledOnMGS)) {
    DVLOG(2) << "Safe browsing real-time reporting is not enabled in Managed "
                "Guest Sessions.";
    return false;
  }

  return true;
}

void RealtimeReportingClient::SetBrowserCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && browser_client_)
    browser_client_->RemoveObserver(this);

  browser_client_ = client;
  if (browser_client_)
    browser_client_->AddObserver(this);
}

void RealtimeReportingClient::SetProfileCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && profile_client_)
    profile_client_->RemoveObserver(this);

  profile_client_ = client;
  if (profile_client_)
    profile_client_->AddObserver(this);
}

void RealtimeReportingClient::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void RealtimeReportingClient::InitRealtimeReportingClient(
    const ReportingSettings& settings) {
  // If the corresponding client is already initialized, do nothing.
  if ((settings.per_profile &&
       IsClientValid(settings.dm_token, profile_client_)) ||
      (!settings.per_profile &&
       IsClientValid(settings.dm_token, browser_client_))) {
    DVLOG(2) << "Safe browsing real-time event reporting already initialized.";
    return;
  }

  if (!ShouldInitRealtimeReportingClient())
    return;

  // |identity_manager_| may be null in tests. If there is no identity
  // manager don't enable the real-time reporting API since the router won't
  // be able to fill in all the info needed for the reports.
  if (!identity_manager_) {
    DVLOG(2) << "Safe browsing real-time event requires an identity manager.";
    return;
  }

  policy::CloudPolicyClient* client = nullptr;
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      InitBrowserReportingClient(settings.dm_token);
#else
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      settings.per_profile ? InitProfileReportingClient(settings.dm_token)
                           : InitBrowserReportingClient(settings.dm_token);
#endif
  if (!desc_and_client.second)
    return;
  policy_client_desc = std::move(desc_and_client.first);
  client = std::move(desc_and_client.second);

  OnCloudPolicyClientAvailable(policy_client_desc, client);
}

std::pair<std::string, policy::CloudPolicyClient*>
RealtimeReportingClient::InitBrowserReportingClient(
    const std::string& dm_token) {
  // |device_management_service| may be null in tests. If there is no device
  // management service don't enable the real-time reporting API since the
  // router won't be able to create the reporting server client below.
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy_client_desc = kPolicyClientDescription;
#else
  policy_client_desc = kChromeBrowserCloudManagementClientDescription;
#endif
  if (!device_management_service) {
    DVLOG(2) << "Safe browsing real-time event requires a device management "
                "service.";
    return {policy_client_desc, nullptr};
  }

  policy::CloudPolicyClient* client = nullptr;
  std::string client_id;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = nullptr;
  const user_manager::User* user = GetChromeOSUser();
  if (user) {
    profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    // If primary user profile is not finalized, use the current profile.
    if (!profile)
      profile = Profile::FromBrowserContext(context_);
  } else {
    LOG(ERROR) << "Could not determine who the user is.";
    profile = Profile::FromBrowserContext(context_);
  }
  DCHECK(profile);

  if (profiles::IsPublicSession()) {
    client_id = reporting::GetMGSUserClientId().value_or("");
  } else {
    client_id = reporting::GetUserClientId(profile).value_or("");
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* main_profile = GetMainProfileLacros();
  if (main_profile) {
    // Prefer the user client id if available.
    client_id = reporting::GetUserClientId(main_profile).value_or(client_id);
  }
#else
  client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#endif

  DCHECK(!client_id.empty());

  // Make sure DeviceManagementService has been initialized.
  device_management_service->ScheduleInitialization(0);

  browser_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  client = browser_private_client_.get();

  // TODO(crbug.com/1069049): when we decide to add the extra URL parameters to
  // the uploaded reports, do the following:
  //     client->add_connector_url_params(true);
  if (!client->is_registered()) {
    client->SetupRegistration(
        dm_token, client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }

  return {policy_client_desc, client};
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::pair<std::string, policy::CloudPolicyClient*>
RealtimeReportingClient::InitProfileReportingClient(
    const std::string& dm_token) {
  policy::UserCloudPolicyManager* policy_manager =
      Profile::FromBrowserContext(context_)->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->core() ||
      !policy_manager->core()->client()) {
    return {kProfilePolicyClientDescription, nullptr};
  }

  profile_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      policy_manager->core()->client()->service(),
      g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  policy::CloudPolicyClient* client = profile_private_client_.get();

  // TODO(crbug.com/1069049): when we decide to add the extra URL parameters to
  // the uploaded reports, do the following:
  //     client->add_connector_url_params(true);

  client->SetupRegistration(dm_token,
                            policy_manager->core()->client()->client_id(),
                            /*user_affiliation_ids*/ {});

  return {kProfilePolicyClientDescription, client};
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void RealtimeReportingClient::OnCloudPolicyClientAvailable(
    const std::string& policy_client_desc,
    policy::CloudPolicyClient* client) {
  if (client == nullptr) {
    LOG(ERROR) << "Could not obtain " << policy_client_desc
               << " for safe browsing real-time event reporting.";
    return;
  }

  if (policy_client_desc == kProfilePolicyClientDescription) {
    DCHECK_NE(profile_client_, client);
    if (profile_client_ == client)
      return;

    if (profile_client_)
      profile_client_->RemoveObserver(this);

    profile_client_ = client;
  } else {
    DCHECK_NE(browser_client_, client);
    if (browser_client_ == client)
      return;

    if (browser_client_)
      browser_client_->RemoveObserver(this);

    browser_client_ = client;
  }

  client->AddObserver(this);

  VLOG(1) << "Ready for safe browsing real-time event reporting.";
}

absl::optional<ReportingSettings>
RealtimeReportingClient::GetReportingSettings() {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(context_);
  if (!service) {
    return absl::nullopt;
  }

  return service->GetReportingSettings(ReportingConnector::SECURITY_EVENT);
}

void RealtimeReportingClient::ReportRealtimeEvent(
    const std::string& name,
    const ReportingSettings& settings,
    base::Value::Dict event) {
  ReportEventWithTimestamp(name, settings, std::move(event), base::Time::Now());
}

void RealtimeReportingClient::ReportPastEvent(const std::string& name,
                                              const ReportingSettings& settings,
                                              base::Value::Dict event,
                                              const base::Time& time) {
  ReportEventWithTimestamp(name, settings, std::move(event), time);
}

void RealtimeReportingClient::ReportEventWithTimestamp(
    const std::string& name,
    const ReportingSettings& settings,
    base::Value::Dict event,
    const base::Time& time) {
  if (rejected_dm_token_timers_.contains(settings.dm_token)) {
    return;
  }

#ifndef NDEBUG
  // Make sure the event is included in the kAllReportingEvents array.
  bool found = false;
  for (const char* event_name : ReportingServiceSettings::kAllReportingEvents) {
    if (event_name == name) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif

  // Make sure real-time reporting is initialized.
  InitRealtimeReportingClient(settings);
  if ((settings.per_profile && !profile_client_) ||
      (!settings.per_profile && !browser_client_)) {
    return;
  }

  // Format the current time (UTC) in RFC3339 format.
  base::Time::Exploded exploded_time;
  time.UTCExplode(&exploded_time);
  std::string time_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded_time.year,
      exploded_time.month, exploded_time.day_of_month, exploded_time.hour,
      exploded_time.minute, exploded_time.second, exploded_time.millisecond);

  policy::CloudPolicyClient* client =
      settings.per_profile ? profile_client_.get() : browser_client_.get();
  base::Value::Dict wrapper;
  wrapper.Set("time", time_str);
  event.Set(kKeyProfileIdentifier, GetProfileIdentifier());
  // TODO(b/270589536): also move other common field setting here.
  wrapper.Set(name, std::move(event));

  auto upload_callback = base::BindOnce(
      [](base::Value::Dict wrapper, bool per_profile, std::string dm_token,
         policy::CloudPolicyClient::Result upload_result) {
        // TODO(b/256553070): Do not crash if the client is unregistered.
        CHECK(!upload_result.IsClientNotRegisteredError());

        // Show the report on chrome://safe-browsing, if appropriate.
        wrapper.Set("uploaded_successfully", upload_result.IsSuccess());
        wrapper.Set(per_profile ? "profile_dm_token" : "browser_dm_token",
                    std::move(dm_token));
        safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
            std::move(wrapper));
      },
      wrapper.Clone(), settings.per_profile, client->dm_token());

  base::Value::List event_list;
  event_list.Append(std::move(wrapper));

  Profile* profile = Profile::FromBrowserContext(context_);

  client->UploadSecurityEventReport(
      context_, IncludeDeviceInfo(profile, settings.per_profile),
      policy::RealtimeReportingJobConfiguration::BuildReport(
          std::move(event_list),
          reporting::GetContext(Profile::FromBrowserContext(context_))),
      std::move(upload_callback));
}

std::string RealtimeReportingClient::GetProfileUserName() const {
  return safe_browsing::GetProfileEmail(identity_manager_);
}

std::string RealtimeReportingClient::GetProfileIdentifier() const {
  if (profile_client_) {
    auto* profile_id_service =
        enterprise::ProfileIdServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context_));
    if (profile_id_service && profile_id_service->GetProfileId().has_value()) {
      return profile_id_service->GetProfileId().value();
    }
    return std::string();
  }

  return Profile::FromBrowserContext(context_)->GetPath().AsUTF8Unsafe();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
const user_manager::User* RealtimeReportingClient::GetChromeOSUser() {
  return user_manager::UserManager::IsInitialized()
             ? user_manager::UserManager::Get()->GetPrimaryUser()
             : nullptr;
}

#endif

void RealtimeReportingClient::RemoveDmTokenFromRejectedSet(
    const std::string& dm_token) {
  rejected_dm_token_timers_.erase(dm_token);
}

void RealtimeReportingClient::OnClientError(policy::CloudPolicyClient* client) {
  base::Value::Dict error_value;
  error_value.Set("error",
                  "An event got an error status and hasn't been reported");
  error_value.Set("status", client->last_dm_status());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
      error_value);

  // This is the status set when the server returned 403, which is what the
  // reporting server returns when the customer is not allowed to report events.
  if (client->last_dm_status() ==
      policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
    // This could happen if a second event was fired before the first one
    // returned an error.
    if (!rejected_dm_token_timers_.contains(client->dm_token())) {
      rejected_dm_token_timers_[client->dm_token()] =
          std::make_unique<base::OneShotTimer>();
      rejected_dm_token_timers_[client->dm_token()]->Start(
          FROM_HERE, base::Hours(24),
          base::BindOnce(&RealtimeReportingClient::RemoveDmTokenFromRejectedSet,
                         weak_ptr_factory_.GetWeakPtr(), client->dm_token()));
    }
  }
}

}  // namespace enterprise_connectors
