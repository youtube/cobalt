/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/h5vcc/h5vcc_account_manager.h"

#include "base/memory/scoped_ptr.h"
#include "starboard/user.h"

namespace cobalt {
namespace h5vcc {

H5vccAccountManager::H5vccAccountManager()
    : thread_("AccountManager"), owning_message_loop_(MessageLoop::current()) {
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

  static size_t kBufferSize = SbUserMaxAuthenticationTokenSizeInBytes();
  scoped_array<char> token_buffer(new char[kBufferSize]);
  SbUserApplicationTokenResults token_results;
  token_results.token_buffer_size = kBufferSize;
  token_results.token_buffer = token_buffer.get();

  bool got_valid_token = false;
  switch (operation) {
    case kPairing:
      got_valid_token =
          SbUserRequestApplicationLinking(current_user, &token_results);
      DLOG_IF(INFO, !got_valid_token) << "Application linking request failed.";
      break;
    case kUnpairing:
      if (SbUserRequestApplicationUnlinking(current_user)) {
        got_valid_token = false;
        break;
      }
      // The user canceled the flow, or there was some error. Fall into the next
      // case to get an access token if available and return that.
      DLOG(INFO) << "Application unlinking request failed. Try to get token.";
    case kGetToken:
      got_valid_token =
          SbUserRequestAuthenticationToken(current_user, &token_results);
      DLOG_IF(INFO, !got_valid_token) << "Authentication token request failed.";
      break;
  }

  uint64_t expiration_in_seconds = 0;
  if (got_valid_token) {
    SbTime expires_in = token_results.expiry - SbTimeGetNow();
    // If this token's expiry is in the past, then we didn't get a valid token.
    if (expires_in < 0) {
      DLOG(WARNING) << "Authentication token expires in the past.";
      got_valid_token = false;
    } else {
      expiration_in_seconds = expires_in / kSbTimeSecond;
    }
  }
  // If we did not get a valid token to return to the caller, set the token
  // buffer to an empty string.
  if (!got_valid_token) {
    token_buffer[0] = '\0';
  }

  owning_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&H5vccAccountManager::SendResult, this,
                 base::Passed(&token_callback), std::string(token_buffer.get()),
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
