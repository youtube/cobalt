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
  @Mock private CobaltService.Natives mockNatives;

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
    CobaltService.setNativesForTesting(mockNatives);
  }

  @Test
  public void sendToClient_callsNativeSendToClient() {
    long testNativeService = 98765L;
    byte[] testData = "sample_payload".getBytes();

    testService.callSendToClient(testNativeService, testData);

    verify(mockNatives).nativeSendToClient(testNativeService, testData);
  }
}
