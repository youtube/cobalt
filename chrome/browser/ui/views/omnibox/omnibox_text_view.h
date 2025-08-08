// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class RenderText;
}  // namespace gfx

// A view containing a render text styled via search results. This differs from
// the general purpose views::Label class by having less general features (such
// as selection) and more specific features (such as suggestion answer styling).
class OmniboxTextView : public views::View {
  METADATA_HEADER(OmniboxTextView, views::View)

 public:
  explicit OmniboxTextView(OmniboxResultView* result_view);
  OmniboxTextView(const OmniboxTextView&) = delete;
  OmniboxTextView& operator=(const OmniboxTextView&) = delete;
  ~OmniboxTextView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool GetCanProcessEventsWithinSubtree() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // Applies given theme color to underlying render text. This is called Apply*
  // instead of Set* because the only state kept is in render_text, so call this
  // after setting text with methods below.
  void ApplyTextColor(ui::ColorId id);

  // Returns the render text, or an empty string if there is none.
  const std::u16string& GetText() const;

  // Used for content/description separator & tail suggest ellipses.
  void SetText(const std::u16string& new_text);

  // Used for standard suggestions.
  void SetTextWithStyling(const std::u16string& new_text,
                          const ACMatchClassifications& classifications);

  // Used for search answers using `RichAnswerTemplate`.
  // Sets the styling for `FormattedString`'s `FormattedStringFragment`s.
  // `fragment_index` specifies where to start appending and styling text from.
  void AppendTextWithStyling(const omnibox::FormattedString& formatted_string,
                             size_t fragment_index,
                             const omnibox::AnswerType& answer_type);

  // Used for suggestions using `RichAnswerTemplate`.
  void SetMultilineText(const omnibox::FormattedString& formatted_string,
                        const omnibox::AnswerType& answer_type);

  // Used for history embedding answers.
  void SetMultilineText(const std::u16string& text);

  // Get the height of one line of text.  This is handy if the view might have
  // multiple lines.
  int GetLineHeight() const;

  // Reapplies text styling to the results text, based on the types of the match
  // parts.
  void ReapplyStyling();

  // Creates a platform-approriate RenderText, sets its format to that of
  // a suggestion and inserts (renders) the provided |text|.
  std::unique_ptr<gfx::RenderText> CreateRenderText(
      const std::u16string& text) const;

 private:
  // Updates the cached maximum line height and recomputes the preferred size.
  void OnStyleChanged();

  // To get color values.
  raw_ptr<OmniboxResultView> result_view_;

  // Font settings for this view.
  int font_height_ = 0;

  // Whether to wrap lines if the width is too narrow for the whole string.
  // TODO(crbug.com/370088101): Ensure `wrap_text_lines_` is set correctly in
  //   the `SetTextWithStyling()` and related methods above. Can we use
  //   `render_text_->multiline()` instead? Ideally, all the text setting
  //   methods should detect if nothing has changed (i.e. text, style, and other
  //   options like wrapping / multiline, etc are all unchanged). If nothing has
  //   changed, then they should early exit because those methods are called
  //   1000's of times per keystroke and cause noticeable lag when not
  //   early-exiting. Otherwise, if something has changed, the methods should
  //   explicitly set all these states and not assume they still hold their
  //   default values from initialization since `OmniboxTextView`s are reused.
  bool wrap_text_lines_ = false;

  // The primary data for this class.
  std::unique_ptr<gfx::RenderText> render_text_;
  // The classifications most recently passed to SetText. Used to exit
  // early instead of setting text when the text and classifications
  // match the current state of the view.
  std::unique_ptr<ACMatchClassifications> cached_classifications_;

  // Caches the param and return value of `CalculatePreferredSize()` for
  // multiline texts. `CalculatePreferredSize()` is called 100's of times per
  // keystroke. For 1-line texts, it's cached via `View::SetPreferredSize()`.
  // But the size of multiline texts depends on `available_size`. Caching for
  // the most recent `available_size` reduces the need to recompute preferred
  // size from 300 to 4 times per keystroke. Increasing the cache size to 2
  // would reduce it further to 0 times per keystroke, but that seems
  // unnecessary. Unused for 1-line texts.
  // The most recent `available_size` param passed to
  // `CalculatePreferredSize()`.
  mutable views::SizeBounds cached_available_size_;
  // The most recent return value from `CalculatePreferredSize()`.
  mutable gfx::Size cached_calculate_preferred_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TEXT_VIEW_H_
