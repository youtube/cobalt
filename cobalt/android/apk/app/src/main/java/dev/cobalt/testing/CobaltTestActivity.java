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

package dev.cobalt.testing;

import android.app.Activity;
import android.app.Service;
import android.content.Context;
import android.os.Bundle;
import dev.cobalt.coat.BaseStarboardBridge;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Holder;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.native_test.NativeUnitTestActivity;

/**
 * CobaltTestActivity is a custom NativeUnitTestActivity used to run native unit tests on Android.
 *
 * <p>It ensures that the Chromium base library's application context, JNI class loader, and
 * StarboardBridge JNI instance are properly initialized before the native test harness starts.
 *
 * <p>Lifetime/Ownership: Created and managed by the Android system's activity lifecycle. It
 * persists for the duration of the native test suite execution.
 *
 * <p>Threading: Must be created and accessed only on the main application thread (UI thread).
 */
public class CobaltTestActivity extends NativeUnitTestActivity {
  /**
   * Lightweight TestStarboardBridge subclass used to initialize JNI bindings and AudioOutputManager
   * for native unit tests without starting a duplicate native Starboard main thread.
   */
  private static class TestStarboardBridge extends BaseStarboardBridge {
    public TestStarboardBridge(Context context, Holder<Activity> activityHolder) {
      super(context, activityHolder, new Holder<Service>());
    }
  }

  private TestStarboardBridge mTestStarboardBridge;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    ContextUtils.initApplicationContext(getApplicationContext());
    JNIUtils.setDefaultClassLoader(getClassLoader());
    DisplayUtil.cacheDefaultDisplay(this);

    // super.onCreate loads the native shared library (.so) into memory via System.loadLibrary.
    super.onCreate(savedInstanceState);

    // Instantiate TestStarboardBridge after super.onCreate so native JNI methods are bound.
    Holder<Activity> activityHolder = new Holder<>();
    activityHolder.set(this);
    mTestStarboardBridge = new TestStarboardBridge(getApplicationContext(), activityHolder);
  }
}
