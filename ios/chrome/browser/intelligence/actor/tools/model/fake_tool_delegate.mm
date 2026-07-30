// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"

#import "base/types/expected.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

FakeToolDelegate::FakeToolDelegate() = default;
FakeToolDelegate::~FakeToolDelegate() = default;

ActorTaskId FakeToolDelegate::GetTaskId() const {
  return ActorTaskId(1);
}

AggregatedJournal& FakeToolDelegate::GetJournal() const {
  return *journal_;
}

ActorToolFactory& FakeToolDelegate::GetToolFactory() const {
  return *tool_factory_;
}

actor_login::ActorLoginService* FakeToolDelegate::GetActorLoginService() {
  return actor_login_service_;
}

void FakeToolDelegate::PromptToSelectCredential(
    const std::vector<actor_login::Credential>& credentials,
    CredentialSelectedCallback callback) {
  prompt_to_select_called_ = true;
  prompted_credentials_ = credentials;
  prompt_callback_ = std::move(callback);
}

std::optional<ToolDelegate::CredentialWithPermission>
FakeToolDelegate::GetUserSelectedCredential(
    const url::Origin& request_origin) const {
  if (user_selected_credential_.has_value()) {
    ToolDelegate::CredentialWithPermission result;
    result.credential = *user_selected_credential_;
    result.always_allow = should_store_permission_;
    return result;
  }
  return std::nullopt;
}

void FakeToolDelegate::InterruptFromTool() {}

void FakeToolDelegate::UninterruptFromTool() {}

}  // namespace actor
