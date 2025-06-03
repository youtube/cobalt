// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static androidx.test.espresso.assertion.ViewAssertions.matches;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryActivity;
import org.chromium.chrome.browser.omnibox.suggestions.action.HistoryClustersAction;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionInSuggest;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.SuggestionInfo;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.EntityInfoProto.ActionInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests of the Omnibox Actions.
 *
 * <p>The suite intentionally disables Autocomplete subsystem to prevent real autocompletions from
 * overriding Test data.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmniboxActionsTest {
    public static @ClassRule ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public static @ClassRule DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    private @Mock AutocompleteController.Natives mAutocompleteControllerJniMock;
    private @Mock OmniboxActionJni mOmniboxActionJni;

    private OmniboxTestUtils mOmniboxUtils;
    private Activity mTargetActivity;

    @BeforeClass
    public static void beforeClass() {
        sActivityTestRule.startMainActivityOnBlankPage();
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        sActivityTestRule.waitForDeferredStartup();
    }

    @Before
    public void setUp() throws InterruptedException {
        sActivityTestRule.loadUrl("about:blank");
        mOmniboxUtils = new OmniboxTestUtils(sActivityTestRule.getActivity());
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, mAutocompleteControllerJniMock);
        mJniMocker.mock(OmniboxActionJni.TEST_HOOKS, mOmniboxActionJni);
    }

    @After
    public void tearDown() throws Exception {
        if (mOmniboxUtils.getFocus()) {
            mOmniboxUtils.clearFocus();
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    IncognitoTabHostUtils.closeAllIncognitoTabs();
                });
        if (mTargetActivity != null) {
            ApplicationTestUtils.finishActivity(mTargetActivity);
        }
        mJniMocker.mock(AutocompleteControllerJni.TEST_HOOKS, null);
        mJniMocker.mock(OmniboxActionJni.TEST_HOOKS, null);
    }

    /**
     * Apply suggestions to the Omnibox. Requires at least one of the suggestions to include at
     * least one OmniboxAction. Verifies that suggestions - and actions - are shown.
     *
     * @param matches the matches to show
     */
    private void setSuggestions(AutocompleteMatch... matches) {
        mOmniboxUtils.requestFocus();
        // Ensure we start from empty suggestions list; don't carry over suggestions from previous
        // run.
        mOmniboxUtils.setSuggestions(AutocompleteResult.fromCache(null, null), "");

        mOmniboxUtils.setSuggestions(
                AutocompleteResult.fromCache(Arrays.asList(matches), null), "");
        mOmniboxUtils.checkSuggestionsShown();
        SuggestionInfo<BaseSuggestionView> info = mOmniboxUtils.findSuggestionWithActionChips();
        Assert.assertNotNull("No suggestions with actions", info);
    }

    /** Returns a dummy AutocompleteMatch that features *all* of supplied actions. */
    private AutocompleteMatch createDummySuggestion(@Nullable List<OmniboxAction> actions) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Suggestion")
                .setActions(actions)
                .build();
    }

    private AutocompleteMatch createDummyHistoryClustersAction(String name) {
        return createDummySuggestion(
                List.of(new HistoryClustersAction(0, "hint", "accessibility", name)));
    }

    private AutocompleteMatch createDummyActionInSuggest(ActionInfo.ActionType... types) {
        var actions = new ArrayList<OmniboxAction>();
        for (var type : types) {
            actions.add(
                    new OmniboxActionInSuggest(
                            type.getNumber(),
                            "hint",
                            "accessibility",
                            type.getNumber(),
                            "https://www.google.com"));
        }

        return createDummySuggestion(actions);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_PROVIDER)
    @EnableFeatures({
        ChromeFeatureList.HISTORY_JOURNEYS,
        ChromeFeatureList.OMNIBOX_HISTORY_CLUSTER_ACTION_CHIP
    })
    public void testHistoryClustersAction() throws Exception {
        setSuggestions(createDummyHistoryClustersAction("query"));
        mOmniboxUtils.clickOnAction(0, 0);

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(sActivityTestRule.getActivity())) {
            CriteriaHelper.pollUiThread(
                    () -> {
                        Tab tab = sActivityTestRule.getActivity().getActivityTab();
                        Criteria.checkThat(tab, Matchers.notNullValue());
                        Criteria.checkThat(
                                tab.getUrl().getSpec(),
                                Matchers.startsWith("chrome://history/journeys"));
                    });
        } else {
            mTargetActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(), HistoryActivity.class);
            Assert.assertNotNull("Could not find the history activity", mTargetActivity);
        }
        verifyNoMoreInteractions(mOmniboxActionJni);
    }

    @Test
    @MediumTest
    public void testActionInSuggestShown() throws Exception {
        setSuggestions(
                createDummySuggestion(null),
                createDummyActionInSuggest(ActionInfo.ActionType.CALL),
                createDummyActionInSuggest(ActionInfo.ActionType.DIRECTIONS));

        mOmniboxUtils.clearFocus();

        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.CALL_VALUE, /* position= */ 1, /* executed= */ false);
        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.DIRECTIONS_VALUE,
                        /* position= */ 2,
                        /* executed= */ false);
        verifyNoMoreInteractions(mOmniboxActionJni);
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_firstAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(
                createDummySuggestion(null),
                createDummyActionInSuggest(ActionInfo.ActionType.CALL),
                createDummyActionInSuggest(ActionInfo.ActionType.DIRECTIONS));

        mOmniboxUtils.clickOnAction(1, 0);

        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.CALL_VALUE, /* position= */ 1, /* executed= */ true);
        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.DIRECTIONS_VALUE,
                        /* position= */ 2,
                        /* executed= */ false);
        verifyNoMoreInteractions(mOmniboxActionJni);
    }

    @Test
    @MediumTest
    public void testActionInSuggestUsed_nthAction() throws Exception {
        // None of these actions have a linked intent, so no action will be taken.
        setSuggestions(
                createDummySuggestion(null),
                createDummyActionInSuggest(
                        ActionInfo.ActionType.CALL,
                        ActionInfo.ActionType.DIRECTIONS,
                        ActionInfo.ActionType.REVIEWS));

        mOmniboxUtils.clickOnAction(1, 2);

        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.CALL_VALUE, /* position= */ 1, /* executed= */ false);
        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.DIRECTIONS_VALUE,
                        /* position= */ 1,
                        /* executed= */ false);
        verify(mOmniboxActionJni, times(1))
                .recordActionShown(
                        ActionInfo.ActionType.REVIEWS_VALUE,
                        /* position= */ 1,
                        /* executed= */ true);
        verifyNoMoreInteractions(mOmniboxActionJni);
    }
}
