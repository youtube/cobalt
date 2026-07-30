// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import dev.cobalt.util.Holder;

/**
 * TODO(https://crbug.com/534656433): Temporary StarboardBridge stub to fix a symbol resolution
 * error on BaseStarboardBridge and CobaltService.
 */
public class StarboardBridge extends BaseStarboardBridge {

  public StarboardBridge(
      Context appContext,
      Holder<Activity> activityHolder,
      Holder<Service> serviceHolder,
      String[] args,
      String startDeepLink) {
    super(appContext, activityHolder, serviceHolder, args, startDeepLink);
  }
}
