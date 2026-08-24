// Copyright 2026 The Cobalt Authors. All Rights Reserved.

package dev.cobalt.testing;

import android.os.Bundle;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.native_test.NativeUnitTestActivity;

public class CobaltTestActivity extends NativeUnitTestActivity {
  @Override
  public void onCreate(Bundle savedInstanceState) {
    ContextUtils.initApplicationContext(getApplicationContext());
    JNIUtils.setDefaultClassLoader(getClassLoader());
    super.onCreate(savedInstanceState);
  }
}
