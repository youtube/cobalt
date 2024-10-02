// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/signing_service.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy {

namespace {

// Translates the DeviceRegisterResponse::DeviceMode |mode| to the enum used
// internally to represent different device modes.
DeviceMode TranslateProtobufDeviceMode(
    em::DeviceRegisterResponse::DeviceMode mode) {
  switch (mode) {
    case em::DeviceRegisterResponse::ENTERPRISE:
      return DEVICE_MODE_ENTERPRISE;
    case em::DeviceRegisterResponse::RETAIL_DEPRECATED:
      return DEPRECATED_DEVICE_MODE_LEGACY_RETAIL_MODE;
    case em::DeviceRegisterResponse::CHROME_AD:
      return DEVICE_MODE_ENTERPRISE_AD;
    case em::DeviceRegisterResponse::DEMO:
      return DEVICE_MODE_DEMO;
  }
  LOG_POLICY(ERROR, CBCM_ENROLLMENT)
      << "Unknown enrollment mode in registration response: " << mode;
  return DEVICE_MODE_NOT_SET;
}

bool IsChromePolicy(const std::string& type) {
  return type == dm_protocol::kChromeDevicePolicyType ||
         type == dm_protocol::kChromeUserPolicyType ||
         IsMachineLevelUserCloudPolicyType(type);
}

em::PolicyValidationReportRequest::ValidationResultType
TranslatePolicyValidationResult(CloudPolicyValidatorBase::Status status) {
  using report = em::PolicyValidationReportRequest;
  using policyValidationStatus = CloudPolicyValidatorBase::Status;
  switch (status) {
    case policyValidationStatus::VALIDATION_OK:
      return report::VALIDATION_RESULT_TYPE_SUCCESS;
    case policyValidationStatus::VALIDATION_BAD_INITIAL_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_INITIAL_SIGNATURE;
    case policyValidationStatus::VALIDATION_BAD_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_SIGNATURE;
    case policyValidationStatus::VALIDATION_ERROR_CODE_PRESENT:
      return report::VALIDATION_RESULT_TYPE_ERROR_CODE_PRESENT;
    case policyValidationStatus::VALIDATION_PAYLOAD_PARSE_ERROR:
      return report::VALIDATION_RESULT_TYPE_PAYLOAD_PARSE_ERROR;
    case policyValidationStatus::VALIDATION_WRONG_POLICY_TYPE:
      return report::VALIDATION_RESULT_TYPE_WRONG_POLICY_TYPE;
    case policyValidationStatus::VALIDATION_WRONG_SETTINGS_ENTITY_ID:
      return report::VALIDATION_RESULT_TYPE_WRONG_SETTINGS_ENTITY_ID;
    case policyValidationStatus::VALIDATION_BAD_TIMESTAMP:
      return report::VALIDATION_RESULT_TYPE_BAD_TIMESTAMP;
    case policyValidationStatus::VALIDATION_BAD_DM_TOKEN:
      return report::VALIDATION_RESULT_TYPE_BAD_DM_TOKEN;
    case policyValidationStatus::VALIDATION_BAD_DEVICE_ID:
      return report::VALIDATION_RESULT_TYPE_BAD_DEVICE_ID;
    case policyValidationStatus::VALIDATION_BAD_USER:
      return report::VALIDATION_RESULT_TYPE_BAD_USER;
    case policyValidationStatus::VALIDATION_POLICY_PARSE_ERROR:
      return report::VALIDATION_RESULT_TYPE_POLICY_PARSE_ERROR;
    case policyValidationStatus::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE:
      return report::VALIDATION_RESULT_TYPE_BAD_KEY_VERIFICATION_SIGNATURE;
    case policyValidationStatus::VALIDATION_VALUE_WARNING:
      return report::VALIDATION_RESULT_TYPE_VALUE_WARNING;
    case policyValidationStatus::VALIDATION_VALUE_ERROR:
      return report::VALIDATION_RESULT_TYPE_VALUE_ERROR;
    case policyValidationStatus::VALIDATION_STATUS_SIZE:
      return report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED;
  }
  return report::VALIDATION_RESULT_TYPE_ERROR_UNSPECIFIED;
}

em::PolicyValueValidationIssue::ValueValidationIssueSeverity
TranslatePolicyValidationResultSeverity(
    ValueValidationIssue::Severity severity) {
  using issue = em::PolicyValueValidationIssue;
  switch (severity) {
    case ValueValidationIssue::Severity::kWarning:
      return issue::VALUE_VALIDATION_ISSUE_SEVERITY_WARNING;
    case ValueValidationIssue::Severity::kError:
      return issue::VALUE_VALIDATION_ISSUE_SEVERITY_ERROR;
  }
  NOTREACHED();
  return issue::VALUE_VALIDATION_ISSUE_SEVERITY_UNSPECIFIED;
}

template <typename T>
std::vector<T> ToVector(
    const google::protobuf::RepeatedPtrField<T>& proto_container) {
  return std::vector<T>(proto_container.begin(), proto_container.end());
}

std::tuple<DeviceManagementStatus, std::vector<em::SignedData>>
DecodeRemoteCommands(DeviceManagementStatus status,
                     const em::DeviceManagementResponse& response) {
  using MakeTuple =
      std::tuple<DeviceManagementStatus, std::vector<em::SignedData>>;

  if (status != DM_STATUS_SUCCESS) {
    return MakeTuple(status, {});
  }
  if (!response.remote_command_response().commands().empty()) {
    // Unsigned remote commands are no longer supported.
    return MakeTuple(DM_STATUS_RESPONSE_DECODING_ERROR, {});
  }

  return MakeTuple(
      DM_STATUS_SUCCESS,
      ToVector(response.remote_command_response().secure_commands()));
}

// Returns a separator-less string with MAC address to match the format of
// reporting MAC addresses.
std::string FormatMacAddress(const CloudPolicyClient::MacAddress& mac_address) {
  CHECK_EQ(mac_address.size(), 6u);
  // Print 2-digit (02) upper-case hex (X) values of MAC address.
  std::string mac_address_string = base::StringPrintf(
      "%02X%02X%02X%02X%02X%02X", mac_address[0], mac_address[1],
      mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
  DCHECK_EQ(mac_address_string.size(), 12u);
  return mac_address_string;
}

}  // namespace

CloudPolicyClient::RegistrationParameters::RegistrationParameters(
    em::DeviceRegisterRequest::Type registration_type,
    em::DeviceRegisterRequest::Flavor flavor)
    : registration_type(registration_type), flavor(flavor) {}

CloudPolicyClient::RegistrationParameters::~RegistrationParameters() = default;

CloudPolicyClient::Observer::~Observer() = default;

CloudPolicyClient::Result::Result(DeviceManagementStatus status)
    : result_(status) {}
CloudPolicyClient::Result::Result(NotRegistered) : result_(NotRegistered()) {}

bool CloudPolicyClient::Result::IsSuccess() const {
  return result_ == absl::variant<NotRegistered, DeviceManagementStatus>(
                        DM_STATUS_SUCCESS);
}

bool CloudPolicyClient::Result::IsClientNotRegisteredError() const {
  return result_ ==
         absl::variant<NotRegistered, DeviceManagementStatus>(NotRegistered());
}

bool CloudPolicyClient::Result::IsDMServerError() const {
  return !IsClientNotRegisteredError() && !IsSuccess();
}

DeviceManagementStatus CloudPolicyClient::Result::GetDMServerError() const {
  return absl::get<DeviceManagementStatus>(result_);
}

CloudPolicyClient::CloudPolicyClient(
    base::StringPiece machine_id,
    base::StringPiece machine_model,
    base::StringPiece brand_code,
    base::StringPiece attested_device_id,
    absl::optional<MacAddress> ethernet_mac_address,
    absl::optional<MacAddress> dock_mac_address,
    base::StringPiece manufacture_date,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceDMTokenCallback device_dm_token_callback)
    : machine_id_(machine_id),
      machine_model_(machine_model),
      brand_code_(brand_code),
      attested_device_id_(attested_device_id),
      ethernet_mac_address_(ethernet_mac_address.has_value()
                                ? FormatMacAddress(ethernet_mac_address.value())
                                : std::string()),
      dock_mac_address_(dock_mac_address.has_value()
                            ? FormatMacAddress(dock_mac_address.value())
                            : std::string()),
      manufacture_date_(manufacture_date),
      service_(service),  // Can be null for unit tests.
      device_dm_token_callback_(device_dm_token_callback),
      url_loader_factory_(url_loader_factory) {}

CloudPolicyClient::CloudPolicyClient(
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceDMTokenCallback device_dm_token_callback)
    : service_(service),  // Can be null for unit tests.
      device_dm_token_callback_(device_dm_token_callback),
      url_loader_factory_(url_loader_factory) {}

CloudPolicyClient::~CloudPolicyClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CloudPolicyClient::SetupRegistration(
    const std::string& dm_token,
    const std::string& client_id,
    const std::vector<std::string>& user_affiliation_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!dm_token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  dm_token_ = dm_token;
  client_id_ = client_id;
  request_jobs_.clear();
  app_install_report_request_job_ = nullptr;
  extension_install_report_request_job_ = nullptr;
  unique_request_job_.reset();
  last_policy_fetch_responses_.clear();
  if (device_dm_token_callback_) {
    device_dm_token_ = device_dm_token_callback_.Run(user_affiliation_ids);
  }

  NotifyRegistrationStateChanged();
}

// Sets the client ID or generate a new one. A new one is intentionally
// generated on each new registration request in order to preserve privacy.
// Reusing IDs would mean the server could track clients by their registration
// attempts.
void CloudPolicyClient::SetClientId(const std::string& client_id) {
  client_id_ = client_id.empty()
                   ? base::Uuid::GenerateRandomV4().AsLowercaseString()
                   : client_id;
}

void CloudPolicyClient::Register(const RegistrationParameters& parameters,
                                 const std::string& client_id,
                                 const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);
  DCHECK(!oauth_token.empty());
  DCHECK(!is_registered());

  SetClientId(client_id);

  std::unique_ptr<RegistrationJobConfiguration> config =
      std::make_unique<RegistrationJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_REGISTRATION, this,
          DMAuth::NoAuth(), oauth_token,
          base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  em::DeviceRegisterRequest* request =
      config->request()->mutable_register_request();
  CreateDeviceRegisterRequest(parameters, client_id, request);

  if (requires_reregistration()) {
    request->set_reregistration_dm_token(reregistration_dm_token_);
  }

  unique_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::RegisterWithCertificate(
    const RegistrationParameters& parameters,
    const std::string& client_id,
    const std::string& pem_certificate_chain,
    const std::string& sub_organization,
    std::unique_ptr<SigningService> signing_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(signing_service);
  DCHECK(service_);
  DCHECK(!is_registered());

  SetClientId(client_id);

  em::CertificateBasedDeviceRegistrationData data;
  data.set_certificate_type(em::CertificateBasedDeviceRegistrationData::
                                ENTERPRISE_ENROLLMENT_CERTIFICATE);
  data.set_device_certificate(pem_certificate_chain);

  em::DeviceRegisterRequest* request = data.mutable_device_register_request();
  CreateDeviceRegisterRequest(parameters, client_id, request);
  if (!sub_organization.empty()) {
    em::DeviceRegisterConfiguration* configuration =
        data.mutable_device_register_configuration();
    configuration->set_device_owner(sub_organization);
  }

  SigningService* signing_service_ptr = signing_service.get();
  signing_service_ptr->SignData(
      data.SerializeAsString(),
      base::BindOnce(&CloudPolicyClient::OnRegisterWithCertificateRequestSigned,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(signing_service)));
}

void CloudPolicyClient::RegisterWithToken(
    const std::string& token,
    const std::string& client_id,
    const ClientDataDelegate& client_data_delegate,
    bool is_mandatory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);
  DCHECK(!token.empty());
  DCHECK(!client_id.empty());
  DCHECK(!is_registered());

  SetClientId(client_id);

  std::unique_ptr<RegistrationJobConfiguration> config =
      std::make_unique<RegistrationJobConfiguration>(
          DeviceManagementService::JobConfiguration::TYPE_TOKEN_ENROLLMENT,
          this, DMAuth::FromEnrollmentToken(token),
          /*oauth_token=*/absl::nullopt,
          base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                         weak_ptr_factory_.GetWeakPtr()));

  // sets CBCM enrollment timeout to 30 seconds when CBCM enrollment is optional
  if (!is_mandatory) {
    config->SetTimeoutDuration(base::Seconds(30));
  }

  enterprise_management::RegisterBrowserRequest* request =
      config->request()->mutable_register_browser_request();
  client_data_delegate.FillRegisterBrowserRequest(
      request, base::BindOnce(&CloudPolicyClient::CreateUniqueRequestJob,
                              base::Unretained(this), std::move(config)));
}

void CloudPolicyClient::OnRegisterWithCertificateRequestSigned(
    std::unique_ptr<SigningService> signing_service,
    bool success,
    em::SignedData signed_data) {
  signing_service.reset();

  if (!success) {
    const em::DeviceManagementResponse response;
    OnRegisterCompleted(
        DMServerJobResult{nullptr, 0, DM_STATUS_CANNOT_SIGN_REQUEST, response});
    return;
  }

  std::unique_ptr<RegistrationJobConfiguration> config = std::make_unique<
      RegistrationJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_BASED_REGISTRATION,
      this, DMAuth::NoAuth(),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnRegisterCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  em::SignedData* signed_request =
      config->request()
          ->mutable_certificate_based_register_request()
          ->mutable_signed_request();
  signed_request->set_data(signed_data.data());
  signed_request->set_signature(signed_data.signature());
  signed_request->set_extra_data_bytes(signed_data.extra_data_bytes());

  unique_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::SetInvalidationInfo(int64_t version,
                                            const std::string& payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  invalidation_version_ = version;
  invalidation_payload_ = payload;
}

void CloudPolicyClient::SetOAuthTokenAsAdditionalAuth(
    const std::string& oauth_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  oauth_token_ = oauth_token;
}

void CloudPolicyClient::FetchPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(is_registered());
  CHECK(!types_to_fetch_.empty());

  VLOG(2) << "Policy fetch starting";
  for (const auto& type : types_to_fetch_) {
    VLOG_POLICY(2, POLICY_FETCHING)
        << "Fetching policy type: " << type.first << " -> " << type.second;
  }

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH, this,
      /*critical=*/true, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/oauth_token_,
      base::BindOnce(&CloudPolicyClient::OnPolicyFetchCompleted,
                     weak_ptr_factory_.GetWeakPtr()));

  em::DeviceManagementRequest* request = config->request();

  // Build policy fetch requests.
  em::DevicePolicyRequest* policy_request = request->mutable_policy_request();
  for (const auto& type_to_fetch : types_to_fetch_) {
    em::PolicyFetchRequest* fetch_request = policy_request->add_requests();
    fetch_request->set_policy_type(type_to_fetch.first);
    if (!type_to_fetch.second.empty()) {
      fetch_request->set_settings_entity_id(type_to_fetch.second);
    }

    // Request signed policy blobs to help prevent tampering on the client.
    fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    if (public_key_version_valid_) {
      fetch_request->set_public_key_version(public_key_version_);
    }

    fetch_request->set_verification_key_hash(kPolicyVerificationKeyHash);

    // These fields are included only in requests for chrome policy.
    if (IsChromePolicy(type_to_fetch.first)) {
      if (!device_dm_token_.empty()) {
        fetch_request->set_device_dm_token(device_dm_token_);
      }
      if (!last_policy_timestamp_.is_null()) {
        fetch_request->set_timestamp(last_policy_timestamp_.ToJavaTime());
      }
      if (!invalidation_payload_.empty()) {
        fetch_request->set_invalidation_version(invalidation_version_);
        fetch_request->set_invalidation_payload(invalidation_payload_);
      }
    }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // Only set browser device identifier for CBCM Chrome cloud policy on
    // desktop.
    if (type_to_fetch.first ==
        dm_protocol::kChromeMachineLevelUserCloudPolicyType) {
      fetch_request->set_allocated_browser_device_identifier(
          GetBrowserDeviceIdentifier().release());
    }
#endif
  }

  // Add device state keys.
  if (!state_keys_to_upload_.empty()) {
    em::DeviceStateKeyUpdateRequest* key_update_request =
        request->mutable_device_state_key_update_request();
    for (std::vector<std::string>::const_iterator key(
             state_keys_to_upload_.begin());
         key != state_keys_to_upload_.end(); ++key) {
      key_update_request->add_server_backed_state_keys(*key);
    }
  }

  // Set the fetched invalidation version to the latest invalidation version
  // since it is now the invalidation version used for the latest fetch.
  fetched_invalidation_version_ = invalidation_version_;

  unique_request_job_ = service_->CreateJob(std::move(config));
}

void CloudPolicyClient::UploadPolicyValidationReport(
    CloudPolicyValidatorBase::Status status,
    const std::vector<ValueValidationIssue>& value_validation_issues,
    const std::string& policy_type,
    const std::string& policy_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());

  ResultCallback callback = base::DoNothing();
  std::unique_ptr<DMServerJobConfiguration> config =
      CreateReportUploadJobConfiguration(
          DeviceManagementService::JobConfiguration::
              TYPE_UPLOAD_POLICY_VALIDATION_REPORT,
          std::move(callback));

  em::DeviceManagementRequest* request = config->request();
  em::PolicyValidationReportRequest* policy_validation_report_request =
      request->mutable_policy_validation_report_request();

  policy_validation_report_request->set_policy_type(policy_type);
  policy_validation_report_request->set_policy_token(policy_token);
  policy_validation_report_request->set_validation_result_type(
      TranslatePolicyValidationResult(status));

  for (const ValueValidationIssue& issue : value_validation_issues) {
    em::PolicyValueValidationIssue* proto_result =
        policy_validation_report_request->add_policy_value_validation_issues();
    proto_result->set_policy_name(issue.policy_name);
    proto_result->set_severity(
        TranslatePolicyValidationResultSeverity(issue.severity));
    proto_result->set_debug_message(issue.message);
  }

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::FetchRobotAuthCodes(
    DMAuth auth,
    enterprise_management::DeviceServiceApiAccessRequest::DeviceType
        device_type,
    const std::set<std::string>& oauth_scopes,
    RobotAuthCodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());
  DCHECK(auth.has_dm_token());

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH, this,
      /*critical=*/false, std::move(auth),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnFetchRobotAuthCodesCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceServiceApiAccessRequest* request =
      config->request()->mutable_service_api_access_request();
  request->set_oauth2_client_id(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id());

  for (const auto& scope : oauth_scopes) {
    request->add_auth_scopes(scope);
  }

  request->set_device_type(device_type);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadEnterpriseMachineCertificate(
    const std::string& certificate_data,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UploadCertificate(certificate_data,
                    em::DeviceCertUploadRequest::ENTERPRISE_MACHINE_CERTIFICATE,
                    std::move(callback));
}

void CloudPolicyClient::UploadEnterpriseEnrollmentCertificate(
    const std::string& certificate_data,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UploadCertificate(
      certificate_data,
      em::DeviceCertUploadRequest::ENTERPRISE_ENROLLMENT_CERTIFICATE,
      std::move(callback));
}

void CloudPolicyClient::UploadEnterpriseEnrollmentId(
    const std::string& enrollment_id,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<DMServerJobConfiguration> config =
      CreateCertUploadJobConfiguration(std::move(callback));
  em::DeviceManagementRequest* request = config->request();
  em::DeviceCertUploadRequest* upload_request =
      request->mutable_cert_upload_request();
  upload_request->set_enrollment_id(enrollment_id);
  ExecuteCertUploadJob(std::move(config));
}

void CloudPolicyClient::UploadDeviceStatus(
    const em::DeviceStatusReportRequest* device_status,
    const em::SessionStatusReportRequest* session_status,
    const em::ChildStatusReportRequest* child_status,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Should pass in at least one type of status.
  DCHECK(device_status || session_status || child_status);

  if (!is_registered()) {
    std::move(callback).Run(Result(NotRegistered()));
    return;
  }

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_STATUS, this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_), oauth_token_,
      base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceManagementRequest* request = config->request();
  if (device_status) {
    *request->mutable_device_status_report_request() = *device_status;
  }
  if (session_status) {
    *request->mutable_session_status_report_request() = *session_status;
  }
  if (child_status) {
    *request->mutable_child_status_report_request() = *child_status;
  }

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadChromeDesktopReport(
    std::unique_ptr<em::ChromeDesktopReportRequest> chrome_desktop_report,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(chrome_desktop_report);

  if (!is_registered()) {
    std::move(callback).Run(Result(NotRegistered()));
    return;
  }

  std::unique_ptr<DMServerJobConfiguration> config =
      CreateReportUploadJobConfiguration(
          DeviceManagementService::JobConfiguration::TYPE_CHROME_DESKTOP_REPORT,
          std::move(callback));

  config->request()->set_allocated_chrome_desktop_report_request(
      chrome_desktop_report.release());

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadChromeOsUserReport(
    std::unique_ptr<em::ChromeOsUserReportRequest> chrome_os_user_report,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(chrome_os_user_report);

  if (!is_registered()) {
    std::move(callback).Run(Result(NotRegistered()));
    return;
  }

  std::unique_ptr<DMServerJobConfiguration> config =
      CreateReportUploadJobConfiguration(
          DeviceManagementService::JobConfiguration::TYPE_CHROME_OS_USER_REPORT,
          std::move(callback));

  config->request()->set_allocated_chrome_os_user_report_request(
      chrome_os_user_report.release());

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadChromeProfileReport(
    std::unique_ptr<em::ChromeProfileReportRequest> chrome_profile_report,
    CloudPolicyClient::ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(chrome_profile_report);

  if (!is_registered()) {
    std::move(callback).Run(Result(NotRegistered()));
    return;
  }

  std::unique_ptr<DMServerJobConfiguration> config =
      CreateReportUploadJobConfiguration(
          DeviceManagementService::JobConfiguration::TYPE_CHROME_PROFILE_REPORT,
          std::move(callback));

  config->request()->set_allocated_chrome_profile_report_request(
      chrome_profile_report.release());

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadSecurityEventReport(
    content::BrowserContext* context,
    bool include_device_info,
    base::Value::Dict report,
    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_registered()) {
    std::move(callback).Run(CloudPolicyClient::Result(NotRegistered()));
    return;
  }

  CreateNewRealtimeReportingJob(
      std::move(report),
      service()->configuration()->GetReportingConnectorServerUrl(context),
      include_device_info, add_connector_url_params_, std::move(callback));
}

void CloudPolicyClient::UploadEncryptedReport(
    base::Value::Dict merging_payload,
    absl::optional<base::Value::Dict> context,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_registered()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto config = std::make_unique<EncryptedReportingJobConfiguration>(
      GetURLLoaderFactory(), DMAuth::FromDMToken(dm_token()),
      service()->configuration()->GetEncryptedReportingServerUrl(),
      std::move(merging_payload), dm_token(), client_id(),
      base::BindOnce(&CloudPolicyClient::OnEncryptedReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  if (context.has_value()) {
    config->UpdateContext(std::move(context.value()));
  }
  const auto delay = config->WhenIsAllowedToProceed();
  if (delay.is_positive()) {
    // Reject upload.
    config->CancelNotAllowedJob();  // Invokes callback to response back.
    return;
  }
  // Accept upload.
  config->AccountForAllowedJob();
  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadAppInstallReport(base::Value::Dict report,
                                               ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_registered()) {
    std::move(callback).Run(CloudPolicyClient::Result(NotRegistered()));
    return;
  }

  CancelAppInstallReportUpload();
  app_install_report_request_job_ = CreateNewRealtimeReportingJob(
      std::move(report),
      service()->configuration()->GetRealtimeReportingServerUrl(),
      /* include_device_info */ true, /* add_connector_url_params=*/false,
      std::move(callback));
  DCHECK(app_install_report_request_job_);
}

void CloudPolicyClient::CancelAppInstallReportUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (app_install_report_request_job_) {
    RemoveJob(app_install_report_request_job_);
    DCHECK_EQ(app_install_report_request_job_, nullptr);
  }
}

void CloudPolicyClient::UploadExtensionInstallReport(base::Value::Dict report,
                                                     ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_registered()) {
    std::move(callback).Run(CloudPolicyClient::Result(NotRegistered()));
    return;
  }

  CancelExtensionInstallReportUpload();
  extension_install_report_request_job_ = CreateNewRealtimeReportingJob(
      std::move(report),
      service()->configuration()->GetRealtimeReportingServerUrl(),
      /* include_device_info */ true,
      /* add_connector_url_params=*/false, std::move(callback));
  DCHECK(extension_install_report_request_job_);
}

void CloudPolicyClient::CancelExtensionInstallReportUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extension_install_report_request_job_) {
    RemoveJob(extension_install_report_request_job_);
    DCHECK_EQ(extension_install_report_request_job_, nullptr);
  }
}

void CloudPolicyClient::FetchRemoteCommands(
    std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
    const std::vector<em::RemoteCommandResult>& command_results,
    em::PolicyFetchRequest::SignatureType signature_type,
    RemoteCommandCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());

  // Unsigned commands and NONE signature are not supported.
  DCHECK_NE(signature_type, em::PolicyFetchRequest::NONE);

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_REMOTE_COMMANDS, this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnRemoteCommandsFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceRemoteCommandRequest* const request =
      config->request()->mutable_remote_command_request();

  if (last_command_id) {
    request->set_last_command_unique_id(*last_command_id);
  }

  for (const auto& command_result : command_results) {
    *request->add_command_results() = command_result;
  }

  request->set_send_secure_commands(true);
  request->set_signature_type(signature_type);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

DeviceManagementService::Job* CloudPolicyClient::CreateNewRealtimeReportingJob(
    base::Value::Dict report,
    const std::string& server_url,
    bool include_device_info,
    bool add_connector_url_params,
    ResultCallback callback) {
  std::unique_ptr<RealtimeReportingJobConfiguration> config =
      std::make_unique<RealtimeReportingJobConfiguration>(
          this, server_url, include_device_info, add_connector_url_params,
          base::BindOnce(&CloudPolicyClient::OnRealtimeReportUploadCompleted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  config->AddReport(std::move(report));
  request_jobs_.push_back(service_->CreateJob(std::move(config)));
  return request_jobs_.back().get();
}

void CloudPolicyClient::GetDeviceAttributeUpdatePermission(
    DMAuth auth,
    CloudPolicyClient::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());
  DCHECK(auth.has_oauth_token() || auth.has_dm_token());
  const bool has_oauth_token = auth.has_oauth_token();
  const std::string oauth_token =
      has_oauth_token ? auth.oauth_token() : std::string();
  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::
          TYPE_ATTRIBUTE_UPDATE_PERMISSION,
      this,
      /*critical=*/false, !has_oauth_token ? auth.Clone() : DMAuth::NoAuth(),
      oauth_token,
      base::BindOnce(
          &CloudPolicyClient::OnDeviceAttributeUpdatePermissionCompleted,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  config->request()->mutable_device_attribute_update_permission_request();

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateDeviceAttributes(
    DMAuth auth,
    const std::string& asset_id,
    const std::string& location,
    CloudPolicyClient::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());
  DCHECK(auth.has_oauth_token() || auth.has_dm_token());

  const bool has_oauth_token = auth.has_oauth_token();
  const std::string oauth_token =
      has_oauth_token ? auth.oauth_token() : std::string();
  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_ATTRIBUTE_UPDATE, this,
      /*critical=*/false, !has_oauth_token ? auth.Clone() : DMAuth::NoAuth(),
      oauth_token,
      base::BindOnce(&CloudPolicyClient::OnDeviceAttributeUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::DeviceAttributeUpdateRequest* request =
      config->request()->mutable_device_attribute_update_request();

  request->set_asset_id(asset_id);
  request->set_location(location);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateGcmId(
    const std::string& gcm_id,
    CloudPolicyClient::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_GCM_ID_UPDATE, this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnGcmIdUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  em::GcmIdUpdateRequest* const request =
      config->request()->mutable_gcm_id_update_request();

  request->set_gcm_id(gcm_id);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UploadEuiccInfo(
    std::unique_ptr<enterprise_management::UploadEuiccInfoRequest> request,
    CloudPolicyClient::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());

  auto config = std::make_unique<DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_EUICC_INFO,
      /*client=*/this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnEuiccInfoUploaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  config->request()->set_allocated_upload_euicc_info_request(request.release());
  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::OnEuiccInfoUploaded(StatusCallback callback,
                                            DMServerJobResult result) {
  last_dm_status_ = result.dm_status;
  if (last_dm_status_ != DM_STATUS_SUCCESS) {
    NotifyClientError();
  }

  std::move(callback).Run(result.dm_status == DM_STATUS_SUCCESS);
  RemoveJob(result.job);
}

void CloudPolicyClient::ClientCertProvisioningRequest(
    em::ClientCertificateProvisioningRequest request,
    ClientCertProvisioningRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_registered());

  if (!device_dm_token_.empty()) {
    request.set_device_dm_token(device_dm_token_);
  }

  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      DeviceManagementService::JobConfiguration::TYPE_CERT_PROVISIONING_REQUEST,
      this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(
          &CloudPolicyClient::OnClientCertProvisioningRequestResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  *config->request()->mutable_client_certificate_provisioning_request() =
      std::move(request);

  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::UpdateServiceAccount(const std::string& account_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NotifyServiceAccountSet(account_email);
}

void CloudPolicyClient::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void CloudPolicyClient::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

void CloudPolicyClient::AddPolicyTypeToFetch(
    const std::string& policy_type,
    const std::string& settings_entity_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  types_to_fetch_.insert(std::make_pair(policy_type, settings_entity_id));
}

void CloudPolicyClient::RemovePolicyTypeToFetch(
    const std::string& policy_type,
    const std::string& settings_entity_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  types_to_fetch_.erase(std::make_pair(policy_type, settings_entity_id));
}

void CloudPolicyClient::SetStateKeysToUpload(
    const std::vector<std::string>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  state_keys_to_upload_ = keys;
}

const em::PolicyFetchResponse* CloudPolicyClient::GetPolicyFor(
    const std::string& policy_type,
    const std::string& settings_entity_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = last_policy_fetch_responses_.find(
      std::make_pair(policy_type, settings_entity_id));
  return it == last_policy_fetch_responses_.end() ? nullptr : &it->second;
}

scoped_refptr<network::SharedURLLoaderFactory>
CloudPolicyClient::GetURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return url_loader_factory_;
}

int CloudPolicyClient::GetActiveRequestCountForTest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return request_jobs_.size();
}

void CloudPolicyClient::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_loader_factory_ = factory;
}

void CloudPolicyClient::UploadCertificate(
    const std::string& certificate_data,
    em::DeviceCertUploadRequest::CertificateType certificate_type,
    CloudPolicyClient::ResultCallback callback) {
  std::unique_ptr<DMServerJobConfiguration> config =
      CreateCertUploadJobConfiguration(std::move(callback));
  PrepareCertUploadRequest(config.get(), certificate_data, certificate_type);
  ExecuteCertUploadJob(std::move(config));
}

void CloudPolicyClient::PrepareCertUploadRequest(
    DMServerJobConfiguration* config,
    const std::string& certificate_data,
    enterprise_management::DeviceCertUploadRequest::CertificateType
        certificate_type) {
  em::DeviceManagementRequest* request = config->request();
  em::DeviceCertUploadRequest* upload_request =
      request->mutable_cert_upload_request();
  upload_request->set_device_certificate(certificate_data);
  upload_request->set_certificate_type(certificate_type);
}

std::unique_ptr<DMServerJobConfiguration>
CloudPolicyClient::CreateCertUploadJobConfiguration(
    CloudPolicyClient::ResultCallback callback) {
  CHECK(is_registered());
  return std::make_unique<DMServerJobConfiguration>(
      service_,
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_CERTIFICATE,
      client_id(),
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt, GetURLLoaderFactory(),
      base::BindOnce(&CloudPolicyClient::OnCertificateUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

std::unique_ptr<DMServerJobConfiguration>
CloudPolicyClient::CreateReportUploadJobConfiguration(
    DeviceManagementService::JobConfiguration::JobType type,
    CloudPolicyClient::ResultCallback callback) {
  return std::make_unique<DMServerJobConfiguration>(
      type, this,
      /*critical=*/false, DMAuth::FromDMToken(dm_token_),
      /*oauth_token=*/absl::nullopt,
      base::BindOnce(&CloudPolicyClient::OnReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudPolicyClient::ExecuteCertUploadJob(
    std::unique_ptr<DMServerJobConfiguration> config) {
  request_jobs_.push_back(service_->CreateJob(std::move(config)));
}

void CloudPolicyClient::OnRegisterCompleted(DMServerJobResult result) {
  if (result.dm_status == DM_STATUS_SUCCESS) {
    if (!result.response.has_register_response() ||
        !result.response.register_response().has_device_management_token()) {
      LOG_POLICY(WARNING, CBCM_ENROLLMENT) << "Invalid registration response.";
      result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
    } else if (!reregistration_dm_token_.empty() &&
               reregistration_dm_token_ != result.response.register_response()
                                               .device_management_token()) {
      LOG_POLICY(WARNING, CBCM_ENROLLMENT)
          << "Reregistration DMToken mismatch.";
      result.dm_status = DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID;
    }
  }

  last_dm_status_ = result.dm_status;
  if (result.dm_status == DM_STATUS_SUCCESS) {
    dm_token_ = result.response.register_response().device_management_token();
    reregistration_dm_token_.clear();
    if (result.response.register_response().has_configuration_seed()) {
      absl::optional<base::Value> configuration_seed = base::JSONReader::Read(
          result.response.register_response().configuration_seed(),
          base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
      if (configuration_seed && configuration_seed->is_dict()) {
        configuration_seed_ = std::make_unique<base::Value::Dict>(
            std::move(*configuration_seed).TakeDict());
      } else {
        configuration_seed_.reset();
        LOG_POLICY(ERROR, CBCM_ENROLLMENT)
            << "Failed to parse configuration seed";
      }
    }
    DVLOG_POLICY(1, CBCM_ENROLLMENT)
        << "Client registration complete - DMToken = " << dm_token_;

    // Device mode is only relevant for device policy really, it's the
    // responsibility of the consumer of the field to check validity.
    device_mode_ = DEVICE_MODE_NOT_SET;
    if (result.response.register_response().has_enrollment_type()) {
      device_mode_ = TranslateProtobufDeviceMode(
          result.response.register_response().enrollment_type());
    }

    if (device_dm_token_callback_) {
      std::vector<std::string> user_affiliation_ids(
          result.response.register_response().user_affiliation_ids().begin(),
          result.response.register_response().user_affiliation_ids().end());
      device_dm_token_ = device_dm_token_callback_.Run(user_affiliation_ids);
    }
    NotifyRegistrationStateChanged();
  } else {
    NotifyClientError();
  }
}

void CloudPolicyClient::OnFetchRobotAuthCodesCompleted(
    RobotAuthCodeCallback callback,
    DMServerJobResult result) {
  // Remove the job before executing the callback because |this| might be
  // deleted during the callback.
  RemoveJob(result.job);

  if (result.dm_status == DM_STATUS_SUCCESS &&
      (!result.response.has_service_api_access_response())) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Invalid service api access response.";
    result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }
  last_dm_status_ = result.dm_status;
  if (result.dm_status == DM_STATUS_SUCCESS) {
    DVLOG_POLICY(1, CBCM_ENROLLMENT)
        << "Device robot account auth code fetch complete - code = "
        << result.response.service_api_access_response().auth_code();
    std::move(callback).Run(
        result.dm_status,
        result.response.service_api_access_response().auth_code());
  } else {
    std::move(callback).Run(result.dm_status, std::string());
  }
  // |this| might be deleted at this point.
}

void CloudPolicyClient::OnPolicyFetchCompleted(DMServerJobResult result) {
  if (result.dm_status == DM_STATUS_SUCCESS) {
    if (!result.response.has_policy_response() ||
        result.response.policy_response().responses_size() == 0) {
      LOG_POLICY(WARNING, CBCM_ENROLLMENT) << "Empty policy response.";
      result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
    }
  }

  last_dm_status_ = result.dm_status;
  if (result.dm_status == DM_STATUS_SUCCESS) {
    const em::DevicePolicyResponse& policy_response =
        result.response.policy_response();
    bool is_first_response = last_policy_fetch_responses_.empty();
    last_policy_fetch_responses_.clear();
    for (int i = 0; i < policy_response.responses_size(); ++i) {
      const em::PolicyFetchResponse& fetch_response =
          policy_response.responses(i);
      em::PolicyData policy_data;
      if (!policy_data.ParseFromString(fetch_response.policy_data()) ||
          !policy_data.IsInitialized() || !policy_data.has_policy_type()) {
        LOG_POLICY(WARNING, CBCM_ENROLLMENT)
            << "Invalid PolicyData received, ignoring";
        continue;
      }
      const std::string& type = policy_data.policy_type();
      if (is_first_response && type == dm_protocol::kChromeDevicePolicyType) {
        // Log histogram on first device policy fetch response to check the
        // state keys. No need to worry about possibility of multiple responses
        // of this type. There's only one device policy possible.
        base::UmaHistogramBoolean("Ash.StateKeysPresent2",
                                  !state_keys_to_upload_.empty());
      }
      std::string entity_id;
      if (policy_data.has_settings_entity_id()) {
        entity_id = policy_data.settings_entity_id();
      }
      std::pair<std::string, std::string> key(type, entity_id);
      if (base::Contains(last_policy_fetch_responses_, key)) {
        LOG_POLICY(WARNING, CBCM_ENROLLMENT)
            << "Duplicate PolicyFetchResponse for type: " << type
            << ", entity: " << entity_id << ", ignoring";
        continue;
      }
      last_policy_fetch_responses_[key] = fetch_response;
    }
    state_keys_to_upload_.clear();
    NotifyPolicyFetched();

    VLOG_POLICY(2, CBCM_ENROLLMENT) << "Policy fetch succeeded";
  } else {
    NotifyClientError();

    VLOG_POLICY(2, CBCM_ENROLLMENT)
        << "Policy fetching failed with DM status error: " << last_dm_status_;

    if (result.dm_status == DM_STATUS_SERVICE_DEVICE_NOT_FOUND ||
        result.dm_status == DM_STATUS_SERVICE_DEVICE_NEEDS_RESET) {
      // Mark as unregistered and initialize re-registration flow.
      reregistration_dm_token_ = dm_token_;
      dm_token_.clear();
      NotifyRegistrationStateChanged();
    }
  }
}

void CloudPolicyClient::OnCertificateUploadCompleted(
    CloudPolicyClient::ResultCallback callback,
    DMServerJobResult result) {
  last_dm_status_ = result.dm_status;
  if (result.dm_status != DM_STATUS_SUCCESS) {
    NotifyClientError();
  } else if (result.dm_status == DM_STATUS_SUCCESS &&
             !result.response.has_cert_upload_response()) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Empty upload certificate response.";
    result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }
  std::move(callback).Run(CloudPolicyClient::Result(result.dm_status));
  RemoveJob(result.job);
}

void CloudPolicyClient::OnDeviceAttributeUpdatePermissionCompleted(
    CloudPolicyClient::StatusCallback callback,
    DMServerJobResult result) {
  bool success = false;

  if (result.dm_status == DM_STATUS_SUCCESS &&
      !result.response.has_device_attribute_update_permission_response()) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Invalid device attribute update permission response.";
    result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  last_dm_status_ = result.dm_status;
  if (result.dm_status == DM_STATUS_SUCCESS &&
      result.response.device_attribute_update_permission_response()
          .has_result() &&
      result.response.device_attribute_update_permission_response().result() ==
          em::DeviceAttributeUpdatePermissionResponse::
              ATTRIBUTE_UPDATE_ALLOWED) {
    success = true;
  }
  std::move(callback).Run(success);
  RemoveJob(result.job);
}

void CloudPolicyClient::OnDeviceAttributeUpdated(
    CloudPolicyClient::StatusCallback callback,
    DMServerJobResult result) {
  bool success = false;

  if (result.dm_status == DM_STATUS_SUCCESS &&
      !result.response.has_device_attribute_update_response()) {
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Invalid device attribute update response.";
    result.dm_status = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  last_dm_status_ = result.dm_status;
  if (result.dm_status == DM_STATUS_SUCCESS &&
      result.response.device_attribute_update_response().has_result() &&
      result.response.device_attribute_update_response().result() ==
          em::DeviceAttributeUpdateResponse::ATTRIBUTE_UPDATE_SUCCESS) {
    success = true;
  }

  std::move(callback).Run(success);
  RemoveJob(result.job);
}

void CloudPolicyClient::RemoveJob(const DeviceManagementService::Job* job) {
  if (app_install_report_request_job_ == job) {
    app_install_report_request_job_ = nullptr;
  } else if (extension_install_report_request_job_ == job) {
    extension_install_report_request_job_ = nullptr;
  }
  for (auto it = request_jobs_.begin(); it != request_jobs_.end(); ++it) {
    if (it->get() == job) {
      request_jobs_.erase(it);
      return;
    }
  }
  // This job was already deleted from our list, somehow. This shouldn't
  // happen since deleting the job should cancel the callback.
  NOTREACHED();
}

void CloudPolicyClient::OnReportUploadCompleted(ResultCallback callback,
                                                DMServerJobResult result) {
  last_dm_status_ = result.dm_status;
  if (result.dm_status != DM_STATUS_SUCCESS) {
    NotifyClientError();
  }

  std::move(callback).Run(Result(result.dm_status));
  RemoveJob(result.job);
}

void CloudPolicyClient::OnRealtimeReportUploadCompleted(
    ResultCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int reponse_code,
    absl::optional<base::Value::Dict> response) {
  last_dm_status_ = status;
  if (status != DM_STATUS_SUCCESS) {
    NotifyClientError();
  }

  std::move(callback).Run(CloudPolicyClient::Result(status));
  RemoveJob(job);
}

// |job| can be null if the owning EncryptedReportingJobConfiguration is
// destroyed prior to calling OnUploadComplete. In that case, callback will be
// called with nullopt value.
void CloudPolicyClient::OnEncryptedReportUploadCompleted(
    ResponseCallback callback,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int reponse_code,
    absl::optional<base::Value::Dict> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (job == nullptr) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  last_dm_status_ = status;
  if (status != DM_STATUS_SUCCESS) {
    NotifyClientError();
  }
  std::move(callback).Run(std::move(response));
  RemoveJob(job);
}

void CloudPolicyClient::OnRemoteCommandsFetched(RemoteCommandCallback callback,
                                                DMServerJobResult result) {
  auto [decoded_status, commands] =
      DecodeRemoteCommands(result.dm_status, result.response);

  std::move(callback).Run(decoded_status, commands);
  RemoveJob(result.job);
}

void CloudPolicyClient::OnGcmIdUpdated(StatusCallback callback,
                                       DMServerJobResult result) {
  last_dm_status_ = result.dm_status;
  if (last_dm_status_ != DM_STATUS_SUCCESS) {
    NotifyClientError();
  }

  std::move(callback).Run(result.dm_status == DM_STATUS_SUCCESS);
  RemoveJob(result.job);
}

void CloudPolicyClient::OnClientCertProvisioningRequestResponse(
    ClientCertProvisioningRequestCallback callback,
    DMServerJobResult result) {
  base::ScopedClosureRunner job_cleaner(base::BindOnce(
      &CloudPolicyClient::RemoveJob, base::Unretained(this), result.job));

  last_dm_status_ = result.dm_status;
  // For DM_STATUS_SUCCESS, always expect that the response contains the correct
  // sub-proto. Forward other error codes without modifying them even if no
  // response sub-proto is filled.
  if (last_dm_status_ == DM_STATUS_SUCCESS &&
      !result.response.has_client_certificate_provisioning_response()) {
    last_dm_status_ = DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  std::move(callback).Run(
      last_dm_status_,
      result.response.client_certificate_provisioning_response());
}

void CloudPolicyClient::NotifyPolicyFetched() {
  for (auto& observer : observers_) {
    observer.OnPolicyFetched(this);
  }
}

void CloudPolicyClient::NotifyRegistrationStateChanged() {
  for (auto& observer : observers_) {
    observer.OnRegistrationStateChanged(this);
  }
}

void CloudPolicyClient::NotifyClientError() {
  for (auto& observer : observers_) {
    observer.OnClientError(this);
  }
}

void CloudPolicyClient::NotifyServiceAccountSet(
    const std::string& account_email) {
  for (auto& observer : observers_) {
    observer.OnServiceAccountSet(this, account_email);
  }
}

void CloudPolicyClient::CreateDeviceRegisterRequest(
    const RegistrationParameters& params,
    const std::string& client_id,
    em::DeviceRegisterRequest* request) {
  if (!client_id.empty()) {
    request->set_reregister(true);
  }
  request->set_type(params.registration_type);
  request->set_flavor(params.flavor);
  request->set_lifetime(params.lifetime);
  if (!machine_id_.empty()) {
    request->set_machine_id(machine_id_);
  }
  if (!machine_model_.empty()) {
    request->set_machine_model(machine_model_);
  }
  if (!brand_code_.empty()) {
    request->set_brand_code(brand_code_);
  }
  if (!attested_device_id_.empty()) {
    request->mutable_device_register_identification()->set_attested_device_id(
        attested_device_id_);
  }
  if (!ethernet_mac_address_.empty()) {
    request->set_ethernet_mac_address(ethernet_mac_address_);
  }
  if (!dock_mac_address_.empty()) {
    request->set_dock_mac_address(dock_mac_address_);
  }
  if (!manufacture_date_.empty()) {
    request->set_manufacture_date(manufacture_date_);
  }
  if (!params.requisition.empty()) {
    request->set_requisition(params.requisition);
  }
  if (!params.current_state_key.empty()) {
    request->set_server_backed_state_key(params.current_state_key);
  }
  if (params.psm_execution_result.has_value()) {
    request->set_psm_execution_result(params.psm_execution_result.value());
  }
  if (params.psm_determination_timestamp.has_value()) {
    request->set_psm_determination_timestamp_ms(
        params.psm_determination_timestamp.value());
  }
  if (params.license_type.has_value()) {
    request->mutable_license_type()->set_license_type(
        params.license_type.value());
  }
}

void CloudPolicyClient::CreateUniqueRequestJob(
    std::unique_ptr<RegistrationJobConfiguration> config) {
  unique_request_job_ = service_->CreateJob(std::move(config));
}

}  // namespace policy
