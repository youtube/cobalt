// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.content_settings.CookieControlsMode;

/**
 * A delegate class to record metrics associated with each card inside
 * Privacy Guide {@link PrivacyGuideFragment}.
 */
class PrivacyGuideMetricsDelegate {
    /**
     * Initial state of the MSBB when {@link MSBBFragment} is created.
     */
    private @Nullable Boolean mInitialMsbbState;
    /**
     * Initial state of History Sync when {@link HistorySyncFragment} is created.
     */
    private @Nullable Boolean mInitialHistorySyncState;
    /**
     * Initial state of the Safe Browsing when {@link SafeBrowsingFragment} is created.
     */
    private @Nullable @SafeBrowsingState Integer mInitialSafeBrowsingState;
    /**
     * Initial mode of the Cookies Control when {@link CookiesFragment} is created.
     */
    private @Nullable @CookieControlsMode Integer mInitialCookiesControlMode;

    /**
     * A method to record metrics on the next click of {@link MSBBFragment}
     */
    private void recordMetricsOnNextForMSBBCard() {
        assert mInitialMsbbState != null : "Initial state of MSSB not set.";

        boolean currentValue = PrivacyGuideUtils.isMsbbEnabled();
        @PrivacyGuideSettingsStates
        int stateChange;

        if (mInitialMsbbState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_ON_TO_ON;
        } else if (mInitialMsbbState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_ON_TO_OFF;
        } else if (!mInitialMsbbState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialMsbbState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.SettingsStates",
                stateChange, PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the MSBB card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickMSBB");
        // Record histogram for clicking the next button on the MSBB card
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.MSBB_NEXT_BUTTON, PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the next click of {@link HistorySyncFragment}.
     */
    private void recordMetricsOnNextForHistorySyncCard() {
        assert mInitialHistorySyncState != null : "Initial state of History Sync not set.";

        boolean currentValue = PrivacyGuideUtils.isHistorySyncEnabled();
        @PrivacyGuideSettingsStates
        int stateChange;

        if (mInitialHistorySyncState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON;
        } else if (mInitialHistorySyncState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF;
        } else if (!mInitialHistorySyncState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialHistorySyncState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.SettingsStates",
                stateChange, PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the History Sync card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickHistorySync");
        // Record histogram for clicking the next button on the History Sync card
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the next click of {@link SafeBrowsingFragment}
     */
    private void recordMetricsOnNextForSafeBrowsingCard() {
        assert mInitialSafeBrowsingState != null : "Initial state of Safe Browsing not set.";

        @SafeBrowsingState
        int currentValue = PrivacyGuideUtils.getSafeBrowsingState();

        boolean isStartStateEnhance =
                mInitialSafeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION;
        boolean isEndStateEnhance = currentValue == SafeBrowsingState.ENHANCED_PROTECTION;

        @PrivacyGuideSettingsStates
        int stateChange;

        if (isStartStateEnhance && isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED;
        } else if (isStartStateEnhance && !isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD;
        } else if (!isStartStateEnhance && isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED;
        } else {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD;
        }

        // Record histogram comparing |mInitialSafeBrowsingState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.SettingsStates",
                stateChange, PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the Safe Browsing card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickSafeBrowsing");
        // Record histogram for clicking the next button on the Safe Browsing card
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the next click of {@link CookiesFragment}
     */
    private void recordMetricsOnNextForCookiesCard() {
        assert mInitialCookiesControlMode != null : "Initial mode of Cookie Control not set.";

        @CookieControlsMode
        int currentValue = PrivacyGuideUtils.getCookieControlsMode();

        boolean isInitialStateBlock3PIncognito =
                mInitialCookiesControlMode == CookieControlsMode.INCOGNITO_ONLY;
        boolean isEndStateBlock3PIncognito = currentValue == CookieControlsMode.INCOGNITO_ONLY;

        @PrivacyGuideSettingsStates
        int stateChange;

        if (isInitialStateBlock3PIncognito && isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO;
        } else if (isInitialStateBlock3PIncognito && !isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P;
        } else if (!isInitialStateBlock3PIncognito && isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO;
        } else {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_TO3P;
        }

        // Record histogram comparing |mInitialCookiesControlMode| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.SettingsStates",
                stateChange, PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the Cookies card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickCookies");
        // Record histogram for clicking the next button on the Cookies card
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.COOKIES_NEXT_BUTTON, PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to set the initial state of a card {@link PrivacyGuideFragment.FragmentType} in
     * Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void setInitialStateForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB: {
                mInitialMsbbState = PrivacyGuideUtils.isMsbbEnabled();
                break;
            }
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC: {
                mInitialHistorySyncState = PrivacyGuideUtils.isHistorySyncEnabled();
                break;
            }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING: {
                mInitialSafeBrowsingState = PrivacyGuideUtils.getSafeBrowsingState();
                break;
            }
            case PrivacyGuideFragment.FragmentType.COOKIES: {
                mInitialCookiesControlMode = PrivacyGuideUtils.getCookieControlsMode();
                break;
            }
            case PrivacyGuideFragment.FragmentType.WELCOME:
            case PrivacyGuideFragment.FragmentType.DONE:
                // The Welcome and Done cards don't store/update any state.
                break;
            default:
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }

    /**
     * A method to record metrics on the next click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void recordMetricsOnNextForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.WELCOME:
                recordMetricsForWelcomeCard();
                break;
            case PrivacyGuideFragment.FragmentType.MSBB: {
                recordMetricsOnNextForMSBBCard();
                break;
            }
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC: {
                recordMetricsOnNextForHistorySyncCard();
                break;
            }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING: {
                recordMetricsOnNextForSafeBrowsingCard();
                break;
            }
            case PrivacyGuideFragment.FragmentType.COOKIES: {
                recordMetricsOnNextForCookiesCard();
                break;
            }
            default:
                // The Done card does not have a next button and we won't support a case for it
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }

    /**
     * A method to record metrics on the next click of the privacy guide welcome page.
     */
    static void recordMetricsForWelcomeCard() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickWelcome");
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.WELCOME_NEXT_BUTTON, PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics for the done click of the privacy guide completion page.
     */
    static void recordMetricsForDoneButton() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickCompletion");
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the Privacy Sandbox link click on the privacy guide done page.
     */
    static void recordMetricsForPsLink() {
        RecordUserAction.record("Settings.PrivacyGuide.CompletionPSClick");
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.EntryExit",
                PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the WAA link click on the privacy guide done page.
     */
    static void recordMetricsForWaaLink() {
        RecordUserAction.record("Settings.PrivacyGuide.CompletionSWAAClick");
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.EntryExit",
                PrivacyGuideInteractions.SWAA_COMPLETION_LINK, PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on MSBB toggle change of the Privacy Guide's {@link MSBBFragment}.
     */
    static void recordMetricsOnMSBBChange(boolean isMSBBOn) {
        if (isMSBBOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeMSBBOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeMSBBOff");
        }
    }

    /**
     * A method to record metrics on the History Sync toggle change of the Privacy Guide's {@link
     * HistorySyncFragment}.
     */
    static void recordMetricsOnHistorySyncChange(boolean isHistorySyncOn) {
        if (isHistorySyncOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeHistorySyncOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeHistorySyncOff");
        }
    }

    /**
     * A method to record metrics on Safe Browsing radio button change of the Privacy Guide's {@link
     * SafeBrowsingFragment}.
     */
    static void recordMetricsOnSafeBrowsingChange(@SafeBrowsingState int safeBrowsingState) {
        switch (safeBrowsingState) {
            case SafeBrowsingState.ENHANCED_PROTECTION:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced");
                break;
            case SafeBrowsingState.STANDARD_PROTECTION:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeSafeBrowsingStandard");
                break;
            default:
                assert false : "Unexpected SafeBrowsingState " + safeBrowsingState;
        }
    }

    /**
     * A method to record metrics on Cookie Controls radio button change of the Privacy Guide's
     * {@link CookiesFragment}.
     */
    static void recordMetricsOnCookieControlsChange(@CookieControlsMode int cookieControlsMode) {
        switch (cookieControlsMode) {
            case CookieControlsMode.INCOGNITO_ONLY:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito");
                break;
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeCookiesBlock3P");
                break;
            default:
                assert false : "Unexpected CookieControlMode " + cookieControlsMode;
        }
    }

    /**
     * A method to record metrics on the back click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    static void recordMetricsOnBackForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC: {
                RecordUserAction.record("Settings.PrivacyGuide.BackClickHistorySync");
                break;
            }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING: {
                RecordUserAction.record("Settings.PrivacyGuide.BackClickSafeBrowsing");
                break;
            }
            case PrivacyGuideFragment.FragmentType.COOKIES: {
                RecordUserAction.record("Settings.PrivacyGuide.BackClickCookies");
                break;
            }
            default:
                // The Welcome, MSBB and Done cards don't have a back button, and so we won't
                // support a case for it.
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }
}
