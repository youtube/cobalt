// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"

namespace actor_login {
class ActorLoginService;
}  // namespace actor_login

namespace actor {

class AggregatedJournal;
class ActorToolFactory;

// A fake implementation of ToolDelegate to use in tests.
class FakeToolDelegate : public ToolDelegate {
 public:
  FakeToolDelegate();
  ~FakeToolDelegate() override;

  // ToolDelegate:
  ActorTaskId GetTaskId() const override;
  AggregatedJournal& GetJournal() const override;
  ActorToolFactory& GetToolFactory() const override;
  actor_login::ActorLoginService* GetActorLoginService() override;
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) override;
  std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const override;
  void InterruptFromTool() override;
  void UninterruptFromTool() override;

  // Simulate credential selection by running the `callback` passed to the
  // method `PromptToSelectCredential`.
  void RunPromptCallback(
      std::optional<actor_login::Credential> selected_credential,
      bool should_store_permission) {
    std::move(prompt_callback_)
        .Run(selected_credential, should_store_permission);
  }

  void set_actor_login_service(actor_login::ActorLoginService* service) {
    actor_login_service_ = service;
  }

  bool prompt_to_select_called() const { return prompt_to_select_called_; }

  const std::vector<actor_login::Credential>& prompted_credentials() const {
    return prompted_credentials_;
  }

  void set_user_selected_credential(
      std::optional<actor_login::Credential> credential) {
    user_selected_credential_ = credential;
  }
  void set_should_store_permission(bool should_store) {
    should_store_permission_ = should_store;
  }

 private:
  raw_ptr<actor_login::ActorLoginService> actor_login_service_ = nullptr;
  raw_ptr<AggregatedJournal> journal_ = nullptr;
  raw_ptr<ActorToolFactory> tool_factory_ = nullptr;

  bool prompt_to_select_called_ = false;
  std::vector<actor_login::Credential> prompted_credentials_;
  CredentialSelectedCallback prompt_callback_;

  std::optional<actor_login::Credential> user_selected_credential_;
  bool should_store_permission_ = false;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_FAKE_TOOL_DELEGATE_H_
