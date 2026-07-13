/*
 * Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
package dev.cobalt.coat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.widget.FrameLayout;

import dev.cobalt.shell.Shell;
import dev.cobalt.shell.ShellManager;
import dev.cobalt.shell.ContentViewRenderView;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.robolectric.RobolectricTestRunner;

/** Tests for the ShellManager. */
@RunWith(RobolectricTestRunner.class)
@Config(
    shadows = {},
    manifest = Config.NONE)
public class ShellManagerTest {
  @Mock private ShellManager.Natives mMockShellManagerNatives;

  private Activity mActivity;
  private ShellManager mShellManager;
  private FrameLayout mRootView;
  @Mock private ContentViewRenderView mMockContentViewRenderView;


  @Before
  public void setUp() {
    initMocks(this);
    ShellManager.setNativesForTesting(mMockShellManagerNatives);
    mActivity = Robolectric.buildActivity(Activity.class).create().get();
    mRootView = new FrameLayout(mActivity);
    mActivity.setContentView(mRootView);
    mShellManager = new ShellManager(mActivity);
  }

  @Test
  public void testSetContentViewRenderView() {
    Shell shell = new Shell(mActivity);
    shell.setRootViewForTesting(mRootView);
    shell.setContentViewRenderView(mMockContentViewRenderView);

    assertEquals(1, mRootView.getChildCount());
    assertEquals(mMockContentViewRenderView, mRootView.getChildAt(0));

    shell.setContentViewRenderView(null);

    assertEquals(0, mRootView.getChildCount());
  }

  @Test
  public void testDestroyClosesActiveShellAndClearsContext() {
    Shell mockShell = mock(Shell.class);
    ReflectionHelpers.setField(mShellManager, "mActiveShell", mockShell);

    mShellManager.destroy();

    verify(mockShell).close();
    verify(mMockShellManagerNatives).destroy(mShellManager);
    assertNull(ReflectionHelpers.getField(mShellManager, "mActiveShell"));
    assertNull(ReflectionHelpers.getField(mShellManager, "mContext"));
  }

  @Test
  public void testDestroyHandlesBeingCalledTwice() {
    Shell mockShell = mock(Shell.class);
    ReflectionHelpers.setField(mShellManager, "mActiveShell", mockShell);

    mShellManager.destroy();
    mShellManager.destroy();

    verify(mockShell, times(1)).close();
    verify(mMockShellManagerNatives, times(1)).destroy(mShellManager);
  }
}
