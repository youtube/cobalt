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

import android.content.Intent;
import dev.cobalt.util.UsedByNative;

/** UserAuthorizer implementation that doesn't try to sign in. */
public class NoopUserAuthorizer implements UserAuthorizer {

  @Override
  public void shutdown() {}

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public void interrupt() {}

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public AccessToken authorizeUser() {
    return null;
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public boolean deauthorizeUser() {
    return false;
  }

  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public AccessToken refreshAuthorization() {
    return null;
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data) {}

  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {}

}
