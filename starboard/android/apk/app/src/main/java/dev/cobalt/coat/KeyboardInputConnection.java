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
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;

/** The communication channel between the on-screen keyboard InputMethod and the KeyboardEditor. */
public class KeyboardInputConnection extends BaseInputConnection {
  private final KeyboardEditor keyboardEditor;
  private final Context context;

  public KeyboardInputConnection(
      Context context, KeyboardEditor keyboardEditor, EditorInfo outAttrs) {
    super(keyboardEditor, true);
    this.context = context;
    this.keyboardEditor = keyboardEditor;
  }
}
