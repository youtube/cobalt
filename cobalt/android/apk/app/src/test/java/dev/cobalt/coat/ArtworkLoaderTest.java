// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.util.Pair;
import java.util.Collections;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

/** Tests for {@link ArtworkLoader}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArtworkLoaderTest {

  private ArtworkLoader.Callback mMockCallback;
  private ArtworkDownloader mMockDownloader;
  private ArtworkLoader mArtworkLoader;

  @Before
  public void setUp() {
    mMockCallback = mock(ArtworkLoader.Callback.class);
    mMockDownloader = mock(ArtworkDownloader.class);
    mArtworkLoader = new ArtworkLoader(mMockCallback, mMockDownloader);
  }

  @Test
  public void testGetOrLoadArtwork_NullOrEmptyList() {
    assertThat(mArtworkLoader.getOrLoadArtwork(null)).isNull();
    assertThat(mArtworkLoader.getOrLoadArtwork(Collections.emptyList())).isNull();
    verify(mMockDownloader, never()).downloadArtwork(any(), any());
  }

  @Test
  public void testOnDownloadFinished_Success() {
    String url = "http://example.com/image.png";
    // Pre-set the requested URL
    mArtworkLoader.mRequestedArtworkUrl = url;

    Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mArtworkLoader.onDownloadFinished(Pair.create(url, bitmap));

    ShadowLooper.runUiThreadTasks();
    verify(mMockCallback).onArtworkLoaded(eq(bitmap));
  }

  @Test
  public void testOnDownloadFinished_WrongUrl() {
    String requestedUrl = "http://example.com/image.png";
    mArtworkLoader.mRequestedArtworkUrl = requestedUrl;

    String wrongUrl = "http://example.com/other.png";
    Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mArtworkLoader.onDownloadFinished(Pair.create(wrongUrl, bitmap));

    // Expected: The callback should not be invoked as the URL does not match the requested one.
    // The downloaded bitmap should still be recycled to free up memory.
    ShadowLooper.runUiThreadTasks();
    verify(mMockCallback, never()).onArtworkLoaded(any());
    assertThat(bitmap.isRecycled()).isTrue();
  }

  @Test
  public void testCropTo16x9_Exact16x9() {
    Bitmap bitmap = Bitmap.createBitmap(160, 90, Bitmap.Config.ARGB_8888);
    Bitmap result = mArtworkLoader.cropTo16x9(bitmap);
    assertThat(result).isEqualTo(bitmap);
    assertThat(result.getWidth()).isEqualTo(160);
    assertThat(result.getHeight()).isEqualTo(90);
    assertThat(result.isRecycled()).isFalse();
  }

  @Test
  public void testCropTo16x9_WiderThan16x9() {
    Bitmap bitmap = Bitmap.createBitmap(200, 90, Bitmap.Config.ARGB_8888);
    Bitmap result = mArtworkLoader.cropTo16x9(bitmap);
    assertThat(result).isEqualTo(bitmap);
  }

  @Test
  public void testCropTo16x9_TallerThan16x9() {
    Bitmap bitmap = Bitmap.createBitmap(160, 200, Bitmap.Config.ARGB_8888);
    Bitmap result = mArtworkLoader.cropTo16x9(bitmap);

    assertThat(result).isNotEqualTo(bitmap);
    assertThat(result.getWidth()).isEqualTo(160);
    assertThat(result.getHeight()).isEqualTo(90);
    assertThat(bitmap.isRecycled()).isTrue();
  }

  @Test
  public void testCropTo16x9_Null() {
    assertThat(mArtworkLoader.cropTo16x9(null)).isNull();
  }

  @Test
  public void testOnDownloadFinished_replacesExistingArtworkAndRecyclesOld() {
    String url = "http://example.com/image.png";
    Bitmap oldBitmap = Bitmap.createBitmap(50, 50, Bitmap.Config.ARGB_8888);
    Bitmap newBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

    // Simulate that an old bitmap is already loaded
    mArtworkLoader.mCurrentArtwork = oldBitmap;
    mArtworkLoader.mRequestedArtworkUrl = url;

    mArtworkLoader.onDownloadFinished(Pair.create(url, newBitmap));

    // Run the UI thread task that calls the callback
    ShadowLooper.runUiThreadTasks();

    verify(mMockCallback).onArtworkLoaded(eq(newBitmap));
    assertThat(newBitmap.isRecycled()).isFalse();
    assertThat(oldBitmap.isRecycled()).isTrue();
  }

  @Test
  public void testScaleDownToMaxSize_LargerThanMax() {
    Bitmap bitmap = Bitmap.createBitmap(1280, 720, Bitmap.Config.ARGB_8888);
    Bitmap result = ArtworkLoader.scaleDownToMaxSize(bitmap, 640);

    assertThat(result.getWidth()).isEqualTo(640);
    assertThat(result.getHeight()).isEqualTo(360);
    assertThat(bitmap.isRecycled()).isTrue();
  }

  @Test
  public void testScaleDownToMaxSize_TallerThanMax() {
    Bitmap bitmap = Bitmap.createBitmap(300, 1000, Bitmap.Config.ARGB_8888);
    Bitmap result = ArtworkLoader.scaleDownToMaxSize(bitmap, 500);

    assertThat(result.getWidth()).isEqualTo(150);
    assertThat(result.getHeight()).isEqualTo(500);
  }

  @Test
  public void testScaleDownToMaxSize_FitsAlready() {
    Bitmap bitmap = Bitmap.createBitmap(640, 360, Bitmap.Config.ARGB_8888);
    Bitmap result = ArtworkLoader.scaleDownToMaxSize(bitmap, 640);

    assertThat(result).isEqualTo(bitmap);
    assertThat(result.isRecycled()).isFalse();
  }

  @Test
  public void testScaleDownToMaxSize_Null() {
    assertThat(ArtworkLoader.scaleDownToMaxSize(null, 640)).isNull();
  }

  @Test
  public void testOnDownloadFinished_ScalesDownLargeArtwork() {
    String url = "http://example.com/maxresdefault.jpg";
    mArtworkLoader.mRequestedArtworkUrl = url;
    int maxSizePx = ArtworkLoader.getMaxArtworkSizePx();

    Bitmap bitmap = Bitmap.createBitmap(maxSizePx * 4, maxSizePx * 2, Bitmap.Config.ARGB_8888);
    mArtworkLoader.onDownloadFinished(Pair.create(url, bitmap));

    ShadowLooper.runUiThreadTasks();
    ArgumentCaptor<Bitmap> captor = ArgumentCaptor.forClass(Bitmap.class);
    verify(mMockCallback).onArtworkLoaded(captor.capture());
    assertThat(captor.getValue().getWidth()).isEqualTo(maxSizePx);
    assertThat(captor.getValue().getHeight()).isEqualTo(maxSizePx / 2);
    assertThat(bitmap.isRecycled()).isTrue();
  }

  @Test
  public void testOnDownloadFinished_StaleDownloadNotScaled() {
    mArtworkLoader.mRequestedArtworkUrl = "http://example.com/new.jpg";
    int maxSizePx = ArtworkLoader.getMaxArtworkSizePx();
    Bitmap bitmap = Bitmap.createBitmap(maxSizePx * 4, maxSizePx * 2, Bitmap.Config.ARGB_8888);

    mArtworkLoader.onDownloadFinished(Pair.create("http://example.com/old.jpg", bitmap));

    ShadowLooper.runUiThreadTasks();
    verify(mMockCallback, never()).onArtworkLoaded(any());
    assertThat(bitmap.isRecycled()).isTrue();
  }

  @Test
  public void testGetMaxArtworkSizePx_Positive() {
    assertThat(ArtworkLoader.getMaxArtworkSizePx()).isGreaterThan(0);
  }

  @Test
  @Config(sdk = 31)
  public void testToShared_Api31_ReturnsImmutableAndRecyclesInput() {
    Bitmap bitmap = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888);
    Bitmap shared = ArtworkLoader.toShared(bitmap);

    assertThat(shared.isMutable()).isFalse();
    assertThat(shared.getWidth()).isEqualTo(64);
    if (shared != bitmap) {
      assertThat(bitmap.isRecycled()).isTrue();
    }
  }

  @Test
  @Config(sdk = 28)
  public void testToShared_BelowApi31_ReturnsInput() {
    Bitmap bitmap = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888);
    assertThat(ArtworkLoader.toShared(bitmap)).isSameInstanceAs(bitmap);
  }

  @Test
  public void testToShared_Null() {
    assertThat(ArtworkLoader.toShared(null)).isNull();
  }

  @Test
  @Config(sdk = 31)
  public void testOnDownloadFinished_Api31_DeliversImmutableBitmap() {
    String url = "http://example.com/shared.png";
    mArtworkLoader.mRequestedArtworkUrl = url;
    Bitmap bitmap = Bitmap.createBitmap(100, 56, Bitmap.Config.ARGB_8888);
    assertThat(bitmap.isMutable()).isTrue();

    mArtworkLoader.onDownloadFinished(Pair.create(url, bitmap));

    ShadowLooper.runUiThreadTasks();
    ArgumentCaptor<Bitmap> captor = ArgumentCaptor.forClass(Bitmap.class);
    verify(mMockCallback).onArtworkLoaded(captor.capture());
    Bitmap delivered = captor.getValue();
    assertThat(delivered.isMutable()).isFalse();
    assertThat(delivered.isRecycled()).isFalse();
    assertThat(delivered.getWidth()).isEqualTo(100);
    assertThat(delivered.getHeight()).isEqualTo(56);
    assertThat(mArtworkLoader.mCurrentArtwork).isSameInstanceAs(delivered);
    if (delivered != bitmap) {
      assertThat(bitmap.isRecycled()).isTrue();
    }
  }

  @Test
  @Config(sdk = 31)
  public void testOnDownloadFinished_Api31_CroppedAndScaled_OldRecycledAfterCallback() {
    String url = "http://example.com/maxresdefault.jpg";
    int maxSizePx = ArtworkLoader.getMaxArtworkSizePx();
    Bitmap oldBitmap = ArtworkLoader.toShared(Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888));
    mArtworkLoader.mCurrentArtwork = oldBitmap;
    mArtworkLoader.mRequestedArtworkUrl = url;

    boolean[] oldRecycledDuringCallback = new boolean[1];
    doAnswer(
            invocation -> {
              oldRecycledDuringCallback[0] = oldBitmap.isRecycled();
              return null;
            })
        .when(mMockCallback)
        .onArtworkLoaded(any());

    // Mirror ArtworkDownloader flow: crop taller-than-16:9 bitmap before onDownloadFinished().
    Bitmap raw = Bitmap.createBitmap(maxSizePx * 2, maxSizePx * 2, Bitmap.Config.ARGB_8888);
    Bitmap cropped = mArtworkLoader.cropTo16x9(raw);
    assertThat(raw.isRecycled()).isTrue();

    mArtworkLoader.onDownloadFinished(Pair.create(url, cropped));

    ShadowLooper.runUiThreadTasks();
    ArgumentCaptor<Bitmap> captor = ArgumentCaptor.forClass(Bitmap.class);
    verify(mMockCallback).onArtworkLoaded(captor.capture());
    Bitmap delivered = captor.getValue();
    assertThat(delivered.isMutable()).isFalse();
    assertThat(delivered.isRecycled()).isFalse();
    assertThat(delivered.getWidth()).isEqualTo(maxSizePx);
    assertThat(delivered.getHeight()).isLessThan(maxSizePx);
    assertThat(cropped.isRecycled()).isTrue();
    assertThat(oldRecycledDuringCallback[0]).isFalse();
    assertThat(oldBitmap.isRecycled()).isTrue();
  }
}
