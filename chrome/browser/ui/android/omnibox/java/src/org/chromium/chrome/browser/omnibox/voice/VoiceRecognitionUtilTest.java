// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.speech.RecognizerIntent;
import android.test.mock.MockPackageManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/** Unit Test for {@link VoiceRecognitionUtil}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class VoiceRecognitionUtilTest {
    private IntentTestMockContext mContextWithSpeech;
    private IntentTestMockContext mContextWithoutSpeech;

    public VoiceRecognitionUtilTest() {
        mContextWithSpeech = new IntentTestMockContext(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);

        mContextWithoutSpeech = new IntentTestMockContext(RecognizerIntent.ACTION_WEB_SEARCH);
    }

    private static class IntentTestPackageManager extends MockPackageManager {
        private final String mAction;

        public IntentTestPackageManager(String recognizesAction) {
            super();
            mAction = recognizesAction;
        }

        @Override
        public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
            List<ResolveInfo> resolveInfoList = new ArrayList<ResolveInfo>();

            if (intent.getAction().equals(mAction)) {
                // Add an entry to the returned list as the action
                // being queried exists.
                ResolveInfo resolveInfo = new ResolveInfo();
                resolveInfoList.add(resolveInfo);
            }

            return resolveInfoList;
        }
    }

    private static class IntentTestMockContext extends AdvancedMockContext {
        private final String mAction;

        public IntentTestMockContext(String recognizesAction) {
            super(ApplicationProvider.getApplicationContext());
            mAction = recognizesAction;
        }

        @Override
        public IntentTestPackageManager getPackageManager() {
            return new IntentTestPackageManager(mAction);
        }
    }

    private static boolean isRecognitionIntentPresent(final boolean useCachedResult) {
        // Context can only be queried on a UI Thread.
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> VoiceRecognitionUtil.isRecognitionIntentPresent(useCachedResult));
    }

    @Test
    @SmallTest
    @Feature({"Flags", "Speech"})
    public void testSpeechFeatureAvailable() {
        ContextUtils.initApplicationContextForTests(mContextWithSpeech);
        final boolean doNotUseCachedResult = false;
        final boolean recognizesSpeech = isRecognitionIntentPresent(doNotUseCachedResult);

        Assert.assertTrue(recognizesSpeech);
    }

    @Test
    @SmallTest
    @Feature({"Flags", "Speech"})
    public void testSpeechFeatureUnavailable() {
        ContextUtils.initApplicationContextForTests(mContextWithoutSpeech);
        final boolean doNotUseCachedResult = false;
        final boolean recognizesSpeech = isRecognitionIntentPresent(doNotUseCachedResult);

        Assert.assertFalse(recognizesSpeech);
    }

    @Test
    @SmallTest
    @Feature({"Flags", "Speech"})
    public void testCachedSpeechFeatureAvailability() {
        ContextUtils.initApplicationContextForTests(mContextWithSpeech);
        // Initial call will cache the fact that speech is recognized.
        final boolean doNotUseCachedResult = false;
        isRecognitionIntentPresent(doNotUseCachedResult);

        ContextUtils.initApplicationContextForTests(mContextWithoutSpeech);
        // Pass a context that does not recognize speech, but use cached result
        // which does recognize speech.
        final boolean useCachedResult = true;
        final boolean recognizesSpeech = isRecognitionIntentPresent(useCachedResult);

        // Check that we still recognize speech as we're using cached result.
        Assert.assertTrue(recognizesSpeech);

        // Check if we can turn cached result off again.
        final boolean RecognizesSpeechUncached = isRecognitionIntentPresent(doNotUseCachedResult);

        Assert.assertFalse(RecognizesSpeechUncached);
    }
}
