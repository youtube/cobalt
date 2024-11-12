// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_DYNAMIC_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_DYNAMIC_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_invalidator.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/backoff_entry.h"

class Profile;
class PrefService;

namespace ash::cert_provisioning {

class CertProvisioningWorkerDynamic : public CertProvisioningWorker {
 public:
  CertProvisioningWorkerDynamic(
      std::string cert_provisioning_process_id,
      CertScope cert_scope,
      Profile* profile,
      PrefService* pref_service,
      const CertProfile& cert_profile,
      CertProvisioningClient* cert_provisioning_client,
      std::unique_ptr<CertProvisioningInvalidator> invalidator,
      base::RepeatingClosure state_change_callback,
      CertProvisioningWorkerCallback result_callback);
  ~CertProvisioningWorkerDynamic() override;

  // CertProvisioningWorker
  void DoStep() override;
  void Stop(CertProvisioningWorkerState state) override;
  void Pause() override;
  void MarkWorkerForReset() override;
  bool IsWorkerMarkedForReset() const override;
  bool IsWaiting() const override;
  const CertProfile& GetCertProfile() const override;
  const std::vector<uint8_t>& GetPublicKey() const override;
  CertProvisioningWorkerState GetState() const override;
  CertProvisioningWorkerState GetPreviousState() const override;
  base::Time GetLastUpdateTime() const override;
  const std::optional<BackendServerError>& GetLastBackendServerError()
      const override;
  std::string GetFailureMessage() const override;

 private:
  friend class CertProvisioningSerializer;

  enum class UpdateStateResult { kFinalState, kNonFinalState };

  void GenerateKey();

  void GenerateRegularKey();
  void OnGenerateRegularKeyDone(std::vector<uint8_t> public_key_spki_der,
                                chromeos::platform_keys::Status status);

  void GenerateKeyForVa();
  void OnGenerateKeyForVaDone(base::TimeTicks start_time,
                              const attestation::TpmChallengeKeyResult& result);

  void Start();
  void OnStartResponse(
      base::expected<enterprise_management::CertProvStartResponse,
                     CertProvisioningClient::Error> response);
  void GetNextInstruction();
  void OnGetNextInstructionResponse(
      base::expected<enterprise_management::CertProvGetNextInstructionResponse,
                     CertProvisioningClient::Error> response);
  void OnAuthorizeInstructionReceived(
      const enterprise_management::CertProvAuthorizeInstruction&
          authorize_instruction);
  void OnProofOfPossessionInstructionReceived(
      const enterprise_management::CertProvProofOfPossessionInstruction&
          proof_of_possession_instruction);
  void OnImportCertificateInstructionReceived(
      const enterprise_management::CertProvImportCertificateInstruction&
          import_certificate_instruction);

  void BuildVaChallengeResponse();
  void OnBuildVaChallengeResponseDone(
      base::TimeTicks start_time,
      const attestation::TpmChallengeKeyResult& result);

  void RegisterKey();
  void OnRegisterKeyDone(const attestation::TpmChallengeKeyResult& result);

  void MarkRegularKey();
  void MarkVaGeneratedKey();
  void MarkKey(CertProvisioningWorkerState target_state);
  void MarkKeyAsCorporate();
  void OnAllowKeyForUsageDone(chromeos::platform_keys::Status status);
  void OnMarkKeyDone(CertProvisioningWorkerState target_state,
                     chromeos::platform_keys::Status status);

  void UploadAuthorization();
  void OnUploadAuthorizationResponse(
      base::expected<void, CertProvisioningClient::Error> response);

  void BuildProofOfPossession();
  void OnBuildProofOfPossessionDone(base::TimeTicks start_time,
                                    std::vector<uint8_t> signature,
                                    chromeos::platform_keys::Status status);

  void UploadProofOfPossession();
  void OnUploadProofOfPossessionResponse(
      base::expected<void, CertProvisioningClient::Error> response);

  void ImportCert();
  void OnImportCertDone(chromeos::platform_keys::Status status);

  // Schedule the next step after the `delay`. If `try_provisioning_on_timeout`
  // is true, the worker will automatically try contacting the server-side after
  // it doesn't receive an invalidation for long enough. If it's false, it will
  // require an invalidation to continue.
  void ScheduleNextStep(base::TimeDelta delay,
                        bool try_provisioning_on_timeout);
  void CancelScheduledTasks();

  enum class ContinueReason {
    kTimeout,
    kSubscribedToInvalidation,
    kInvalidationReceived
  };
  void OnShouldContinue(ContinueReason reason);

  // Registers for |invalidation_topic_| that allows to receive notification
  // when server side is ready to continue provisioning process.
  void RegisterForInvalidationTopic();
  // Should be called only when provisioning process is finished (successfully
  // or not). Should not be called when the worker is destroyed, but will be
  // deserialized back later.
  void UnregisterFromInvalidationTopic();

  // Callback from invalidations system.
  void OnInvalidationEvent(InvalidationEvent invalidation_event);

  // If it is called with kSucceed or kFailed, it will call the |callback_|. The
  // worker can be destroyed in callback and should not use any member fields
  // after that.
  // Returns kNonFinalState if this worker can perform steps after the state
  // update, or kFinalState if the worker has reached a final state. When this
  // returns kFinalState, the worker should return immediately and not perform
  // additional actions because it could be destroyed from within the callback
  // that UpdateState calls when reaching a final state.
  [[nodiscard]] UpdateStateResult UpdateState(
      const base::Location& from_here,
      CertProvisioningWorkerState state);

  // Serializes the worker or deletes serialized state according to the current
  // state. Some states are considered unrecoverable, some can be reached again
  // from previous ones.
  void HandleSerialization();
  // Handles recreation of some internal objects after deserialization. Intended
  // to be called from CertProvisioningDeserializer.
  void InitAfterDeserialization();
  void CleanUpAndRunCallback();
  void OnDeleteVaKeyDone(bool delete_result);
  void OnRemoveKeyDone(chromeos::platform_keys::Status status);
  void OnCleanUpDone();

  CertProvisioningClient::ProvisioningProcess GetProvisioningProcessForClient();

  // Processes the general status of a "dynamic flow" response and sets members
  // accordingly. If this returns true, processing of the actual response should
  // continue. If this returns false, processing should not continue, and this
  // function has already set the worker to the corresponding state.
  template <typename ResultType>
  bool ProcessResponseErrors(
      const base::expected<ResultType, CertProvisioningClient::Error>&
          response);
  // Helper method for the above overload of ProcessResponseErrors. All other
  // callers should use the above overload.
  void ProcessResponseErrors(const CertProvisioningClient::Error& error);

  // A convenience method to generate a string that contains some additional
  // info and should be included in all logs.
  std::string GetLogInfoBlock();

  std::string process_id_;
  CertScope cert_scope_ = CertScope::kUser;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  CertProfile cert_profile_;
  base::RepeatingClosure state_change_callback_;
  CertProvisioningWorkerCallback result_callback_;

  // This field should be updated only via |UpdateState| function. It will
  // trigger update of the serialized data.
  CertProvisioningWorkerState state_ = CertProvisioningWorkerState::kInitState;
  // State that was before the current one. Useful for debugging and cleaning
  // on failure.
  CertProvisioningWorkerState prev_state_ = state_;
  // Time when this worker has been last updated. An update is when the worker
  // advances to the next state or for states that wait for a backend-side
  // condition (e.g CertProvisioningWorkerState:kFinishCsrResponseReceived):
  // when it successfully checked with the backend that the condition is not
  // fulfilled yet.
  base::Time last_update_time_;
  // Consequently, it is not updated if waiting for a backend-side condition,
  // but communication with the backend is not possible (e.g. due to server
  // errors or network connectivity issues).
  // The last error received in communicating to the backend server.
  std::optional<BackendServerError> last_backend_server_error_;
  bool is_waiting_ = false;
  bool is_schedueled_for_reset_ = false;
  // Used for an UMA metric to track situation when the worker did not receive
  // an invalidation for a completed server side task.
  bool is_continued_without_invalidation_for_uma_ = false;
  // Calculates retry timeout for network related failures.
  net::BackoffEntry request_backoff_;
  // Calculates retry timeout for fetching the next instruction.
  net::BackoffEntry fetch_instruction_backoff_;

  // Marks where a key pair used by this worker is located.
  KeyLocation key_location_ = KeyLocation::kNone;
  // This is true when this worker has attempted to generate a Verified Access
  // challenge response already.
  bool attempted_va_challenge_ = false;
  // This is true when this worker has attempted to generate a Proof of
  // Possession signature.
  bool attempted_proof_of_possession_ = false;

  // Public key - represented as DER-encoded X.509 SubjectPublicKeyInfo
  // (binary).
  std::vector<uint8_t> public_key_;
  std::string invalidation_topic_;

  // These variables may not contain valid values after
  // kFinishCsrResponseReceived state because of deserialization (and they don't
  // need to).
  // Instruction payload and response for "Authorize".
  // TODO(b/192071491): Switch these to `std::vector<uint8_t>`.
  std::string va_challenge_;
  std::string va_challenge_response_;

  // Instruction payload and response for "Proof Of Possession".
  // Must be provided by DMServer.
  enterprise_management::CertProvSignatureAlgorithm signature_algorithm_ =
      enterprise_management::CertProvSignatureAlgorithm::
          SIGNATURE_ALGORITHM_UNSPECIFIED;
  std::vector<uint8_t> data_to_sign_;
  std::vector<uint8_t> signature_;

  // Instruction payload for "Import Cert".
  std::string pem_encoded_certificate_;

  // Holds a message describing the reason for failure when the worker fails.
  // This may not contain PII or stable identifiers as it will be logged.
  // If the worker did not fail, this message is empty.
  std::string failure_message_;
  // Optionally holds a message like `failure_message_` but containing PII or
  // stable identifiers for display on the UI.
  // If the worker did not fail, this is absent.
  // If the worker did fail and this is absent, the UI should display
  // failure_message_.
  std::optional<std::string> failure_message_ui_;

  // IMPORTANT:
  // Increment this when you add/change any member in
  // CertProvisioningWorkerDynamic that affects serialization (and update all
  // functions that fail to compile because of it).
  static constexpr int kVersion = 3;

  // Unowned PlatformKeysService. Note that the CertProvisioningWorker does not
  // observe the PlatformKeysService for shutdown events. Instead, it relies on
  // the CertProvisioningScheduler to destroy all CertProvisioningWorker
  // instances when the corresponding PlatformKeysService is shutting down.
  raw_ptr<platform_keys::PlatformKeysService> platform_keys_service_ = nullptr;
  std::unique_ptr<attestation::TpmChallengeKeySubtle>
      tpm_challenge_key_subtle_impl_;
  const raw_ptr<CertProvisioningClient> cert_provisioning_client_;

  std::unique_ptr<CertProvisioningInvalidator> invalidator_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CertProvisioningWorkerDynamic> weak_factory_{this};
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_WORKER_DYNAMIC_H_
