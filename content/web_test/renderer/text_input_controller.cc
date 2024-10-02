// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/text_input_controller.h"

#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/events/base_event_utils.h"
#include "v8/include/v8.h"

namespace content {

class TextInputControllerBindings
    : public gin::Wrappable<TextInputControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  TextInputControllerBindings(const TextInputControllerBindings&) = delete;
  TextInputControllerBindings& operator=(const TextInputControllerBindings&) =
      delete;

  static void Install(base::WeakPtr<TextInputController> controller,
                      blink::WebLocalFrame* frame);

 private:
  explicit TextInputControllerBindings(
      base::WeakPtr<TextInputController> controller);
  ~TextInputControllerBindings() override;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void InsertText(const std::string& text);
  void UnmarkText();
  void UnmarkAndUnselectText();
  void DoCommand(const std::string& text);
  void ExtendSelectionAndDelete(int before, int after);
  void DeleteSurroundingText(int before, int after);
  void SetMarkedText(const std::string& text, int start, int length);
  void SetMarkedTextFromExistingText(int start, int end);
  bool HasMarkedText();
  std::vector<int> MarkedRange();
  std::vector<int> SelectedRange();
  std::vector<int> FirstRectForCharacterRange(uint32_t location,
                                              uint32_t length);
  void SetComposition(const std::string& text);
  void SetCompositionWithReplacementRange(const std::string& text,
                                          int replacement_start,
                                          int replacement_end);
  void ForceTextInputStateUpdate();

  base::WeakPtr<TextInputController> controller_;
};

gin::WrapperInfo TextInputControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void TextInputControllerBindings::Install(
    base::WeakPtr<TextInputController> controller,
    blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<TextInputControllerBindings> bindings =
      gin::CreateHandle(isolate, new TextInputControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global
      ->Set(context, gin::StringToV8(isolate, "textInputController"),
            bindings.ToV8())
      .Check();
}

TextInputControllerBindings::TextInputControllerBindings(
    base::WeakPtr<TextInputController> controller)
    : controller_(controller) {}

TextInputControllerBindings::~TextInputControllerBindings() {}

gin::ObjectTemplateBuilder
TextInputControllerBindings::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<TextInputControllerBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("insertText", &TextInputControllerBindings::InsertText)
      .SetMethod("unmarkText", &TextInputControllerBindings::UnmarkText)
      .SetMethod("unmarkAndUnselectText",
                 &TextInputControllerBindings::UnmarkAndUnselectText)
      .SetMethod("doCommand", &TextInputControllerBindings::DoCommand)
      .SetMethod("extendSelectionAndDelete",
                 &TextInputControllerBindings::ExtendSelectionAndDelete)
      .SetMethod("deleteSurroundingText",
                 &TextInputControllerBindings::DeleteSurroundingText)
      .SetMethod("setMarkedText", &TextInputControllerBindings::SetMarkedText)
      .SetMethod("setMarkedTextFromExistingText",
                 &TextInputControllerBindings::SetMarkedTextFromExistingText)
      .SetMethod("hasMarkedText", &TextInputControllerBindings::HasMarkedText)
      .SetMethod("markedRange", &TextInputControllerBindings::MarkedRange)
      .SetMethod("selectedRange", &TextInputControllerBindings::SelectedRange)
      .SetMethod("firstRectForCharacterRange",
                 &TextInputControllerBindings::FirstRectForCharacterRange)
      .SetMethod("setComposition", &TextInputControllerBindings::SetComposition)
      .SetMethod(
          "setCompositionWithReplacementRange",
          &TextInputControllerBindings::SetCompositionWithReplacementRange)
      .SetMethod("forceTextInputStateUpdate",
                 &TextInputControllerBindings::ForceTextInputStateUpdate);
}

void TextInputControllerBindings::InsertText(const std::string& text) {
  if (controller_)
    controller_->InsertText(text);
}

void TextInputControllerBindings::UnmarkText() {
  if (controller_)
    controller_->UnmarkText();
}

void TextInputControllerBindings::UnmarkAndUnselectText() {
  if (controller_)
    controller_->UnmarkAndUnselectText();
}

void TextInputControllerBindings::DoCommand(const std::string& text) {
  if (controller_)
    controller_->DoCommand(text);
}

void TextInputControllerBindings::ExtendSelectionAndDelete(int before,
                                                           int after) {
  if (controller_)
    controller_->ExtendSelectionAndDelete(before, after);
}

void TextInputControllerBindings::DeleteSurroundingText(int before, int after) {
  if (controller_)
    controller_->DeleteSurroundingText(before, after);
}

void TextInputControllerBindings::SetMarkedText(const std::string& text,
                                                int start,
                                                int length) {
  if (controller_)
    controller_->SetMarkedText(text, start, length);
}

void TextInputControllerBindings::SetMarkedTextFromExistingText(int start,
                                                                int end) {
  if (controller_)
    controller_->SetMarkedTextFromExistingText(start, end);
}

bool TextInputControllerBindings::HasMarkedText() {
  return controller_ ? controller_->HasMarkedText() : false;
}

std::vector<int> TextInputControllerBindings::MarkedRange() {
  return controller_ ? controller_->MarkedRange() : std::vector<int>();
}

std::vector<int> TextInputControllerBindings::SelectedRange() {
  return controller_ ? controller_->SelectedRange() : std::vector<int>();
}

std::vector<int> TextInputControllerBindings::FirstRectForCharacterRange(
    uint32_t location,
    uint32_t length) {
  return controller_ ? controller_->FirstRectForCharacterRange(location, length)
                     : std::vector<int>();
}

void TextInputControllerBindings::SetComposition(const std::string& text) {
  if (controller_)
    controller_->SetComposition(text, -1, -1);
}
void TextInputControllerBindings::SetCompositionWithReplacementRange(
    const std::string& text,
    int replacement_start,
    int replacement_end) {
  if (controller_)
    controller_->SetComposition(text, replacement_start, replacement_end);
}
void TextInputControllerBindings::ForceTextInputStateUpdate() {
  if (controller_)
    controller_->ForceTextInputStateUpdate();
}
// TextInputController ---------------------------------------------------------

TextInputController::TextInputController(
    WebFrameTestProxy* web_frame_test_proxy)
    : web_frame_test_proxy_(web_frame_test_proxy) {}

TextInputController::~TextInputController() {}

void TextInputController::Install(blink::WebLocalFrame* frame) {
  TextInputControllerBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void TextInputController::InsertText(const std::string& text) {
  if (auto* controller = GetInputMethodController()) {
    controller->CommitText(blink::WebString::FromUTF8(text),
                           std::vector<ui::ImeTextSpan>(), blink::WebRange(),
                           0);
  }
}

void TextInputController::UnmarkText() {
  if (auto* controller = GetInputMethodController()) {
    controller->FinishComposingText(
        blink::WebInputMethodController::kKeepSelection);
  }
}

void TextInputController::UnmarkAndUnselectText() {
  if (auto* controller = GetInputMethodController()) {
    controller->FinishComposingText(
        blink::WebInputMethodController::kDoNotKeepSelection);
  }
}

void TextInputController::DoCommand(const std::string& text) {
  if (view()->MainFrame()) {
    CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                     "called if the main frame "
                                                     "is not a local frame.";
    view()->MainFrame()->ToWebLocalFrame()->ExecuteCommand(
        blink::WebString::FromUTF8(text));
  }
}

void TextInputController::ExtendSelectionAndDelete(int before, int after) {
  if (view()->MainFrame()) {
    CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                     "called if the main frame "
                                                     "is not a local frame.";
    view()->MainFrame()->ToWebLocalFrame()->ExtendSelectionAndDelete(before,
                                                                     after);
  }
}

void TextInputController::DeleteSurroundingText(int before, int after) {
  if (view()->MainFrame()) {
    CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                     "called if the main frame "
                                                     "is not a local frame.";
    view()->MainFrame()->ToWebLocalFrame()->DeleteSurroundingText(before,
                                                                  after);
  }
}

void TextInputController::SetMarkedText(const std::string& text,
                                        int start,
                                        int length) {
  blink::WebString web_text(blink::WebString::FromUTF8(text));

  // Split underline into up to 3 elements (before, selection, and after).
  std::vector<ui::ImeTextSpan> ime_text_spans;
  ui::ImeTextSpan ime_text_span;
  if (!start) {
    ime_text_span.end_offset = length;
  } else {
    ime_text_span.end_offset = start;
    ime_text_spans.push_back(ime_text_span);
    ime_text_span.start_offset = start;
    ime_text_span.end_offset = start + length;
  }
  ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThick;
  ime_text_span.underline_style = ui::ImeTextSpan::UnderlineStyle::kSolid;
  ime_text_spans.push_back(ime_text_span);
  if (start + length < static_cast<int>(web_text.length())) {
    ime_text_span.start_offset = ime_text_span.end_offset;
    ime_text_span.end_offset = web_text.length();
    ime_text_span.thickness = ui::ImeTextSpan::Thickness::kThin;
    ime_text_span.underline_style = ui::ImeTextSpan::UnderlineStyle::kSolid;
    ime_text_spans.push_back(ime_text_span);
  }

  if (auto* controller = GetInputMethodController()) {
    controller->SetComposition(web_text, ime_text_spans, blink::WebRange(),
                               start, start + length);
  }
}

void TextInputController::SetMarkedTextFromExistingText(int start, int end) {
  if (!view()->MainFrame())
    return;

  CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                   "called if the main frame "
                                                   "is not a local frame.";

  view()->MainFrame()->ToWebLocalFrame()->SetCompositionFromExistingText(
      start, end, std::vector<ui::ImeTextSpan>());
}

bool TextInputController::HasMarkedText() {
  if (!view()->MainFrame())
    return false;

  CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                   "called if the main frame "
                                                   "is not a local frame.";

  return view()->MainFrame()->ToWebLocalFrame()->HasMarkedText();
}

std::vector<int> TextInputController::MarkedRange() {
  if (!view()->MainFrame())
    return std::vector<int>();

  CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                   "called if the main frame "
                                                   "is not a local frame.";

  blink::WebRange range = view()->MainFrame()->ToWebLocalFrame()->MarkedRange();
  std::vector<int> int_array(2);
  int_array[0] = range.StartOffset();
  int_array[1] = range.EndOffset();

  return int_array;
}

std::vector<int> TextInputController::SelectedRange() {
  if (!view()->MainFrame())
    return std::vector<int>();

  CHECK(view()->MainFrame()->ToWebLocalFrame()) << "This function cannot be "
                                                   "called if the main frame "
                                                   "is not a local frame.";

  blink::WebRange range =
      view()->MainFrame()->ToWebLocalFrame()->SelectionRange();
  if (range.IsNull())
    return std::vector<int>();
  std::vector<int> int_array(2);
  int_array[0] = range.StartOffset();
  int_array[1] = range.EndOffset();

  return int_array;
}

std::vector<int> TextInputController::FirstRectForCharacterRange(
    uint32_t location,
    uint32_t length) {
  gfx::Rect rect;
  if (!view()->FocusedFrame() ||
      !view()->FocusedFrame()->FirstRectForCharacterRange(location, length,
                                                          rect)) {
    return std::vector<int>();
  }

  std::vector<int> int_array(4);
  int_array[0] = rect.x();
  int_array[1] = rect.y();
  int_array[2] = rect.width();
  int_array[3] = rect.height();

  return int_array;
}

void TextInputController::SetComposition(const std::string& text,
                                         int replacement_range_start,
                                         int replacement_range_end) {
  // Sends a keydown event with key code = 0xE5 to emulate input method
  // behavior.
  blink::WebKeyboardEvent key_down(blink::WebInputEvent::Type::kRawKeyDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   ui::EventTimeForNow());

  key_down.windows_key_code = 0xE5;  // VKEY_PROCESSKEY
  view()->MainFrameWidget()->HandleInputEvent(
      blink::WebCoalescedInputEvent(key_down, ui::LatencyInfo()));

  // The value returned by std::string::length() may not correspond to the
  // actual number of encoded characters in sequences of multi-byte or
  // variable-length characters.
  blink::WebString newText = blink::WebString::FromUTF8(text);
  size_t textLength = newText.length();

  std::vector<ui::ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, 0, textLength,
      ui::ImeTextSpan::Thickness::kThin,
      ui::ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT));
  blink::WebRange replacement_range =
      (replacement_range_start == -1 && replacement_range_end == -1)
          ? blink::WebRange()
          : blink::WebRange(replacement_range_start,
                            replacement_range_end - replacement_range_start);
  if (auto* controller = GetInputMethodController()) {
    controller->SetComposition(
        newText, blink::WebVector<ui::ImeTextSpan>(std::move(ime_text_spans)),
        replacement_range, textLength, textLength);
  }
}

void TextInputController::ForceTextInputStateUpdate() {
  blink::WebFrameWidget* frame_widget =
      web_frame_test_proxy_->GetLocalRootWebFrameWidget();
  frame_widget->ShowVirtualKeyboard();
}

blink::WebView* TextInputController::view() {
  return web_frame_test_proxy_->GetWebFrame()->View();
}

blink::WebInputMethodController*
TextInputController::GetInputMethodController() {
  if (!view()->MainFrame())
    return nullptr;

  // TODO(lukasza): Finish adding OOPIF support to the web tests harness.
  CHECK(view()->MainFrame()->IsWebLocalFrame())
      << "WebView does not have a local main frame and"
         " cannot handle input method controller tasks.";

  return view()->MainFrameWidget()->GetActiveWebInputMethodController();
}

}  // namespace content
