// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_site;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.RequestDesktopUtilsUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link DesktopSiteSettingsIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class, ShadowSysUtils.class})
public class DesktopSiteSettingsIphControllerUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock private WebsitePreferenceBridge mWebsitePreferenceBridge;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private View mToolbarMenuButton;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private MessageDispatcher mMessageDispatcher;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private DesktopSiteSettingsIphController mController;
    private GURL mTabUrl;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        mContext = ApplicationProvider.getApplicationContext();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(mContext).when(mToolbarMenuButton).getContext();

        TrackerFactory.setTrackerForTests(mTracker);
        when(mTracker.wouldTriggerHelpUi(
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE))
                .thenReturn(true);
        when(mTracker.wouldTriggerHelpUi(
                        FeatureConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_FEATURE))
                .thenReturn(true);

        mTabUrl = JUnitTestGURLs.EXAMPLE_URL;
        when(mTab.getUrl()).thenReturn(mTabUrl);
        when(mTab.isIncognito()).thenReturn(false);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);

        when(mWebsitePreferenceBridge.getContentSettingsExceptions(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(new ArrayList<>());

        initializeController();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreateTabObserver_GenericIph() {
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, mTabUrl);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
    }

    // This tests the fix for the crash reported in crbug.com/1416519.
    @Test
    @Config(qualifiers = "sw600dp")
    public void testCreateTabObserver_NullTab() {
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(null, mTabUrl);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPerSiteIphPreChecksFailed_NonTabletDevice() {
        boolean failed =
                mController.perSiteIphPreChecksFailed(
                        mTab,
                        mTracker,
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue("Generic site IPH should be triggered only on tablet devices.", failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIphPreChecksFailed_TrackerWouldNotTrigger() {
        when(mTracker.wouldTriggerHelpUi(
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE))
                .thenReturn(false);
        boolean failed =
                mController.perSiteIphPreChecksFailed(
                        mTab,
                        mTracker,
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        verify(mTracker)
                .wouldTriggerHelpUi(
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue(
                "Generic site IPH should not trigger when Tracker#wouldTriggerHelpUi returns"
                        + " false.",
                failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIphPreChecksFailed_IncognitoTab() {
        when(mTab.isIncognito()).thenReturn(true);

        boolean failed =
                mController.perSiteIphPreChecksFailed(
                        mTab,
                        mTracker,
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue("Generic site IPH should not be triggered in incognito.", failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testPerSiteIphPreChecksFailed_ChromePage() {
        mTabUrl = JUnitTestGURLs.CHROME_ABOUT;
        when(mTab.getUrl()).thenReturn(mTabUrl);

        boolean failed =
                mController.perSiteIphPreChecksFailed(
                        mTab,
                        mTracker,
                        FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertTrue("Generic site IPH should not be triggered on a chrome:// page.", failed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIph_SwitchToDesktop() {
        testShowGenericIph(true);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShowGenericIph_SwitchToMobile() {
        testShowGenericIph(false);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testGenericIph_NotShown_SettingUsed() {
        // The user must have previously used the site-level setting if exceptions are added.
        when(mWebsitePreferenceBridge.getContentSettingsExceptions(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(
                        List.of(
                                mock(ContentSettingException.class),
                                mock(ContentSettingException.class)));
        mController.showGenericIph(mTab, mProfile);
        verify(mUserEducationHelper, never()).requestShowIph(mIphCommandCaptor.capture());
    }

    @Test
    public void testCreateTabObserver_WindowSettingIph() {
        simulateActiveWindowSetting();
        ActivityTabTabObserver activityTabTabObserver =
                mController.getActiveTabObserverForTesting();
        activityTabTabObserver.onPageLoadFinished(mTab, mTabUrl);
        verify(mMessageDispatcher)
                .enqueueMessage(any(), eq(mWebContents), eq(MessageScopeType.ORIGIN), eq(false));
    }

    @Test
    public void testShowWindowSettingIph() {
        simulateActiveWindowSetting();
        Assert.assertTrue(
                "The window setting IPH should be shown.",
                mController.showWindowSettingIph(mTab, mProfile));

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        message.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.ORIGIN),
                        eq(false));
        verify(mTracker).notifyEvent(EventConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_IPH_SHOWN);

        Assert.assertEquals(
                "Message identifier should match.",
                MessageIdentifier.DESKTOP_SITE_WINDOW_SETTING,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                "Message title should match.",
                mContext.getString(R.string.rds_window_setting_message_title),
                message.getValue().get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                "Message primary button text should match.",
                mContext.getString(R.string.rds_window_setting_message_button),
                message.getValue().get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertEquals(
                "Message icon resource ID should match.",
                R.drawable.ic_desktop_windows,
                message.getValue().get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testShowWindowSettingIph_TrackerWouldNotTrigger() {
        simulateActiveWindowSetting();
        when(mTracker.wouldTriggerHelpUi(
                        FeatureConstants.REQUEST_DESKTOP_SITE_WINDOW_SETTING_FEATURE))
                .thenReturn(false);
        mController.showWindowSettingIph(mTab, mProfile);
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    public void testShowWindowSettingIph_NotShown_GlobalSettingDisabled() {
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(false);
        mController.showWindowSettingIph(mTab, mProfile);
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    public void testShowWindowSettingIph_NotShown_SiteExceptionPresent() {
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mTabUrl, mTabUrl))
                .thenReturn(ContentSettingValues.BLOCK);
        mController.showWindowSettingIph(mTab, mProfile);
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    public void testShowWindowSettingIph_NotShown_DesktopSiteInUse() {
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(true);
        mController.showWindowSettingIph(mTab, mProfile);
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    private void simulateActiveWindowSetting() {
        // Assume global setting is enabled.
        when(mWebsitePreferenceBridgeJniMock.isContentSettingEnabled(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE))
                .thenReturn(true);
        // Assume that the site has no site-level exceptions.
        when(mWebsitePreferenceBridgeJniMock.getContentSetting(
                        mProfile, ContentSettingsType.REQUEST_DESKTOP_SITE, mTabUrl, mTabUrl))
                .thenReturn(ContentSettingValues.ALLOW);
        // Assume that the site is using a mobile UA, this should mean that the window setting is in
        // use.
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
    }

    private void testShowGenericIph(boolean switchToDesktop) {
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(!switchToDesktop);

        mController.showGenericIph(mTab, mProfile);
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        Assert.assertEquals(
                "IphCommand feature should match.",
                command.featureName,
                FeatureConstants.REQUEST_DESKTOP_SITE_EXCEPTIONS_GENERIC_FEATURE);
        Assert.assertEquals(
                "IphCommand stringId should match.",
                switchToDesktop
                        ? R.string.rds_site_settings_generic_iph_text_desktop
                        : R.string.rds_site_settings_generic_iph_text_mobile,
                command.stringId);
        Assert.assertEquals(
                "IphCommand stringArgs should match.", mTabUrl.getHost(), command.stringArgs[0]);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.request_desktop_site_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }

    private void initializeController() {
        mController =
                new DesktopSiteSettingsIphController(
                        mWindowAndroid,
                        mActivityTabProvider,
                        mProfile,
                        mToolbarMenuButton,
                        mAppMenuHandler,
                        mUserEducationHelper,
                        mWebsitePreferenceBridge,
                        mMessageDispatcher);
    }
}
