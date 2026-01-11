// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
// limitations under the License.

package dev.cobalt.coat;

import static org.junit.Assert.assertArrayEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.KeyEvent;
import java.util.ArrayList;
import java.util.List;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

/** CobaltActivityTest. */
@RunWith(RobolectricTestRunner.class)
public class CobaltActivityTest {
  public ImeAdapterImpl createImeAdapterImplForRemapTests() {
    ImeAdapterImpl mockImeAdapterImpl = mock(ImeAdapterImpl.class);
    when(mockImeAdapterImpl.dispatchKeyEvent(any())).thenReturn(true);
    return mockImeAdapterImpl;
  }

  public CobaltActivity createActivityForRemapTests(ImeAdapterImpl imeAdapterImpl) {
    CobaltActivity cobaltActivity =
        new CobaltActivity() {
          @Override
          protected ImeAdapterImpl getImeAdapterImpl() {
            return imeAdapterImpl;
          }

          @Override
          protected StarboardBridge createStarboardBridge(String[] args, String startDeepLink) {
            return null;
          }
        };
    return cobaltActivity;
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_NullMetaData() {
    String[] args = new String[] {"--arg1"};
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(/*metaData=*/null, args);
    assertArrayEquals(args, result);
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_EmptyMetaData() {
    Bundle metaData = mock(Bundle.class);
    when(metaData.getString("cobalt.ENABLE_FEATURES")).thenReturn(null);
    String[] args = new String[] {"--arg1"};
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(metaData, args);
    assertArrayEquals(args, result);
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_EmptyStringFeature() {
    Bundle metaData = mock(Bundle.class);
    when(metaData.getString("cobalt.ENABLE_FEATURES")).thenReturn("");
    String[] args = new String[] {"--arg1"};
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(metaData, args);
    assertArrayEquals(args, result);
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_WithFeature() {
    Bundle metaData = mock(Bundle.class);
    when(metaData.getString("cobalt.ENABLE_FEATURES")).thenReturn("FeatureA");
    String[] args = new String[] {"--arg1"};
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(metaData, args);
    assertArrayEquals(new String[] {"--arg1", "--enable-features=FeatureA"}, result);
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_WithMultipleFeatures() {
    Bundle metaData = mock(Bundle.class);
    when(metaData.getString("cobalt.ENABLE_FEATURES")).thenReturn("FeatureA;FeatureB");
    String[] args = new String[] {"--arg1"};
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(metaData, args);
    assertArrayEquals(new String[] {"--arg1", "--enable-features=FeatureA;FeatureB"}, result);
  }

  @Test
  public void testAppendEnableFeaturesIfNecessary_NullArgs() {
    Bundle metaData = mock(Bundle.class);
    when(metaData.getString("cobalt.ENABLE_FEATURES")).thenReturn("FeatureA");
    String[] result = CobaltActivity.appendEnableFeaturesIfNecessary(metaData, /*commandLineArgs=*/null);
    assertArrayEquals(new String[] {"--enable-features=FeatureA"}, result);
  }

  @Test
  public void testAppendUrlParamsToUrl_NoUrlArg() {
    List<String> args = new ArrayList<>();
    args.add("--other-arg");
    CharSequence[] urlParams = new CharSequence[] {"p1=v1"};
    CobaltActivity.appendUrlParamsToUrl(args, urlParams);
    assertArrayEquals(new String[] {"--other-arg"}, args.toArray());
  }

  @Test
  public void testAppendUrlParamsToUrl_WithUrlArg_NoParams() {
    List<String> args = new ArrayList<>();
    args.add("--url=https://example.com");
    CharSequence[] urlParams = new CharSequence[] {"p1=v1"};
    CobaltActivity.appendUrlParamsToUrl(args, urlParams);
    assertArrayEquals(new String[] {"--url=https://example.com?p1=v1"}, args.toArray());
  }

  @Test
  public void testAppendUrlParamsToUrl_WithUrlArg_AlreadyHasParams() {
    List<String> args = new ArrayList<>();
    args.add("--url=https://example.com?existing=1");
    CharSequence[] urlParams = new CharSequence[] {"p1=v1", "p2=v2"};
    CobaltActivity.appendUrlParamsToUrl(args, urlParams);
    assertArrayEquals(new String[] {"--url=https://example.com?existing=1&p1=v1&p2=v2"}, args.toArray());
  }

  @Test
  public void testAppendUrlParamsToUrl_Sanitization() {
    List<String> args = new ArrayList<>();
    args.add("--url=https://example.com");
    CharSequence[] urlParams = new CharSequence[] {"p1=v1", "invalid!param", "p2=v2"};
    CobaltActivity.appendUrlParamsToUrl(args, urlParams);
    assertArrayEquals(new String[] {"--url=https://example.com?p1=v1&p2=v2"}, args.toArray());
  }

  @Test
  public void testGetArgs_ManifestUrlOnly() {
    Bundle metaData = new Bundle();
    metaData.putString("cobalt.APP_URL", "https://manifest.com");
    String[] result = CobaltActivity.constructArgs(/*commandLineArgs=*/null, metaData, /*extras=*/null);
    assertArrayEquals(new String[] {"--url=https://manifest.com"}, result);
  }

  @Test
  public void testGetArgs_IntentArgsOverrideManifest() {
    String[] commandLineArgs = new String[] {"--url=https://intent.com"};
    Bundle metaData = new Bundle();
    metaData.putString("cobalt.APP_URL", "https://manifest.com");
    String[] result = CobaltActivity.constructArgs(commandLineArgs, metaData, /*extras=*/null);
    // Intent args take precedence, and since URL is already present, manifest URL is ignored.
    assertArrayEquals(new String[] {"--url=https://intent.com"}, result);
  }

  @Test
  public void testGetArgs_WithUrlParams() {
    String[] commandLineArgs = new String[] {"--url=https://intent.com"};
    Bundle extras = new Bundle();
    extras.putCharSequenceArray("url_params", new CharSequence[] {"p1=v1"});
    Bundle metaData = new Bundle();
    String[] result = CobaltActivity.constructArgs(commandLineArgs, metaData, extras);
    assertArrayEquals(new String[] {"--url=https://intent.com?p1=v1"}, result);
  }

  @Test
  public void testGetArgs_NullMetaData_DoesNotThrow() {
    String[] result = CobaltActivity.constructArgs(/*commandLineArgs=*/null, /*metaData=*/null, /*extras=*/null);
    assertArrayEquals(new String[0], result);
  }

  @Test
  public void testGetArgs_NoUrlInIntentOrManifest_DoesNotThrow() {
    Bundle metaData = new Bundle(); // No cobalt.APP_URL
    String[] result = CobaltActivity.constructArgs(/*commandLineArgs=*/null, metaData, /*extras=*/null);
    // Should return empty array if no URL found and no other args
    assertArrayEquals(new String[0], result);
  }

  @Test
  @Ignore("Test failing - b/429212231")
  public void testOnKeyDownDispatchEscapeWhenKeyCodeBack() {
    ImeAdapterImpl mockImeAdapterImpl = createImeAdapterImplForRemapTests();
    CobaltActivity cobaltActivity = createActivityForRemapTests(mockImeAdapterImpl);

    cobaltActivity.onKeyDown(
        KeyEvent.KEYCODE_BACK, new KeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_DOWN));
    verify(mockImeAdapterImpl, times(1))
        .dispatchKeyEvent(
            argThat(
                e -> {
                  return e.getKeyCode() == KeyEvent.KEYCODE_ESCAPE
                      && e.getAction() == KeyEvent.ACTION_DOWN;
                }));
  }

  @Test
  @Ignore("Test failing - b/429212231")
  public void testOnKeyUpDispatchEscapeWhenKeyCodeBack() {
    ImeAdapterImpl mockImeAdapterImpl = createImeAdapterImplForRemapTests();
    CobaltActivity cobaltActivity = createActivityForRemapTests(mockImeAdapterImpl);

    cobaltActivity.onKeyUp(
        KeyEvent.KEYCODE_BACK, new KeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_UP));
    verify(mockImeAdapterImpl, times(1))
        .dispatchKeyEvent(
            argThat(
                e -> {
                  return e.getKeyCode() == KeyEvent.KEYCODE_ESCAPE
                      && e.getAction() == KeyEvent.ACTION_UP;
                }));
  }

  @Test
  @Ignore("Test failing - b/429212231")
  public void testOnKeyDownSuperMethodCalledWhenKeyCodeA() {
    ImeAdapterImpl mockImeAdapterImpl = createImeAdapterImplForRemapTests();
    CobaltActivity cobaltActivity = createActivityForRemapTests(mockImeAdapterImpl);

    try {
      cobaltActivity.onKeyDown(
          KeyEvent.KEYCODE_A, new KeyEvent(KeyEvent.KEYCODE_A, KeyEvent.ACTION_DOWN));
    } catch (NullPointerException e) {
      // Expected when calling super method.
    }
    verify(mockImeAdapterImpl, never()).dispatchKeyEvent(any());
  }

  @Test
  @Ignore("Test failing - b/429212231")
  public void testOnKeyUpSuperMethodCalledWhenKeyCodeA() {
    ImeAdapterImpl mockImeAdapterImpl = createImeAdapterImplForRemapTests();
    CobaltActivity cobaltActivity = createActivityForRemapTests(mockImeAdapterImpl);

    try {
      cobaltActivity.onKeyUp(
          KeyEvent.KEYCODE_A, new KeyEvent(KeyEvent.KEYCODE_A, KeyEvent.ACTION_UP));
    } catch (NullPointerException e) {
      // Expected when calling super method.
    }
    verify(mockImeAdapterImpl, never()).dispatchKeyEvent(any());
  }
}
