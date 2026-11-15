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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.os.Looper;
import android.os.Parcel;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.MediaSessionObserver;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.services.media_session.MediaPosition;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

/**
 * Unit tests for {@link CobaltMediaSession} lifecycle callback threading.
 *
 * <p>Lifetime: Instantiated per test method by RobolectricTestRunner.
 *
 * <p>Threading: Affine to the Robolectric main thread except where tests explicitly spawn a
 * background platform service thread.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CobaltMediaSessionTest {
  private Context mContext;
  private ArtworkDownloader mMockDownloader;
  private MediaSessionCompat mMockMediaSession;
  private MediaSessionCompat.Token mTestToken;

  @Before
  public void setUp() {
    mContext = RuntimeEnvironment.getApplication();
    mMockDownloader = mock(ArtworkDownloader.class);
    mMockMediaSession = mock(MediaSessionCompat.class);
    Parcel parcel = Parcel.obtain();
    parcel.writeParcelable(null, 0);
    parcel.setDataPosition(0);
    mTestToken = MediaSessionCompat.Token.CREATOR.createFromParcel(parcel);
    parcel.recycle();
    when(mMockMediaSession.getSessionToken()).thenReturn(mTestToken);
  }

  private CobaltMediaSession createMediaSession(boolean platformServicesOffUiThreadEnabled) {
    return createMediaSession(
        platformServicesOffUiThreadEnabled, /* mediaSessionOptimizationEnabled= */ false);
  }

  private CobaltMediaSession createMediaSession(
      boolean platformServicesOffUiThreadEnabled, boolean mediaSessionOptimizationEnabled) {
    CobaltMediaSession session =
        new CobaltMediaSession(mContext, mMockDownloader) {
          @Override
          protected boolean isPlatformServicesOffUiThreadEnabled() {
            return platformServicesOffUiThreadEnabled;
          }

          @Override
          protected boolean isMediaSessionOptimizationEnabled() {
            return mediaSessionOptimizationEnabled;
          }
        };
    session.setMediaSessionForTesting(mMockMediaSession);
    return session;
  }

  @Test
  public void setLifecycleCallback_onMainThread_invokesImmediatelyWhenActive() {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ true);
    AtomicBoolean invoked = new AtomicBoolean(false);
    AtomicReference<Looper> callbackLooper = new AtomicReference<>();
    AtomicReference<MediaSessionCompat.Token> receivedToken = new AtomicReference<>();

    cobaltMediaSession.setLifecycleCallback(
        (isActive, token) -> {
          invoked.set(isActive);
          callbackLooper.set(Looper.myLooper());
          receivedToken.set(token);
        });

    assertThat(invoked.get()).isTrue();
    assertThat(callbackLooper.get()).isEqualTo(Looper.getMainLooper());
    assertThat(receivedToken.get()).isSameInstanceAs(mTestToken);
  }

  @Test
  public void setLifecycleCallback_fromBackgroundThreadWithFlagEnabled_marshalsToMainLooper()
      throws Exception {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ true);
    ShadowLooper.pauseMainLooper();
    AtomicBoolean invoked = new AtomicBoolean(false);
    AtomicReference<Looper> callbackLooper = new AtomicReference<>();
    AtomicReference<MediaSessionCompat.Token> receivedToken = new AtomicReference<>();

    Thread serviceThread =
        new Thread(
            () ->
                cobaltMediaSession.setLifecycleCallback(
                    (isActive, token) -> {
                      invoked.set(isActive);
                      callbackLooper.set(Looper.myLooper());
                      receivedToken.set(token);
                    }),
            "CobaltPlatformService");
    serviceThread.start();
    serviceThread.join();

    // Must not execute synchronously on the background thread while main looper is paused.
    assertThat(invoked.get()).isFalse();

    ShadowLooper.idleMainLooper();

    assertThat(invoked.get()).isTrue();
    assertThat(callbackLooper.get()).isEqualTo(Looper.getMainLooper());
    assertThat(receivedToken.get()).isSameInstanceAs(mTestToken);
  }

  @Test
  public void setLifecycleCallback_fromBackgroundThreadWithFlagDisabled_invokesImmediately()
      throws Exception {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ false);
    ShadowLooper.pauseMainLooper();
    AtomicBoolean invoked = new AtomicBoolean(false);
    AtomicReference<Thread> callbackThread = new AtomicReference<>();
    AtomicReference<MediaSessionCompat.Token> receivedToken = new AtomicReference<>();

    Thread serviceThread =
        new Thread(
            () ->
                cobaltMediaSession.setLifecycleCallback(
                    (isActive, token) -> {
                      invoked.set(isActive);
                      callbackThread.set(Thread.currentThread());
                      receivedToken.set(token);
                    }),
            "CobaltPlatformService");
    serviceThread.start();
    serviceThread.join();

    // With CobaltPlatformServicesOffUiThread disabled, no post to main handler occurs.
    assertThat(invoked.get()).isTrue();
    assertThat(callbackThread.get()).isSameInstanceAs(serviceThread);
    assertThat(receivedToken.get()).isSameInstanceAs(mTestToken);
  }

  private static void runOnBackgroundThread(Runnable runnable) throws Exception {
    Thread serviceThread = new Thread(runnable, "CobaltPlatformService");
    serviceThread.start();
    serviceThread.join();
  }

  @Test
  public void setLifecycleCallback_backgroundSetThenMainThreadClear_staleUpdateIsDropped()
      throws Exception {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ true);
    ShadowLooper.pauseMainLooper();
    AtomicInteger staleInvocations = new AtomicInteger();
    CobaltMediaSession.LifecycleCallback staleCallback =
        (isActive, token) -> staleInvocations.incrementAndGet();

    // Background set is posted to the (paused) main looper...
    runOnBackgroundThread(() -> cobaltMediaSession.setLifecycleCallback(staleCallback));
    // ...then a newer main-thread clear is applied inline before the post runs.
    cobaltMediaSession.setLifecycleCallback(null);
    assertThat(cobaltMediaSession.getLifecycleCallbackForTesting()).isNull();

    ShadowLooper.idleMainLooper();

    // The stale background update must not resurrect the old callback.
    assertThat(cobaltMediaSession.getLifecycleCallbackForTesting()).isNull();
    assertThat(staleInvocations.get()).isEqualTo(0);
  }

  @Test
  public void setLifecycleCallback_mainThreadSetThenBackgroundSet_laterCallWins() throws Exception {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ true);
    ShadowLooper.pauseMainLooper();
    AtomicInteger firstInvocations = new AtomicInteger();
    AtomicInteger secondInvocations = new AtomicInteger();
    CobaltMediaSession.LifecycleCallback firstCallback =
        (isActive, token) -> firstInvocations.incrementAndGet();
    CobaltMediaSession.LifecycleCallback secondCallback =
        (isActive, token) -> secondInvocations.incrementAndGet();

    cobaltMediaSession.setLifecycleCallback(firstCallback);
    assertThat(firstInvocations.get()).isEqualTo(1);

    runOnBackgroundThread(() -> cobaltMediaSession.setLifecycleCallback(secondCallback));
    assertThat(cobaltMediaSession.getLifecycleCallbackForTesting()).isSameInstanceAs(firstCallback);

    ShadowLooper.idleMainLooper();

    assertThat(cobaltMediaSession.getLifecycleCallbackForTesting())
        .isSameInstanceAs(secondCallback);
    assertThat(secondInvocations.get()).isEqualTo(1);
    assertThat(firstInvocations.get()).isEqualTo(1);
  }

  @Test
  public void setLifecycleCallback_twoBackgroundSets_onlyLatestIsApplied() throws Exception {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(/* platformServicesOffUiThreadEnabled= */ true);
    ShadowLooper.pauseMainLooper();
    AtomicInteger firstInvocations = new AtomicInteger();
    AtomicInteger secondInvocations = new AtomicInteger();
    CobaltMediaSession.LifecycleCallback firstCallback =
        (isActive, token) -> firstInvocations.incrementAndGet();
    CobaltMediaSession.LifecycleCallback secondCallback =
        (isActive, token) -> secondInvocations.incrementAndGet();

    runOnBackgroundThread(() -> cobaltMediaSession.setLifecycleCallback(firstCallback));
    runOnBackgroundThread(() -> cobaltMediaSession.setLifecycleCallback(secondCallback));

    ShadowLooper.idleMainLooper();

    assertThat(cobaltMediaSession.getLifecycleCallbackForTesting())
        .isSameInstanceAs(secondCallback);
    assertThat(firstInvocations.get()).isEqualTo(0);
    assertThat(secondInvocations.get()).isEqualTo(1);
  }

  @Test
  public void mediaSessionPositionChanged_withOptimization_skipsSetMetadataWhenDurationUnchanged() {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(
            /* platformServicesOffUiThreadEnabled= */ false,
            /* mediaSessionOptimizationEnabled= */ true);
    MediaSessionObserver observer =
        cobaltMediaSession.createMediaSessionObserver(mock(MediaSession.class));

    observer.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    observer.mediaSessionMetadataChanged(new MediaMetadata("Video 1", "Channel", "Album"));
    verify(mMockMediaSession, times(1)).setMetadata(any());

    // First position update sets initial duration (Long.MIN_VALUE -> 120_000L): calls setMetadata.
    observer.mediaSessionPositionChanged(new MediaPosition(120_000L, 1_000L, 1.0f, 100L));
    verify(mMockMediaSession, times(2)).setMetadata(any());
    verify(mMockMediaSession, times(2)).setPlaybackState(any());

    // Subsequent position ticks with unchanged duration skip setMetadata but still update state.
    observer.mediaSessionPositionChanged(new MediaPosition(120_000L, 2_000L, 1.0f, 200L));
    observer.mediaSessionPositionChanged(new MediaPosition(120_000L, 3_000L, 1.0f, 300L));
    verify(mMockMediaSession, times(2)).setMetadata(any());
    verify(mMockMediaSession, times(4)).setPlaybackState(any());

    // Duration change triggers setMetadata again.
    observer.mediaSessionPositionChanged(new MediaPosition(180_000L, 0L, 1.0f, 400L));
    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(3)).setMetadata(captor.capture());
    assertThat(captor.getValue().getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(180_000L);

    // Explicit mediaSessionMetadataChanged still calls setMetadata even when duration is unchanged.
    observer.mediaSessionMetadataChanged(new MediaMetadata("Video 2", "Channel", "Album"));
    verify(mMockMediaSession, times(4)).setMetadata(captor.capture());
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video 2");
  }

  @Test
  public void mediaSessionPositionChanged_withoutOptimization_callsSetMetadataEveryTick() {
    CobaltMediaSession cobaltMediaSession =
        createMediaSession(
            /* platformServicesOffUiThreadEnabled= */ false,
            /* mediaSessionOptimizationEnabled= */ false);
    MediaSessionObserver observer =
        cobaltMediaSession.createMediaSessionObserver(mock(MediaSession.class));

    observer.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    observer.mediaSessionMetadataChanged(new MediaMetadata("Video 1", "Channel", "Album"));
    observer.mediaSessionPositionChanged(new MediaPosition(120_000L, 1_000L, 1.0f, 100L));
    observer.mediaSessionPositionChanged(new MediaPosition(120_000L, 2_000L, 1.0f, 200L));

    verify(mMockMediaSession, times(3)).setMetadata(any());
  }

  @Test
  public void mediaSessionDestroyed_withOptimization_clearsCachedArtwork() {
    CobaltMediaSession optimizedSession =
        createMediaSession(
            /* platformServicesOffUiThreadEnabled= */ false,
            /* mediaSessionOptimizationEnabled= */ true);
    ArtworkLoader loader = optimizedSession.getArtworkLoaderForTesting();
    loader.mRequestedArtworkUrl = "https://i.ytimg.com/pending.jpg";
    loader.mCurrentArtworkUrl = "https://i.ytimg.com/current.jpg";
    loader.mCurrentArtwork = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888);

    MediaSessionObserver observer =
        optimizedSession.createMediaSessionObserver(mock(MediaSession.class));
    observer.mediaSessionDestroyed();

    assertThat(loader.mRequestedArtworkUrl).isEmpty();
    assertThat(loader.mCurrentArtworkUrl).isEmpty();
    assertThat(loader.mCurrentArtwork).isNull();
  }

  @Test
  public void mediaSessionDestroyed_withoutOptimization_retainsCachedArtwork() {
    CobaltMediaSession unoptimizedSession =
        createMediaSession(
            /* platformServicesOffUiThreadEnabled= */ false,
            /* mediaSessionOptimizationEnabled= */ false);
    ArtworkLoader loader = unoptimizedSession.getArtworkLoaderForTesting();
    Bitmap cachedBitmap = Bitmap.createBitmap(64, 36, Bitmap.Config.ARGB_8888);
    loader.mCurrentArtworkUrl = "https://i.ytimg.com/current.jpg";
    loader.mCurrentArtwork = cachedBitmap;

    MediaSessionObserver observer =
        unoptimizedSession.createMediaSessionObserver(mock(MediaSession.class));
    observer.mediaSessionDestroyed();

    assertThat(loader.mCurrentArtworkUrl).isEqualTo("https://i.ytimg.com/current.jpg");
    assertThat(loader.mCurrentArtwork).isSameInstanceAs(cachedBitmap);
  }
}
