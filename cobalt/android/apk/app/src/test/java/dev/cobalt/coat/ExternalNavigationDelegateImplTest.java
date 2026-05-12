// Copyright 2026 The Cobalt Authors. All Rights Reserved.

package dev.cobalt.coat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for ExternalNavigationDelegateImpl. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExternalNavigationDelegateImplTest {

    @Rule public final MockitoRule mocks = MockitoJUnit.rule();

    @Mock private WebContents mockWebContents;
    @Mock private Context mockContext;
    @Mock private PackageManager mockPackageManager;
    @Mock private GURL mockUrl;

    private ExternalNavigationDelegateImpl delegate;

    @Before
    public void setUp() {
        delegate = new ExternalNavigationDelegateImpl(mockWebContents, mockContext);
    }

    @Test
    public void canCloseTabOnIncognitoIntentLaunch_returnsFalse() {
        assertFalse(delegate.canCloseTabOnIncognitoIntentLaunch());
    }

    @Test
    public void isIncognito_delegatesToWebContents() {
        when(mockWebContents.isIncognito()).thenReturn(true);
        assertTrue(delegate.isIncognito());

        when(mockWebContents.isIncognito()).thenReturn(false);
        assertFalse(delegate.isIncognito());
    }

    @Test
    public void willAppHandleIntent_returnsTrueWhenResolved() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        Intent intent = new Intent();
        when(mockPackageManager.resolveActivity(intent, 0)).thenReturn(mock(ResolveInfo.class));
        assertTrue(delegate.willAppHandleIntent(intent));
    }

    @Test
    public void willAppHandleIntent_returnsFalseWhenNotResolved() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        Intent intent = new Intent();
        when(mockPackageManager.resolveActivity(intent, 0)).thenReturn(null);
        assertFalse(delegate.willAppHandleIntent(intent));
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_OtherScheme_CallsHandleExternalURL() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        when(mockUrl.getScheme()).thenReturn("tel");
        when(mockUrl.getSpec()).thenReturn("tel:12345");

        // Mock handleExternalURL behavior indirectly by testing if it tries to resolve activity
        when(mockPackageManager.resolveActivity(any(Intent.class), anyInt())).thenReturn(null);

        delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl);

        // Verify that resolveActivity was called at least once (which handleExternalURL does)
        verify(mockPackageManager).resolveActivity(any(Intent.class), anyInt());
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_IntentWithFallbackAppNotInstalled_launchesFallback() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        String fallbackUrl = "https://play.google.com/store/apps/details?id=com.example.app";
        String spec = "intent://example.com/#Intent;scheme=http;package=com.example.app;S.browser_fallback_url="
                + Uri.encode(fallbackUrl) + ";end";
        when(mockUrl.getScheme()).thenReturn("intent");
        when(mockUrl.getSpec()).thenReturn(spec);

        // App not installed, resolveActivity returns null for the main intent
        when(mockPackageManager.resolveActivity(any(Intent.class), anyInt())).thenReturn(null);

        assertTrue(delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl));

        // Verify that the fallback intent was launched
        org.mockito.ArgumentCaptor<Intent> intentCaptor = org.mockito.ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(intentCaptor.capture());

        Intent launchedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals(fallbackUrl, launchedIntent.getDataString());
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_IntentAppInstalled_launchesMainIntent() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        String fallbackUrl = "https://play.google.com/store/apps/details?id=com.example.app";
        String spec = "intent://example.com/#Intent;scheme=http;package=com.example.app;S.browser_fallback_url="
                + Uri.encode(fallbackUrl) + ";end";
        when(mockUrl.getScheme()).thenReturn("intent");
        when(mockUrl.getSpec()).thenReturn(spec);

        // App is installed, resolveActivity returns mock ResolveInfo
        when(mockPackageManager.resolveActivity(any(Intent.class), anyInt())).thenReturn(mock(ResolveInfo.class));

        assertTrue(delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl));

        // Verify that the main intent was launched, NOT the fallback
        org.mockito.ArgumentCaptor<Intent> intentCaptor = org.mockito.ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(intentCaptor.capture());

        Intent launchedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals("http://example.com/", launchedIntent.getDataString());
        assertEquals("com.example.app", launchedIntent.getPackage());
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_IntentNoFallbackAppNotInstalled_doesNothing() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        String spec = "intent://example.com/#Intent;scheme=http;package=com.example.app;end";
        when(mockUrl.getScheme()).thenReturn("intent");
        when(mockUrl.getSpec()).thenReturn(spec);

        // App not installed, resolveActivity returns null for the main intent
        when(mockPackageManager.resolveActivity(any(Intent.class), anyInt())).thenReturn(null);

        assertTrue(delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl));

        // Verify that no activity was launched
        verify(mockContext, org.mockito.Mockito.never()).startActivity(any(Intent.class));
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_IntentMalformedUri_doesNothing() {
        // Malformed Intent URL with unknown attribute to trigger URISyntaxException in Intent.parseUri
        String spec = "intent://example.com/#Intent;badattribute=123;end";
        when(mockUrl.getScheme()).thenReturn("intent");
        when(mockUrl.getSpec()).thenReturn(spec);

        assertTrue(delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl));

        // Verify that no activity was launched
        verify(mockContext, org.mockito.Mockito.never()).startActivity(any(Intent.class));
    }

    @Test
    public void shouldDisableExternalIntentRequestsForUrl_IntentFallbackThrowsActivityNotFound_doesNotCrash() {
        when(mockContext.getPackageManager()).thenReturn(mockPackageManager);
        String fallbackUrl = "https://play.google.com/store/apps/details?id=com.example.app";
        String spec = "intent://example.com/#Intent;scheme=http;package=com.example.app;S.browser_fallback_url="
                + Uri.encode(fallbackUrl) + ";end";
        when(mockUrl.getScheme()).thenReturn("intent");
        when(mockUrl.getSpec()).thenReturn(spec);

        // App not installed, resolveActivity returns null for the main intent
        when(mockPackageManager.resolveActivity(any(Intent.class), anyInt())).thenReturn(null);

        // Throw ActivityNotFoundException when attempting to launch the fallback URL
        doThrow(new ActivityNotFoundException()).when(mockContext).startActivity(any(Intent.class));

        assertTrue(delegate.shouldDisableExternalIntentRequestsForUrl(mockUrl));

        // Verify that startActivity was attempted (since it threw during that call)
        verify(mockContext).startActivity(any(Intent.class));
    }
}
