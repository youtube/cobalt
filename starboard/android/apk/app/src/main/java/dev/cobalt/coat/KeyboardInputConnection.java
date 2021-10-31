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
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;

/** The communication channel between the on-screen keyboard InputMethod and the KeyboardEditor. */
public class KeyboardInputConnection extends BaseInputConnection {
  private final KeyboardEditor keyboardEditor;
  private final Context context;
  private int numNestedBatchEdits = 0;

  public KeyboardInputConnection(
      Context context, KeyboardEditor keyboardEditor, EditorInfo outAttrs) {
    super(keyboardEditor, true);
    this.context = context;
    this.keyboardEditor = keyboardEditor;
  }

  /**
   * Start a batch edit, indicating to the editor that a batch of editor operations is occurring.
   * The editor will avoid sending updates about its state until endBatchEdit() is called.
   */
  @Override
  public boolean beginBatchEdit() {
    numNestedBatchEdits++;
    return super.beginBatchEdit();
  }

  /**
   * End a batch edit, indicating to the editor that a batch edit previously initiated with
   * beginBatchEdit() is done. This ends the latest batch only.
   */
  @Override
  public boolean endBatchEdit() {
    boolean result = super.endBatchEdit();
    numNestedBatchEdits--;
    updateEditingState();
    return result;
  }

  /** Replace the currently composing text with the given text, and set the new cursor position. */
  @Override
  public boolean setComposingText(CharSequence text, int newCursorPosition) {
    boolean result;
    if (text.length() == 0) {
      result = super.commitText(text, newCursorPosition);
    } else {
      result = super.setComposingText(text, newCursorPosition);
    }

    updateEditingState();
    Editable editable = getEditable();
    return result;
  }

  /** Remove the composing state from the editable text. */
  @Override
  public boolean finishComposingText() {
    boolean result = super.finishComposingText();
    updateEditingState();
    return result;
  }

  /** Change the selection position in the current editable text. */
  @Override
  public boolean setSelection(int start, int end) {
    boolean result = super.setSelection(start, end);
    updateEditingState();
    return result;
  }

  /** Send the current state of the editable to the editor. */
  private void updateEditingState() {
    if (numNestedBatchEdits > 0) {
      // The IME is in the middle of a batch edit; wait until it finishes.
      return;
    }

    Editable editable = getEditable();
    int selectionStart = Selection.getSelectionStart(editable);
    int selectionEnd = Selection.getSelectionEnd(editable);
    int composingStart = BaseInputConnection.getComposingSpanStart(editable);
    int composingEnd = BaseInputConnection.getComposingSpanEnd(editable);
    keyboardEditor.updateSelection(
        keyboardEditor, selectionStart, selectionEnd, composingStart, composingEnd);

    if (composingStart != -1) {
      // Send the composing text as an input event with isComposing set to true.
      nativeSendText(editable.toString().substring(composingStart, composingEnd), true);
    } else {
      // Send the committed text as an input event with isComposing set to false.
      nativeSendText(editable.toString(), false);
    }
  }

  /** Send text to the search bar and set the new cursor position. */
  @Override
  public boolean commitText(CharSequence newText, int newCursorPosition) {
    if (TextUtils.isEmpty(newText)) {
      return false;
    }
    boolean result = super.commitText(newText, newCursorPosition);
    updateEditingState();
    return result;
  }

  /** Delete around the current selection position of the editable text. */
  @Override
  public boolean deleteSurroundingText(int leftLength, int rightLength) {
    Editable editable = getEditable();
    if (Selection.getSelectionStart(editable) == -1) {
      return true;
    }

    boolean result = super.deleteSurroundingText(leftLength, rightLength);
    updateEditingState();
    return result;
  }

  /** Send a key event to the editor. */
  @Override
  public boolean sendKeyEvent(KeyEvent event) {
    if (event.getAction() == KeyEvent.ACTION_DOWN) {
      if (event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
        keyboardEditor.search();
      } else if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
        Editable editable = getEditable();
        int selStart = Selection.getSelectionStart(editable);
        int selEnd = Selection.getSelectionEnd(editable);
        if (selEnd > selStart) {
          // Delete the selection.
          Selection.setSelection(editable, selStart);
          editable.delete(selStart, selEnd);
          updateEditingState();
          return true;
        } else if (selStart > 0) {
          // Delete to the left of the cursor.
          int newSel = Math.max(selStart - 1, 0);
          Selection.setSelection(editable, newSel);
          editable.delete(newSel, selStart);
          updateEditingState();
          return true;
        }
      }
    }
    return false;
  }

  /** Have the editor perform an action associated with a specific key press. */
  @Override
  public boolean performEditorAction(int editorAction) {
    if (editorAction == EditorInfo.IME_ACTION_SEARCH) {
      keyboardEditor.search();
    }
    return true;
  }

  public static native boolean nativeHasOnScreenKeyboard();

  public static native void nativeSendText(CharSequence text, boolean isComposing);
}
