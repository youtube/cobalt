// Copyright 2017 Google Inc. All Rights Reserved.
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

import static dev.cobalt.media.Log.TAG;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.media.AudioAttributes;
import android.media.AudioAttributes.Builder;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaSession;
import android.media.session.PlaybackState;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;
import dev.cobalt.util.DisplayUtil;
import dev.cobalt.util.Holder;

/**
 * Cobalt MediaSession glue, as well as collection of state
 * and logic to switch on/off Android OS features used in media playback,
 * such as audio focus, "KEEP_SCREEN_ON" mode, and "visible behind".
 */
public class CobaltMediaSession
    implements AudioManager.OnAudioFocusChangeListener, ArtworkLoader.Callback {

  // We do handle transport controls and set this flag on all API levels, even though it's
  // deprecated and unnecessary on API 26+.
  @SuppressWarnings("deprecation")
  private static final int MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS =
      MediaSession.FLAG_HANDLES_TRANSPORT_CONTROLS;

  private AudioFocusRequest audioFocusRequest;

  interface UpdateVolumeListener {
    /**
     * Called when there is a change in audio focus.
     */
    void onUpdateVolume(float gain);
  }

  /**
   * When losing audio focus with the option of ducking, we reduce the volume to 10%. This
   * arbitrary number is what YouTube Android Player infrastructure uses.
   */
  private static final float AUDIO_FOCUS_DUCK_LEVEL = 0.1f;

  private final Handler mainHandler = new Handler(Looper.getMainLooper());

  private final Context context;
  private final Holder<Activity> activityHolder;

  private final UpdateVolumeListener volumeListener;
  private final ArtworkLoader artworkLoader;
  private final MediaSession mediaSession;


  // We re-use the builder to hold onto the most recent metadata and add artwork later.
  MediaMetadata.Builder metadataBuilder = new MediaMetadata.Builder();

  // Accessed on the main looper thread only.
  private boolean active = false;
  private boolean mediaPlaying = false;
  private boolean suspended = true;

  // Duplicated in starboard/android/shared/android_media_session_client.h
  // PlaybackState
  private static final int PLAYBACK_STATE_PLAYING = 0;
  private static final int PLAYBACK_STATE_PAUSED = 1;
  private static final int PLAYBACK_STATE_NONE = 2;

  public CobaltMediaSession(
      Context context, Holder<Activity> activityHolder, UpdateVolumeListener volumeListener) {
    this.context = context;
    this.activityHolder = activityHolder;

    this.volumeListener = volumeListener;
    artworkLoader = new ArtworkLoader(this, DisplayUtil.getDisplaySize(context));
    mediaSession = new MediaSession(context, TAG);
    mediaSession.setFlags(MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS);
    mediaSession.setCallback(
        new MediaSession.Callback() {
          @Override
          public void onFastForward() {
            nativeInvokeAction(PlaybackState.ACTION_FAST_FORWARD);
          }

          @Override
          public void onPause() {
            nativeInvokeAction(PlaybackState.ACTION_PAUSE);
          }

          @Override
          public void onPlay() {
            nativeInvokeAction(PlaybackState.ACTION_PLAY);
          }

          @Override
          public void onRewind() {
            nativeInvokeAction(PlaybackState.ACTION_REWIND);
          }

          @Override
          public void onSkipToNext() {
            nativeInvokeAction(PlaybackState.ACTION_SKIP_TO_NEXT);
          }

          @Override
          public void onSkipToPrevious() {
            nativeInvokeAction(PlaybackState.ACTION_SKIP_TO_PREVIOUS);
          }
        });
  }

  private static void checkMainLooperThread() {
    if (Looper.getMainLooper() != Looper.myLooper()) {
      throw new RuntimeException("Must be on main thread");
    }
  }

  /** Must be called on the main looper thread */
  private void onMediaStart() {
    checkMainLooperThread();
    Activity activity = activityHolder.get();
    if (activity != null) {
      activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    if (Build.VERSION.SDK_INT < 26) {
      requestAudioFocus();
    } else {
      requestAudioFocusV26();
    }
  }

  /** Must be called on the main looper thread */
  private void onMediaStop() {
    checkMainLooperThread();
    Activity activity = activityHolder.get();
    if (activity != null) {
      activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    if (Build.VERSION.SDK_INT < 26) {
      abandonAudioFocus();
    } else {
      abandonAudioFocusV26();
    }
  }

  @SuppressWarnings("deprecation")
  private void requestAudioFocus() {
    getAudioManager()
        .requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
  }

  @TargetApi(26)
  private void requestAudioFocusV26() {
    if (audioFocusRequest == null) {
      AudioAttributes audioAtrributes = new Builder()
          .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
          .build();
      audioFocusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
          .setOnAudioFocusChangeListener(this)
          .setAudioAttributes(audioAtrributes)
          .build();
    }
    getAudioManager().requestAudioFocus(audioFocusRequest);
  }

  @SuppressWarnings("deprecation")
  private void abandonAudioFocus() {
    getAudioManager().abandonAudioFocus(this);
  }

  @TargetApi(26)
  private void abandonAudioFocusV26() {
    if (audioFocusRequest != null) {
      getAudioManager().abandonAudioFocusRequest(audioFocusRequest);
    }
  }

  /** AudioManager.OnAudioFocusChangeListener implementation. */
  @Override
  public void onAudioFocusChange(int focusChange) {
    switch (focusChange) {
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
      case AudioManager.AUDIOFOCUS_LOSS:
        if (mediaPlaying) {
          nativeInvokeAction(PlaybackState.ACTION_PAUSE);
        }
        break;
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
        // Lower the volume, keep current play state.
        volumeListener.onUpdateVolume(AUDIO_FOCUS_DUCK_LEVEL);
        break;
      case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
      case AudioManager.AUDIOFOCUS_GAIN:
      case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
        // The app has been granted audio focus again. Raise volume to normal,
        // restart playback if necessary.
        volumeListener.onUpdateVolume(1.0f);
        if (!mediaPlaying) {
          nativeInvokeAction(PlaybackState.ACTION_PLAY);
        }
        break;
      default: // fall out
    }
  }

  private AudioManager getAudioManager() {
    return (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
  }

  public void resume() {
    mainHandler.post(new Runnable() {
      @Override
      public void run() {
        resumeInternal();
      }
    });
  }

  private void resumeInternal() {
    checkMainLooperThread();
    suspended = false;
    // Undoing what may have been done in onStop().
    if (mediaPlaying) {
      onMediaStart();
    }
    mediaSession.setActive(active);
  }

  public void suspend() {
    mainHandler.post(new Runnable() {
      @Override
      public void run() {
        suspendInternal();
      }
    });
  }

  private void suspendInternal() {
    checkMainLooperThread();
    suspended = true;

    // When Cobalt is suspended it destroys any active SbPlayer instances.
    // However, the HTML5 app may still indicate that it's playing.
    //
    // In general, when the HTML5 app says it's playing and Cobalt has
    // no active media, we may be in between videos so we want MediaSession
    // to stay active. In the onStop() case, though, we do not.
    // So we set the set the MediaSession to inactive in this case.
    mediaSession.setActive(false);
    if (mediaPlaying) {
      onMediaStop();
    }
  }

  private static native void nativeInvokeAction(long action);

  public void updateMediaSession(final int playbackState, final long actions,
      final String title, final String artist, final String album,
      final MediaImage[] artwork) {
    mainHandler.post(new Runnable() {
      @Override
      public void run() {
        updateMediaSessionInternal(playbackState, actions, title, artist, album, artwork);
      }
    });
  }

  /** Called on main looper thread when media session changes. */
  private void updateMediaSessionInternal(int playbackState, long actions,
      String title, String artist, String album, MediaImage[] artwork) {
    checkMainLooperThread();
    boolean nowActive;
    boolean nowMediaPlaying;
    int androidPlaybackState;

    switch (playbackState) {
      case PLAYBACK_STATE_PLAYING:
        nowActive = true;
        nowMediaPlaying = true;
        androidPlaybackState = PlaybackState.STATE_PLAYING;
        break;
      case PLAYBACK_STATE_PAUSED:
        nowActive = true;
        nowMediaPlaying = false;
        androidPlaybackState = PlaybackState.STATE_PAUSED;
        break;
      case PLAYBACK_STATE_NONE:
        nowActive = false;
        nowMediaPlaying = false;
        androidPlaybackState = PlaybackState.STATE_NONE;
        break;
      default:
        throw new RuntimeException("Unrecognized playbackState");
    }

    if (nowMediaPlaying && !mediaPlaying) {
      onMediaStart();
    } else if (!nowMediaPlaying && mediaPlaying) {
      onMediaStop();
    }

    active = nowActive;
    mediaPlaying = nowMediaPlaying;

    // The Android MediaSession becomes inactive when JavaScript says so,
    // but don't allow JavaScript to enable it while suspended.
    if (!active || suspended) {
      mediaSession.setActive(false);
      return;
    }

    mediaSession.setActive(true);

    mediaSession.setPlaybackState(
        new PlaybackState.Builder()
            .setActions(actions)
            .setState(
                androidPlaybackState,
                PlaybackState.PLAYBACK_POSITION_UNKNOWN,
                playbackState == PLAYBACK_STATE_PLAYING ? 1.0f : 0.0f)
            .build());

    metadataBuilder
        .putString(MediaMetadata.METADATA_KEY_TITLE, title)
        .putString(MediaMetadata.METADATA_KEY_ARTIST, artist)
        .putString(MediaMetadata.METADATA_KEY_ALBUM, album)
        .putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, artworkLoader.getOrLoadArtwork(artwork));
    mediaSession.setMetadata(metadataBuilder.build());
  }

  @Override
  public void onArtworkLoaded(Bitmap bitmap) {
    metadataBuilder.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, bitmap);
    mediaSession.setMetadata(metadataBuilder.build());
  }

}
