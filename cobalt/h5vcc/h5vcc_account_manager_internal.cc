// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/h5vcc/h5vcc_account_manager_internal.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/browser/switches.h"
#include "cobalt/web/environment_settings_helper.h"

namespace cobalt {
namespace h5vcc {

H5vccAccountManagerInternal::H5vccAccountManagerInternal(
    script::EnvironmentSettings* environment_settings)
    : user_authorizer_(account::UserAuthorizer::Create()),
      owning_message_loop_(base::MessageLoop::current()),
      thread_("AccountManager"),
      environment_settings_(environment_settings) {
  thread_.Start();
}

// static
bool H5vccAccountManagerInternal::IsSupported() {
  auto account_manager = account::UserAuthorizer::Create();
  if (account_manager) {
    delete account_manager;
    return true;
  }
  return false;
}

void H5vccAccountManagerInternal::GetAuthToken(
    const AccessTokenCallbackHolder& callback) {
  DLOG(INFO) << "Get authorization token.";
  PostOperation(kGetToken, callback);
}

void H5vccAccountManagerInternal::RequestPairing(
    const AccessTokenCallbackHolder& callback) {
  DLOG(INFO) << "Request application linking.";
  PostOperation(kPairing, callback);
}

void H5vccAccountManagerInternal::RequestUnpairing(
    const AccessTokenCallbackHolder& callback) {
  DLOG(INFO) << "Request application unlinking.";
  PostOperation(kUnpairing, callback);
}

void H5vccAccountManagerInternal::PostOperation(
    OperationType operation_type, const AccessTokenCallbackHolder& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* global_wrappable = web::get_global_wrappable(environment_settings_);
  AccessTokenCallbackReference* token_callback =
      new AccessTokenCallbackHolder::Reference(global_wrappable, callback);
  pending_callbacks_.push_back(
      std::unique_ptr<AccessTokenCallbackReference>(token_callback));
  thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&H5vccAccountManagerInternal::RequestOperationInternal,
                 user_authorizer_.get(), operation_type,
                 base::Bind(&H5vccAccountManagerInternal::PostResult,
                            owning_message_loop_, base::AsWeakPtr(this),
                            token_callback)));
}

H5vccAccountManagerInternal::~H5vccAccountManagerInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Give the UserAuthorizer a chance to abort any long running pending requests
  // before the message loop gets shut down.
  if (user_authorizer_) {
    user_authorizer_->Shutdown();
  }
}

// static
void H5vccAccountManagerInternal::RequestOperationInternal(
    account::UserAuthorizer* user_authorizer, OperationType operation,
    const base::Callback<void(const std::string&, uint64_t)>& post_result) {
  bool enabled = user_authorizer != nullptr;
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kDisableSignIn)) {
    enabled = false;
  }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  if (!enabled) {
    post_result.Run(std::string(), 0);
    return;
  }

  std::unique_ptr<account::AccessToken> access_token(nullptr);

  switch (operation) {
    case kPairing:
      access_token = user_authorizer->AuthorizeUser();
      DLOG_IF(INFO, !access_token) << "User authorization request failed.";
      break;
    case kUnpairing:
      if (user_authorizer->DeauthorizeUser()) {
        break;
      }
      // The user canceled the flow, or there was some error. Fall into the next
      // case to get an access token if available and return that.
      DLOG(INFO) << "User deauthorization request failed. Try to get token.";
    case kGetToken:
      access_token = user_authorizer->RefreshAuthorization();
      DLOG_IF(INFO, !access_token) << "Authorization refresh request failed.";
      break;
  }

  std::string token_value;
  uint64_t expiration_in_seconds = 0;
  if (access_token) {
    token_value = access_token->token_value;
    if (access_token->expiry) {
      base::TimeDelta expires_in =
          access_token->expiry.value() - base::Time::Now();
      // If this token's expiry is in the past, then it's not a valid token.
      base::TimeDelta zero;
      if (expires_in < zero) {
        DLOG(WARNING) << "User authorization expires in the past.";
        token_value.clear();
      } else {
        expiration_in_seconds = expires_in.InSeconds();
      }
    }
  }

  post_result.Run(token_value, expiration_in_seconds);
}

// static
void H5vccAccountManagerInternal::PostResult(
    base::MessageLoop* message_loop,
    base::WeakPtr<H5vccAccountManagerInternal> h5vcc_account_manager,
    AccessTokenCallbackReference* token_callback, const std::string& token,
    uint64_t expiration_in_seconds) {
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::Bind(&H5vccAccountManagerInternal::SendResult,
                            h5vcc_account_manager, token_callback, token,
                            expiration_in_seconds));
}

void H5vccAccountManagerInternal::SendResult(
    AccessTokenCallbackReference* token_callback, const std::string& token,
    uint64_t expiration_in_seconds) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<std::unique_ptr<AccessTokenCallbackReference>>::iterator found =
      std::find_if(pending_callbacks_.begin(), pending_callbacks_.end(),
                   [token_callback](
                       const std::unique_ptr<AccessTokenCallbackReference>& i) {
                     return i.get() == token_callback;
                   });
  if (found == pending_callbacks_.end()) {
    DLOG(ERROR) << "Account manager callback not valid.";
    return;
  }
  // In case a new account manager request is made as part of the callback,
  // erase the callback in the pending vector before running it, but we can't
  // delete it until after we've made the callback.
  found->release();
  pending_callbacks_.erase(found);
  token_callback->value().Run(token, expiration_in_seconds);
  delete token_callback;
}

}  // namespace h5vcc
}  // namespace cobalt
