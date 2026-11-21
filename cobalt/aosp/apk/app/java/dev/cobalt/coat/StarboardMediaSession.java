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

import static dev.cobalt.util.Log.TAG;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.text.TextUtils;
import androidx.annotation.Nullable;
import dev.cobalt.util.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Android MediaSession for the AOSP (Starboard) build.
 *
 * <p>The Android TV build observes Chromium's MediaSession directly in {@code CobaltMediaSession}.
 * AOSP runs a prebuilt Evergreen Core and has no Java WebContents, so the web app's media session
 * reaches this class through the Starboard MediaSession extension
 * (starboard/android/shared/android_media_session_client.cc) instead. This class mirrors that state
 * into a {@link MediaSessionCompat} so that system controllers (e.g. Alexa on Fire TV, Google
 * Assistant, the launcher's "Now Playing") can see and control playback, and forwards their
 * transport controls back to native code.
 */
@JNINamespace("starboard")
public class StarboardMediaSession {
  // Must match starboard::PlaybackState in android_media_session_client.h.
  private static final int PLAYBACK_STATE_PLAYING = 0;
  private static final int PLAYBACK_STATE_PAUSED = 1;
  private static final int PLAYBACK_STATE_NONE = 2;

  @NativeMethods
  interface Natives {
    /**
     * Invokes a media session action in the web app.
     *
     * @param action An android.media.session.PlaybackState ACTION_* constant.
     * @param seekMs Target position for ACTION_SEEK_TO, otherwise ignored.
     */
    void invokeAction(long action, long seekMs);
  }

  private static final Object sLock = new Object();
  @Nullable private static StarboardMediaSession sInstance;

  private final Context mContext;
  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  // Only accessed on the main thread.
  @Nullable private MediaSessionCompat mMediaSession;

  /** Creates the process-wide instance. Called once by the AOSP StarboardBridge. */
  public static void initialize(Context appContext) {
    synchronized (sLock) {
      if (sInstance == null) {
        sInstance = new StarboardMediaSession(appContext);
      }
    }
  }

  @Nullable
  private static StarboardMediaSession getInstance() {
    synchronized (sLock) {
      return sInstance;
    }
  }

  private StarboardMediaSession(Context appContext) {
    mContext = appContext;
  }

  /**
   * Called from native code (any thread) whenever the web app's media session changes.
   *
   * @param playbackState One of the PLAYBACK_STATE_* constants.
   * @param actions Bitmask of android.media.session.PlaybackState ACTION_* constants.
   * @param positionMs Current playback position, in milliseconds.
   * @param playbackRate Current playback rate.
   * @param durationMs Media duration in milliseconds, or -1 if unknown/live.
   */
  @CalledByNative
  private static void updateMediaSession(
      int playbackState,
      long actions,
      long positionMs,
      float playbackRate,
      @Nullable String title,
      @Nullable String artist,
      @Nullable String album,
      @Nullable String artworkUri,
      long durationMs) {
    StarboardMediaSession instance = getInstance();
    if (instance == null) {
      Log.w(TAG, "StarboardMediaSession update before initialization; dropping.");
      return;
    }
    instance.mMainHandler.post(() ->
        instance.updateOnMainThread(
            playbackState,
            actions,
            positionMs,
            playbackRate,
            title,
            artist,
            album,
            artworkUri,
            durationMs));
  }

  /**
   * Called from native code (any thread) when the web engine's media session goes away, and by
   * StarboardBridge before the app is suspended so that the "Now Playing" state is cleared
   * promptly.
   */
  @CalledByNative
  static void deactivateMediaSession() {
    StarboardMediaSession instance = getInstance();
    if (instance != null) {
      instance.mMainHandler.post(instance::deactivate);
    }
  }

  private void updateOnMainThread(
      int playbackState,
      long actions,
      long positionMs,
      float playbackRate,
      @Nullable String title,
      @Nullable String artist,
      @Nullable String album,
      @Nullable String artworkUri,
      long durationMs) {
    if (playbackState == PLAYBACK_STATE_NONE) {
      deactivate();
      return;
    }
    activate();

    int androidState =
        playbackState == PLAYBACK_STATE_PLAYING
            ? PlaybackStateCompat.STATE_PLAYING
            : PlaybackStateCompat.STATE_PAUSED;
    // Play/pause are always offered while a session exists so that controllers can toggle
    // playback even if the web app did not register explicit handlers; the web engine falls
    // back to pausing/resuming the media element.
    long androidActions =
        actions
            | PlaybackStateCompat.ACTION_PLAY
            | PlaybackStateCompat.ACTION_PAUSE
            | PlaybackStateCompat.ACTION_PLAY_PAUSE;
    mMediaSession.setPlaybackState(
        new PlaybackStateCompat.Builder()
            .setState(androidState, positionMs, playbackRate)
            .setActions(androidActions)
            .build());

    MediaMetadataCompat.Builder metadata = new MediaMetadataCompat.Builder();
    if (!TextUtils.isEmpty(title)) {
      metadata.putString(MediaMetadataCompat.METADATA_KEY_TITLE, title);
      metadata.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE, title);
    }
    if (!TextUtils.isEmpty(artist)) {
      metadata.putString(MediaMetadataCompat.METADATA_KEY_ARTIST, artist);
      metadata.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_SUBTITLE, artist);
    }
    if (!TextUtils.isEmpty(album)) {
      metadata.putString(MediaMetadataCompat.METADATA_KEY_ALBUM, album);
    }
    if (!TextUtils.isEmpty(artworkUri)) {
      // TODO: Download and set METADATA_KEY_ART like CobaltMediaSession/ArtworkLoader do. URIs
      // are enough for most controllers and avoid network access here.
      metadata.putString(MediaMetadataCompat.METADATA_KEY_ART_URI, artworkUri);
      metadata.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON_URI, artworkUri);
    }
    metadata.putLong(MediaMetadataCompat.METADATA_KEY_DURATION, durationMs);
    mMediaSession.setMetadata(metadata.build());
  }

  private void activate() {
    if (mMediaSession != null) {
      return;
    }
    mMediaSession = new MediaSessionCompat(mContext, TAG);
    mMediaSession.setFlags(MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS);
    mMediaSession.setCallback(new Callback(), mMainHandler);
    mMediaSession.setActive(true);
    Log.i(TAG, "StarboardMediaSession activated.");
  }

  private void deactivate() {
    if (mMediaSession == null) {
      return;
    }
    mMediaSession.setActive(false);
    mMediaSession.release();
    mMediaSession = null;
    Log.i(TAG, "StarboardMediaSession deactivated.");
  }

  private static void invokeAction(long action, long seekMs) {
    Log.i(TAG, "StarboardMediaSession action: %d", action);
    StarboardMediaSessionJni.get().invokeAction(action, seekMs);
  }

  private static class Callback extends MediaSessionCompat.Callback {
    @Override
    public void onPlay() {
      invokeAction(PlaybackStateCompat.ACTION_PLAY, 0);
    }

    @Override
    public void onPause() {
      invokeAction(PlaybackStateCompat.ACTION_PAUSE, 0);
    }

    @Override
    public void onStop() {
      invokeAction(PlaybackStateCompat.ACTION_STOP, 0);
    }

    @Override
    public void onSkipToNext() {
      invokeAction(PlaybackStateCompat.ACTION_SKIP_TO_NEXT, 0);
    }

    @Override
    public void onSkipToPrevious() {
      invokeAction(PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS, 0);
    }

    @Override
    public void onFastForward() {
      invokeAction(PlaybackStateCompat.ACTION_FAST_FORWARD, 0);
    }

    @Override
    public void onRewind() {
      invokeAction(PlaybackStateCompat.ACTION_REWIND, 0);
    }

    @Override
    public void onSeekTo(long pos) {
      invokeAction(PlaybackStateCompat.ACTION_SEEK_TO, pos);
    }
  }
}
