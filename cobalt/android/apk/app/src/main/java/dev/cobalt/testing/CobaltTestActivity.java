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

import android.os.Bundle;
import dev.cobalt.util.DisplayUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.native_test.NativeUnitTestActivity;

/**
 * CobaltTestActivity is a custom NativeUnitTestActivity used to run native unit tests on Android.
 *
 * <p>It ensures that the Chromium base library's application context and JNI class loader are
 * properly initialized before the native test harness starts.
 */
public class CobaltTestActivity extends NativeUnitTestActivity {
  @Override
  public void onCreate(Bundle savedInstanceState) {
    ContextUtils.initApplicationContext(getApplicationContext());
    JNIUtils.setDefaultClassLoader(getClassLoader());
    DisplayUtil.cacheDefaultDisplay(this);
    super.onCreate(savedInstanceState);
  }
}
