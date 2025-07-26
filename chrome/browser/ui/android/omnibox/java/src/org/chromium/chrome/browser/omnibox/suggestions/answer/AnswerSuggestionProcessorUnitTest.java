// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.Spannable;

import androidx.annotation.DrawableRes;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxPedal;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AnswerDataProto.AnswerData;
import org.chromium.components.omnibox.AnswerDataProto.FormattedString;
import org.chromium.components.omnibox.AnswerDataProto.Image;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.RichAnswerTemplateProto.RichAnswerTemplate;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxPedalId;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Optional;

/** Tests for {@link AnswerSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.SUGGESTION_ANSWERS_COLOR_REVERSE)
public class AnswerSuggestionProcessorUnitTest {
    private static final AnswerType[] ANSWER_TYPES = {
        AnswerType.ANSWER_TYPE_DICTIONARY,
        AnswerType.ANSWER_TYPE_FINANCE,
        AnswerType.ANSWER_TYPE_GENERIC_ANSWER,
        AnswerType.ANSWER_TYPE_SPORTS,
        AnswerType.ANSWER_TYPE_SUNRISE_SUNSET,
        AnswerType.ANSWER_TYPE_TRANSLATION,
        AnswerType.ANSWER_TYPE_WEATHER,
        AnswerType.ANSWER_TYPE_WHEN_IS,
        AnswerType.ANSWER_TYPE_CURRENCY
    };

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock UrlBarEditingTextStateProvider mUrlStateProvider;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Bitmap mBitmap;
    private @Mock AutocompleteInput mInput;

    private AnswerSuggestionProcessor mProcessor;
    private Locale mDefaultLocale;
    private Context mContext;

    /**
     * Base Suggestion class that can be used for testing. Holds all mechanisms that are required to
     * processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        // Stores created AutocompleteMatch
        protected final AutocompleteMatch mSuggestion;

        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;

        private SuggestionTestHelper(
                AutocompleteMatch suggestion, PropertyModel model, String userQuery) {
            mSuggestion = suggestion;
            mModel = model;
            when(mUrlStateProvider.getTextWithoutAutocomplete()).thenReturn(userQuery);
            mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        }

        /** Check the content of first suggestion line. */
        private void verifyLine(
                String expectedTitle,
                int expectedMaxLineCount,
                String expectedDescription,
                WritableObjectPropertyKey<Spannable> titleKey,
                WritableIntPropertyKey maxLineCountKey,
                WritableObjectPropertyKey<String> descriptionKey) {
            final Spannable actualTitleSpan = mModel.get(titleKey);
            final String actualTitle = actualTitleSpan == null ? null : actualTitleSpan.toString();
            final String actualDescription = mModel.get(descriptionKey);

            Assert.assertNotNull(actualTitle);
            Assert.assertEquals(expectedTitle, actualTitle);

            Assert.assertEquals(expectedDescription, actualDescription);
            Assert.assertEquals(expectedMaxLineCount, mModel.get(maxLineCountKey));
        }

        void verifyLine1(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(
                    expectedTitle,
                    expectedMaxLineCount,
                    expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION);
        }

        /** Check the content of second suggestion line. */
        void verifyLine2(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(
                    expectedTitle,
                    expectedMaxLineCount,
                    expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION);
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final OmniboxDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            Assert.assertTrue(state.isLarge);
            return state == null ? null : state.drawable;
        }

        @DrawableRes
        int getIconRes() {
            return shadowOf(getIcon()).getCreatedFromResId();
        }
    }

    /** Create Calculation Suggestion. */
    SuggestionTestHelper createCalculationSuggestion(String displayText, String userQuery) {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.CALCULATOR)
                        .setDisplayText(displayText)
                        .setDescription(userQuery)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model, userQuery);
    }

    SuggestionTestHelper createRichAnswerSuggestion(
            AnswerType type, int numberOfActions, boolean includeImage) {
        AnswerData.Builder answerDataBuilder =
                AnswerData.newBuilder()
                        .setHeadline(FormattedString.newBuilder().setText("").build())
                        .setSubhead(FormattedString.newBuilder().setText(""));
        if (includeImage) {
            answerDataBuilder.setImage(
                    Image.newBuilder().setUrl("https://imageserver.com/icon.png"));
        }

        RichAnswerTemplate answer =
                RichAnswerTemplate.newBuilder().addAnswers(answerDataBuilder).build();
        List<OmniboxAction> actions = new ArrayList<>();
        for (int i = 0; i < numberOfActions; i++) {
            actions.add(
                    new OmniboxPedal(123L, "hint", "hint", OmniboxPedalId.CHANGE_GOOGLE_PASSWORD));
        }

        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setSerializedAnswerTemplate(answer.toByteArray())
                        .setAnswerType(type)
                        .setActions(actions)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model, null);
    }

    @Before
    public void setUp() {
        mDefaultLocale = Locale.getDefault();
        mContext = Robolectric.buildActivity(Activity.class).setup().get();
        mContext.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI_DayNight);
        mProcessor =
                new AnswerSuggestionProcessor(
                        mContext, mSuggestionHost, mUrlStateProvider, Optional.of(mImageSupplier));
        OmniboxResourceProvider.disableCachesForTesting();
    }

    @After
    public void tearDown() {
        Locale.setDefault(mDefaultLocale);
        OmniboxResourceProvider.reenableCachesForTesting();
    }

    @Test
    public void calculationAnswer_order() {
        final SuggestionTestHelper suggHelper = createCalculationSuggestion("12345", "123 + 45");

        suggHelper.verifyLine1("123 + 45", 1, null);
        suggHelper.verifyLine2("12345", 1, null);
    }

    @Test
    public void answerImage_fallbackIcons_richAnswerTemplate() {
        for (AnswerType type : ANSWER_TYPES) {
            SuggestionTestHelper suggHelper = createRichAnswerSuggestion(type, 0, false);
            // Note: model is re-created on every iteration.
            Assert.assertNotNull(
                    "No icon associated with type: " + type.name(), suggHelper.getIcon());
        }
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.OMNIBOX_ANSWER_ACTIONS)
    public void richAnswerCard() {
        OmniboxFeatures.sAnswerActionsShowRichCard.setForTesting(true);
        SuggestionTestHelper suggHelper =
                createRichAnswerSuggestion(AnswerType.ANSWER_TYPE_DICTIONARY, 1, true);
        Assert.assertEquals(
                suggHelper.mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                mContext.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.omnibox.R.dimen
                                        .omnibox_simple_card_leadin));
        Assert.assertTrue(suggHelper.mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION));
        Assert.assertTrue(suggHelper.mModel.get(BaseSuggestionViewProperties.SHOW_DECORATION));
        Assert.assertNull(suggHelper.mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        Assert.assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.omnibox.R.dimen
                                        .omnibox_simple_card_top_padding),
                suggHelper.mModel.get(BaseSuggestionViewProperties.TOP_PADDING));
        Assert.assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.omnibox.R.dimen
                                        .omnibox_simple_card_leadin),
                suggHelper.mModel.get(AnswerSuggestionViewProperties.RIGHT_PADDING));

        suggHelper = createRichAnswerSuggestion(AnswerType.ANSWER_TYPE_DICTIONARY, 1, false);
        Assert.assertFalse(suggHelper.mModel.get(BaseSuggestionViewProperties.SHOW_DECORATION));

        // A rich answer with no actions shouldn't get the card treatment.
        suggHelper = createRichAnswerSuggestion(AnswerType.ANSWER_TYPE_DICTIONARY, 0, true);
        Assert.assertEquals(
                suggHelper.mModel.get(BaseSuggestionViewProperties.ACTION_CHIP_LEAD_IN_SPACING),
                OmniboxResourceProvider.getSuggestionDecorationIconSizeWidth(mContext));
        Assert.assertFalse(
                suggHelper.mModel.get(BaseSuggestionViewProperties.USE_LARGE_DECORATION));
        Assert.assertTrue(suggHelper.mModel.get(BaseSuggestionViewProperties.SHOW_DECORATION));
        Assert.assertEquals(0, suggHelper.mModel.get(BaseSuggestionViewProperties.TOP_PADDING));
        Assert.assertEquals(0, suggHelper.mModel.get(AnswerSuggestionViewProperties.RIGHT_PADDING));
        Assert.assertEquals(
                1, suggHelper.mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS).size());
    }

    @Test
    public void answerImage_calculatorIcon() {
        var suggHelper = createCalculationSuggestion("", "");
        Assert.assertEquals(R.drawable.ic_equals_sign_round, suggHelper.getIconRes());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SUGGESTION_ANSWERS_COLOR_REVERSE)
    public void checkColorReversalRequired_ReturnsFalseIfOmniBoxAnswerColorReversalDisabled() {
        mProcessor.onNativeInitialized();
        for (AnswerType type : ANSWER_TYPES) {
            Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
        }
    }

    @Test
    public void
            checkColorReversalRequired_ReturnsTrueIfOmniBoxAnswerColorReversalEnabledAndIncludedInCountryList() {
        mProcessor.onNativeInitialized();
        Locale.setDefault(new Locale("ja", "JP"));
        for (AnswerType type : ANSWER_TYPES) {
            if (type == AnswerType.ANSWER_TYPE_FINANCE) {
                Assert.assertTrue(mProcessor.checkColorReversalRequired(type));
            } else {
                Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
            }
        }
    }

    @Test
    public void
            checkColorReversalRequired_ReturnsFalseIfOmniBoxAnswerColorReversalEnabledAndNotIncludedInCountryList() {
        mProcessor.onNativeInitialized();
        Locale.setDefault(new Locale("en", "US"));
        for (AnswerType type : ANSWER_TYPES) {
            Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
        }
    }

    @Test
    public void doesProcessSuggestion_suggestionWithRichAnswer() {
        SuggestionTestHelper suggHelper =
                createRichAnswerSuggestion(AnswerType.ANSWER_TYPE_DICTIONARY, 1, false);
        Assert.assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_calculatorSuggestion() {
        SuggestionTestHelper suggHelper = createCalculationSuggestion("abcd", "efgh");
        Assert.assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_ignoreNonCalculatorSuggestionsWithNoAnswers() {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        Assert.assertFalse(mProcessor.doesProcessSuggestion(suggestion, 0));
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        Assert.assertEquals(OmniboxSuggestionUiType.ANSWER_SUGGESTION, mProcessor.getViewTypeId());
    }
}
