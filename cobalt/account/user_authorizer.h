// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/memory/scoped_ptr.h"
#include "starboard/user.h"

namespace cobalt {
namespace account {

struct AccessToken {
  // The size of the buffer pointed to by |token_buffer|.
  const uint32_t token_buffer_size;

  // Buffer holding the token value.
  // TODO: Change to std::string and remove token_buffer_size
  scoped_array<char> token_buffer;

  // If true, |expiry| will be set. If false, the token never expires.
  bool has_expiry;

  // The absolute time that this token expires. It is valid to use the value
  // of |expiry| only if |has_expiry| is true.
  // TODO: Change to base::optional<base::Time> and remove has_expiry
  SbTime expiry;

  explicit AccessToken(uint32_t token_buffer_size)
      : token_buffer_size(token_buffer_size),
        token_buffer(new char[token_buffer_size]) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessToken);
};

// Platform-specific mechanism to authorize a user to access protected resources
// in a web app running in Cobalt. Manages getting, refreshing, and discarding
// an access token associated with a platform user identified by a |SbUser|.
class UserAuthorizer {
 public:
  UserAuthorizer() {}

  virtual ~UserAuthorizer() {}

  // Initiates a process to get an access token authorizing |user| to use
  // application resources (i.e. sign-in).  This call will block until the
  // authorization process is complete, which may involve user input.
  // After this call completes successfully, subsequent calls to
  // RefreshAuthorization for the same |user| will return valid tokens.
  // Returns a scoped_ptr holding NULL if the process failed for any reason
  // including an invalid |user|, or user cancellation of the authorization
  // process.
  // On success, a scoped_ptr holding a valid AccessToken is returned.
  virtual scoped_ptr<AccessToken> AuthorizeUser(SbUser user) = 0;

  // Remove authorization for |user| to use application resources (i.e.
  // sign-out).  This call will block until the linking process is complete,
  // which may involve user input.
  // After this call completes successfully, subsequent calls to
  // RefreshAuthorization for the same |user| will fail.
  // Returns true if the process to remove authorization unlink the token
  // succeeded.
  // Returns false if the process failed for any reason including an invalid
  // |user|, or user cancellation.
  virtual bool DeauthorizeUser(SbUser user) = 0;

  // Requests a new access token to extend authorization for |user| to continue
  // using application resources.
  // This call will block until the token has been received.
  // Returns a scoped_ptr holding NULL if the process failed for any reason
  // including an invalid |user|, or |user| not already being authorized (in
  // which case AuthorizeUser should be called).
  // On success, a scoped_ptr holding a valid AccessToken is returned.
  virtual scoped_ptr<AccessToken> RefreshAuthorization(SbUser user) = 0;

  // Instantiates an instance of the platform-specific implementation.
  static UserAuthorizer* Create();

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAuthorizer);
};

}  // namespace account
}  // namespace cobalt

#endif  // COBALT_ACCOUNT_USER_AUTHORIZER_H_
