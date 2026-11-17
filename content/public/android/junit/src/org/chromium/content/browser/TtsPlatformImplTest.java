// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.Voice;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Locale;

/** Unit tests for {@link TtsPlatformImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TtsPlatformImplTest {
    private static final long NATIVE_TTS_PLATFORM_IMPL_ANDROID = 1L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TextToSpeech mTextToSpeech;

    private Locale mOriginalDefaultLocale;
    private TtsPlatformImpl mTtsPlatformImpl;

    @Before
    public void setUp() {
        mOriginalDefaultLocale = Locale.getDefault();
        when(mTextToSpeech.speak(anyString(), anyInt(), any(Bundle.class), anyString()))
                .thenReturn(TextToSpeech.SUCCESS);
        mTtsPlatformImpl = new TtsPlatformImpl(NATIVE_TTS_PLATFORM_IMPL_ANDROID, mTextToSpeech);
    }

    @After
    public void tearDown() {
        Locale.setDefault(mOriginalDefaultLocale);
    }

    @Test
    public void testSpeakEmptyLangPreservesEngineDefaultLocale() {
        assertTrue(mTtsPlatformImpl.speak(1, "Hello world", "", "", 1.0f, 1.0f, 1.0f));

        verify(mTextToSpeech, never()).setLanguage(any());
        verify(mTextToSpeech)
                .speak(eq("Hello world"), eq(TextToSpeech.QUEUE_FLUSH), any(Bundle.class), eq("1"));
    }

    @Test
    public void testSpeakExplicitLangSetsLanguageWithoutRedundantCalls() {
        assertTrue(mTtsPlatformImpl.speak(1, "Hola", "es_ES", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(Locale.forLanguageTag("es-ES"));
        verify(mTextToSpeech)
                .speak(eq("Hola"), eq(TextToSpeech.QUEUE_FLUSH), any(Bundle.class), eq("1"));

        // Speaking again with the same language must not redundantly call setLanguage.
        assertTrue(mTtsPlatformImpl.speak(2, "Adios", "es_ES", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(any());
        verify(mTextToSpeech)
                .speak(eq("Adios"), eq(TextToSpeech.QUEUE_FLUSH), any(Bundle.class), eq("2"));
    }

    @Test
    public void testSpeakEmptyLangAfterExplicitLangResetsToDefaultVoiceLocale() {
        Locale.setDefault(Locale.US);
        Voice defaultVoice = mock(Voice.class);
        when(defaultVoice.getLocale()).thenReturn(Locale.UK);
        when(mTextToSpeech.getDefaultVoice()).thenReturn(defaultVoice);

        assertTrue(mTtsPlatformImpl.speak(1, "Hola", "es-ES", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(Locale.forLanguageTag("es-ES"));

        // Transitioning back to an unset/empty lang resets to the engine's default voice locale.
        assertTrue(mTtsPlatformImpl.speak(2, "Hello", "", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(Locale.UK);
        verify(mTextToSpeech, times(2)).setLanguage(any());

        // Subsequent empty-lang utterances must not call setLanguage again.
        assertTrue(mTtsPlatformImpl.speak(3, "Hello again", "", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(2)).setLanguage(any());
        verify(mTextToSpeech)
                .speak(eq("Hello again"), eq(TextToSpeech.QUEUE_FLUSH), any(Bundle.class), eq("3"));
    }

    @Test
    public void testSpeakEmptyLangAfterExplicitLangFallsBackToSystemDefaultLocale() {
        Locale.setDefault(Locale.US);
        when(mTextToSpeech.getDefaultVoice()).thenReturn(null);

        assertTrue(mTtsPlatformImpl.speak(1, "Hola", "es-ES", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(Locale.forLanguageTag("es-ES"));

        // When getDefaultVoice() is null, falls back to Locale.getDefault().
        assertTrue(mTtsPlatformImpl.speak(2, "Hello", "", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(1)).setLanguage(Locale.US);
        verify(mTextToSpeech, times(2)).setLanguage(any());

        // Subsequent empty-lang utterances must not call setLanguage again.
        assertTrue(mTtsPlatformImpl.speak(3, "Hello again", "", "", 1.0f, 1.0f, 1.0f));
        verify(mTextToSpeech, times(2)).setLanguage(any());
        verify(mTextToSpeech)
                .speak(eq("Hello again"), eq(TextToSpeech.QUEUE_FLUSH), any(Bundle.class), eq("3"));
    }
}
