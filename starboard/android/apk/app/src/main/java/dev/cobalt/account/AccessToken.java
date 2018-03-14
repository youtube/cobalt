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

package dev.cobalt.account;

import dev.cobalt.util.UsedByNative;

/**
 * POJO holding the token for a signed-in user.
 */
public class AccessToken {

  private final String tokenValue;
  private final long expirySeconds;

  public AccessToken(String tokenValue, long expirySeconds) {
    this.tokenValue = tokenValue;
    this.expirySeconds = expirySeconds;
  }

  /**
   * Returns the token value.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public String getTokenValue() {
    return tokenValue;
  }

  /**
   * Returns number of seconds since epoch when this token expires, or 0 if it doesn't expire.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public long getExpirySeconds() {
    return expirySeconds;
  }
}
