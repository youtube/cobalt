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
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.text.TextUtils;
import dev.cobalt.util.Log;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * The AOSP Android MediaSession. Cobalt sends the web app's media session through the Starboard
 * MediaSession extension (android_media_session_client_aosp.cc), and platform media actions are
 * sent back the same way. It also holds audio focus while the web app plays, so that the platform
 * sends media controls to this session.
 */
@JNINamespace("starboard")
public class StarboardMediaSession implements AudioManager.OnAudioFocusChangeListener {
  // Playback states, in sync with starboard::PlaybackState in android_media_session_client.h.
  private static final int PLAYBACK_STATE_PLAYING = 0;
  private static final int PLAYBACK_STATE_PAUSED = 1;
  private static final int PLAYBACK_STATE_NONE = 2;

  // We do handle transport controls and set this flag on all API levels, even though it's
  // deprecated and unnecessary on API 26+.
  @SuppressWarnings("deprecation")
  private static final int MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS =
      MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS;

  private static StarboardMediaSession sInstance;

  private final Handler mMainHandler = new Handler(Looper.getMainLooper());
  private final Context mContext;
  // The fields below are only used on the main looper.
  private MediaSessionCompat mMediaSession;
  private AudioFocusRequest mAudioFocusRequest;
  private boolean mHasAudioFocus;
  // Whether the web app was paused for a transient audio focus loss, so it resumes when focus
  // comes back.
  private boolean mTransientPause;
  private int mPlaybackState = PLAYBACK_STATE_NONE;

  private final MediaSessionCompat.Callback mMediaSessionCallback =
      new MediaSessionCompat.Callback() {
        @Override
        public void onPlay() {
          invokeAction(PlaybackStateCompat.ACTION_PLAY, 0);
        }

        @Override
        public void onPause() {
          // Stay paused when audio focus comes back.
          mTransientPause = false;
          invokeAction(PlaybackStateCompat.ACTION_PAUSE, 0);
        }

        @Override
        public void onSkipToPrevious() {
          invokeAction(PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS, 0);
        }

        @Override
        public void onSkipToNext() {
          invokeAction(PlaybackStateCompat.ACTION_SKIP_TO_NEXT, 0);
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

        @Override
        public void onStop() {
          mTransientPause = false;
          invokeAction(PlaybackStateCompat.ACTION_STOP, 0);
        }
      };

  private StarboardMediaSession(Context context) {
    mContext = context;
  }

  /** Creates the instance that Cobalt's media session updates go to. */
  public static void initialize(Context context) {
    sInstance = new StarboardMediaSession(context);
  }

  private static void invokeAction(long action, long seekMs) {
    Log.i(TAG, "MediaSession action: %d, seek: %d ms", action, seekMs);
    StarboardMediaSessionJni.get().invokeAction(action, seekMs);
  }

  @CalledByNative
  private static void updateMediaSession(
      int playbackState,
      long actions,
      long positionMs,
      float speed,
      String title,
      String artist,
      String album,
      String artworkUri,
      long durationMs) {
    StarboardMediaSession instance = sInstance;
    if (instance == null) {
      return;
    }
    instance.mMainHandler.post(
        () ->
            instance.update(
                playbackState,
                actions,
                positionMs,
                speed,
                title,
                artist,
                album,
                artworkUri,
                durationMs));
  }

  /**
   * Removes the session. Cobalt calls it when the web app's session ends, and StarboardBridge calls
   * it before the app suspends, so that the platform stops showing it as playing.
   */
  @CalledByNative
  static void deactivateMediaSession() {
    StarboardMediaSession instance = sInstance;
    if (instance == null) {
      return;
    }
    instance.mMainHandler.post(instance::deactivate);
  }

  private void update(
      int playbackState,
      long actions,
      long positionMs,
      float speed,
      String title,
      String artist,
      String album,
      String artworkUri,
      long durationMs) {
    mPlaybackState = playbackState;
    if (playbackState == PLAYBACK_STATE_NONE) {
      deactivate();
      return;
    }
    if (playbackState == PLAYBACK_STATE_PLAYING) {
      requestAudioFocus();
    } else if (!mTransientPause) {
      // Keep audio focus during a transient pause, otherwise it doesn't come back. The web app
      // can still report playing before it pauses, so only a focus gain or a pause or stop
      // action ends the transient pause.
      abandonAudioFocus();
    }
    if (mMediaSession == null) {
      mMediaSession = new MediaSessionCompat(mContext, TAG);
      mMediaSession.setFlags(MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS);
      mMediaSession.setCallback(mMediaSessionCallback);
      mMediaSession.setActive(true);
      Log.i(TAG, "MediaSession is activated.");
    }

    int state =
        playbackState == PLAYBACK_STATE_PLAYING
            ? PlaybackStateCompat.STATE_PLAYING
            : PlaybackStateCompat.STATE_PAUSED;
    mMediaSession.setPlaybackState(
        new PlaybackStateCompat.Builder()
            .setActions(
                PlaybackStateCompat.ACTION_PLAY
                    | PlaybackStateCompat.ACTION_PAUSE
                    | PlaybackStateCompat.ACTION_PLAY_PAUSE
                    | actions)
            .setState(state, positionMs, speed)
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
      // TODO(b/583627308): Load the image and set METADATA_KEY_ART, as CobaltMediaSession does.
      metadata.putString(MediaMetadataCompat.METADATA_KEY_ART_URI, artworkUri);
      metadata.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON_URI, artworkUri);
    }
    if (durationMs >= 0) {
      metadata.putLong(MediaMetadataCompat.METADATA_KEY_DURATION, durationMs);
    }
    mMediaSession.setMetadata(metadata.build());
  }

  private void requestAudioFocus() {
    if (mHasAudioFocus) {
      return;
    }
    if (mAudioFocusRequest == null) {
      mAudioFocusRequest =
          new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
              .setAudioAttributes(
                  new AudioAttributes.Builder()
                      .setUsage(AudioAttributes.USAGE_MEDIA)
                      .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                      .build())
              .setOnAudioFocusChangeListener(this, mMainHandler)
              .build();
    }
    int result = getAudioManager().requestAudioFocus(mAudioFocusRequest);
    mHasAudioFocus = result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
    if (!mHasAudioFocus) {
      Log.w(TAG, "Audio focus not granted, pausing.");
      invokeAction(PlaybackStateCompat.ACTION_PAUSE, 0);
    }
  }

  private void abandonAudioFocus() {
    mTransientPause = false;
    if (!mHasAudioFocus) {
      return;
    }
    getAudioManager().abandonAudioFocusRequest(mAudioFocusRequest);
    mHasAudioFocus = false;
  }

  private AudioManager getAudioManager() {
    return (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
  }

  /** Called on the main looper when another app takes or returns audio focus. */
  @Override
  public void onAudioFocusChange(int focusChange) {
    switch (focusChange) {
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
        Log.i(TAG, "Audio focus lost (transient).");
        if (mPlaybackState == PLAYBACK_STATE_PLAYING) {
          mTransientPause = true;
          invokeAction(PlaybackStateCompat.ACTION_PAUSE, 0);
        }
        break;
      case AudioManager.AUDIOFOCUS_LOSS:
        Log.i(TAG, "Audio focus lost.");
        abandonAudioFocus();
        if (mPlaybackState == PLAYBACK_STATE_PLAYING) {
          invokeAction(PlaybackStateCompat.ACTION_PAUSE, 0);
        }
        break;
      case AudioManager.AUDIOFOCUS_GAIN:
        Log.i(TAG, "Audio focus gained.");
        if (mTransientPause && mPlaybackState == PLAYBACK_STATE_PAUSED) {
          invokeAction(PlaybackStateCompat.ACTION_PLAY, 0);
        }
        mTransientPause = false;
        break;
      default:
        // The platform ducks the volume itself on AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK.
        break;
    }
  }

  private void deactivate() {
    abandonAudioFocus();
    if (mMediaSession == null) {
      return;
    }
    mMediaSession.setCallback(null);
    mMediaSession.setActive(false);
    mMediaSession.release();
    mMediaSession = null;
    Log.i(TAG, "MediaSession has been deactivated.");
  }

  @NativeMethods
  interface Natives {
    void invokeAction(long action, long seekMs);
  }
}
