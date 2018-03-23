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

import android.graphics.Bitmap;
import dev.cobalt.util.UsedByNative;
import java.util.HashMap;

/** Java side implementation for starboard::android::shared::cobalt::FeedbackService. */
public interface FeedbackService {
  /**
   * Sends the given product specific data to the GMS Feedback Service.
   *
   * <p>Implementations must annotate this method with @UsedByNative so Proguard doesn't remove it.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  void sendFeedback(HashMap<String, String> productSpecificData, Bitmap screenshot);
}
