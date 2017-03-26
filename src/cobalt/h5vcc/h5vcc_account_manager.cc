// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_account_manager.h"

#include "base/memory/scoped_ptr.h"
#include "starboard/user.h"

namespace cobalt {
namespace h5vcc {

H5vccAccountManager::H5vccAccountManager()
    : thread_("AccountManager"), owning_message_loop_(MessageLoop::current()),
      user_authorizer_(account::UserAuthorizer::Create()) {
  thread_.Start();
}

void H5vccAccountManager::GetAuthToken(
    const AccessTokenCallbackHolder& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG(INFO) << "Get authorization token.";
  scoped_ptr<AccessTokenCallbackReference> token_callback(
      new AccessTokenCallbackHolder::Reference(this, callback));
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&H5vccAccountManager::RequestOperationInternal,
                            this, kGetToken, base::Passed(&token_callback)));
}

void H5vccAccountManager::RequestPairing(
    const AccessTokenCallbackHolder& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG(INFO) << "Request application linking.";
  scoped_ptr<AccessTokenCallbackReference> token_callback(
      new AccessTokenCallbackHolder::Reference(this, callback));
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&H5vccAccountManager::RequestOperationInternal,
                            this, kPairing, base::Passed(&token_callback)));
}

void H5vccAccountManager::RequestUnpairing(
    const AccessTokenCallbackHolder& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG(INFO) << "Request application unlinking.";
  scoped_ptr<AccessTokenCallbackReference> token_callback(
      new AccessTokenCallbackHolder::Reference(this, callback));
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&H5vccAccountManager::RequestOperationInternal,
                            this, kUnpairing, base::Passed(&token_callback)));
}

H5vccAccountManager::~H5vccAccountManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void H5vccAccountManager::RequestOperationInternal(
    OperationType operation,
    scoped_ptr<AccessTokenCallbackReference> token_callback) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  SbUser current_user = SbUserGetCurrent();
  DCHECK(SbUserIsValid(current_user));

  scoped_ptr<account::AccessToken> access_token(NULL);

  switch (operation) {
    case kPairing:
      access_token = user_authorizer_->AuthorizeUser(current_user);
      DLOG_IF(INFO, !access_token) << "User authorization request failed.";
      break;
    case kUnpairing:
      if (user_authorizer_->DeauthorizeUser(current_user)) {
        break;
      }
      // The user canceled the flow, or there was some error. Fall into the next
      // case to get an access token if available and return that.
      DLOG(INFO) << "User deauthorization request failed. Try to get token.";
    case kGetToken:
      access_token = user_authorizer_->RefreshAuthorization(current_user);
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

  owning_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&H5vccAccountManager::SendResult, this,
                 base::Passed(&token_callback), token_value,
                 expiration_in_seconds));
}

void H5vccAccountManager::SendResult(
    scoped_ptr<AccessTokenCallbackReference> token_callback,
    const std::string& token, uint64_t expiration_in_seconds) {
  DCHECK(thread_checker_.CalledOnValidThread());
  token_callback->value().Run(token, expiration_in_seconds);
}

}  // namespace h5vcc
}  // namespace cobalt
