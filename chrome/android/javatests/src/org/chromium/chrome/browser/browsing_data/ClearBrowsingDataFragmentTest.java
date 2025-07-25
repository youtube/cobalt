// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.collection.ArraySet;
import androidx.fragment.app.Fragment;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.tabs.TabLayout;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment.DialogOption;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.SpinnerPreference;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.Set;

/** Tests for ClearBrowsingDataFragment interaction with underlying data model. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ClearBrowsingDataFragmentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SettingsActivityTestRule<ClearBrowsingDataFragmentAdvanced> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(
                    ClearBrowsingDataFragmentAdvanced.class,
                    ClearBrowsingDataFragment.createFragmentArgs(
                            /* isFetcherSuppliedFromOutside= */ false));

    @Rule
    public SettingsActivityTestRule<ClearBrowsingDataTabsFragment>
            mSettingsActivityTabFragmentTestRule =
                    new SettingsActivityTestRule<>(ClearBrowsingDataTabsFragment.class);

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeMock;

    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private final CallbackHelper mCallbackHelper = new CallbackHelper();

    @TimePeriod private static final int DEFAULT_TIME_PERIOD = TimePeriod.ALL_TIME;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(BrowsingDataBridgeJni.TEST_HOOKS, mBrowsingDataBridgeMock);
        // Ensure that whenever the mock is asked to clear browsing data, the callback is
        // immediately called.
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    ((BrowsingDataBridge) invocation.getArgument(0))
                                            .browsingDataCleared();
                                    mCallbackHelper.notifyCalled();
                                    return null;
                                })
                .when(mBrowsingDataBridgeMock)
                .clearBrowsingData(any(), any(), any(), anyInt(), any(), any(), any(), any());

        // Default to delete all history.
        when(mBrowsingDataBridgeMock.getBrowsingDataDeletionTimePeriod(any(), anyInt()))
                .thenReturn(DEFAULT_TIME_PERIOD);

        mActivityTestRule.startMainActivityOnBlankPage();

        // There can be some left-over notification channels from other tests.
        // TODO(crbug.com/951402): Find a general solution to avoid leaking channels between tests.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        SiteChannelsManager manager = SiteChannelsManager.getInstance();
                        manager.deleteAllSiteChannels();
                    });
        }
    }

    /** Waits for the progress dialog to disappear from the given CBD preference. */
    private void waitForProgressToComplete(final ClearBrowsingDataFragment preferences) {
        // For pre-M, give it a bit more time to complete.
        final long kDelay = CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(preferences.getProgressDialog(), Matchers.nullValue());
                },
                kDelay,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private static void clickClearButton(Fragment preferences) {
        Button clearButton = preferences.getView().findViewById(R.id.clear_button);
        Assert.assertNotNull(clearButton);
        Assert.assertTrue(clearButton.isEnabled());
        clearButton.callOnClick();
    }

    private SettingsActivity startPreferences() {
        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        ClearBrowsingDataFragment fragment = mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                fragment.getClearBrowsingDataFetcher()::fetchImportantSites);
        return settingsActivity;
    }

    @Test
    @LargeTest
    public void testSigningOut() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.menu_id_targeted_help)
                            != null;
                });

        final ClearBrowsingDataFragment clearBrowsingDataFragment =
                mSettingsActivityTestRule.getFragment();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RecyclerView recyclerView =
                            clearBrowsingDataFragment.getView().findViewById(R.id.recycler_view);
                    recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() - 1);
                });
        onView(withText(clearBrowsingDataFragment.buildSignOutOfChromeText().toString()))
                .perform(clickOnSignOutLink());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        CriteriaHelper.pollUiThread(
                () ->
                        !IdentityServicesProvider.get()
                                .getIdentityManager(Profile.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SIGNIN),
                "Account should be signed out!");

        // Footer should be hidden after sign-out.
        onView(withText(clearBrowsingDataFragment.buildSignOutOfChromeText().toString()))
                .check(doesNotExist());
    }

    /** Test that Clear Browsing Data offers two tabs and records a preference when switched. */
    @Test
    @MediumTest
    public void testTabsSwitcher() {
        setDataTypesToClear(ClearBrowsingDataFragment.getAllOptions().toArray(new Integer[0]));
        // Set "Advanced" as the user's cached preference.
        when(mBrowsingDataBridgeMock.getLastClearBrowsingDataTab(any())).thenReturn(1);

        mSettingsActivityTabFragmentTestRule.startSettingsActivity();
        final ClearBrowsingDataTabsFragment preferences =
                mSettingsActivityTabFragmentTestRule.getFragment();

        // Verify tab preference is loaded.
        verify(mBrowsingDataBridgeMock).getLastClearBrowsingDataTab(any());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewPager2 viewPager =
                            (ViewPager2)
                                    preferences
                                            .getView()
                                            .findViewById(R.id.clear_browsing_data_viewpager);
                    RecyclerView.Adapter adapter = viewPager.getAdapter();
                    Assert.assertEquals(2, adapter.getItemCount());
                    Assert.assertEquals(1, viewPager.getCurrentItem());
                    TabLayout tabLayout =
                            preferences.getView().findViewById(R.id.clear_browsing_data_tabs);
                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.clear_browsing_data_basic_tab_title),
                            tabLayout.getTabAt(0).getText());
                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.prefs_section_advanced),
                            tabLayout.getTabAt(1).getText());
                    viewPager.setCurrentItem(0);
                });
        // Verify the tab preference is saved.
        verify(mBrowsingDataBridgeMock).setLastClearBrowsingDataTab(any(), eq(0));
    }

    /**
     * Tests that a fragment with all options preselected indeed has all checkboxes checked on
     * startup, and that deletion with all checkboxes checked completes successfully.
     */
    @Test
    @MediumTest
    public void testClearingEverything() throws Exception {
        setDataTypesToClear(ClearBrowsingDataFragment.getAllOptions().toArray(new Integer[0]));

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Privacy.DeleteBrowsingData.Action",
                        DeleteBrowsingDataAction.CLEAR_BROWSING_DATA_DIALOG);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PreferenceScreen screen = preferences.getPreferenceScreen();

                    for (int i = 0; i < screen.getPreferenceCount(); ++i) {
                        Preference pref = screen.getPreference(i);
                        if (!(pref instanceof CheckBoxPreference)) {
                            continue;
                        }
                        CheckBoxPreference checkbox = (CheckBoxPreference) pref;
                        Assert.assertTrue(checkbox.isChecked());
                    }
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForFirst();

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();

        // Verify that we got the appropriate call to clear all data.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(getAllDataTypes()),
                        eq(DEFAULT_TIME_PERIOD),
                        any(),
                        any(),
                        any(),
                        any());
    }

    private static int[] getAllDataTypes() {
        Set<Integer> dialogTypes = ClearBrowsingDataFragment.getAllOptions();
        int[] datatypes = new int[dialogTypes.size()];
        for (int i = 0; i < datatypes.length; i++) {
            datatypes[i] = ClearBrowsingDataFragment.getDataType(i);
        }
        Arrays.sort(datatypes);
        return datatypes;
    }

    /** Tests that changing the time interval for deletion affects the delete request. */
    @Test
    @MediumTest
    public void testClearTimeInterval() throws Exception {
        setDataTypesToClear(DialogOption.CLEAR_CACHE);

        final ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    changeTimePeriodTo(preferences, TimePeriod.LAST_HOUR);
                    clickClearButton(preferences);
                });

        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForFirst();

        // Verify that we got the appropriate call to clear all data.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(new int[] {BrowsingDataType.CACHE}),
                        eq(TimePeriod.LAST_HOUR),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /** Selects the specified time for browsing data removal. */
    private void changeTimePeriodTo(ClearBrowsingDataFragment preferences, @TimePeriod int time) {
        SpinnerPreference spinnerPref =
                (SpinnerPreference)
                        preferences.findPreference(ClearBrowsingDataFragment.PREF_TIME_RANGE);
        Spinner spinner = spinnerPref.getSpinnerForTesting();
        int itemCount = spinner.getAdapter().getCount();
        for (int i = 0; i < itemCount; i++) {
            var option = (TimePeriodUtils.TimePeriodSpinnerOption) spinner.getAdapter().getItem(i);
            if (option.getTimePeriod() == time) {
                spinner.setSelection(i);
                return;
            }
        }
        Assert.fail("Failed to find time period " + time);
    }

    @Test
    @MediumTest
    public void testHelpButtonClicked() {
        SettingsActivity activity = startPreferences();
        ClearBrowsingDataFragment fragment = mSettingsActivityTestRule.getFragment();

        fragment.setHelpAndFeedbackLauncher(mHelpAndFeedbackLauncher);
        onView(withId(R.id.menu_id_targeted_help)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verify(mHelpAndFeedbackLauncher)
                            .show(
                                    activity,
                                    fragment.getString(R.string.help_context_clear_browsing_data),
                                    null);
                });
    }

    /**
     * A helper Runnable that opens the Settings activity containing a ClearBrowsingDataFragment
     * fragment and clicks the "Clear" button.
     */
    static class OpenPreferencesEnableDialogAndClickClearRunnable implements Runnable {
        final SettingsActivity mSettingsActivity;

        /**
         * Instantiates this OpenPreferencesEnableDialogAndClickClearRunnable.
         *
         * @param settingsActivity A Settings activity containing ClearBrowsingDataFragment
         *     fragment.
         */
        public OpenPreferencesEnableDialogAndClickClearRunnable(SettingsActivity settingsActivity) {
            mSettingsActivity = settingsActivity;
        }

        @Override
        public void run() {
            ClearBrowsingDataFragment fragment =
                    (ClearBrowsingDataFragment) mSettingsActivity.getMainFragment();
            PreferenceScreen screen = fragment.getPreferenceScreen();

            // Enable the dialog and click the "Clear" button.
            ((ClearBrowsingDataFragment) mSettingsActivity.getMainFragment())
                    .getClearBrowsingDataFetcher()
                    .enableDialogAboutOtherFormsOfBrowsingHistory();
            clickClearButton(fragment);
        }
    }

    /**
     * A criterion that is satisfied when a ClearBrowsingDataFragment fragment in the given Settings
     * activity is closed.
     */
    static class PreferenceScreenClosedCriterion implements Runnable {
        final SettingsActivity mSettingsActivity;

        /**
         * Instantiates this PreferenceScreenClosedCriterion.
         *
         * @param settingsActivity A Settings activity containing ClearBrowsingDataFragment
         *     fragment.
         */
        public PreferenceScreenClosedCriterion(SettingsActivity settingsActivity) {
            mSettingsActivity = settingsActivity;
        }

        @Override
        public void run() {
            ClearBrowsingDataFragment fragment =
                    (ClearBrowsingDataFragment) mSettingsActivity.getMainFragment();
            if (fragment == null) return;
            Criteria.checkThat(fragment.isVisible(), Matchers.is(false));
        }
    }

    /**
     * Tests that if the dialog about other forms of browsing history is enabled, it will be shown
     * after the deletion completes, if and only if browsing history was checked for deletion and it
     * has not been shown before.
     */
    @Test
    @LargeTest
    public void testDialogAboutOtherFormsOfBrowsingHistory() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        OtherFormsOfHistoryDialogFragment.clearShownPreferenceForTesting();

        // History is not selected. We still need to select some other datatype, otherwise the
        // "Clear" button won't be enabled.
        setDataTypesToClear(DialogOption.CLEAR_CACHE);
        final SettingsActivity settingsActivity1 = startPreferences();
        TestThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity1));
        mCallbackHelper.waitForCallback(0);

        assertDataTypesCleared(BrowsingDataType.CACHE);

        // The dialog about other forms of history is not shown. The Clear Browsing Data preferences
        // is closed as usual.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity1));
        // Reopen Clear Browsing Data preferences, this time with history selected for clearing.
        setDataTypesToClear(DialogOption.CLEAR_HISTORY);
        final SettingsActivity settingsActivity2 = startPreferences();
        TestThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity2));

        // The dialog about other forms of history should now be shown.
        CriteriaHelper.pollUiThread(
                () -> {
                    ClearBrowsingDataFragment fragment =
                            (ClearBrowsingDataFragment) settingsActivity2.getMainFragment();
                    OtherFormsOfHistoryDialogFragment dialog =
                            fragment.getDialogAboutOtherFormsOfBrowsingHistory();
                    Criteria.checkThat(dialog, Matchers.notNullValue());
                    Criteria.checkThat(dialog.getActivity(), Matchers.notNullValue());
                });

        // Close that dialog.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ClearBrowsingDataFragment fragment =
                            (ClearBrowsingDataFragment) settingsActivity2.getMainFragment();
                    fragment.getDialogAboutOtherFormsOfBrowsingHistory()
                            .onClick(null, AlertDialog.BUTTON_POSITIVE);
                });

        // That should close the preference screen as well.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity2));

        mCallbackHelper.waitForCallback(1);
        // Verify history cleared.
        assertDataTypesCleared(BrowsingDataType.HISTORY);

        // Reopen Clear Browsing Data preferences and clear history once again.
        setDataTypesToClear(DialogOption.CLEAR_HISTORY);
        final SettingsActivity settingsActivity3 = startPreferences();
        TestThreadUtils.runOnUiThreadBlocking(
                new OpenPreferencesEnableDialogAndClickClearRunnable(settingsActivity3));

        // The dialog about other forms of browsing history is still enabled, and history has been
        // selected for deletion. However, the dialog has already been shown before, and therefore
        // we won't show it again. Expect that the preference screen closes.
        CriteriaHelper.pollUiThread(new PreferenceScreenClosedCriterion(settingsActivity3));

        int[] expectedTypes = new int[] {BrowsingDataType.HISTORY};
        // Should be cleared again.
        verify(mBrowsingDataBridgeMock, times(2))
                .clearBrowsingData(
                        any(), any(), eq(expectedTypes), anyInt(), any(), any(), any(), any());
    }

    /**
     * Verify that we got the appropriate call to clear browsing data with the given types.
     *
     * @param types Values of BrowsingDataType to remove.
     */
    private void assertDataTypesCleared(int... types) {
        // TODO(yfriedman): Add testing for time period.
        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(types),
                        eq(DEFAULT_TIME_PERIOD),
                        any(),
                        any(),
                        any(),
                        any());
    }

    /** This presses the 'clear' button on the root preference page. */
    private Runnable getPressClearRunnable(final ClearBrowsingDataFragment preferences) {
        return () -> clickClearButton(preferences);
    }

    /** This presses the clear button in the important sites dialog */
    private Runnable getPressButtonInImportantDialogRunnable(
            final ClearBrowsingDataFragment preferences, final int whichButton) {
        return () -> {
            Assert.assertNotNull(preferences);
            ConfirmImportantSitesDialogFragment dialog =
                    preferences.getImportantSitesDialogFragment();
            ((AlertDialog) dialog.getDialog()).getButton(whichButton).performClick();
        };
    }

    /**
     * This waits until the important dialog fragment & the given number of important sites are
     * shown.
     */
    private void waitForImportantDialogToShow(
            final ClearBrowsingDataFragment preferences, final int numImportantSites) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(preferences, Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment(), Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment().getDialog(),
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            preferences.getImportantSitesDialogFragment().getDialog().isShowing(),
                            Matchers.is(true));

                    ListView sitesList =
                            preferences.getImportantSitesDialogFragment().getSitesList();
                    Criteria.checkThat(
                            sitesList.getAdapter().getCount(), Matchers.is(numImportantSites));
                    Criteria.checkThat(
                            sitesList.getChildCount(),
                            Matchers.greaterThanOrEqualTo(numImportantSites));
                });
    }

    private void markOriginsAsImportant(final String[] importantOrigins) {
        doAnswer(
                        invocation -> {
                            BrowsingDataBridge.ImportantSitesCallback callback =
                                    invocation.getArgument(1);
                            // Reason '0' is still a reason so just init the array.
                            int[] reasons = new int[importantOrigins.length];
                            callback.onImportantRegisterableDomainsReady(
                                    importantOrigins, importantOrigins, reasons, false);
                            return null;
                        })
                .when(mBrowsingDataBridgeMock)
                .fetchImportantSites(any(), any());

        // Return arbitrary number above 1.
        when(mBrowsingDataBridgeMock.getMaxImportantSites()).thenReturn(3);
    }

    /**
     * Tests that the important sites dialog is shown, and if we don't deselect anything we
     * correctly clear everything.
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoFiltering() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String[] importantOrigins = {"http://www.facebook.com", "https://www.google.com"};
        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);
        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        ClearBrowsingDataFragment preferences =
                (ClearBrowsingDataFragment) startPreferences().getMainFragment();

        // Clear in root preference.
        TestThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(preferences));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(preferences, 2);
        // Clear in important dialog.
        TestThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(preferences, AlertDialog.BUTTON_POSITIVE));
        waitForProgressToComplete(preferences);
        mCallbackHelper.waitForFirst();

        // Verify history cleared.
        assertDataTypesCleared(BrowsingDataType.HISTORY, BrowsingDataType.CACHE);
    }

    /**
     * Tests that the important sites dialog is shown and if we cancel nothing happens.
     *
     * <p>http://crbug.com/727310
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialogNoopOnCancel() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String[] importantOrigins = {"http://www.facebook.com", "http://www.google.com"};
        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);
        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        SettingsActivity settingsActivity = startPreferences();
        ClearBrowsingDataFragment fragment =
                (ClearBrowsingDataFragment) settingsActivity.getMainFragment();
        TestThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        // Check that the important sites dialog is shown, and the list is visible.
        waitForImportantDialogToShow(fragment, 2);
        // Press the cancel button.
        TestThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_NEGATIVE));
        settingsActivity.finish();

        // Nothing was cleared.
        verify(mBrowsingDataBridgeMock, never())
                .clearBrowsingData(any(), any(), any(), anyInt(), any(), any(), any(), any());
    }

    /**
     * Tests that the important sites dialog is shown, we can successfully uncheck options, and
     * clicking clear doesn't clear the protected domain.
     */
    @Test
    @MediumTest
    @Feature({"SiteEngagement"})
    public void testImportantSitesDialog() throws Exception {
        // Sign in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        final String kKeepDomain = "https://www.chrome.com";
        final String kClearDomain = "https://www.google.com";

        final String[] importantOrigins = {kKeepDomain, kClearDomain};

        // First mark our origins as important.
        markOriginsAsImportant(importantOrigins);

        setDataTypesToClear(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE);

        final SettingsActivity settingsActivity = startPreferences();
        final ClearBrowsingDataFragment fragment =
                (ClearBrowsingDataFragment) settingsActivity.getMainFragment();

        // Uncheck the first item (our internal web server).
        TestThreadUtils.runOnUiThreadBlocking(getPressClearRunnable(fragment));
        waitForImportantDialogToShow(fragment, 2);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListView sitesList = fragment.getImportantSitesDialogFragment().getSitesList();
                    sitesList.performItemClick(
                            sitesList.getChildAt(0), 0, sitesList.getAdapter().getItemId(0));
                });

        // Check that our server origin is in the set of deselected domains.
        CriteriaHelper.pollUiThread(
                () -> {
                    ConfirmImportantSitesDialogFragment dialog =
                            fragment.getImportantSitesDialogFragment();
                    Criteria.checkThat(
                            dialog.getDeselectedDomains(), Matchers.hasItem(kKeepDomain));
                });

        // Click the clear button.
        TestThreadUtils.runOnUiThreadBlocking(
                getPressButtonInImportantDialogRunnable(fragment, AlertDialog.BUTTON_POSITIVE));

        waitForProgressToComplete(fragment);
        mCallbackHelper.waitForFirst();

        int[] expectedTypes = new int[] {BrowsingDataType.HISTORY, BrowsingDataType.CACHE};
        String[] keepDomains = new String[] {kKeepDomain};
        String[] ignoredDomains = new String[] {kClearDomain};

        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(expectedTypes),
                        eq(DEFAULT_TIME_PERIOD),
                        eq(keepDomains),
                        any(),
                        eq(ignoredDomains),
                        any());
    }

    private void setDataTypesToClear(final Integer... typesToClear) {
        Set<Integer> typesToClearSet = new ArraySet<Integer>(Arrays.asList(typesToClear));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (@DialogOption Integer option : ClearBrowsingDataFragment.getAllOptions()) {
                        boolean enabled = typesToClearSet.contains(option);
                        when(mBrowsingDataBridgeMock.getBrowsingDataDeletionPreference(
                                        any(),
                                        eq(ClearBrowsingDataFragment.getDataType(option)),
                                        anyInt()))
                                .thenReturn(enabled);
                    }
                });
    }

    // TODO(https://crbug.com/1334586): Move this to a test util class.
    private ViewAction clickOnSignOutLink() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the sign out link in the clear browsing data footer";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length != 1) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                spans[0].onClick(view);
            }
        };
    }
}
