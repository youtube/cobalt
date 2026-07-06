/*
 * Copyright 2026 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package dev.cobalt.util;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.hardware.display.DisplayManager;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.util.ReflectionHelpers;

@RunWith(RobolectricTestRunner.class)
public class DisplayUtilTest {
  @Test
  public void testRemoveDisplayListenerUnregistersFrameworkCallback() {
    Context context = mock(Context.class);
    DisplayManager displayManager = mock(DisplayManager.class);
    DisplayManager.DisplayListener displayListener =
        ReflectionHelpers.getStaticField(DisplayUtil.class, "sDisplayerListener");
    ReflectionHelpers.setStaticField(DisplayUtil.class, "sDisplayerListenerAdded", true);
    when(context.getSystemService(Context.DISPLAY_SERVICE)).thenReturn(displayManager);

    DisplayUtil.removeDisplayListener(context);

    verify(displayManager).unregisterDisplayListener(displayListener);
    assertFalse(ReflectionHelpers.getStaticField(DisplayUtil.class, "sDisplayerListenerAdded"));
  }
}
