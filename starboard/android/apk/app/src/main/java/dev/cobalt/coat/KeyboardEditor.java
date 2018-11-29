// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

/**
 * The custom editor that receives text and displays it for the on-screen keyboard. It interacts
 * with the Input Method Engine (IME) by receiving commands through the InputConnection interface
 * and sending commands through InputMethodManager.
 */
public class KeyboardEditor extends View {
  private final Context context;

  public KeyboardEditor(Context context) {
    this(context, null);
  }

  public KeyboardEditor(Context context, AttributeSet attrs) {
    super(context, attrs);
    this.context = context;
    setFocusable(true);
  }

  /** Show the on-screen keyboard. */
  public void showKeyboard() {
    // Stub.
  }

  /** Hide the on-screen keyboard. */
  public void hideKeyboard() {
    // Stub.
  }
}
