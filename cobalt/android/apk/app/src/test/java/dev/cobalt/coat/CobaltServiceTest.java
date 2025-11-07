// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
// limitations under the License.import static org.junit.Assert.assertEquals;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.junit.Assert.assertEquals;

import android.util.Base64;
import dev.cobalt.coat.CobaltActivity;
import dev.cobalt.coat.CobaltService;
import java.util.Locale;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;

/** CobaltServiceTest. */
@RunWith(RobolectricTestRunner.class)
public class CobaltServiceTest {
  @Rule public final MockitoRule mocks = MockitoJUnit.rule();
  @Mock private CobaltActivity mockCobaltActivity;
  @Captor private ArgumentCaptor<String> jsCodeCaptor;

  private TestCobaltService testService;

  // Minimal concrete implementation of CobaltService for testing purposes.
  private static class TestCobaltService extends CobaltService {
    @Override
    public void beforeStartOrResume() {}

    @Override
    public void beforeSuspend() {}

    @Override
    public void afterStopped() {}

    @Override
    public ResponseToClient receiveFromClient(byte[] data) {
      return null;
    }

    @Override
    public void close() {}

    public void callSendToClient(long nativeService, byte[] data) {
      sendToClient(nativeService, data);
    }
  }

  @Before
  public void setUp() {
    testService = new TestCobaltService();
    testService.setCobaltActivity(mockCobaltActivity);
  }

  @Test
  public void sendToClient_formatsJsCodeWithLocaleUS() {
    long testNativeService = 98765L;
    byte[] testData = "sample_payload".getBytes();
    String expectedBase64Data = Base64.encodeToString(testData, Base64.NO_WRAP);

    // Call the method under test.
    testService.callSendToClient(testNativeService, testData);

    // Verify that evaluateJavaScript was called on the mockCobaltActivity.
    verify(mockCobaltActivity).evaluateJavaScript(jsCodeCaptor.capture());

    // Capture the JavaScript code passed to evaluateJavaScript.
    String actualJsCode = jsCodeCaptor.getValue();

    String expectedJsCode = String.format(Locale.US, CobaltService.jsCodeTemplate, testNativeService, expectedBase64Data);

    // Assert that the captured JS code matches the expected code formatted with Locale.US.
    assertEquals(expectedJsCode, actualJsCode);
  }

  @Test
  public void sendToClient_usesLocaleUSRegardlessOfDefaultLocale() {
    // Save the current default locale.
    Locale originalLocale = Locale.getDefault();
    try {
      // Set the default locale to Arabic, which uses different number formatting.
      Locale.setDefault(new Locale("ar"));

      long testNativeService = 12345L;
      byte[] testData = "test_data".getBytes();
      String expectedBase64Data = Base64.encodeToString(testData, Base64.NO_WRAP);

      // Call the method under test.
      testService.callSendToClient(testNativeService, testData);

      // Verify and capture the argument.
      verify(mockCobaltActivity).evaluateJavaScript(jsCodeCaptor.capture());
      String actualJsCode = jsCodeCaptor.getValue();

      String expectedJsCode = String.format(Locale.US, CobaltService.jsCodeTemplate, testNativeService, expectedBase64Data);

      // Assert that the captured JS code matches the expected code formatted with Locale.US.
      // If Locale.US was NOT used in sendToClient, and the default was Arabic,
      // the number 12345 would likely be formatted differently (e.g., with Arabic numerals),
      // causing this assertion to fail.
      assertEquals(expectedJsCode, actualJsCode);
    } finally {
      // Restore the original default locale to avoid affecting other tests.
      Locale.setDefault(originalLocale);
    }
  }

  @Test
  public void sendToClient_doesNotCallEvaluateJavaScriptIfActivityIsNull() {
    TestCobaltService serviceWithNullActivity = new TestCobaltService();
    // We don't call setCobaltActivity, so cobaltActivity remains null.

    // Calling sendToClient should not cause a crash.
    serviceWithNullActivity.callSendToClient(123L, new byte[] {1, 2, 3});

    // Verify that evaluateJavaScript was never called on the mock.
    verifyNoInteractions(mockCobaltActivity);
  }
}
