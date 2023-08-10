// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_ACCOUNT_USER_AUTHORIZER_H_
#define COBALT_ACCOUNT_USER_AUTHORIZER_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"

namespace cobalt {
namespace account {

struct AccessToken {
  // The token value.
  std::string token_value;

  // The absolute time that this token expires, if any.
  base::Optional<base::Time> expiry;
};

// Platform-specific mechanism to authorize a user to access protected resources
// in a web app running in Cobalt. Manages getting, refreshing, and discarding
// an access token associated with a platform user.
class UserAuthorizer {
 public:
  UserAuthorizer() {}

  virtual ~UserAuthorizer() {}

  // Initiates a process to get an access token authorizing |user| to use
  // application resources (i.e. sign-in).  This call will block until the
  // authorization process is complete, which may involve user input.
  // After this call completes successfully, subsequent calls to
  // RefreshAuthorization for the same |user| will return valid tokens.
  // Returns a std::unique_ptr holding NULL if the process failed for any reason
  // including an invalid |user|, or user cancellation of the authorization
  // process.
  // On success, a std::unique_ptr holding a valid AccessToken is returned.
  virtual std::unique_ptr<AccessToken> AuthorizeUser() = 0;

  // Remove authorization for |user| to use application resources (i.e.
  // sign-out).  This call will block until the linking process is complete,
  // which may involve user input.
  // After this call completes successfully, subsequent calls to
  // RefreshAuthorization for the same |user| will fail.
  // Returns true if the process to remove authorization unlink the token
  // succeeded.
  // Returns false if the process failed for any reason including an invalid
  // |user|, or user cancellation.
  virtual bool DeauthorizeUser() = 0;

  // Requests a new access token to extend authorization for |user| to continue
  // using application resources.
  // This call will block until the token has been received.
  // Returns a std::unique_ptr holding NULL if the process failed for any reason
  // including an invalid |user|, or |user| not already being authorized (in
  // which case AuthorizeUser should be called).
  // On success, a std::unique_ptr holding a valid AccessToken is returned.
  virtual std::unique_ptr<AccessToken> RefreshAuthorization() = 0;

  // Signals that the account manager is shutting down, and unblocks any pending
  // request. Calling other methods after |Shutdown| may have no effect.
  virtual void Shutdown() {}

  // Instantiates an instance of the platform-specific implementation. This may
  // return nullptr if UserAuthorizer functions are not supported.
  static UserAuthorizer* Create();

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthorizer);
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_USER_AUTHORIZER_H_
