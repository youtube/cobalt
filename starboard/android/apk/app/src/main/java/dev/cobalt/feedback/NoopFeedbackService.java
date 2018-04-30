// Copyright 2018 Google Inc. All Rights Reserved.
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

package dev.cobalt.feedback;

import static dev.cobalt.util.Log.TAG;

import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.HashMap;

/** FeedbackService implementation that doesn't send any feedback. */
public class NoopFeedbackService implements FeedbackService {
  @Override
  @SuppressWarnings("unused")
  @UsedByNative
  public void sendFeedback(
      HashMap<String, String> productSpecificData, String categoryTag, Bitmap screenshot) {
    Log.i(TAG, "Feedback product specific data:");
    for (String key : productSpecificData.keySet()) {
      Log.i(TAG, key + ": " + productSpecificData.get(key));
    }
    if (screenshot != null) {
      Log.i(TAG, "Screenshot dimensions: " + screenshot.getWidth() + "x" + screenshot.getHeight());
    }
    if (!TextUtils.isEmpty(categoryTag)) {
      Log.i(TAG, "Category tag: " + categoryTag);
    }
  }

  @Override
  public void connect() {}

  @Override
  public void disconnect() {}
}
