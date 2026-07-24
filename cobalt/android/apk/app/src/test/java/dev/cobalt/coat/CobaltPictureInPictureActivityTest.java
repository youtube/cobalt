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

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Intent;
import android.view.View;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.components.thinwebview.CompositorView;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ActivityController;

/** Tests for CobaltPictureInPictureActivity. */
@RunWith(RobolectricTestRunner.class)
public class CobaltPictureInPictureActivityTest {
  @Mock private CobaltPictureInPictureActivity.Natives mMockNatives;
  @Mock private CompositorView mMockCompositorView;
  @Mock private View mMockView;

  @Before
  public void setUp() {
    initMocks(this);
    if (!CommandLine.isInitialized()) {
      CommandLine.init(null);
    }
    ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
    ApplicationStatus.initialize(RuntimeEnvironment.getApplication());
    CobaltPictureInPictureActivity.setNativesForTesting(mMockNatives);
    when(mMockCompositorView.getView()).thenReturn(mMockView);
    CobaltPictureInPictureActivity.setCompositorViewForTesting(mMockCompositorView);
  }

  @After
  public void tearDown() {
    CobaltPictureInPictureActivity.setNativesForTesting(null);
    CobaltPictureInPictureActivity.setCompositorViewForTesting(null);
    ApplicationStatus.destroyForJUnitTests();
  }

  @Test
  public void testActivityCreationWithoutNativePointerDoesNotCallNatives() {
    Intent intent = new Intent();
    ActivityController<CobaltPictureInPictureActivity> controller =
        Robolectric.buildActivity(CobaltPictureInPictureActivity.class, intent);
    CobaltPictureInPictureActivity activity = controller.create().get();

    assertTrue(activity.isFinishing());
    assertNull(activity.getWindowAndroid());
    verify(mMockNatives, never()).setJavaActivity(anyLong(), any());
    verify(mMockNatives, never()).compositorViewCreated(anyLong(), any());

    controller.destroy();
    verify(mMockNatives, never()).onActivityDestroyed(anyLong());
  }

  @Test
  public void testActivityCreationWithNativePointerCallsNatives() {
    long nativePointer = 12345L;
    Intent intent = new Intent();
    intent.putExtra("native_pointer", nativePointer);

    ActivityController<CobaltPictureInPictureActivity> controller =
        Robolectric.buildActivity(CobaltPictureInPictureActivity.class, intent);
    CobaltPictureInPictureActivity activity = controller.create().get();

    assertNotNull(activity.getWindowAndroid());
    verify(mMockNatives).setJavaActivity(eq(nativePointer), eq(activity));
    verify(mMockNatives).compositorViewCreated(eq(nativePointer), any());

    controller.destroy();
    verify(mMockNatives).onActivityDestroyed(eq(nativePointer));
  }

  @Test
  public void testCloseActivityFinishesAndNotifiesNative() {
    long nativePointer = 12345L;
    Intent intent = new Intent();
    intent.putExtra("native_pointer", nativePointer);

    ActivityController<CobaltPictureInPictureActivity> controller =
        Robolectric.buildActivity(CobaltPictureInPictureActivity.class, intent);
    CobaltPictureInPictureActivity activity = controller.create().get();

    activity.closeActivity();
    assertTrue(activity.isFinishing());

    controller.destroy();
    // Because closeActivity() no longer artificially clears the pointer,
    // onActivityDestroyed MUST be safely called to notify C++ of final teardown.
    verify(mMockNatives).onActivityDestroyed(eq(nativePointer));
  }
}
