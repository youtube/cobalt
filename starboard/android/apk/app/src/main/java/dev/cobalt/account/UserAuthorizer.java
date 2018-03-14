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

/**
 * Java side implementation for starboard::android::shared::cobalt::AndroidUserAuthorizer.
 */
public interface UserAuthorizer {

  /**
   * Cleans up at the end of the object's lifecycle.
   */
  void shutdown();

  /**
   * Unblocks any pending request.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  void interrupt();

  /**
   * Prompts the user as necessary to choose an account, then gets an auth token for the selected
   * account. I.e., pairs the web app to be signed-in as a selected account.
   *
   * This method blocks until finished, and must be called off the UI thread.
   *
   * Implementations must annotate this method with @UsedByNative so Proguard doesn't remove it.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  AccessToken authorizeUser();

  /**
   * Prompts the user as necessary to stop using an account. i.e., unpairs the web app to be
   * signed-out.
   *
   * This method blocks until finished, and must be called off the UI thread.
   *
   * Implementations must annotate this method with @UsedByNative so Proguard doesn't remove it.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  boolean deauthorizeUser();

  /**
   * Gets a new auth token for the account most recently authorized.
   *
   * No UI will be shown if there is already a current account restored at construction or selected
   * in authorizeUser(). If there is no current account, or the restored account has been deleted,
   * the account picker may be shown to let the user specify which account should become the current
   * account.
   *
   * This method blocks until finished, and must be called off the UI thread.
   *
   * Implementations must annotate this method with @UsedByNative so Proguard doesn't remove it.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  AccessToken refreshAuthorization();

  /**
   * @see android.app.Activity#onActivityResult(int, int, Intent)
   */
  void onActivityResult(int requestCode, int resultCode, Intent data);

  /**
   * @see android.app.Activity#onRequestPermissionsResult(int, String[], int[])
   */
  void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults);
}
