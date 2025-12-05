// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Looper;
import android.util.Pair;
import android.util.Size;
import androidx.annotation.CheckResult;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.DisplayUtil;
import java.util.List;
import org.chromium.services.media_session.MediaImage;

/** Loads MediaImage artwork, and caches one image. */
public class ArtworkLoader {

  /** Callback to receive the image loaded in the background by getOrLoadArtwork() */
  public interface Callback {
    void onArtworkLoaded(Bitmap bitmap);
  }

  @VisibleForTesting @NonNull volatile String mRequestedArtworkUrl = "";
  @VisibleForTesting @NonNull volatile String mCurrentArtworkUrl = "";
  @VisibleForTesting volatile Bitmap mCurrentArtwork = null;

  private final Handler mHandler = new Handler(Looper.getMainLooper());
  private final ArtworkDownloader mArtworkDownloader;
  private final Callback mCallback;

  /**
   * Constructs a new ArtworkLoader.
   *
   * @param callback The callback to receive the loaded image.
   * @param artworkDownloader The downloader to use for fetching the image.
   */
  public ArtworkLoader(Callback callback, ArtworkDownloader artworkDownloader) {
    this.mCallback = callback;
    this.mArtworkDownloader = artworkDownloader;
  }

  /**
   * Returns a cached image if available. If not cached, returns null and starts downloading it in
   * the background, and then when ready the callback will be called with the image.
   */
  public synchronized Bitmap getOrLoadArtwork(List<MediaImage> images) {
    if (images == null || images.isEmpty()) {
      return null;
    }

    MediaImage image = getBestFitImage(images, DisplayUtil.getDisplaySize());
    String url = image.getSrc().getSpec();

    // Check if this artwork is already loaded or requested.
    if (url.equals(mCurrentArtworkUrl)) {
      return mCurrentArtwork;
    } else if (url.equals(mRequestedArtworkUrl)) {
      return null;
    }

    mRequestedArtworkUrl = url;
    new DownloadArtworkThread(url, this).start();
    return null;
  }

  /**
   * Returns the image that most closely matches the display size, or null if there are no images.
   * We don't really know what size view the image may appear in (on the Now Playing card on Android
   * TV launcher, or any other observer of the MediaSession), so we use display size as the largest
   * useful size on any particular device.
   */
  private MediaImage getBestFitImage(List<MediaImage> images, Size displaySize) {
    MediaImage bestImage = images.get(0);
    int minDiagonalSquared = Integer.MAX_VALUE;
    for (MediaImage image : images) {
      if (image.getSizes().isEmpty()) {
        continue;
      }
      Rect imageRect = image.getSizes().get(0);
      int widthDelta = displaySize.getWidth() - imageRect.width();
      int heightDelta = displaySize.getHeight() - imageRect.height();
      int diagonalSquared = widthDelta * widthDelta + heightDelta * heightDelta;
      if (diagonalSquared < minDiagonalSquared) {
        bestImage = image;
        minDiagonalSquared = diagonalSquared;
      }
    }
    return bestImage;
  }

  /**
   * Crops the bitmap to 16:9 aspect ratio if necessary.
   *
   * If the bitmap is taller than 16:9, it is cropped from the center. If it is already 16:9 or
   * wider, it is returned as is.
   *
   * The input bitmap is recycled if a new cropped bitmap is created.
   * NOTE: This method is accessed from google3
   * http://google3/java/com/google/android/libraries/youtube/tv/media/ArtworkDownloaderNoGMS.java;l=54;rcl=705660438
   *
   * @param bitmap The source bitmap.
   * @return The 16:9 cropped bitmap, or the original bitmap.
   */
  @CheckResult
  public Bitmap cropTo16x9(Bitmap bitmap) {
    // Crop to 16:9 as needed
    if (bitmap == null) {
      return null;
    }
    int height = bitmap.getWidth() * 9 / 16;
    if (bitmap.getHeight() <= height) {
      return bitmap;
    }

    int top = (bitmap.getHeight() - height) / 2;
    Bitmap cropped = Bitmap.createBitmap(bitmap, 0, top, bitmap.getWidth(), height);
    bitmap.recycle();
    return cropped;
  }

  /**
   * Called when an artwork download has finished.
   *
   * @param urlBitmapPair A pair containing the URL and the downloaded Bitmap.
   */
  public synchronized void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
    String url = urlBitmapPair.first;
    Bitmap bitmap = urlBitmapPair.second;

    if (!mRequestedArtworkUrl.equals(url)) {
      if (bitmap != null) {
        bitmap.recycle();
      }
      return;
    }

    mRequestedArtworkUrl = "";

    if (bitmap == null) {
      return;
    }

    final Bitmap oldArtwork = mCurrentArtwork;
    mCurrentArtworkUrl = url;
    mCurrentArtwork = bitmap;

    mHandler.post(
        new Runnable() {
          @Override
          public void run() {
            try {
              mCallback.onArtworkLoaded(bitmap);
            } finally {
              if (oldArtwork != null) {
                oldArtwork.recycle();
              }
            }
          }
        });
  }

  private class DownloadArtworkThread extends Thread {

    private final String mUrl;
    private final ArtworkLoader mArtworkLoader;

    DownloadArtworkThread(String url, ArtworkLoader artworkLoader) {
      super("ArtworkLoader");
      this.mUrl = url;
      this.mArtworkLoader = artworkLoader;
    }

    @Override
    public void run() {
      mArtworkDownloader.downloadArtwork(mUrl, mArtworkLoader);
    }
  }
}
