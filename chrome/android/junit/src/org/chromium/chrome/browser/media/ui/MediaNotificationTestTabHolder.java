// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.media.MediaSessionHelper;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.services.media_session.MediaMetadata;
import org.chromium.url.GURL;

import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Utility class for holding a Tab and relevant objects for media notification tests.
 */
@SuppressWarnings("DoNotMock") // Mocks GURL
public class MediaNotificationTestTabHolder {
    @Mock
    UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    GURLUtils.Natives mGURLUtilsJniMock;
    @Mock
    WebContents mWebContents;
    @Mock
    MediaSession mMediaSession;
    @Mock
    Tab mTab;

    String mTitle;
    String mUrl;

    MediaSessionTabHelper mMediaSessionTabHelper;

    // Mock LargeIconBridge that always returns false.
    private class TestLargeIconBridge extends LargeIconBridge {
        @Override
        public boolean getLargeIconForStringUrl(
                final String pageUrl, int desiredSizePx, final LargeIconCallback callback) {
            return false;
        }
    }

    public MediaNotificationTestTabHolder(int tabId, String url, String title, JniMocker mocker) {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        // We don't want this matcher to match the current value of mUrl. Wrapping it in a matcher
        // allows us to match on the updated value of mUrl.
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
                     argThat(urlArg -> urlArg.equals(mUrl))))
                .thenAnswer(invocation -> mUrl);

        mocker.mock(GURLUtilsJni.TEST_HOOKS, mGURLUtilsJniMock);
        when(mGURLUtilsJniMock.getOrigin(argThat(urlArg -> urlArg.equals(mUrl))))
                .thenAnswer(invocation -> mUrl);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getId()).thenReturn(tabId);
        when(mWebContents.isIncognito()).thenReturn(false);

        MediaSessionHelper.sOverriddenMediaSession = mMediaSession;
        mMediaSessionTabHelper = new MediaSessionTabHelper(mTab);
        mMediaSessionTabHelper.mMediaSessionHelper.mLargeIconBridge = new TestLargeIconBridge();

        simulateNavigation(url, false);
        simulateTitleUpdated(title);

        // Default actions.
        simulateMediaSessionActionsChanged(
                Stream.of(MediaSessionAction.PLAY).collect(Collectors.toSet()));
    }

    public void simulateTitleUpdated(String title) {
        mTitle = title;
        mMediaSessionTabHelper.mMediaSessionHelper.mWebContentsObserver.titleWasSet(title);
    }

    public void simulateFaviconUpdated(Bitmap icon, GURL iconUrl) {
        mMediaSessionTabHelper.mTabObserver.onFaviconUpdated(mTab, icon, iconUrl);
    }

    public void simulateMediaSessionStateChanged(boolean isControllable, boolean isSuspended) {
        mMediaSessionTabHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionStateChanged(
                isControllable, isSuspended);
    }

    public void simulateMediaSessionMetadataChanged(MediaMetadata metadata) {
        mMediaSessionTabHelper.mMediaSessionHelper.mMediaSessionObserver
                .mediaSessionMetadataChanged(metadata);
    }

    public void simulateMediaSessionActionsChanged(Set<Integer> actions) {
        mMediaSessionTabHelper.mMediaSessionHelper.mMediaSessionObserver.mediaSessionActionsChanged(
                actions);
    }

    public void simulateNavigation(String url, boolean isSameDocument) {
        mUrl = url;

        // The following hoop jumping is necessary because loading real GURLs fails under junit.
        GURL gurl = mock(GURL.class);
        when(mWebContents.getVisibleUrl()).thenAnswer(invocation -> gurl);
        GURL gurlOrigin = mock(GURL.class);
        when(gurl.getOrigin()).thenAnswer(invocation -> gurlOrigin);
        when(gurlOrigin.getSpec()).thenAnswer(invocation -> url);

        NavigationHandle navigation = NavigationHandle.createForTesting(gurl,
                true /* isInPrimaryMainFrame */, isSameDocument, false /* isRendererInitiated */,
                0 /* pageTransition */, false /* hasUserGesture */, false /* isReload */);

        mMediaSessionTabHelper.mMediaSessionHelper.mWebContentsObserver
                .didStartNavigationInPrimaryMainFrame(navigation);

        navigation.didFinish(gurl, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, 0 /* errorCode */,
                200 /* httpStatusCode */, false /* isExternalProtocol */);
        mMediaSessionTabHelper.mMediaSessionHelper.mWebContentsObserver
                .didFinishNavigationInPrimaryMainFrame(navigation);
    }
}
