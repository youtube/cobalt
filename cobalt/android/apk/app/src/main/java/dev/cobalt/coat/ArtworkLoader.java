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
import android.os.Handler;
import android.os.Looper;
import android.util.Pair;
import android.util.Size;
import androidx.annotation.NonNull;
import dev.cobalt.util.DisplayUtil;
import java.util.Locale;

/** Loads MediaImage artwork, and caches one image. */
public class ArtworkLoader {

  /** Callback to receive the image loaded in the background by getOrLoadArtwork() */
  public interface Callback {
    void onArtworkLoaded(Bitmap bitmap);
  }

  @NonNull private volatile String requestedArtworkUrl = "";
  @NonNull private volatile String currentArtworkUrl = "";
  private volatile Bitmap currentArtwork = null;

  private final Handler handler = new Handler(Looper.getMainLooper());
  private final ArtworkDownloader artworkDownloader;
  private final Callback callback;

  public ArtworkLoader(Callback callback, ArtworkDownloader artworkDownloader) {
    this.callback = callback;
    this.artworkDownloader = artworkDownloader;
  }

  /**
   * Returns a cached image if available. If not cached, returns null and starts downloading it in
   * the background, and then when ready the callback will be called with the image.
   */
  public synchronized Bitmap getOrLoadArtwork(MediaImage[] artwork) {
    MediaImage image = getBestFitImage(artwork, DisplayUtil.getDisplaySize());
    String url = (image == null) ? "" : image.src;

    // Check if this artwork is already loaded or requested.
    if (url.equals(currentArtworkUrl)) {
      return currentArtwork;
    } else if (url.equals(requestedArtworkUrl)) {
      return null;
    }

    requestedArtworkUrl = url;
    new DownloadArtworkThread(url, this).start();
    return null;
  }

  /**
   * Returns the image that most closely matches the display size, or null if there are no images.
   * We don't really know what size view the image may appear in (on the Now Playing card on Android
   * TV launcher, or any other observer of the MediaSession), so we use display size as the largest
   * useful size on any particular device.
   */
  private MediaImage getBestFitImage(MediaImage[] artwork, Size displaySize) {
    if (artwork == null || artwork.length == 0) {
      return null;
    }
    MediaImage bestImage = artwork[0];
    int minDiagonalSquared = Integer.MAX_VALUE;
    for (MediaImage image : artwork) {
      Size imageSize = parseImageSize(image);
      int widthDelta = displaySize.getWidth() - imageSize.getWidth();
      int heightDelta = displaySize.getHeight() - imageSize.getHeight();
      int diagonalSquared = widthDelta * widthDelta + heightDelta * heightDelta;
      if (diagonalSquared < minDiagonalSquared) {
        bestImage = image;
        minDiagonalSquared = diagonalSquared;
      }
    }
    return bestImage;
  }

  private Size parseImageSize(MediaImage image) {
    try {
      String sizeStr = image.sizes.split("\\s+", -1)[0];
      return Size.parseSize(sizeStr.toLowerCase(Locale.US));
    } catch (NumberFormatException | NullPointerException e) {
      return new Size(0, 0);
    }
  }

  public Bitmap cropTo16x9(Bitmap bitmap) {
    // Crop to 16:9 as needed
    if (bitmap != null) {
      int height = bitmap.getWidth() * 9 / 16;
      if (bitmap.getHeight() > height) {
        int top = (bitmap.getHeight() - height) / 2;
        return Bitmap.createBitmap(bitmap, 0, top, bitmap.getWidth(), height);
      }
    }
    return bitmap;
  }

  public synchronized void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
    String url = urlBitmapPair.first;
    Bitmap bitmap = urlBitmapPair.second;
    if (url.equals(requestedArtworkUrl)) {
      requestedArtworkUrl = "";
      if (bitmap != null) {
        currentArtworkUrl = url;
        currentArtwork = bitmap;

        handler.post(
            new Runnable() {
              @Override
              public void run() {
                callback.onArtworkLoaded(bitmap);
              }
            });
      }
    }
  }

  private class DownloadArtworkThread extends Thread {

    private final String url;
    private final ArtworkLoader artworkLoader;

    DownloadArtworkThread(String url, ArtworkLoader artworkLoader) {
      super("ArtworkLoader");
      this.url = url;
      this.artworkLoader = artworkLoader;
    }

    @Override
    public void run() {
      artworkDownloader.downloadArtwork(url, artworkLoader);
    }
  }
}
