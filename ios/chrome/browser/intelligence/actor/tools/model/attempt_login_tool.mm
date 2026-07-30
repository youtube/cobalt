// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_login_tool.h"

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// static
base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
AttemptLoginTool::Create(
    const optimization_guide::proto::AttemptLoginAction& action,
    ToolDelegate* tool_delegate,
    ProfileIOS* profile) {
  if (!action.has_tab_id()) {
    return base::unexpected(ToolExecutionResult(
        InternalToolErrorCode::kCreationMissingRequiredFields));
  }

  base::expected<TabResolutionResult, ToolExecutionResult> resolution_result =
      ResolveTab(action.tab_id(), profile);
  if (!resolution_result.has_value()) {
    return base::unexpected(resolution_result.error());
  }

  return std::unique_ptr<AttemptLoginTool>(
      new AttemptLoginTool(resolution_result.value().web_state, tool_delegate));
}

AttemptLoginTool::AttemptLoginTool(base::WeakPtr<web::WebState> web_state,
                                   ToolDelegate* tool_delegate)
    : web_state_(web_state), tool_delegate_(tool_delegate) {}

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Execute(ToolExecutionCallback callback) {
  if (!web_state_) {
    std::move(callback).Run(
        ToolExecutionResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  // TODO(crbug.com/472291829): This is a placeholder.
  std::move(callback).Run(ToolExecutionResult::Ok());
}

base::WeakPtr<web::WebState> AttemptLoginTool::GetTargetWebState() const {
  return web_state_;
}

ToolType AttemptLoginTool::GetToolType() const {
  return ToolType::kAttemptLogin;
}

}  // namespace actor
