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

package dev.cobalt.app;

import android.app.Activity;
import dev.cobalt.account.UserAuthorizerImpl;
import dev.cobalt.coat.CobaltActivity;
import dev.cobalt.coat.StarboardBridge;
import dev.cobalt.feedback.NoopFeedbackService;
import dev.cobalt.util.Holder;

/**
 * Main Activity for the "Cobalt on Android TV" app.
 *
 * <p>The real work is done in the abstract base class. This class is really just some factory
 * methods to "inject" things that can be customized.
 */
public class MainActivity extends CobaltActivity {

  @Override
  protected StarboardBridge createStarboardBridge(String[] args, String startDeepLink) {
    Holder<Activity> activityHolder = new Holder<>();
    Runnable stopRequester =
        new Runnable() {
          @Override
          public void run() {
            getStarboardBridge().requestStop(0);
          }
        };
    UserAuthorizerImpl userAuthorizer =
        new UserAuthorizerImpl(getApplicationContext(), activityHolder, stopRequester);
    NoopFeedbackService feedbackService = new NoopFeedbackService();
    return new StarboardBridge(
        getApplicationContext(),
        activityHolder,
        userAuthorizer,
        feedbackService,
        args,
        startDeepLink);
  }
}
