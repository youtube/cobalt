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
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import java.util.List;
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

/**
 * Unit tests for {@link CobaltMediaSession} metadata deduplication, artwork updates, and fallback
 * when {@link MediaSessionCompat#setMetadata} throws on artwork.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CobaltMediaSessionMetadataTest {
  private static final long VIDEO_DURATION_MS = 313801L;
  private static final long AD_DURATION_MS = 60081L;

  private Context mContext;
  private ArtworkDownloader mMockDownloader;
  private MediaSessionCompat mMockMediaSession;
  private MediaSessionCompat mNextMediaSession;
  private CobaltMediaSession mCobaltMediaSession;
  private MediaSessionObserver mObserver;

  @Before
  public void setUp() {
    mContext = RuntimeEnvironment.getApplication();
    mMockDownloader = mock(ArtworkDownloader.class);
    mMockMediaSession = mock(MediaSessionCompat.class);
    mCobaltMediaSession =
        new CobaltMediaSession(mContext, mMockDownloader) {
          @Override
          MediaSessionCompat createMediaSessionCompat() {
            return mNextMediaSession != null ? mNextMediaSession : mMockMediaSession;
          }
        };
    mCobaltMediaSession.setMediaSessionForTesting(mMockMediaSession);
    mObserver = mCobaltMediaSession.createMediaSessionObserver(mock(MediaSession.class));
    // Mark the session controllable so mediaSessionPositionChanged() does not deactivate it.
    mObserver.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    clearInvocations(mMockMediaSession);
  }

  private static MediaPosition positionWithDuration(long durationMs, long positionMs) {
    return new MediaPosition(
        durationMs, positionMs, /* playbackRate= */ 1.0f, /* lastUpdatedTime= */ 0L);
  }

  @Test
  public void updateMetadata_sameMetadataAndDurationSentTwice_callsSetMetadataOnce() {
    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    mObserver.mediaSessionMetadataChanged(metadata);
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 1000L));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    assertThat(captor.getAllValues().get(1).getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(VIDEO_DURATION_MS);
    clearInvocations(mMockMediaSession);

    // Position advances with the same duration and metadata; playback state updates, metadata
    // skips.
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 5000L));
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 10000L));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));

    verify(mMockMediaSession, never()).setMetadata(any());
    verify(mMockMediaSession, times(2)).setPlaybackState(any());
  }

  @Test
  public void updateMetadata_duplicateMetadataWithoutPosition_callsSetMetadataOnce() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(1)).setMetadata(captor.capture());
    MediaMetadataCompat sent = captor.getValue();
    assertThat(sent.getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isEqualTo("Video Title");
    assertThat(sent.getString(MediaMetadataCompat.METADATA_KEY_ARTIST)).isEqualTo("Channel");
    assertThat(sent.getString(MediaMetadataCompat.METADATA_KEY_ALBUM)).isEqualTo("Album");
    assertThat(sent.containsKey(MediaMetadataCompat.METADATA_KEY_DURATION)).isFalse();
  }

  @Test
  public void updateMetadata_titleChanged_resendsWithNewTitle() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("First Video", "Channel", "Album"));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Second Video", "Channel", "Album"));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("First Video");
    assertThat(sent.get(1).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Second Video");
  }

  @Test
  public void updateMetadata_artistOrAlbumChanged_resendsMetadata() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Title", "Artist A", "Album A"));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Title", "Artist B", "Album A"));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Title", "Artist B", "Album B"));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(3)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(1).getString(MediaMetadataCompat.METADATA_KEY_ARTIST))
        .isEqualTo("Artist B");
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_ALBUM)).isEqualTo("Album B");
  }

  @Test
  public void updateMetadata_adSameTitleDifferentDuration_resendsWithNewDuration() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", ""));
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 0L));
    // Ad starts with the same title strings but a short duration.
    mObserver.mediaSessionPositionChanged(positionWithDuration(AD_DURATION_MS, 0L));
    // Returning to the main video restores its duration.
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 1000L));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(4)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).containsKey(MediaMetadataCompat.METADATA_KEY_DURATION)).isFalse();
    assertThat(sent.get(1).getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(VIDEO_DURATION_MS);
    assertThat(sent.get(2).getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(AD_DURATION_MS);
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
    assertThat(sent.get(3).getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(VIDEO_DURATION_MS);
  }

  @Test
  public void updateMetadata_adWithEmptyTitleAndArtistAfterVideo_resendsEmptyMetadata() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("", "", ""));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    MediaMetadataCompat adMetadata = captor.getAllValues().get(1);
    assertThat(adMetadata.getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isEmpty();
    assertThat(adMetadata.getString(MediaMetadataCompat.METADATA_KEY_ARTIST)).isEmpty();
    assertThat(adMetadata.getString(MediaMetadataCompat.METADATA_KEY_ALBUM)).isEmpty();
  }

  @Test
  public void updateMetadata_nullTitleToEmptyTitle_resendsMetadata() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata(null, "", ""));
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("", "", ""));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isNull();
    assertThat(sent.get(1).getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isEmpty();
  }

  @Test
  public void updateMetadata_positionZeroVsNull_resendsMetadata() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", ""));
    mObserver.mediaSessionPositionChanged(positionWithDuration(0L, 0L));
    mObserver.mediaSessionPositionChanged(null);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(3)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).containsKey(MediaMetadataCompat.METADATA_KEY_DURATION)).isFalse();
    assertThat(sent.get(1).containsKey(MediaMetadataCompat.METADATA_KEY_DURATION)).isTrue();
    assertThat(sent.get(1).getLong(MediaMetadataCompat.METADATA_KEY_DURATION)).isEqualTo(0L);
    assertThat(sent.get(2).containsKey(MediaMetadataCompat.METADATA_KEY_DURATION)).isFalse();
  }

  @Test
  public void onArtworkLoaded_newBitmap_resetsAndResendsWithNewArtwork() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", ""));
    Bitmap firstBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    Bitmap secondBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    clearInvocations(mMockMediaSession);

    mCobaltMediaSession.onArtworkLoaded(firstBitmap);

    ArgumentCaptor<MediaMetadataCompat> firstCaptor =
        ArgumentCaptor.forClass(MediaMetadataCompat.class);
    // onArtworkLoaded resets metadata (empty), then calls updateMetadata() with the new bitmap.
    verify(mMockMediaSession, times(2)).setMetadata(firstCaptor.capture());
    assertThat(firstCaptor.getAllValues().get(0).size()).isEqualTo(0);
    MediaMetadataCompat firstSent = firstCaptor.getAllValues().get(1);
    assertThat(firstSent.getBitmap(MediaMetadataCompat.METADATA_KEY_ART))
        .isSameInstanceAs(firstBitmap);
    assertThat(firstSent.getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");

    clearInvocations(mMockMediaSession);
    // Same dimensions, different Bitmap object: must still reset and resend with secondBitmap.
    mCobaltMediaSession.onArtworkLoaded(secondBitmap);

    ArgumentCaptor<MediaMetadataCompat> secondCaptor =
        ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(secondCaptor.capture());
    assertThat(secondCaptor.getAllValues().get(0).size()).isEqualTo(0);
    assertThat(secondCaptor.getAllValues().get(1).getBitmap(MediaMetadataCompat.METADATA_KEY_ART))
        .isSameInstanceAs(secondBitmap);
  }

  @Test
  public void onArtworkLoaded_sameBitmapInstance_skipsExtraSetMetadata() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", ""));
    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mCobaltMediaSession.onArtworkLoaded(artwork);
    clearInvocations(mMockMediaSession);

    mCobaltMediaSession.onArtworkLoaded(artwork);

    verify(mMockMediaSession, never()).setMetadata(any());
  }

  @Test
  public void onArtworkLoaded_nullAfterArtwork_clearsArtwork() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", ""));
    clearInvocations(mMockMediaSession);

    // Passing null when mArtworkImage is already null returns early.
    mCobaltMediaSession.onArtworkLoaded(null);
    verify(mMockMediaSession, never()).setMetadata(any());

    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mCobaltMediaSession.onArtworkLoaded(artwork);
    clearInvocations(mMockMediaSession);

    mCobaltMediaSession.onArtworkLoaded(null);
    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    assertThat(captor.getAllValues().get(0).size()).isEqualTo(0);
    MediaMetadataCompat cleared = captor.getAllValues().get(1);
    assertThat(cleared.containsKey(MediaMetadataCompat.METADATA_KEY_ART)).isFalse();
    assertThat(cleared.getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isEqualTo("Video Title");
  }

  @Test
  public void updateMetadata_setMetadataThrowsWithArtwork_retriesWithoutArtwork() {
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 0L));
    clearInvocations(mMockMediaSession);

    doAnswer(
            invocation -> {
              MediaMetadataCompat metadata = invocation.getArgument(0);
              if (metadata.containsKey(MediaMetadataCompat.METADATA_KEY_ART)) {
                throw new RuntimeException("Could not copy bitmap to parcel blob.");
              }
              return null;
            })
        .when(mMockMediaSession)
        .setMetadata(any());

    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mCobaltMediaSession.onArtworkLoaded(artwork);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    // 1) resetMetaData() empty, 2) updateMetadata() with artwork (throws), 3) retry without
    // artwork.
    verify(mMockMediaSession, times(3)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).size()).isEqualTo(0);
    assertThat(sent.get(1).getBitmap(MediaMetadataCompat.METADATA_KEY_ART))
        .isSameInstanceAs(artwork);
    assertThat(sent.get(2).containsKey(MediaMetadataCompat.METADATA_KEY_ART)).isFalse();
    assertThat(sent.get(2).getBitmap(MediaMetadataCompat.METADATA_KEY_ART)).isNull();
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_ARTIST)).isEqualTo("Channel");
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_ALBUM)).isEqualTo("Album");
    assertThat(sent.get(2).getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(VIDEO_DURATION_MS);

    // Subsequent position updates with the same duration do not retry the failing bitmap.
    clearInvocations(mMockMediaSession);
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 2000L));
    verify(mMockMediaSession, never()).setMetadata(any());
  }

  @Test
  public void updateMetadata_setMetadataThrowsOnMetadataChangeWithArtwork_retriesWithoutArtwork() {
    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("First Video", "Channel", "Album"));
    mCobaltMediaSession.onArtworkLoaded(artwork);
    clearInvocations(mMockMediaSession);

    doAnswer(
            invocation -> {
              MediaMetadataCompat metadata = invocation.getArgument(0);
              if (metadata.containsKey(MediaMetadataCompat.METADATA_KEY_ART)) {
                throw new RuntimeException("Could not copy bitmap to parcel blob.");
              }
              return null;
            })
        .when(mMockMediaSession)
        .setMetadata(any());

    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Second Video", "Channel", "Album"));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).getBitmap(MediaMetadataCompat.METADATA_KEY_ART))
        .isSameInstanceAs(artwork);
    assertThat(sent.get(0).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Second Video");
    assertThat(sent.get(1).containsKey(MediaMetadataCompat.METADATA_KEY_ART)).isFalse();
    assertThat(sent.get(1).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Second Video");
  }

  @Test
  public void updateMetadata_setMetadataThrowsWithoutArtwork_propagatesAndAllowsRetry() {
    doThrow(new RuntimeException("Could not copy bitmap to parcel blob."))
        .when(mMockMediaSession)
        .setMetadata(any());

    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    RuntimeException thrown =
        assertThrows(RuntimeException.class, () -> mObserver.mediaSessionMetadataChanged(metadata));
    assertThat(thrown).hasMessageThat().isEqualTo("Could not copy bitmap to parcel blob.");
    verify(mMockMediaSession, times(1)).setMetadata(any());

    // Because setMetadata() threw, the key must not be cached; a retry sends metadata again.
    doNothing().when(mMockMediaSession).setMetadata(any());
    clearInvocations(mMockMediaSession);
    mObserver.mediaSessionMetadataChanged(metadata);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(1)).setMetadata(captor.capture());
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
  }

  @Test
  public void updateMetadata_retryWithoutArtworkAlsoThrows_propagates() {
    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mCobaltMediaSession.onArtworkLoaded(artwork);
    clearInvocations(mMockMediaSession);

    doThrow(new RuntimeException("Binder failed")).when(mMockMediaSession).setMetadata(any());

    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    RuntimeException thrown =
        assertThrows(RuntimeException.class, () -> mObserver.mediaSessionMetadataChanged(metadata));
    assertThat(thrown).hasMessageThat().isEqualTo("Binder failed");

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(2)).setMetadata(captor.capture());
    assertThat(captor.getAllValues().get(0).containsKey(MediaMetadataCompat.METADATA_KEY_ART))
        .isTrue();
    assertThat(captor.getAllValues().get(1).containsKey(MediaMetadataCompat.METADATA_KEY_ART))
        .isFalse();

    // Since neither setMetadata succeeded, the same metadata should be sent on a later retry.
    doNothing().when(mMockMediaSession).setMetadata(any());
    clearInvocations(mMockMediaSession);
    mObserver.mediaSessionMetadataChanged(metadata);
    verify(mMockMediaSession, times(1)).setMetadata(any());
  }

  @Test
  public void updateMetadata_afterResetViaNullMetadata_resendsSameMetadata() {
    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    mObserver.mediaSessionMetadataChanged(metadata);
    mObserver.mediaSessionMetadataChanged(null);
    mObserver.mediaSessionMetadataChanged(metadata);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(3)).setMetadata(captor.capture());
    List<MediaMetadataCompat> sent = captor.getAllValues();
    assertThat(sent.get(0).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
    assertThat(sent.get(1).size()).isEqualTo(0);
    assertThat(sent.get(2).getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
  }

  @Test
  public void updateMetadata_afterSessionReactivation_resendsSameMetadata() {
    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    mObserver.mediaSessionMetadataChanged(metadata);
    verify(mMockMediaSession, times(1)).setMetadata(any());

    // Deactivate the session, then reactivate it with a fresh MediaSessionCompat instance.
    mNextMediaSession = mock(MediaSessionCompat.class);
    mObserver.mediaSessionStateChanged(/* isControllable= */ false, /* isPaused= */ true);
    verify(mMockMediaSession).release();

    mObserver.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    mObserver.mediaSessionMetadataChanged(metadata);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mNextMediaSession, times(1)).setMetadata(captor.capture());
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_ARTIST))
        .isEqualTo("Channel");
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_ALBUM))
        .isEqualTo("Album");
  }

  @Test
  public void updateMetadata_afterActivityStopAndReactivation_positionUpdateResendsMetadata() {
    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 1000L));
    mCobaltMediaSession.onArtworkLoaded(artwork);

    mNextMediaSession = mock(MediaSessionCompat.class);
    mCobaltMediaSession.onActivityStop();
    verify(mMockMediaSession).release();

    // Reactivate with the existing metadata/artwork and send a position update with the same
    // duration.
    mObserver.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    mObserver.mediaSessionPositionChanged(positionWithDuration(VIDEO_DURATION_MS, 2000L));

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mNextMediaSession, times(1)).setMetadata(captor.capture());
    MediaMetadataCompat sent = captor.getValue();
    assertThat(sent.getString(MediaMetadataCompat.METADATA_KEY_TITLE)).isEqualTo("Video Title");
    assertThat(sent.getBitmap(MediaMetadataCompat.METADATA_KEY_ART)).isSameInstanceAs(artwork);
    assertThat(sent.getLong(MediaMetadataCompat.METADATA_KEY_DURATION))
        .isEqualTo(VIDEO_DURATION_MS);
  }

  @Test
  public void onArtworkLoaded_sameBitmapAfterReactivation_sendsMetadata() {
    Bitmap artwork = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    mObserver.mediaSessionMetadataChanged(new MediaMetadata("Video Title", "Channel", "Album"));
    mCobaltMediaSession.onArtworkLoaded(artwork);

    mNextMediaSession = mock(MediaSessionCompat.class);
    mCobaltMediaSession.onActivityStop();
    mObserver.mediaSessionStateChanged(/* isControllable= */ true, /* isPaused= */ false);
    // The artwork loader returns its cached bitmap, the one already held.
    mCobaltMediaSession.onArtworkLoaded(artwork);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mNextMediaSession, times(1)).setMetadata(captor.capture());
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
    assertThat(captor.getValue().getBitmap(MediaMetadataCompat.METADATA_KEY_ART))
        .isSameInstanceAs(artwork);
  }

  @Test
  public void updateMetadata_afterMediaSessionDestroyed_resendsSameMetadata() {
    MediaMetadata metadata = new MediaMetadata("Video Title", "Channel", "Album");
    mObserver.mediaSessionMetadataChanged(metadata);
    clearInvocations(mMockMediaSession);

    mObserver.mediaSessionDestroyed();
    mObserver.mediaSessionMetadataChanged(metadata);

    ArgumentCaptor<MediaMetadataCompat> captor = ArgumentCaptor.forClass(MediaMetadataCompat.class);
    verify(mMockMediaSession, times(1)).setMetadata(captor.capture());
    assertThat(captor.getValue().getString(MediaMetadataCompat.METADATA_KEY_TITLE))
        .isEqualTo("Video Title");
  }
}
