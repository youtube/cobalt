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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.DisplayMetrics;
import android.util.Pair;
import dev.cobalt.util.DisplayUtil;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.chromium.services.media_session.MediaImage;
import org.chromium.url.GURL;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.util.ReflectionHelpers;

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

  private static MediaImage createMediaImage(String url, int width, int height) {
    GURL mockGurl = mock(GURL.class);
    when(mockGurl.getSpec()).thenReturn(url);
    return new MediaImage(
        mockGurl, "image/jpeg", Collections.singletonList(new Rect(0, 0, width, height)));
  }

  @Test
  public void testGetOrLoadArtwork_withOptimization_capsTargetSizeTo640x360() {
    DisplayMetrics metrics = new DisplayMetrics();
    metrics.widthPixels = 1920;
    metrics.heightPixels = 1080;
    ReflectionHelpers.setStaticField(DisplayUtil.class, "sCachedDisplayMetrics", metrics);

    String url360p = "https://i.ytimg.com/vi/abc/hqdefault_640x360.jpg";
    String url1080p = "https://i.ytimg.com/vi/abc/maxresdefault_1920x1080.jpg";
    List<MediaImage> images =
        Arrays.asList(createMediaImage(url360p, 640, 360), createMediaImage(url1080p, 1920, 1080));

    ArtworkLoader optimizedLoader =
        new ArtworkLoader(mMockCallback, mMockDownloader) {
          @Override
          protected boolean isMediaSessionOptimizationEnabled() {
            return true;
          }
        };
    assertThat(optimizedLoader.getOrLoadArtwork(images)).isNull();
    assertThat(optimizedLoader.mRequestedArtworkUrl).isEqualTo(url360p);
    verify(mMockDownloader, timeout(1000)).downloadArtwork(eq(url360p), eq(optimizedLoader));

    ArtworkDownloader unoptimizedDownloader = mock(ArtworkDownloader.class);
    ArtworkLoader unoptimizedLoader =
        new ArtworkLoader(mMockCallback, unoptimizedDownloader) {
          @Override
          protected boolean isMediaSessionOptimizationEnabled() {
            return false;
          }
        };
    assertThat(unoptimizedLoader.getOrLoadArtwork(images)).isNull();
    assertThat(unoptimizedLoader.mRequestedArtworkUrl).isEqualTo(url1080p);
    verify(unoptimizedDownloader, timeout(1000))
        .downloadArtwork(eq(url1080p), eq(unoptimizedLoader));
  }

  @Test
  public void testClearCachedArtwork_clearsUrlsAndBitmapReference() {
    Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mArtworkLoader.mRequestedArtworkUrl = "https://i.ytimg.com/pending.jpg";
    mArtworkLoader.mCurrentArtworkUrl = "https://i.ytimg.com/current.jpg";
    mArtworkLoader.mCurrentArtwork = bitmap;

    mArtworkLoader.clearCachedArtwork();

    assertThat(mArtworkLoader.mRequestedArtworkUrl).isEmpty();
    assertThat(mArtworkLoader.mCurrentArtworkUrl).isEmpty();
    assertThat(mArtworkLoader.mCurrentArtwork).isNull();
  }

  @Test
  public void testClearCachedArtwork_suppressesStaleUiCallbackAndRecyclesBitmap() {
    String url = "https://i.ytimg.com/vi/abc/hqdefault.jpg";
    Bitmap staleBitmap = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888);
    mArtworkLoader.mRequestedArtworkUrl = url;

    // Background thread finishes and posts UI callback before session destruction.
    mArtworkLoader.onDownloadFinished(android.util.Pair.create(url, staleBitmap));
    verify(mMockCallback, org.mockito.Mockito.never()).onArtworkLoaded(staleBitmap);

    // MediaSession is destroyed before the posted UI runnable executes.
    mArtworkLoader.clearCachedArtwork();
    ShadowLooper.runUiThreadTasks();

    verify(mMockCallback, org.mockito.Mockito.never()).onArtworkLoaded(staleBitmap);
    assertThat(staleBitmap.isRecycled()).isTrue();
  }
}
