// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"

#include <stdint.h>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state_message_processor.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;
namespace psm_rlwe = ::private_membership::rlwe;
using EnrollmentCheckType =
    em::DeviceAutoEnrollmentRequest::EnrollmentCheckType;

// Returns the power of the next power-of-2 starting at |value|.
int NextPowerOf2(int64_t value) {
  for (int i = 0; i <= AutoEnrollmentClient::kMaximumPower; ++i) {
    if ((INT64_C(1) << i) >= value)
      return i;
  }
  // No other value can be represented in an int64_t.
  return AutoEnrollmentClient::kMaximumPower + 1;
}

// Provides device identifier for Forced Re-Enrollment (FRE), where the
// server-backed state key is used. It will set the identifier for the
// DeviceAutoEnrollmentRequest.
class DeviceIdentifierProviderFRE {
 public:
  explicit DeviceIdentifierProviderFRE(
      const std::string& server_backed_state_key) {
    CHECK(!server_backed_state_key.empty());
    server_backed_state_key_hash_ =
        crypto::SHA256HashString(server_backed_state_key);
  }

  DeviceIdentifierProviderFRE(const DeviceIdentifierProviderFRE&) = delete;
  DeviceIdentifierProviderFRE& operator=(const DeviceIdentifierProviderFRE&) =
      delete;

  ~DeviceIdentifierProviderFRE() = default;

  // Should return the `EnrollmentCheckType` to be used in the
  // DeviceAutoEnrollmentRequest. This specifies the identifier set used on
  // the server.
  em::DeviceAutoEnrollmentRequest::EnrollmentCheckType GetEnrollmentCheckType()
      const {
    return em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FRE;
  }

  // Should return the hash of this device's identifier. The
  // DeviceAutoEnrollmentRequest exchange will check if this hash is in the
  // server-side identifier set specified by `GetEnrollmentCheckType()`
  const std::string& GetIdHash() const { return server_backed_state_key_hash_; }

 private:
  // SHA-256 digest of the stable identifier.
  std::string server_backed_state_key_hash_;
};

}  // namespace

enum class AutoEnrollmentClientImpl::ServerStateAvailabilityResult {
  // Indicates that request has been successful and server state availability is
  // known.
  kSuccess = 0,
  // Indicates a connection error during request.
  kConnectionError = 1,
  // Indicates an invalid response from server.
  kServerError = 2,
  // Special case for server state availability result via auto enrollment
  // request.
  // Indicates that request shall be immediately retried.
  kAutoEnrollmentRetriableError = 3,
  // Special case for server state availability result via PSM.
  // Indicates an internal non-recoverable error.
  kPsmInternalError = 4,
};

// Base class to handle server state availability requests.
class AutoEnrollmentClientImpl::ServerStateAvailabilityRequester {
 public:
  using CompletionCallback =
      base::OnceCallback<void(ServerStateAvailabilityResult)>;

  virtual ~ServerStateAvailabilityRequester() = default;

  // Initiates request and reports back with `callback` once request is
  // finished.
  virtual void Start(CompletionCallback callback) = 0;

  // Returns:
  // * nullopt if server state is not obtained yet,
  // * false if server state has been obtained and the answer is: it is not
  // available.
  // * true if server state has been obtained and the answer is: it is
  // available.
  virtual absl::optional<bool> GetServerStateIfObtained() const = 0;
};

// Responsible for resolving server state availability status via auto
// enrollment requests for force re-enrollment.
class AutoEnrollmentClientImpl::FREServerStateAvailabilityRequester
    : public ServerStateAvailabilityRequester {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
    registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
  }

  FREServerStateAvailabilityRequester(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state,
      const std::string& device_id,
      const std::string& uma_suffix,
      int current_power,
      int power_limit,
      const std::string& server_backed_state_key)
      : device_management_service_(device_management_service),
        url_loader_factory_(url_loader_factory),
        local_state_(local_state),
        device_id_(device_id),
        uma_suffix_(uma_suffix),
        current_power_(current_power),
        power_limit_(power_limit),
        device_identifier_provider_fre_(server_backed_state_key) {
    DCHECK_LE(current_power_, power_limit_);
  }

  FREServerStateAvailabilityRequester(
      const FREServerStateAvailabilityRequester&) = delete;
  FREServerStateAvailabilityRequester& operator=(
      const FREServerStateAvailabilityRequester&) = delete;

  void Start(CompletionCallback callback) override {
    StartImpl(std::move(callback));
  }

  absl::optional<bool> GetServerStateIfObtained() const override {
    const PrefService::Preference* has_server_state_pref =
        local_state_->FindPreference(prefs::kShouldAutoEnroll);
    const PrefService::Preference* previous_limit_pref =
        local_state_->FindPreference(prefs::kAutoEnrollmentPowerLimit);

    if (!has_server_state_pref || has_server_state_pref->IsDefaultValue() ||
        !previous_limit_pref || previous_limit_pref->IsDefaultValue()) {
      return absl::nullopt;
    }

    DCHECK(has_server_state_pref->GetValue()->is_bool());
    DCHECK(previous_limit_pref->GetValue()->is_int());

    if (power_limit_ > previous_limit_pref->GetValue()->GetInt()) {
      return absl::nullopt;
    }

    return has_server_state_pref->GetValue()->GetBool();
  }

 private:
  void StartImpl(CompletionCallback callback) {
    DCHECK(!request_job_);
    DCHECK(callback);
    DCHECK(!completion_callback_);

    completion_callback_ = std::move(callback);

    // Start the Hash dance timer during the first attempt.
    if (hash_dance_time_start_.is_null())
      hash_dance_time_start_ = base::TimeTicks::Now();

    std::string id_hash = device_identifier_provider_fre_.GetIdHash();
    // Currently AutoEnrollmentClientImpl supports working with hashes that are
    // at least 8 bytes long. If this is reduced, the computation of the
    // remainder must also be adapted to handle the case of a shorter hash
    // gracefully.
    DCHECK_GE(id_hash.size(), 8u);

    uint64_t remainder = 0;
    const size_t last_byte_index = id_hash.size() - 1;
    for (int i = 0; 8 * i < current_power_; ++i) {
      uint64_t byte = id_hash[last_byte_index - i] & 0xff;
      remainder = remainder | (byte << (8 * i));
    }
    remainder = remainder & ((UINT64_C(1) << current_power_) - 1);

    // Record the time when the bucket download request is started. Note that
    // the time may be set multiple times. This is fine, only the last request
    // is the one where the hash bucket is actually downloaded.
    time_start_bucket_download_ = base::TimeTicks::Now();

    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "Request bucket #" << remainder;

    std::unique_ptr<DMServerJobConfiguration> config =
        std::make_unique<DMServerJobConfiguration>(
            device_management_service_,
            DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
            device_id_,
            /*critical=*/false, DMAuth::NoAuth(),
            /*oauth_token=*/absl::nullopt, url_loader_factory_,
            base::BindOnce(
                &FREServerStateAvailabilityRequester::HandleRequestCompletion,
                base::Unretained(this)));

    em::DeviceAutoEnrollmentRequest* request =
        config->request()->mutable_auto_enrollment_request();
    request->set_remainder(remainder);
    request->set_modulus(INT64_C(1) << current_power_);
    request->set_enrollment_check_type(
        device_identifier_provider_fre_.GetEnrollmentCheckType());

    request_job_ = device_management_service_->CreateJob(std::move(config));
  }

  void HandleRequestCompletion(DMServerJobResult result) {
    DCHECK(request_job_);
    DCHECK(completion_callback_);

    request_job_.reset();

    base::UmaHistogramSparse(kUMAHashDanceRequestStatus + uma_suffix_,
                             result.dm_status);
    // TODO(crbug.com/1312919): Check `result.dm_status` for specific errors.
    if (result.dm_status != DM_STATUS_SUCCESS) {
      LOG(ERROR) << "Auto enrollment error: " << result.dm_status;
      if (result.dm_status == DM_STATUS_REQUEST_FAILED)
        base::UmaHistogramSparse(kUMAHashDanceNetworkErrorCode + uma_suffix_,
                                 -result.net_error);
      RunCallback(result.dm_status == DM_STATUS_REQUEST_FAILED
                      ? ServerStateAvailabilityResult::kConnectionError
                      : ServerStateAvailabilityResult::kServerError);
      return;
    }

    ServerStateAvailabilityResult availability_result =
        ServerStateAvailabilityResult::kSuccess;
    const em::DeviceAutoEnrollmentResponse& enrollment_response =
        result.response.auto_enrollment_response();
    if (!result.response.has_auto_enrollment_response()) {
      LOG(ERROR) << "Server failed to provide auto-enrollment response.";
      availability_result = ServerStateAvailabilityResult::kServerError;
    } else if (enrollment_response.has_expected_modulus()) {
      // Server is asking us to retry with a different modulus.
      modulus_updates_received_++;

      int64_t modulus = enrollment_response.expected_modulus();
      int power = NextPowerOf2(modulus);
      if ((INT64_C(1) << power) != modulus) {
        LOG(ERROR) << "Auto enrollment: the server didn't ask for a power-of-2 "
                   << "modulus. Using the closest power-of-2 instead "
                   << "(" << modulus << " vs 2^" << power << ")";
        availability_result = ServerStateAvailabilityResult::kServerError;
      }
      if (modulus_updates_received_ >= 2) {
        LOG(ERROR) << "Auto enrollment error: already retried with an updated "
                   << "modulus but the server asked for a new one again: "
                   << power;
        availability_result = ServerStateAvailabilityResult::kServerError;
      } else if (power > power_limit_) {
        LOG(ERROR) << "Auto enrollment error: the server asked for a larger "
                   << "modulus than the client accepts (" << power << " vs "
                   << power_limit_ << ").";
        availability_result = ServerStateAvailabilityResult::kServerError;
      } else {
        // Retry at most once with the modulus that the server requested.
        if (power <= current_power_) {
          LOG(WARNING) << "Auto enrollment: the server asked to use a modulus ("
                       << power << ") that isn't larger than the first used ("
                       << current_power_ << "). Retrying anyway.";
        }
        // Remember this value, so that eventual retries start with the correct
        // modulus.
        current_power_ = power;
        DCHECK(!GetServerStateIfObtained());
        RunCallback(
            ServerStateAvailabilityResult::kAutoEnrollmentRetriableError);
        return;
      }
    } else {
      // Server should have sent down a list of hashes to try.
      const bool has_server_state =
          IsIdHashInProtobuf(enrollment_response.hashes());
      // Cache the current decision in local_state, so that it is reused in case
      // the device reboots before enrolling.
      local_state_->SetBoolean(prefs::kShouldAutoEnroll, has_server_state);
      local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
      local_state_->CommitPendingWrite();

      // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's
      // preserved in the logs.
      LOG(WARNING) << "Received has_state=" << has_server_state;

      availability_result = ServerStateAvailabilityResult::kSuccess;
      RecordHashDanceSuccessTimeHistogram();
    }

    const bool succeeded_with_result =
        availability_result == ServerStateAvailabilityResult::kSuccess &&
        GetServerStateIfObtained();
    const bool failed_without_result =
        availability_result != ServerStateAvailabilityResult::kSuccess &&
        !GetServerStateIfObtained();
    DCHECK(succeeded_with_result || failed_without_result);

    // Bucket download done, update UMA.
    UpdateBucketDownloadTimingHistograms();
    RunCallback(availability_result);
  }

  void RunCallback(ServerStateAvailabilityResult availability_result) {
    DCHECK(completion_callback_);
    std::move(completion_callback_).Run(availability_result);
  }

  bool IsIdHashInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes) const {
    const std::string id_hash = device_identifier_provider_fre_.GetIdHash();
    for (int i = 0; i < hashes.size(); ++i) {
      if (hashes.Get(i) == id_hash)
        return true;
    }
    return false;
  }

  void UpdateBucketDownloadTimingHistograms() const {
    // These values determine bucketing of the histogram, they should not be
    // changed.
    // The minimum time can't be 0, must be at least 1.
    static const base::TimeDelta kMin = base::Milliseconds(1);
    static const base::TimeDelta kMax = base::Minutes(5);
    static const int kBuckets = 50;

    base::TimeTicks now = base::TimeTicks::Now();
    if (!hash_dance_time_start_.is_null()) {
      base::TimeDelta delta = now - hash_dance_time_start_;
      base::UmaHistogramCustomTimes(kUMAHashDanceProtocolTime + uma_suffix_,
                                    delta, kMin, kMax, kBuckets);
    }
    if (!time_start_bucket_download_.is_null()) {
      base::TimeDelta delta = now - time_start_bucket_download_;
      base::UmaHistogramCustomTimes(
          kUMAHashDanceBucketDownloadTime + uma_suffix_, delta, kMin, kMax,
          kBuckets);
    }
  }

  void RecordHashDanceSuccessTimeHistogram() const {
    // These values determine bucketing of the histogram, they should not be
    // changed.
    static const base::TimeDelta kMin = base::Milliseconds(1);
    static const base::TimeDelta kMax = base::Seconds(25);
    static const int kBuckets = 50;

    base::TimeTicks now = base::TimeTicks::Now();
    if (!hash_dance_time_start_.is_null()) {
      base::TimeDelta delta = now - hash_dance_time_start_;
      base::UmaHistogramCustomTimes(kUMAHashDanceSuccessTime + uma_suffix_,
                                    delta, kMin, kMax, kBuckets);
    }
  }

  raw_ptr<DeviceManagementService, ExperimentalAsh> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<PrefService, ExperimentalAsh> local_state_;
  const std::string device_id_;
  const std::string uma_suffix_;

  // Power-of-2 modulus to try next.
  int current_power_;

  // Power of the maximum power-of-2 modulus that this client will accept from
  // a retry response from the server.
  const int power_limit_;

  // Number of requests for a different modulus received from the server.
  // Used to determine if the server keeps asking for different moduli.
  int modulus_updates_received_ = 0;

  // Times used to determine the duration of the protocol, and the extra time
  // needed to complete after the signin was complete.
  // If `hash_dance_time_start_` is not null, the protocol is still running.
  base::TimeTicks hash_dance_time_start_;

  // The time when the bucket download part of the protocol started.
  base::TimeTicks time_start_bucket_download_;

  std::unique_ptr<DeviceManagementService::Job> request_job_;

  const DeviceIdentifierProviderFRE device_identifier_provider_fre_;

  CompletionCallback completion_callback_;
};

// Responsible for resolving server state availability status via private
// membership check requests for initial enrollment.
class AutoEnrollmentClientImpl::InitialServerStateAvailabilityRequester
    : public ServerStateAvailabilityRequester {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterBooleanPref(prefs::kShouldRetrieveDeviceState, false);
    registry->RegisterIntegerPref(prefs::kEnrollmentPsmResult, -1);
    registry->RegisterTimePref(prefs::kEnrollmentPsmDeterminationTime,
                               base::Time());
  }

  explicit InitialServerStateAvailabilityRequester(
      std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client,
      PrefService* local_state)
      : psm_rlwe_dmserver_client_(std::move(psm_rlwe_dmserver_client)),
        local_state_(local_state) {}

  InitialServerStateAvailabilityRequester(
      const InitialServerStateAvailabilityRequester&) = delete;
  InitialServerStateAvailabilityRequester& operator=(
      const InitialServerStateAvailabilityRequester&) = delete;

  void Start(CompletionCallback callback) override {
    StartImpl(std::move(callback));
  }

  absl::optional<bool> GetServerStateIfObtained() const override {
    const PrefService::Preference* has_psm_server_state_pref =
        local_state_->FindPreference(prefs::kShouldRetrieveDeviceState);

    if (!has_psm_server_state_pref ||
        has_psm_server_state_pref->IsDefaultValue()) {
      return absl::nullopt;
    }

    DCHECK(has_psm_server_state_pref->GetValue()->is_bool());

    return has_psm_server_state_pref->GetValue()->GetBool();
  }

 private:
  void StartImpl(CompletionCallback callback) {
    DCHECK(callback);
    DCHECK(!completion_callback_);
    DCHECK(!psm_rlwe_dmserver_client_->IsCheckMembershipInProgress());

    PrepareLocalState();

    completion_callback_ = std::move(callback);

    psm_rlwe_dmserver_client_->CheckMembership(base::BindOnce(
        &InitialServerStateAvailabilityRequester::HandlePsmCompletion,
        base::Unretained(this)));
  }

  void HandlePsmCompletion(
      psm::RlweDmserverClient::ResultHolder psm_result_holder) {
    UpdateLocalState(psm_result_holder);

    switch (psm_result_holder.psm_result) {
      case psm::RlweResult::kConnectionError:
        RunCallback(ServerStateAvailabilityResult::kConnectionError);
        break;
      case psm::RlweResult::kServerError:
        RunCallback(ServerStateAvailabilityResult::kServerError);
        break;

      case psm::RlweResult::kSuccessfulDetermination:
        DCHECK(GetServerStateIfObtained());
        RunCallback(ServerStateAvailabilityResult::kSuccess);
        break;

      // At the moment, `AutoEnrollmentClientImpl` will not distinguish
      // between any of the PSM errors (except for connection error, and server
      // error) and will report final progress with given server state even if
      // it's not available.
      // TODO(crbug.com/1249792): Handle internal PSM Errors.
      case psm::RlweResult::kCreateRlweClientLibraryError:
      case psm::RlweResult::kCreateOprfRequestLibraryError:
      case psm::RlweResult::kCreateQueryRequestLibraryError:
      case psm::RlweResult::kProcessingQueryResponseLibraryError:
      case psm::RlweResult::kEmptyOprfResponseError:
      case psm::RlweResult::kEmptyQueryResponseError:
        DCHECK(!GetServerStateIfObtained());
        RunCallback(ServerStateAvailabilityResult::kPsmInternalError);
        break;
    }
  }

  void RunCallback(ServerStateAvailabilityResult availability_result) {
    DCHECK(completion_callback_);
    std::move(completion_callback_).Run(availability_result);
  }

  void PrepareLocalState() {
    // Set the initial PSM execution result as unknown until it finishes
    // successfully or due to an error.
    // Also, clear the PSM determination timestamp.
    local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                             em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN);
    local_state_->ClearPref(prefs::kEnrollmentPsmDeterminationTime);
  }

  void UpdateLocalState(
      const psm::RlweDmserverClient::ResultHolder& psm_result_holder) {
    if (psm_result_holder.IsError()) {
      local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                               em::DeviceRegisterRequest::PSM_RESULT_ERROR);
      return;
    }

    local_state_->SetBoolean(prefs::kShouldRetrieveDeviceState,
                             psm_result_holder.membership_result.value());
    local_state_->SetTime(
        prefs::kEnrollmentPsmDeterminationTime,
        psm_result_holder.membership_determination_time.value());
    local_state_->SetInteger(
        prefs::kEnrollmentPsmResult,
        psm_result_holder.membership_result.value()
            ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
            : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  }

  // Obtains the device state using PSM protocol. Handles all communications
  // related to PSM protocol with DMServer.
  std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client_;

  raw_ptr<PrefService, ExperimentalAsh> local_state_;

  CompletionCallback completion_callback_;
};

enum class AutoEnrollmentClientImpl::ServerStateRetrievalResult {
  // Indicates that request has been successful and server state is available.
  kSuccess = 0,
  // Indicates a connection error during request.
  kConnectionError = 1,
  // Indicates an invalid response from server.
  kServerError = 2,
};

// Responsible fro resolving server state status for both force re-enrollment
// and initial enrollment.
class AutoEnrollmentClientImpl::ServerStateRetriever {
 public:
  using CompletionCallback =
      base::OnceCallback<void(ServerStateRetrievalResult)>;

  ServerStateRetriever(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state,
      const std::string& device_id,
      const std::string& uma_suffix,
      std::unique_ptr<AutoEnrollmentStateMessageProcessor>
          state_download_message_processor)
      : device_management_service_(device_management_service),
        url_loader_factory_(url_loader_factory),
        local_state_(local_state),
        device_id_(device_id),
        uma_suffix_(uma_suffix),
        state_download_message_processor_(
            std::move(state_download_message_processor)) {}

  ServerStateRetriever(const ServerStateRetriever&) = delete;
  ServerStateRetriever& operator=(const ServerStateRetriever&) = delete;

  void Start(CompletionCallback callback) { StartImpl(std::move(callback)); }

  absl::optional<AutoEnrollmentState> GetAutoEnrollmentStateIfObtained() const {
    if (!device_state_available_) {
      return absl::nullopt;
    }

    const DeviceStateMode device_state_mode = GetDeviceStateMode();
    switch (device_state_mode) {
      case RESTORE_MODE_NONE:
        return AutoEnrollmentState::kNoEnrollment;
      case RESTORE_MODE_DISABLED:
        return AutoEnrollmentState::kDisabled;
      case RESTORE_MODE_REENROLLMENT_REQUESTED:
      case RESTORE_MODE_REENROLLMENT_ENFORCED:
      case INITIAL_MODE_ENROLLMENT_ENFORCED:
      case RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
      case INITIAL_MODE_ENROLLMENT_ZERO_TOUCH:
        return AutoEnrollmentState::kEnrollment;
    }
  }

 private:
  void StartImpl(CompletionCallback callback) {
    DCHECK(!request_job_);
    DCHECK(callback);
    DCHECK(!completion_callback_);
    DCHECK(!device_state_available_);

    completion_callback_ = std::move(callback);

    std::unique_ptr<DMServerJobConfiguration> config =
        std::make_unique<DMServerJobConfiguration>(
            device_management_service_,
            state_download_message_processor_->GetJobType(), device_id_,
            /*critical=*/false, DMAuth::NoAuth(),
            /*oauth_token=*/absl::nullopt, url_loader_factory_,
            base::BindRepeating(&ServerStateRetriever::HandleRequestCompletion,
                                base::Unretained(this)));

    state_download_message_processor_->FillRequest(config->request());
    request_job_ = device_management_service_->CreateJob(std::move(config));
  }

  void HandleRequestCompletion(DMServerJobResult result) {
    DCHECK(request_job_);
    DCHECK(completion_callback_);

    request_job_.reset();

    base::UmaHistogramSparse(kUMAHashDanceRequestStatus + uma_suffix_,
                             result.dm_status);
    // TODO(crbug.com/1312919): Check `result.dm_status` for specific errors.
    if (result.dm_status != DM_STATUS_SUCCESS) {
      LOG(ERROR) << "Auto enrollment error: " << result.dm_status;
      if (result.dm_status == DM_STATUS_REQUEST_FAILED)
        base::UmaHistogramSparse(kUMAHashDanceNetworkErrorCode + uma_suffix_,
                                 -result.net_error);
      RunCallback(result.dm_status == DM_STATUS_REQUEST_FAILED
                      ? ServerStateRetrievalResult::kConnectionError
                      : ServerStateRetrievalResult::kServerError);
      return;
    }

    absl::optional<AutoEnrollmentStateMessageProcessor::ParsedResponse>
        parsed_response_result =
            state_download_message_processor_->ParseResponse(result.response);
    if (!parsed_response_result) {
      RunCallback(ServerStateRetrievalResult::kServerError);
      return;
    }

    AutoEnrollmentStateMessageProcessor::ParsedResponse& parsed_response =
        *parsed_response_result;

    base::Value::Dict state;
    if (parsed_response.management_domain.has_value())
      state.Set(kDeviceStateManagementDomain,
                *parsed_response.management_domain);

    if (!parsed_response.restore_mode.empty())
      state.Set(kDeviceStateMode, parsed_response.restore_mode);

    if (parsed_response.disabled_message.has_value())
      state.Set(kDeviceStateDisabledMessage, *parsed_response.disabled_message);

    if (parsed_response.is_license_packaged_with_device.has_value())
      state.Set(kDeviceStatePackagedLicense,
                *parsed_response.is_license_packaged_with_device);

    if (parsed_response.license_type.has_value())
      state.Set(kDeviceStateLicenseType, *parsed_response.license_type);

    if (parsed_response.assigned_upgrade_type.has_value()) {
      state.Set(kDeviceStateAssignedUpgradeType,
                *parsed_response.assigned_upgrade_type);
    }

    // Store the enrollment state obtained from the server to local state.
    // Depending on the value, this can be used later to trigger enrollment or
    // to disable the device.
    local_state_->SetDict(prefs::kServerBackedDeviceState, std::move(state));

    device_state_available_ = true;
    RunCallback(ServerStateRetrievalResult::kSuccess);
  }

  void RunCallback(ServerStateRetrievalResult result) {
    DCHECK(completion_callback_);
    std::move(completion_callback_).Run(result);
  }

  raw_ptr<DeviceManagementService, ExperimentalAsh> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<PrefService, ExperimentalAsh> local_state_;
  const std::string device_id_;
  const std::string uma_suffix_;

  // Whether the download of server-kept device state completed successfully.
  bool device_state_available_ = false;

  std::unique_ptr<DeviceManagementService::Job> request_job_;

  // Fills and parses state retrieval request / response.
  std::unique_ptr<AutoEnrollmentStateMessageProcessor>
      state_download_message_processor_;

  CompletionCallback completion_callback_;
};

AutoEnrollmentClientImpl::FactoryImpl::FactoryImpl() = default;
AutoEnrollmentClientImpl::FactoryImpl::~FactoryImpl() = default;

std::unique_ptr<AutoEnrollmentClient>
AutoEnrollmentClientImpl::FactoryImpl::CreateForFRE(
    const ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& server_backed_state_key,
    int power_initial,
    int power_limit) {
  const std::string device_id =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback,
      std::make_unique<FREServerStateAvailabilityRequester>(
          device_management_service, url_loader_factory, local_state, device_id,
          kUMASuffixFRE, power_initial, power_limit, server_backed_state_key),
      std::make_unique<ServerStateRetriever>(
          device_management_service, url_loader_factory, local_state, device_id,
          kUMASuffixFRE,
          AutoEnrollmentStateMessageProcessor::CreateForFRE(
              server_backed_state_key))));
}

std::unique_ptr<AutoEnrollmentClient>
AutoEnrollmentClientImpl::FactoryImpl::CreateForInitialEnrollment(
    const ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& device_serial_number,
    const std::string& device_brand_code,
    std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client) {
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback,
      std::make_unique<InitialServerStateAvailabilityRequester>(
          std::move(psm_rlwe_dmserver_client), local_state),
      std::make_unique<ServerStateRetriever>(
          device_management_service, url_loader_factory, local_state,
          /*device_id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(),
          kUMASuffixInitialEnrollment,
          AutoEnrollmentStateMessageProcessor::CreateForInitialEnrollment(
              device_serial_number, device_brand_code))));
}

// static
void AutoEnrollmentClientImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  FREServerStateAvailabilityRequester::RegisterPrefs(registry);
  InitialServerStateAvailabilityRequester::RegisterPrefs(registry);
}

AutoEnrollmentClientImpl::AutoEnrollmentClientImpl(
    ProgressCallback callback,
    std::unique_ptr<ServerStateAvailabilityRequester>
        server_state_availability_requester,
    std::unique_ptr<ServerStateRetriever> server_state_retriever)
    : progress_callback_(std::move(callback)),
      server_state_availability_requester_(
          std::move(server_state_availability_requester)),
      server_state_retriever_(std::move(server_state_retriever)) {
  DCHECK(progress_callback_);
}

AutoEnrollmentClientImpl::~AutoEnrollmentClientImpl() = default;

void AutoEnrollmentClientImpl::Start() {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!server_state_retriever_->GetAutoEnrollmentStateIfObtained());

  network_connection_observer_.Observe(content::GetNetworkConnectionTracker());

  RequestServerStateAvailability();
}

void AutoEnrollmentClientImpl::Retry() {
  switch (state_) {
    case State::kIdle:
      Start();
      break;

    // Request in progress, nothing to do.
    case State::kRequestingServerStateAvailability:
    case State::kRequestingStateRetrieval:
      break;

    case State::kRequestServerStateAvailabilityConnectionError:
    case State::kRequestServerStateAvailabilityServerError:
      RequestServerStateAvailability();
      break;

    case State::kRequestStateRetrievalConnectionError:
    case State::kRequestStateRetrievalServerError:
      RequestStateRetrieval();
      break;

    // All possible requests are done and the final device state has been
    // reported. Nothing to to do.
    case State::kFinished:
      break;
    case State::kRequestServerStateAvailabilitySuccess:
      NOTREACHED() << "kRequestServerStateAvailabilitySuccess supposed to "
                      "immediately resolve to kRequestingStateRetrieval.";
      break;
  }
}

void AutoEnrollmentClientImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    Retry();
  }
}

void AutoEnrollmentClientImpl::RequestServerStateAvailability() {
  DCHECK(state_ == State::kIdle ||
         state_ == State::kRequestServerStateAvailabilityConnectionError ||
         state_ == State::kRequestServerStateAvailabilityServerError);
  state_ = State::kRequestingServerStateAvailability;

  if (server_state_availability_requester_->GetServerStateIfObtained()) {
    OnServerStateAvailabilityCompleted(ServerStateAvailabilityResult::kSuccess);
    return;
  }

  ReportProgress(AutoEnrollmentState::kPending);

  server_state_availability_requester_->Start(base::BindOnce(
      &AutoEnrollmentClientImpl::OnServerStateAvailabilityCompleted,
      base::Unretained(this)));
}

void AutoEnrollmentClientImpl::OnServerStateAvailabilityCompleted(
    ServerStateAvailabilityResult result) {
  DCHECK(state_ == State::kRequestingServerStateAvailability);

  switch (result) {
    case ServerStateAvailabilityResult::kSuccess:
      DCHECK(server_state_availability_requester_->GetServerStateIfObtained());
      if (server_state_availability_requester_->GetServerStateIfObtained()
              .value()) {
        state_ = State::kRequestServerStateAvailabilitySuccess;
        RequestStateRetrieval();
      } else {
        state_ = State::kFinished;
        ReportFinished();
      }
      break;
    case ServerStateAvailabilityResult::kConnectionError:
      state_ = State::kRequestServerStateAvailabilityConnectionError;
      ReportProgress(AutoEnrollmentState::kConnectionError);
      break;
    case ServerStateAvailabilityResult::kServerError:
      state_ = State::kRequestServerStateAvailabilityServerError;
      ReportProgress(AutoEnrollmentState::kServerError);
      break;
    case ServerStateAvailabilityResult::kAutoEnrollmentRetriableError:
      state_ = State::kRequestServerStateAvailabilityServerError;
      Retry();
      break;
    case ServerStateAvailabilityResult::kPsmInternalError:
      DCHECK(!server_state_availability_requester_->GetServerStateIfObtained());
      state_ = State::kFinished;
      ReportFinished();
      break;
  }
}

void AutoEnrollmentClientImpl::RequestStateRetrieval() {
  DCHECK(state_ == State::kRequestServerStateAvailabilitySuccess ||
         state_ == State::kRequestStateRetrievalConnectionError ||
         state_ == State::kRequestStateRetrievalServerError);
  DCHECK(server_state_availability_requester_->GetServerStateIfObtained());
  DCHECK(
      server_state_availability_requester_->GetServerStateIfObtained().value());
  DCHECK(!server_state_retriever_->GetAutoEnrollmentStateIfObtained());
  state_ = State::kRequestingStateRetrieval;

  ReportProgress(AutoEnrollmentState::kPending);

  server_state_retriever_->Start(
      base::BindOnce(&AutoEnrollmentClientImpl::OnStateRetrievalCompleted,
                     base::Unretained(this)));
}

void AutoEnrollmentClientImpl::OnStateRetrievalCompleted(
    ServerStateRetrievalResult result) {
  DCHECK(state_ == State::kRequestingStateRetrieval);

  switch (result) {
    case ServerStateRetrievalResult::kSuccess:
      DCHECK(server_state_retriever_->GetAutoEnrollmentStateIfObtained());
      state_ = State::kFinished;
      ReportFinished();
      break;
    case ServerStateRetrievalResult::kConnectionError:
      state_ = State::kRequestStateRetrievalConnectionError;
      ReportProgress(AutoEnrollmentState::kConnectionError);
      break;
    case ServerStateRetrievalResult::kServerError:
      state_ = State::kRequestStateRetrievalServerError;
      ReportProgress(AutoEnrollmentState::kServerError);
      break;
  }
}

void AutoEnrollmentClientImpl::ReportProgress(AutoEnrollmentState state) const {
  DCHECK(progress_callback_);
  progress_callback_.Run(state);
}

void AutoEnrollmentClientImpl::ReportFinished() const {
  DCHECK_EQ(state_, State::kFinished);

  const auto auto_enrollment_state_result =
      server_state_retriever_->GetAutoEnrollmentStateIfObtained();
  if (auto_enrollment_state_result) {
    ReportProgress(auto_enrollment_state_result.value());
  } else {
    ReportProgress(AutoEnrollmentState::kNoEnrollment);
  }
}

}  // namespace policy
