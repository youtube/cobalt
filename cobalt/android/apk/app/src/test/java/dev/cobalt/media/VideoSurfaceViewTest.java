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

package dev.cobalt.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.view.SurfaceControl;
import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for {@link VideoSurfaceView}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VideoSurfaceViewTest {

  @After
  public void tearDown() {
    VideoSurfaceView.setInstanceForTesting(null);
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.Q)
  public void testSetVideoSurfaceVisible_noOpWhenNoInstance() {
    VideoSurfaceView.setInstanceForTesting(null);
    VideoSurfaceView.setVideoSurfaceVisible(false);
    VideoSurfaceView.setVideoSurfaceVisible(true);
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.Q)
  public void testSetVideoSurfaceVisible_queriesSurfaceControlOnInstance() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    when(view.getSurfaceControl()).thenReturn(sc);
    VideoSurfaceView.setInstanceForTesting(view);

    VideoSurfaceView.setVideoSurfaceVisible(false);

    verify(view).getSurfaceControl();
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.Q)
  public void testSetVideoSurfaceVisible_noOpWhenSurfaceControlInvalid() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    sc.release();
    when(view.getSurfaceControl()).thenReturn(sc);
    VideoSurfaceView.setInstanceForTesting(view);

    VideoSurfaceView.setVideoSurfaceVisible(false);

    verify(view).getSurfaceControl();
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.TIRAMISU)
  public void testSetVideoSurfaceVisibleWithNextFrame_returnsFalseBelowApi34() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    VideoSurfaceView.setInstanceForTesting(view);

    assertFalse(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
    verify(view, never()).getSurfaceControl();
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  public void testSetVideoSurfaceVisibleWithNextFrame_returnsFalseWhenNoInstance() {
    VideoSurfaceView.setInstanceForTesting(null);

    assertFalse(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  public void testSetVideoSurfaceVisibleWithNextFrame_returnsFalseWhenSurfaceControlInvalid() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    sc.release();
    when(view.getSurfaceControl()).thenReturn(sc);
    VideoSurfaceView.setInstanceForTesting(view);

    assertFalse(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
    verify(view, never()).applyTransactionToFrame(any());
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  public void testSetVideoSurfaceVisibleWithNextFrame_appliesTransactionToFrameOnApi34() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    when(view.getSurfaceControl()).thenReturn(sc);
    VideoSurfaceView.setInstanceForTesting(view);

    assertTrue(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
    verify(view).applyTransactionToFrame(any(SurfaceControl.Transaction.class));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  public void testSetVideoSurfaceVisibleWithNextFrame_returnsFalseWhenApplyTransactionThrows() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    when(view.getSurfaceControl()).thenReturn(sc);
    doThrow(new IllegalStateException("No surface"))
        .when(view)
        .applyTransactionToFrame(any(SurfaceControl.Transaction.class));
    VideoSurfaceView.setInstanceForTesting(view);

    assertFalse(VideoSurfaceView.setVideoSurfaceVisibleWithNextFrame(true));
  }

  @Test
  @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
  public void testShowVideoSurfaceWithNextFrame_attachesTransactionOnApi34() {
    VideoSurfaceView view = mock(VideoSurfaceView.class);
    SurfaceControl sc = new SurfaceControl.Builder().setName("test").build();
    when(view.getSurfaceControl()).thenReturn(sc);
    VideoSurfaceView.setInstanceForTesting(view);

    VideoSurfaceView.showVideoSurfaceWithNextFrame();

    verify(view).applyTransactionToFrame(any(SurfaceControl.Transaction.class));
  }
}
