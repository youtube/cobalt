// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/shared_types.h"
#include "content/public/browser/render_frame_host.h"

namespace actor {

namespace {

// We need to make sure that we don't skip user confirmation for OTPs that do
// not belong to actor login flows. Actor login fills credentials in all iframes
// that it considers trustworthy because it doesn't know which one contains the
// correct login form. It also uses 2 different trust levels (based on user
// permission type), both are based on iframe's and main frame's origins.
// This method needs to match the same trust levels, hence the
// `should_use_strong_matching` parameter. To avoid checking each filled
// frame, we try to match the OTP form's origin with the origin of the main
// frame where actor login flow started and rely on the fact that affiliations
// are transitive.
bool OtpFormOriginMatchesLoginMainFrameOrigin(
    const url::Origin& login_main_frame_origin,
    const url::Origin& otp_origin,
    bool should_use_strong_matching) {
  // TODO(crbug.com/504573041): Implement
  NOTIMPLEMENTED();
  return false;
}

// Returns the RenderFrameHost containing the OTP fields.
content::RenderFrameHost* GetOtpFrame(
    tabs::TabHandle tab_handle,
    base::span<const PageTarget> trigger_fields) {
  // TODO(crbug.com/504573041): Implement
  NOTIMPLEMENTED();
  return nullptr;
}

// Checks if the tool execution corresponds to an actor login's sign in flow.
// This is used to determine if we can skip the confirmation UI. The
// reasoning being that the user already consented to the login attempt
// during actor login execution and this OTP filling is considered part of
// the same flow.
bool IsActorLoginFlow(tabs::TabHandle tab_handle,
                      base::span<const PageTarget> trigger_fields,
                      const autofill::ActorLoginContext& context) {
  if (trigger_fields.empty()) {
    return false;
  }

  content::RenderFrameHost* otp_frame = GetOtpFrame(tab_handle, trigger_fields);
  if (!otp_frame) {
    return false;
  }

  const url::Origin& otp_form_origin = otp_frame->GetLastCommittedOrigin();
  content::FrameTreeNodeId otp_frame_id = otp_frame->GetFrameTreeNodeId();

  if (context.navigations_per_frame.contains(otp_frame_id) &&
      OtpFormOriginMatchesLoginMainFrameOrigin(
          context.origin, otp_form_origin,
          context.should_use_strong_matching)) {
    // Actor Login filled credentials in all of these frames but we don't know
    // which one was the actual login frame. While finding an OTP field in one
    // of those frames is a signal that the frame was the login frame, it's not
    // guaranteed. Therefore, require all frames to have <2 navigations to
    // avoid accidentally skipping user confirmation for OTPs not meant for
    // login flows.
    return std::ranges::all_of(
        context.navigations_per_frame,
        [](const std::pair<const content::FrameTreeNodeId, int>& entry) {
          return entry.second < 2;
        });
  }

  return false;
}

}  // namespace

AttemptOtpFillingTool::AttemptOtpFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin) {
  // Guaranteed by validation in CreateAttemptOtpFillingRequest in
  // actor_proto_conversion.cc.
  CHECK(!trigger_fields_.empty());
}

AttemptOtpFillingTool::~AttemptOtpFillingTool() = default;

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
  // This could be a place to check (once more) if the feature is enabled and
  // that the user has not permanently opted out.
  // Note: There's also the method TimeOfUseValidation for checks that happen
  // synchronously just before Invoke().

  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptOtpFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  if (!last_observation) {
    return MakeResult(mojom::ActionResultCode::kFormFillingNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "Last tab observation is null.");
  }

  trigger_field_ids_.clear();
  trigger_field_ids_.reserve(trigger_fields_.size());
  for (const auto& trigger_field : trigger_fields_) {
    autofill::FieldGlobalId field_id =
        GetFieldIdFromPageTarget(last_observation, tab, trigger_field);
    if (!field_id) {
      return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound,
                        /*requires_page_stabilization=*/false,
                        "Trigger field not found.");
    }
    trigger_field_ids_.push_back(field_id);
  }

  return MakeOkResult();
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  // TODO(b/484334125): Check for user consent before filling!
  // The checks will be somewhat different, depending on the for_signin
  // parameter.
  // For now, we just log it ...
  journal().Log(JournalURL(), task_id(), "AttemptOtpFillingTool::Invoke",
                JournalDetailsBuilder()
                    .Add("trigger_fields_count", trigger_field_ids_.size())
                    .Add("for_signin", for_signin_)
                    .Build());

  // Consume the context. The service clears its state and stops observing.
  std::optional<autofill::ActorLoginContext> context =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ConsumeLoginContext();

  if (context.has_value() &&
      IsActorLoginFlow(GetTargetTab(), trigger_fields_, *context)) {
    // Verified sign-in journey: proceed with silent OTP filling.
    // TODO(crbug.com/504573041): Implement
  } else {
    // No recent login, origin mismatch, untracked frame, or sequence broken
    // by too many navigations: require confirmation UI (Post-MVP).
    // TODO(crbug.com/504573041): Implement
  }

  tool_delegate().GetActorOneTimeTokenFillingService().RetrieveOtp(
      GetTargetTab(), trigger_field_ids_,
      base::BindOnce(&AttemptOtpFillingTool::OnOtpRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpRetrieved(ToolCallback callback,
                                           std::string otp) {
  journal().Log(
      JournalURL(), task_id(), "AttemptOtpFillingTool::OnOtpRetrieved",
      JournalDetailsBuilder().Add("otp_received", !otp.empty()).Build());

  // TODO(b/502907994): There might be other errors happening, not just a
  // timeout. If we want to treat them less generically, we need to change the
  // API of the service to also return more detailed error codes.
  if (otp.empty()) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kToolTimeout,
                   /*requires_page_stabilization=*/false,
                   "Failed to retrieve OTP within timeout."));
    return;
  }

  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetTargetTab(), trigger_field_ids_, otp,
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpFilled(ToolCallback callback, bool success) {
  journal().Log(JournalURL(), task_id(), "AttemptOtpFillingTool::OnOtpFilled",
                JournalDetailsBuilder().Add("success", success).Build());

  if (success) {
    std::move(callback).Run(MakeOkResult());
  } else {
    std::move(callback).Run(MakeResult(
        mojom::ActionResultCode::kFormFillingUnknownAutofillError,
        /*requires_page_stabilization=*/false, "Failed to fill OTP."));
  }
}

void AttemptOtpFillingTool::UpdateTaskBeforeInvoke(
    ActorTask& task,
    ToolCallback callback) const {
  task.AddTab(GetTargetTab(), /*stop_task_on_detach=*/true,
              std::move(callback));
}

std::string AttemptOtpFillingTool::DebugString() const {
  // This ends up in chrome://actor-internals and will be used for debugging.
  return "AttemptOtpFillingTool";
}

std::string AttemptOtpFillingTool::JournalEvent() const {
  return "AttemptOtpFillingTool";
}

std::unique_ptr<ObservationDelayController>
AttemptOtpFillingTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

tabs::TabHandle AttemptOtpFillingTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
