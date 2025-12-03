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
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.util.Pair;
import java.util.Collections;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
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

  /*
   * The following tests are ignored because they require MediaImage and GURL.
   * GURL requires native library initialization which is difficult to set up in this environment,
   * and MediaImage is a final class so it cannot be mocked easily.
   */
  @Ignore("Requires GURL native initialization")
  @Test
  public void testGetOrLoadArtwork_RequestsDownload() {
    // String url = "http://example.com/image.png";
    // MediaImage mediaImage = createMediaImage(url, 1920, 1080);
    // List<MediaImage> images = Collections.singletonList(mediaImage);

    // Bitmap result = mArtworkLoader.getOrLoadArtwork(images);

    // assertThat(result).isNull();
    // // Allow background thread to start
    // try {
    //     Thread.sleep(100);
    // } catch (InterruptedException e) {
    //     // ignore
    // }
    // verify(mMockDownloader).downloadArtwork(eq(url), eq(mArtworkLoader));
  }

  @Ignore("Requires GURL native initialization")
  @Test
  public void testGetOrLoadArtwork_AlreadyRequested() {
    // ...
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

      // Check that it's now cached
      // We can't use getOrLoadArtwork to verify cache because of GURL,
      // but we can verify internal state via reflection if needed, or trust that onArtworkLoaded was called.
      // Also we can check if it calls download again if we could call getOrLoadArtwork...
    }

    @Test
    public void testOnDownloadFinished_WrongUrl() {
      String requestedUrl = "http://example.com/image.png";
      mArtworkLoader.mRequestedArtworkUrl = requestedUrl;

      String wrongUrl = "http://example.com/other.png";
      Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
      mArtworkLoader.onDownloadFinished(Pair.create(wrongUrl, bitmap));

      ShadowLooper.runUiThreadTasks();
      verify(mMockCallback, never()).onArtworkLoaded(any());
      assertThat(bitmap.isRecycled()).isTrue();
    }

    @Test
    public void testConsumeBitmapAndCropTo16x9_Exact16x9() {
      Bitmap bitmap = Bitmap.createBitmap(160, 90, Bitmap.Config.ARGB_8888);
      Bitmap result = mArtworkLoader.consumeBitmapAndCropTo16x9(bitmap);
      assertThat(result).isEqualTo(bitmap);
      assertThat(result.getWidth()).isEqualTo(160);
      assertThat(result.getHeight()).isEqualTo(90);
      assertThat(result.isRecycled()).isFalse();
    }

    @Test
    public void testConsumeBitmapAndCropTo16x9_WiderThan16x9() {
      Bitmap bitmap = Bitmap.createBitmap(200, 90, Bitmap.Config.ARGB_8888);
      Bitmap result = mArtworkLoader.consumeBitmapAndCropTo16x9(bitmap);
      assertThat(result).isEqualTo(bitmap);
    }

    @Test
    public void testConsumeBitmapAndCropTo16x9_TallerThan16x9() {
      Bitmap bitmap = Bitmap.createBitmap(160, 200, Bitmap.Config.ARGB_8888);
      Bitmap result = mArtworkLoader.consumeBitmapAndCropTo16x9(bitmap);

      assertThat(result).isNotEqualTo(bitmap);
      assertThat(result.getWidth()).isEqualTo(160);
      assertThat(result.getHeight()).isEqualTo(90);
      assertThat(bitmap.isRecycled()).isTrue();
    }

    @Test
    public void testConsumeBitmapAndCropTo16x9_Null() {
        assertThat(mArtworkLoader.consumeBitmapAndCropTo16x9(null)).isNull();
    }
  }
