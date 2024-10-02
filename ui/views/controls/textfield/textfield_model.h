// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_MODEL_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_MODEL_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ime/composition_text.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/views_export.h"

namespace views {

namespace internal {
// Internal Edit class that keeps track of edits for undo/redo.
class Edit;

// The types of merge behavior implemented by Edit operations.
enum class MergeType {
  // The edit should not usually be merged with next edit.
  kDoNotMerge,
  // The edit should be merged with next edit when possible.
  kMergeable,
  // The edit should be merged with the prior edit, even if marked kDoNotMerge.
  kForceMerge,
};

}  // namespace internal

namespace test {
class BridgedNativeWidgetTest;
class TextfieldTest;
}  // namespace test

// A model that represents text content for a views::Textfield.
// It supports editing, selection and cursor manipulation.
class VIEWS_EXPORT TextfieldModel {
 public:
  // Delegate interface implemented by the textfield view class to provide
  // additional functionalities required by the model.
  class VIEWS_EXPORT Delegate {
   public:
    // Called when the current composition text is confirmed or cleared.
    virtual void OnCompositionTextConfirmedOrCleared() = 0;

    // Called any time that the text property is modified in TextfieldModel
    virtual void OnTextChanged() {}

   protected:
    virtual ~Delegate();
  };

  explicit TextfieldModel(Delegate* delegate);

  TextfieldModel(const TextfieldModel&) = delete;
  TextfieldModel& operator=(const TextfieldModel&) = delete;

  virtual ~TextfieldModel();

  // Edit related methods.

  const std::u16string& text() const { return render_text_->text(); }
  // Sets the text. Returns true if the text was modified. The current
  // composition text will be confirmed first. Setting the same text, even with
  // an updated |cursor_position|, will neither add edit history nor change the
  // cursor because it's neither a user visible change nor user-initiated
  // change. This allows clients to set the same text multiple times without
  // messing up edit history. The resulting history edit will have
  // |new_cursor_pos| set to |cursor_position|. This is important even if
  // subsequent calls will override the cursor position because updating the
  // cursor alone won't update the edit history. I.e. the cursor position after
  // applying or redoing the edit will be determined by |cursor_position|.
  bool SetText(const std::u16string& new_text, size_t cursor_position);

  gfx::RenderText* render_text() { return render_text_.get(); }

  // Inserts given |new_text| at the current cursor position.
  // The current composition text will be cleared.
  void InsertText(const std::u16string& new_text) {
    InsertTextInternal(new_text, false);
  }

  // Inserts a character at the current cursor position.
  void InsertChar(char16_t c) {
    InsertTextInternal(std::u16string(&c, 1), true);
  }

  // Replaces characters at the current position with characters in given text.
  // The current composition text will be cleared.
  void ReplaceText(const std::u16string& new_text) {
    ReplaceTextInternal(new_text, false);
  }

  // Replaces the char at the current position with given character.
  void ReplaceChar(char16_t c) {
    ReplaceTextInternal(std::u16string(&c, 1), true);
  }

  // Appends the text.
  // The current composition text will be confirmed.
  void Append(const std::u16string& new_text);

  // Deletes the first character after the current cursor position (as if, the
  // the user has pressed delete key in the textfield). Returns true if
  // the deletion is successful. If |add_to_kill_buffer| is true, the deleted
  // text is copied to the kill buffer.
  // If there is composition text, it'll be deleted instead.
  bool Delete(bool add_to_kill_buffer = false);

  // Deletes the first character before the current cursor position (as if, the
  // the user has pressed backspace key in the textfield). Returns true if
  // the removal is successful. If |add_to_kill_buffer| is true, the deleted
  // text is copied to the kill buffer.
  // If there is composition text, it'll be deleted instead.
  bool Backspace(bool add_to_kill_buffer = false);

  // Cursor related methods.

  // Returns the current cursor position.
  size_t GetCursorPosition() const;

  // Moves the cursor, see RenderText for additional details.
  // The current composition text will be confirmed.
  void MoveCursor(gfx::BreakType break_type,
                  gfx::VisualCursorDirection direction,
                  gfx::SelectionBehavior selection_behavior);

  // Updates the cursor to the specified selection model. Any composition text
  // will be confirmed, which may alter the specified selection range start.
  bool MoveCursorTo(const gfx::SelectionModel& cursor);

  // Sugar for MoveCursorTo({0, CURSOR_FORWARD}).
  bool MoveCursorTo(size_t pos);

  // Calls the corresponding function on the associated RenderText instance. Any
  // composition text will be confirmed.
  bool MoveCursorTo(const gfx::Point& point, bool select);

  // Selection related methods.

  // Returns the primary selected text associated with the cursor. Does not
  // return secondary selections.
  std::u16string GetSelectedText() const;

  // The current composition text will be confirmed. If |primary| is true, the
  // selection starts with the range's start position and ends with the range's
  // end position; therefore the cursor position becomes the end position. If
  // |primary| is false, then the selection is not associated with the cursor.
  void SelectRange(const gfx::Range& range, bool primary = true);

  // The current composition text will be confirmed.
  // render_text_'s selection model is set to |sel|.
  void SelectSelectionModel(const gfx::SelectionModel& sel);

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  // The current composition text will be confirmed.
  void SelectAll(bool reversed);

  // Selects the word at which the cursor is currently positioned. If there is a
  // non-empty selection, the selection bounds are extended to their nearest
  // word boundaries.
  // The current composition text will be confirmed.
  void SelectWord();

  // Clears selection.
  // The current composition text will be confirmed.
  void ClearSelection();

  // Returns true if there is an undoable edit.
  bool CanUndo();

  // Returns true if there is an redoable edit.
  bool CanRedo();

  // Undo edit. Returns true if undo changed the text.
  bool Undo();

  // Redo edit. Returns true if redo changed the text.
  bool Redo();

  // Cuts the currently selected text and puts it to clipboard. Returns true
  // if text has changed after cutting.
  bool Cut();

  // Copies the currently selected text and puts it to clipboard. Returns true
  // if something was copied to the clipboard.
  bool Copy();

  // Pastes text from the clipboard at current cursor position. Returns true
  // if any text is pasted.
  bool Paste();

  // Transposes the characters to either side of the insertion point and
  // advances the insertion point past both of them. Returns true if text is
  // changed.
  bool Transpose();

  // Pastes text from the kill buffer at the current cursor position. Returns
  // true if the text has changed after yanking.
  bool Yank();

  // Tells if any text is selected, even if the selection is in composition
  // text. |primary_only| indicates whether secondary selections should also be
  // considered.
  bool HasSelection(bool primary_only = false) const;

  // Deletes the selected text. This method shouldn't be called with
  // composition text.
  void DeleteSelection();

  // Deletes the selected text (if any) and insert text at given position.
  void DeletePrimarySelectionAndInsertTextAt(const std::u16string& new_text,
                                             size_t position);

  // Retrieves the text content in a given range.
  std::u16string GetTextFromRange(const gfx::Range& range) const;

  // Retrieves the range containing all text in the model.
  void GetTextRange(gfx::Range* range) const;

  // Sets composition text and attributes. If there is composition text already,
  // it'll be replaced by the new one. Otherwise, current selection will be
  // replaced. If there is no selection, the composition text will be inserted
  // at the insertion point.
  // Any changes to the model except text insertion will confirm the current
  // composition text.
  void SetCompositionText(const ui::CompositionText& composition);

#if BUILDFLAG(IS_CHROMEOS)
  // Return the text range corresponding to the autocorrected text.
  const gfx::Range& autocorrect_range() const { return autocorrect_range_; }

  // Sets the autocorrect range to |range|. If |range| is empty, then the
  // autocorrect range is cleared. Returns true if the range was set or cleared
  // successfully.
  bool SetAutocorrectRange(const gfx::Range& range);
#endif

  // Puts the text in the specified range into composition mode.
  // This method should not be called with composition text or an invalid range.
  // The provided range is checked against the string's length, if |range| is
  // out of bounds, the composition will be cleared.
  void SetCompositionFromExistingText(const gfx::Range& range);

  // Converts current composition text into final content and returns the
  // length of the text committed.
  size_t ConfirmCompositionText();

  // Removes current composition text.
  void CancelCompositionText();

  // Retrieves the range of current composition text.
  void GetCompositionTextRange(gfx::Range* range) const;

  // Returns true if there is composition text.
  bool HasCompositionText() const;

  // Clears all edit history.
  void ClearEditHistory();

 private:
  friend class internal::Edit;
  friend class test::BridgedNativeWidgetTest;
  friend class TextfieldModelTest;
  friend class test::TextfieldTest;

  FRIEND_TEST_ALL_PREFIXES(TextfieldModelTest, UndoRedo_BasicTest);
  FRIEND_TEST_ALL_PREFIXES(TextfieldModelTest, UndoRedo_CutCopyPasteTest);
  FRIEND_TEST_ALL_PREFIXES(TextfieldModelTest, UndoRedo_ReplaceTest);

  // Insert the given |new_text| at the cursor. |mergeable| indicates if this
  // operation can be merged with previous edits in the history. Will delete any
  // selected text.
  void InsertTextInternal(const std::u16string& new_text, bool mergeable);

  // Replace the current selected text with the given |new_text|. |mergeable|
  // indicates if this operation can be merged with previous edits in the
  // history.
  void ReplaceTextInternal(const std::u16string& new_text, bool mergeable);

  // Clears redo history.
  void ClearRedoHistory();

  // Executes and records edit operations.
  void ExecuteAndRecordDelete(std::vector<gfx::Range> ranges, bool mergeable);
  void ExecuteAndRecordReplaceSelection(internal::MergeType merge_type,
                                        const std::u16string& new_text);
  void ExecuteAndRecordReplace(internal::MergeType merge_type,
                               std::vector<gfx::Range> replacement_range,
                               size_t new_cursor_pos,
                               const std::u16string& new_text,
                               size_t new_text_start);
  void ExecuteAndRecordInsert(const std::u16string& new_text, bool mergeable);

  // Adds or merges |edit| into the edit history.
  void AddOrMergeEditHistory(std::unique_ptr<internal::Edit> edit);

  // Modify the text buffer in following way:
  // 1) Delete the |deletions|.
  // 2) Insert the |insertion_texts| at the |insertion_positions|.
  // 3) Select |primary_selection| and |secondary_selections|.
  // Deletions and insertions are applied in order and affect later edit
  // indices. E.g., given 'xyz', inserting 'A' at 1 and 'B' at 2 will result in
  // 'xAByz', not 'xAyBz'. On the other hand, inserting 'B' at 2 then 'A' at 1
  // will result in 'xAyBz'. Thus, for applying or redoing edits, they should be
  // ordered with increasing indices; while for undoing edits, they should be
  // ordered decreasing.
  void ModifyText(const std::vector<gfx::Range>& deletions,
                  const std::vector<std::u16string>& insertion_texts,
                  const std::vector<size_t>& insertion_positions,
                  const gfx::Range& primary_selection,
                  const std::vector<gfx::Range>& secondary_selections);

  // Calls render_text->SetText() and delegate's callback.
  void SetRenderTextText(const std::u16string& text);

  void ClearComposition();

  // Clears the kill buffer. Used to clear global state between tests.
  static void ClearKillBuffer();

  // The TextfieldModel::Delegate instance should be provided by the owner.
  raw_ptr<Delegate> delegate_;

  // The stylized text, cursor, selection, and the visual layout model.
  std::unique_ptr<gfx::RenderText> render_text_;

  gfx::Range composition_range_;

#if BUILDFLAG(IS_CHROMEOS)
  gfx::Range autocorrect_range_;
#endif

  // The list of Edits. The oldest Edits are at the front of the list, and the
  // newest ones are at the back of the list.
  using EditHistory = std::list<std::unique_ptr<internal::Edit>>;
  EditHistory edit_history_;

  // An iterator that points to the current edit that can be undone.
  // This iterator moves from the |end()|, meaning no edit to undo,
  // to the last element (one before |end()|), meaning no edit to redo.
  //
  // There is no edit to undo (== end()) when:
  //   1) in initial state. (nothing to undo)
  //   2) very 1st edit is undone.
  //   3) all edit history is removed.
  // There is no edit to redo (== last element or no element) when:
  //   1) in initial state. (nothing to redo)
  //   2) new edit is added. (redo history is cleared)
  //   3) redone all undone edits.
  EditHistory::iterator current_edit_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_MODEL_H_
