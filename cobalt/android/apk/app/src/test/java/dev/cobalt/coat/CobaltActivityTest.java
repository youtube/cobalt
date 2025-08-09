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

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;
import java.util.Optional;
import org.chromium.content.browser.input.ImeAdapterImpl;
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
  public void testRemapBackToEscape() {
    Optional<KeyEvent> event;

    event = CobaltActivity.getRemappedKeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_DOWN);
    assertThat(event.isPresent()).isTrue();
    event.ifPresent(
        e -> {
          assertThat(e.getKeyCode()).isEqualTo(KeyEvent.KEYCODE_ESCAPE);
          assertThat(e.getAction()).isEqualTo(KeyEvent.ACTION_DOWN);
          assertThat(e.getDownTime()).isNotEqualTo(0);
          assertThat(e.getEventTime()).isNotEqualTo(0);
        });

    event = CobaltActivity.getRemappedKeyEvent(KeyEvent.KEYCODE_BACK, KeyEvent.ACTION_UP);
    assertThat(event.isPresent()).isTrue();
    event.ifPresent(
        e -> {
          assertThat(e.getKeyCode()).isEqualTo(KeyEvent.KEYCODE_ESCAPE);
          assertThat(e.getAction()).isEqualTo(KeyEvent.ACTION_UP);
          assertThat(e.getDownTime()).isNotEqualTo(0);
          assertThat(e.getEventTime()).isNotEqualTo(0);
        });

    event = CobaltActivity.getRemappedKeyEvent(KeyEvent.KEYCODE_A, KeyEvent.ACTION_DOWN);
    assertThat(event.isPresent()).isFalse();
  }

  @Test
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
