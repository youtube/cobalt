// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_

#import <optional>
#import <utility>
#import <vector>

#import "base/functional/callback_forward.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "url/origin.h"

namespace actor_login {
class ActorLoginService;
}  // namespace actor_login

namespace actor {

class ActorToolFactory;
class AggregatedJournal;

// Provides the tools layer with access to objects owned by the orchestration
// layer. The lifetime of this is tied to the ActorTask, which outlives any
// tools that use it.
//
// This is based on chrome/browser/actor/tools/tool_delegate.h.
class ToolDelegate {
 public:
  virtual ~ToolDelegate() = default;

  // Returns the unique identifier of the active task.
  virtual ActorTaskId GetTaskId() const = 0;

  // Returns the journal used for logging.
  virtual AggregatedJournal& GetJournal() const = 0;

  // Returns the tool factory associated with this task.
  virtual ActorToolFactory& GetToolFactory() const = 0;

  // TODO(crbug.com/472287741): Consider moving all transaction related member
  // variables into a sub delegate.

  // Returns the ActorLoginService used to log into websites.
  virtual actor_login::ActorLoginService* GetActorLoginService() = 0;

  // Prompts the user to select a credential from the list of credentials.
  // The callback is called with the selected credential, or with std::nullopt
  // if the user closed the prompt without making a selection.
  // `should_store_permission` means the `credential` can be used in future
  // calls to the tool.
  using CredentialSelectedCallback = base::OnceCallback<void(
      std::optional<actor_login::Credential> selected_credential,
      bool should_store_permission)>;
  virtual void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) = 0;

  // Combines a `credential` and a user chose in the account picker, and whether
  // user has permitted Chrome to always actuate with this credential.
  struct CredentialWithPermission {
    actor_login::Credential credential;
    bool always_allow;
  };

  // Retrieves the credential that the user has chosen to allow the
  // actor to use. The selected credential can be used for multi-step login
  // within the same task.
  virtual std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const = 0;

  // Temporarily interrupts the task's execution flow because the tool is
  // waiting for user interaction (e.g. device re-authentication). This pauses
  // the task and transitions it to the `ActorTaskState::kWaitingOnUser` state.
  virtual void InterruptFromTool() = 0;

  // Resumes the task's execution flow once the user interaction is completed.
  // This restores the task to `ActorTaskState::kActing`.
  virtual void UninterruptFromTool() = 0;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_TOOL_DELEGATE_H_
