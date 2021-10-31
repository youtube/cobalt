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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.media.AudioAttributes;
import android.media.AudioAttributes.Builder;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.view.WindowManager;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Holder;
import dev.cobalt.util.Log;

/**
 * Cobalt MediaSession glue, as well as collection of state and logic to switch on/off Android OS
 * features used in media playback, such as audio focus, "KEEP_SCREEN_ON" mode, and "visible
 * behind".
 */
public class CobaltMediaSession
    implements AudioManager.OnAudioFocusChangeListener, ArtworkLoader.Callback {

  // We do handle transport controls and set this flag on all API levels, even though it's
  // deprecated and unnecessary on API 26+.
  @SuppressWarnings("deprecation")
  private static final int MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS =
      MediaSessionCompat.FLAG_HANDLES_TRANSPORT_CONTROLS;

  private AudioFocusRequest audioFocusRequest;

  interface UpdateVolumeListener {
    /** Called when there is a change in audio focus. */
    void onUpdateVolume(float gain);
  }

  /**
   * When losing audio focus with the option of ducking, we reduce the volume to 10%. This arbitrary
   * number is what YouTube Android Player infrastructure uses.
   */
  private static final float AUDIO_FOCUS_DUCK_LEVEL = 0.1f;

  private final Handler mainHandler = new Handler(Looper.getMainLooper());

  private final Context context;
  private final Holder<Activity> activityHolder;

  private final UpdateVolumeListener volumeListener;
  private final ArtworkLoader artworkLoader;
  private MediaSessionCompat mediaSession;

  // We re-use the builder to hold onto the most recent playback state.
  private PlaybackStateCompat.Builder playbackStateBuilder = new PlaybackStateCompat.Builder();

  // Duplicated in starboard/android/shared/android_media_session_client.h
  // PlaybackStateCompat
  private static final int PLAYBACK_STATE_PLAYING = 0;
  private static final int PLAYBACK_STATE_PAUSED = 1;
  private static final int PLAYBACK_STATE_NONE = 2;
  private static final String[] PLAYBACK_STATE_NAME = {"playing", "paused", "none"};

  // Accessed on the main looper thread only.
  private int currentPlaybackState = PLAYBACK_STATE_NONE;
  private boolean transientPause = false;
  private boolean suspended = true;
  private boolean explicitUserActionRequired = false;

  /** LifecycleCallback to notify listeners when |mediaSession| becomes active or inactive. */
  public interface LifecycleCallback {
    void onMediaSessionLifecycle(boolean isActive, MediaSessionCompat.Token token);
  }

  private LifecycleCallback lifecycleCallback = null;

  /** We use the MediaMetadata to hold onto the most recent metadata and add artwork later. */
  public class MediaMetadata {
    public String title = "";
    public String artist = "";
    public String album = "";
    public Bitmap artwork = null;
    public long duration = (long) 0.0;

    public void SetMetadata(
        String title, String artist, String album, Bitmap artwork, long duration) {
      this.title = title;
      this.artist = artist;
      this.album = album;
      this.artwork = artwork;
      this.duration = duration;
    }
  }

  private MediaMetadata metadata = new MediaMetadata();

  public CobaltMediaSession(
      Context context, Holder<Activity> activityHolder, UpdateVolumeListener volumeListener) {
    this.context = context;
    this.activityHolder = activityHolder;

    this.volumeListener = volumeListener;
    artworkLoader = new ArtworkLoader(this);
    setMediaSession();
  }

  public void setLifecycleCallback(LifecycleCallback lifecycleCallback) {
    this.lifecycleCallback = lifecycleCallback;
    if (lifecycleCallback != null && this.mediaSession != null) {
      lifecycleCallback.onMediaSessionLifecycle(
          this.mediaSession.isActive(), this.mediaSession.getSessionToken());
    }
  }

  private void setMediaSession() {
    Log.i(TAG, "MediaSession new");
    if (mediaSession == null) {
      mediaSession = new MediaSessionCompat(context, TAG);
      mediaSession.setFlags(MEDIA_SESSION_FLAG_HANDLES_TRANSPORT_CONTROLS);
      mediaSession.setCallback(
          new MediaSessionCompat.Callback() {
            @Override
            public void onFastForward() {
              Log.i(TAG, "MediaSession action: FAST FORWARD");
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_FAST_FORWARD);
            }

            @Override
            public void onPause() {
              Log.i(TAG, "MediaSession action: PAUSE");
              nativeInvokeAction(PlaybackStateCompat.ACTION_PAUSE);
            }

            @Override
            public void onPlay() {
              Log.i(TAG, "MediaSession action: PLAY");
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_PLAY);
            }

            @Override
            public void onRewind() {
              Log.i(TAG, "MediaSession action: REWIND");
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_REWIND);
            }

            @Override
            public void onSkipToNext() {
              Log.i(TAG, "MediaSession action: SKIP NEXT");
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_SKIP_TO_NEXT);
            }

            @Override
            public void onSkipToPrevious() {
              Log.i(TAG, "MediaSession action: SKIP PREVIOUS");
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS);
            }

            @Override
            public void onSeekTo(long pos) {
              Log.i(TAG, "MediaSession action: SEEK " + pos);
              explicitUserActionRequired = false;
              nativeInvokeAction(PlaybackStateCompat.ACTION_SEEK_TO, pos);
            }

            @Override
            public void onStop() {
              Log.i(TAG, "MediaSession action: STOP");
              nativeInvokeAction(PlaybackStateCompat.ACTION_STOP);
            }
          });
    }
    // |metadataBuilder| may still have no fields at this point, yielding empty metadata.
    MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();
    mediaSession.setMetadata(metadataBuilder.build());
    // |playbackStateBuilder| may still have no fields at this point.
    mediaSession.setPlaybackState(playbackStateBuilder.build());
  }

  private static void checkMainLooperThread() {
    if (Looper.getMainLooper() != Looper.myLooper()) {
      throw new RuntimeException("Must be on main thread");
    }
  }

  /**
   * Sets system media resources active or not according to whether media is playing. The concept of
   * "media focus" encapsulates wake lock, audio focus and active media session so that all three
   * are set together to stay coherent as playback state changes. This is idempotent as it may be
   * called multiple times during the course of a media session.
   */
  private void configureMediaFocus(int playbackState) {
    checkMainLooperThread();
    if (transientPause && playbackState == PLAYBACK_STATE_PAUSED) {
      Log.i(TAG, "Media focus: paused (transient)");
      // Don't release media focus while transiently paused, otherwise we won't get audiofocus back
      // when the transient condition ends and we would leave playback paused.
      return;
    }
    Log.i(TAG, "Media focus: " + PLAYBACK_STATE_NAME[playbackState]);
    wakeLock(playbackState == PLAYBACK_STATE_PLAYING);
    audioFocus(playbackState == PLAYBACK_STATE_PLAYING);

    boolean activating = true;
    boolean deactivating = false;
    if (mediaSession != null) {
      activating = playbackState != PLAYBACK_STATE_NONE && !mediaSession.isActive();
      deactivating = playbackState == PLAYBACK_STATE_NONE && mediaSession.isActive();
    }
    if (activating) {
      // Resuming or new playbacks land here.
      setMediaSession();
    }
    mediaSession.setActive(playbackState != PLAYBACK_STATE_NONE);
    if (lifecycleCallback != null) {
      lifecycleCallback.onMediaSessionLifecycle(
          this.mediaSession.isActive(), this.mediaSession.getSessionToken());
    }
    if (deactivating) {
      // Suspending lands here.
      Log.i(TAG, "MediaSession release");
      mediaSession.release();
      mediaSession = null;
    }
  }

  private void wakeLock(boolean lock) {
    Activity activity = activityHolder.get();
    if (activity == null) {
      return;
    }
    if (lock) {
      activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    } else {
      activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
  }

  private void audioFocus(boolean focus) {
    if (focus) {
      int res;
      if (Build.VERSION.SDK_INT < 26) {
        res = requestAudioFocus();
      } else {
        res = requestAudioFocusV26();
      }
      // This shouldn't happen, but pause playback to be nice if it does.
      if (res != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
        Log.w(TAG, "Audiofocus action: PAUSE (not granted)");
        nativeInvokeAction(PlaybackStateCompat.ACTION_PAUSE);
      }
    } else {
      if (Build.VERSION.SDK_INT < 26) {
        abandonAudioFocus();
      } else {
        abandonAudioFocusV26();
      }
    }
  }

  @SuppressWarnings("deprecation")
  private int requestAudioFocus() {
    return getAudioManager()
        .requestAudioFocus(this, AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN);
  }

  @RequiresApi(26)
  private int requestAudioFocusV26() {
    if (audioFocusRequest == null) {
      AudioAttributes audioAttributes =
          new Builder().setContentType(AudioAttributes.CONTENT_TYPE_MOVIE).build();
      audioFocusRequest =
          new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
              .setOnAudioFocusChangeListener(this)
              .setAudioAttributes(audioAttributes)
              .build();
    }
    return getAudioManager().requestAudioFocus(audioFocusRequest);
  }

  @SuppressWarnings("deprecation")
  private void abandonAudioFocus() {
    getAudioManager().abandonAudioFocus(this);
  }

  @RequiresApi(26)
  private void abandonAudioFocusV26() {
    if (audioFocusRequest != null) {
      getAudioManager().abandonAudioFocusRequest(audioFocusRequest);
    }
  }

  /** AudioManager.OnAudioFocusChangeListener implementation. */
  @Override
  public void onAudioFocusChange(int focusChange) {
    String logExtra = "";
    switch (focusChange) {
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
        logExtra = " (transient)";
        // fall through
      case AudioManager.AUDIOFOCUS_LOSS:
        Log.i(TAG, "Audiofocus loss" + logExtra);
        if (currentPlaybackState == PLAYBACK_STATE_PLAYING) {
          Log.i(TAG, "Audiofocus action: PAUSE");
          nativeInvokeAction(PlaybackStateCompat.ACTION_PAUSE);
        }
        break;
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
        Log.i(TAG, "Audiofocus duck");
        // Lower the volume, keep current play state.
        // Starting with API 26 the system does automatic ducking without calling our listener,
        // but we still need this for API < 26.
        volumeListener.onUpdateVolume(AUDIO_FOCUS_DUCK_LEVEL);
        break;
      case AudioManager.AUDIOFOCUS_GAIN:
        Log.i(TAG, "Audiofocus gain");
        // The app has been granted audio focus (again). Raise volume to normal,
        // restart playback if necessary.
        volumeListener.onUpdateVolume(1.0f);
        if (transientPause && currentPlaybackState == PLAYBACK_STATE_PAUSED) {
          Log.i(TAG, "Audiofocus action: PLAY");
          nativeInvokeAction(PlaybackStateCompat.ACTION_PLAY);
        }
        break;
      default: // fall out
    }

    // Keep track of whether we're currently paused because of a transient loss of audiofocus.
    transientPause = (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT);
    // To restart playback after permanent loss, the user must take an explicit action.
    // See: https://developer.android.com/guide/topics/media-apps/audio-focus
    explicitUserActionRequired = (focusChange == AudioManager.AUDIOFOCUS_LOSS);
  }

  private AudioManager getAudioManager() {
    return (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
  }

  public void resume() {
    mainHandler.post(
        new Runnable() {
          @Override
          public void run() {
            resumeInternal();
          }
        });
  }

  private void resumeInternal() {
    checkMainLooperThread();
    suspended = false;
    // Undoing what may have been done in suspendInternal().
    configureMediaFocus(currentPlaybackState);
  }

  public void suspend() {
    if (Looper.getMainLooper() == Looper.myLooper()) {
      suspendInternal();
    } else {
      mainHandler.post(
          new Runnable() {
            @Override
            public void run() {
              suspendInternal();
            }
          });
    }
  }

  private void suspendInternal() {
    checkMainLooperThread();
    suspended = true;

    // We generally believe the HTML5 app playback state as the source of truth for configuring
    // media focus since only it can know about a momentary pause between videos in a playlist, or
    // other autoplay scenario when we should keep media focus. However, when suspending, any
    // active SbPlayer is destroyed and we release media focus, even if the HTML5 app still thinks
    // it's in a playing state. We'll configure it again in resumeInternal() and the HTML5 app will
    // be none the wiser.
    playbackStateBuilder.setState(
        currentPlaybackState,
        PlaybackStateCompat.PLAYBACK_POSITION_UNKNOWN,
        currentPlaybackState == PLAYBACK_STATE_PLAYING ? 1.0f : 0.0f);
    configureMediaFocus(PLAYBACK_STATE_NONE);
  }

  private static void nativeInvokeAction(long action) {
    nativeInvokeAction(action, 0);
  }

  private static native void nativeInvokeAction(long action, long seekMs);

  public void updateMediaSession(
      final int playbackState,
      final long actions,
      final long positionMs,
      final float speed,
      final String title,
      final String artist,
      final String album,
      final MediaImage[] artwork,
      final long duration) {
    mainHandler.post(
        new Runnable() {
          @Override
          public void run() {
            updateMediaSessionInternal(
                playbackState, actions, positionMs, speed, title, artist, album, artwork, duration);
          }
        });
  }

  /** Called on main looper thread when media session changes. */
  private void updateMediaSessionInternal(
      int playbackState,
      long actions,
      long positionMs,
      float speed,
      String title,
      String artist,
      String album,
      MediaImage[] artwork,
      final long duration) {
    checkMainLooperThread();

    boolean hasStateChange = this.currentPlaybackState != playbackState;
    // Always keep track of what the HTML5 app thinks the playback state is so we can configure the
    // media focus correctly, either immediately or when resuming from being suspended.
    this.currentPlaybackState = playbackState;

    // Don't update anything while suspended.
    if (suspended) {
      Log.i(TAG, "Playback state change while suspended: " + PLAYBACK_STATE_NAME[playbackState]);
      return;
    }

    if (hasStateChange) {
      if (playbackState == PLAYBACK_STATE_PLAYING) {
        // We don't want to request media focus if |explicitUserActionRequired| is true when we
        // don't have window focus. Ideally, we should recognize user action to re-request audio
        // focus if |explicitUserActionRequired| is true. Currently we're not able to recognize
        // it. But if we don't have window focus, we know the user is not interacting with our app
        // and we should not request media focus.
        if (!explicitUserActionRequired
            || (activityHolder.get() != null && activityHolder.get().hasWindowFocus())) {
          explicitUserActionRequired = false;
          configureMediaFocus(playbackState);
        } else {
          Log.w(TAG, "Audiofocus action: PAUSE (explicit user action required)");
          nativeInvokeAction(PlaybackStateCompat.ACTION_PAUSE);
        }
      } else {
        // It's fine to abandon media focus anytime.
        configureMediaFocus(playbackState);
      }
    }

    // Ignore updates to the MediaSession metadata if playback is stopped.
    if (playbackState == PLAYBACK_STATE_NONE || mediaSession == null) {
      return;
    }

    int androidPlaybackState;
    String stateName;
    switch (playbackState) {
      case PLAYBACK_STATE_PLAYING:
        androidPlaybackState = PlaybackStateCompat.STATE_PLAYING;
        stateName = "PLAYING";
        break;
      case PLAYBACK_STATE_PAUSED:
        androidPlaybackState = PlaybackStateCompat.STATE_PAUSED;
        stateName = "PAUSED";
        break;
      case PLAYBACK_STATE_NONE:
      default:
        androidPlaybackState = PlaybackStateCompat.STATE_NONE;
        stateName = "NONE";
        break;
    }

    Log.i(
        TAG,
        String.format(
            "MediaSession state: %s, position: %d ms, speed: %f x, duration: %d ms",
            stateName, positionMs, speed, duration));

    playbackStateBuilder =
        new PlaybackStateCompat.Builder()
            .setActions(actions)
            .setState(androidPlaybackState, positionMs, speed);
    mediaSession.setPlaybackState(playbackStateBuilder.build());

    // Let metadata hold onto the most recent metadata and add artwork later.
    metadata.SetMetadata(title, artist, album, artworkLoader.getOrLoadArtwork(artwork), duration);

    // Update the metadata as soon as we can - even before artwork is loaded.
    updateMetadata(false);
  }

  private void updateMetadata(boolean resetMetadataWithEmptyBuilder) {
    if (mediaSession == null) {
      return;
    }

    MediaMetadataCompat.Builder metadataBuilder = new MediaMetadataCompat.Builder();
    // Reset the metadata to make sure the artwork update correctly.
    if (resetMetadataWithEmptyBuilder) mediaSession.setMetadata(metadataBuilder.build());

    metadataBuilder
        .putString(MediaMetadataCompat.METADATA_KEY_TITLE, metadata.title)
        .putString(MediaMetadataCompat.METADATA_KEY_ARTIST, metadata.artist)
        .putString(MediaMetadataCompat.METADATA_KEY_ALBUM, metadata.album)
        .putBitmap(MediaMetadataCompat.METADATA_KEY_ART, metadata.artwork)
        .putLong(MediaMetadataCompat.METADATA_KEY_DURATION, metadata.duration);

    // Set mediaSession's metadata to update the "Now Playing Card".
    mediaSession.setMetadata(metadataBuilder.build());
  }

  @Override
  public void onArtworkLoaded(Bitmap bitmap) {
    metadata.artwork = bitmap;
    // Update artwork when it is ready to use.
    updateMetadata(true);
  }
}
