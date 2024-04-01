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

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.util.AttributeSet;
import android.view.View;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

/**
 * The custom editor that receives text and displays it for the on-screen keyboard. It interacts
 * with the Input Method Engine (IME) by receiving commands through the InputConnection interface
 * and sending commands through InputMethodManager.
 */
public class KeyboardEditor extends View {
  private final Context context;
  private Editable editable;
  private KeyboardInputConnection inputConnection;
  private boolean keepFocus;
  private boolean keyboardShowing;

  public KeyboardEditor(Context context) {
    this(context, null);
  }

  public KeyboardEditor(Context context, AttributeSet attrs) {
    super(context, attrs);
    this.context = context;
    this.keepFocus = false;
    this.keyboardShowing = false;
    setFocusable(true);
  }

  /**
   * Create a new InputConnection for the on-screen keyboard InputMethod to interact with the view.
   */
  @Override
  public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
    outAttrs.inputType = EditorInfo.TYPE_CLASS_TEXT;
    outAttrs.inputType |= EditorInfo.TYPE_TEXT_FLAG_AUTO_COMPLETE;
    outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN;
    outAttrs.imeOptions |= EditorInfo.IME_ACTION_SEARCH;
    outAttrs.initialSelStart = Selection.getSelectionStart(editable);
    outAttrs.initialSelEnd = Selection.getSelectionEnd(editable);

    this.inputConnection = new KeyboardInputConnection(context, this, outAttrs);
    return inputConnection;
  }

  /** Update the keepFocus boolean. */
  public void updateKeepFocus(boolean keepFocus) {
    this.keepFocus = keepFocus;
  }

  /** If the on-screen keyboard is showing. */
  public boolean isKeyboardShowing() {
    return this.keyboardShowing;
  }

  /** Hide the on-screen keyboard if keepFocus is set to false. */
  public void search() {
    if (!this.keepFocus) {
      hideKeyboard();
    }
  }

  /** Show the on-screen keyboard. */
  public void showKeyboard() {
    final Activity activity = (Activity) context;
    final KeyboardEditor view = this;

    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            view.setFocusable(true);
            view.requestFocus();

            InputMethodManager imm =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
            boolean success = imm.showSoftInput(view, 0);
            if (success) {
              view.updateKeyboardShowing(true);
            }
          }
        });
  }

  /** Hide the on-screen keyboard. */
  public void hideKeyboard() {
    final Activity activity = (Activity) context;
    final KeyboardEditor view = this;

    activity.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            view.setFocusable(true);
            view.requestFocus();

            InputMethodManager imm =
                (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
            boolean success = imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            if (success) {
              view.updateKeyboardShowing(false);
            }
          }
        });
  }

  /** Send the current state of the editable to the Input Method Manager. */
  public void updateSelection(View view, int selStart, int selEnd, int compStart, int compEnd) {
    InputMethodManager imm =
        (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    imm.updateSelection(view, selStart, selEnd, compStart, compEnd);
  }

  /** Update the custom list of completions shown within the on-screen keyboard. */
  public void updateCustomCompletions(CompletionInfo[] completions) {
    InputMethodManager imm =
        (InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE);
    imm.displayCompletions(this, completions);
  }

  private void updateKeyboardShowing(boolean keyboardShowing) {
    this.keyboardShowing = keyboardShowing;
  }
}
