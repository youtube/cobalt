// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_event_publisher.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

#define LOG_FCP_METHOD_EXECUTION(name) DVLOG(4) << "FCP event: " << name

#define LOG_FCP_EVENT(name, event)                          \
  base::UmaHistogramEnumeration(kFcpEventHistogram, event), \
      LOG_FCP_METHOD_EXECUTION(name)

namespace private_insights {

FcpSecAggEventPublisher::FcpSecAggEventPublisher() = default;
FcpSecAggEventPublisher::~FcpSecAggEventPublisher() = default;

void FcpSecAggEventPublisher::PublishStateTransition(fcp::secagg::ClientState,
                                                     size_t,
                                                     size_t) {}

void FcpSecAggEventPublisher::PublishError() {}

void FcpSecAggEventPublisher::PublishAbort(bool, const std::string&) {}

void FcpSecAggEventPublisher::set_execution_session_id(int64_t) {}

FcpEventPublisher::FcpEventPublisher() = default;
FcpEventPublisher::~FcpEventPublisher() = default;

void FcpEventPublisher::PublishEligibilityEvalCheckin() {
  LOG_FCP_EVENT("EligibilityEvalCheckin", FcpEvent::kEligibilityEvalCheckin);
}

void FcpEventPublisher::PublishEligibilityEvalPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalPlanUriReceived",
                FcpEvent::kEligibilityEvalPlanUriReceived);
}

void FcpEventPublisher::PublishEligibilityEvalPlanReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalPlanReceived",
                FcpEvent::kEligibilityEvalPlanReceived);
}

void FcpEventPublisher::PublishEligibilityEvalNotConfigured(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalNotConfigured",
                FcpEvent::kEligibilityEvalNotConfigured);
}

void FcpEventPublisher::PublishEligibilityEvalRejected(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalRejected", FcpEvent::kEligibilityEvalRejected);
}

void FcpEventPublisher::PublishCheckin() {
  LOG_FCP_EVENT("Checkin", FcpEvent::kCheckin);
}

void FcpEventPublisher::PublishCheckinFinished(const fcp::client::NetworkStats&,
                                               FcpDuration) {
  LOG_FCP_EVENT("CheckinFinished", FcpEvent::kCheckinFinished);
}

void FcpEventPublisher::PublishRejected() {
  LOG_FCP_EVENT("Rejected", FcpEvent::kRejected);
}

void FcpEventPublisher::PublishTensorFlowError(int, absl::string_view) {
  LOG_FCP_EVENT("TensorFlowError", FcpEvent::kTensorFlowError);
}

void FcpEventPublisher::PublishIoError(absl::string_view s) {
  LOG_FCP_EVENT("IoError", FcpEvent::kIoError) << ": " << s;
}

void FcpEventPublisher::PublishExampleSelectorError(int, absl::string_view s) {
  LOG_FCP_EVENT("ExampleSelectorError", FcpEvent::kExampleSelectorError)
      << ": " << s;
}

void FcpEventPublisher::PublishInterruption(const fcp::client::ExampleStats&,
                                            FcpTime) {
  LOG_FCP_EVENT("Interruption", FcpEvent::kInterruption);
}

void FcpEventPublisher::PublishTaskNotStarted(absl::string_view s) {
  LOG_FCP_EVENT("TaskNotStarted", FcpEvent::kTaskNotStarted) << ": " << s;
}

void FcpEventPublisher::PublishNonfatalInitializationError(
    absl::string_view s) {
  LOG_FCP_EVENT("NonfatalInitializationError",
                FcpEvent::kNonfatalInitializationError)
      << ": " << s;
}

void FcpEventPublisher::PublishFatalInitializationError(absl::string_view s) {
  LOG_FCP_EVENT("FatalInitializationError", FcpEvent::kFatalInitializationError)
      << ": " << s;
}

void FcpEventPublisher::PublishEligibilityEvalCheckinIoError(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalCheckinIoError",
                FcpEvent::kEligibilityEvalCheckinIoError);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinClientInterrupted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalCheckinClientInterrupted",
                FcpEvent::kEligibilityEvalCheckinClientInterrupted);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinServerAborted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalCheckinServerAborted",
                FcpEvent::kEligibilityEvalCheckinServerAborted);
}

void FcpEventPublisher::PublishEligibilityEvalCheckinErrorInvalidPayload(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalCheckinErrorInvalidPayload",
                FcpEvent::kEligibilityEvalCheckinErrorInvalidPayload);
}

void FcpEventPublisher::PublishEligibilityEvalComputationStarted() {
  LOG_FCP_EVENT("EligibilityEvalComputationStarted",
                FcpEvent::kEligibilityEvalComputationStarted);
}

void FcpEventPublisher::PublishEligibilityEvalComputationInvalidArgument(
    absl::string_view,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationInvalidArgument",
                FcpEvent::kEligibilityEvalComputationInvalidArgument);
}

void FcpEventPublisher::PublishEligibilityEvalComputationIOError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationIOError",
                FcpEvent::kEligibilityEvalComputationIOError);
}

void FcpEventPublisher::PublishEligibilityEvalComputationExampleIteratorError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationExampleIteratorError",
                FcpEvent::kEligibilityEvalComputationExampleIteratorError);
}

void FcpEventPublisher::PublishEligibilityEvalComputationTensorflowError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationTensorflowError",
                FcpEvent::kEligibilityEvalComputationTensorflowError);
}

void FcpEventPublisher::PublishEligibilityEvalComputationInterrupted(
    absl::string_view,
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationInterrupted",
                FcpEvent::kEligibilityEvalComputationInterrupted);
}

void FcpEventPublisher::PublishEligibilityEvalComputationErrorNonfatal(
    absl::string_view) {
  LOG_FCP_EVENT("EligibilityEvalComputationErrorNonfatal",
                FcpEvent::kEligibilityEvalComputationErrorNonfatal);
}

void FcpEventPublisher::PublishEligibilityEvalComputationCompleted(
    const fcp::client::ExampleStats&,
    FcpDuration) {
  LOG_FCP_EVENT("EligibilityEvalComputationCompleted",
                FcpEvent::kEligibilityEvalComputationCompleted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsStarted() {
  LOG_FCP_EVENT("MultipleTaskAssignmentsStarted",
                FcpEvent::kMultipleTaskAssignmentsStarted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsIOError(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsIOError",
                FcpEvent::kMultipleTaskAssignmentsIOError);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPayloadIOError(
    absl::string_view) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsPayloadIOError",
                FcpEvent::kMultipleTaskAssignmentsPayloadIOError);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsInvalidPayload(
    absl::string_view) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsInvalidPayload",
                FcpEvent::kMultipleTaskAssignmentsInvalidPayload);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsClientInterrupted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsClientInterrupted",
                FcpEvent::kMultipleTaskAssignmentsClientInterrupted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsServerAborted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsServerAborted",
                FcpEvent::kMultipleTaskAssignmentsServerAborted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsTurnedAway(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsTurnedAway",
                FcpEvent::kMultipleTaskAssignmentsTurnedAway);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsPlanUriReceived",
                FcpEvent::kMultipleTaskAssignmentsPlanUriReceived);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPlanUriPartialReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsPlanUriPartialReceived",
                FcpEvent::kMultipleTaskAssignmentsPlanUriPartialReceived);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsPartialCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsPartialCompleted",
                FcpEvent::kMultipleTaskAssignmentsPartialCompleted);
}

void FcpEventPublisher::PublishMultipleTaskAssignmentsCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("MultipleTaskAssignmentsCompleted",
                FcpEvent::kMultipleTaskAssignmentsCompleted);
}

void FcpEventPublisher::PublishCheckinIoError(absl::string_view,
                                              const fcp::client::NetworkStats&,
                                              FcpDuration) {
  LOG_FCP_EVENT("CheckinIoError", FcpEvent::kCheckinIoError);
}

void FcpEventPublisher::PublishCheckinClientInterrupted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("CheckinClientInterrupted",
                FcpEvent::kCheckinClientInterrupted);
}

void FcpEventPublisher::PublishCheckinServerAborted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("CheckinServerAborted", FcpEvent::kCheckinServerAborted);
}

void FcpEventPublisher::PublishCheckinInvalidPayload(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("CheckinInvalidPayload", FcpEvent::kCheckinInvalidPayload);
}

void FcpEventPublisher::PublishRejected(const fcp::client::NetworkStats&,
                                        FcpDuration) {
  LOG_FCP_EVENT("Rejected", FcpEvent::kRejected);
}

void FcpEventPublisher::PublishCheckinPlanUriReceived(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("CheckinPlanUriReceived", FcpEvent::kCheckinPlanUriReceived);
}

void FcpEventPublisher::PublishCheckinFinishedV2(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("CheckinFinishedV2", FcpEvent::kCheckinFinishedV2);
}

void FcpEventPublisher::PublishComputationStarted() {
  LOG_FCP_EVENT("ComputationStarted", FcpEvent::kComputationStarted);
}

void FcpEventPublisher::PublishComputationInvalidArgument(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationInvalidArgument",
                FcpEvent::kComputationInvalidArgument);
}

void FcpEventPublisher::PublishComputationIOError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationIOError", FcpEvent::kComputationIOError);
}

void FcpEventPublisher::PublishComputationExampleIteratorError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationExampleIteratorError",
                FcpEvent::kComputationExampleIteratorError);
}

void FcpEventPublisher::PublishComputationTensorflowError(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationTensorflowError",
                FcpEvent::kComputationTensorflowError);
}

void FcpEventPublisher::PublishComputationInterrupted(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationInterrupted", FcpEvent::kComputationInterrupted);
}

void FcpEventPublisher::PublishComputationCompleted(
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationCompleted", FcpEvent::kComputationCompleted);
}

void FcpEventPublisher::PublishComputationInsufficientData(
    absl::string_view,
    const fcp::client::ExampleStats&,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ComputationInsufficientData",
                FcpEvent::kComputationInsufficientData);
}

void FcpEventPublisher::PublishResultUploadStarted() {
  LOG_FCP_EVENT("ResultUploadStarted", FcpEvent::kResultUploadStarted);
}

void FcpEventPublisher::PublishResultUploadIOError(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ResultUploadIOError", FcpEvent::kResultUploadIOError);
}

void FcpEventPublisher::PublishResultUploadClientInterrupted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ResultUploadClientInterrupted",
                FcpEvent::kResultUploadClientInterrupted);
}

void FcpEventPublisher::PublishResultUploadServerAborted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ResultUploadServerAborted",
                FcpEvent::kResultUploadServerAborted);
}

void FcpEventPublisher::PublishResultUploadCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("ResultUploadCompleted", FcpEvent::kResultUploadCompleted);
}

void FcpEventPublisher::PublishFailureUploadStarted() {
  LOG_FCP_EVENT("FailureUploadStarted", FcpEvent::kFailureUploadStarted);
}

void FcpEventPublisher::PublishFailureUploadIOError(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("FailureUploadIOError", FcpEvent::kFailureUploadIOError);
}

void FcpEventPublisher::PublishFailureUploadClientInterrupted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("FailureUploadClientInterrupted",
                FcpEvent::kFailureUploadClientInterrupted);
}

void FcpEventPublisher::PublishFailureUploadServerAborted(
    absl::string_view,
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("FailureUploadServerAborted",
                FcpEvent::kFailureUploadServerAborted);
}

void FcpEventPublisher::PublishFailureUploadCompleted(
    const fcp::client::NetworkStats&,
    FcpDuration) {
  LOG_FCP_EVENT("FailureUploadCompleted", FcpEvent::kFailureUploadCompleted);
}

void FcpEventPublisher::SetModelIdentifier(
    const std::string& model_identifier) {
  LOG_FCP_METHOD_EXECUTION("SetModelIdentifier") << ": " << model_identifier;
}

fcp::client::SecAggEventPublisher* FcpEventPublisher::secagg_event_publisher() {
  return &secagg_event_publisher_;
}

}  // namespace private_insights
