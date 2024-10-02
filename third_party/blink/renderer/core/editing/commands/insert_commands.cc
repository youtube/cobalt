/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_commands.h"

#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/editing/commands/insert_list_command.h"
#include "third_party/blink/renderer/core/editing/commands/replace_selection_command.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

LocalFrame& InsertCommands::TargetFrame(LocalFrame& frame, Event* event) {
  if (!event)
    return frame;
  const Node* node = event->target()->ToNode();
  if (!node)
    return frame;
  LocalFrame* local_frame = node->GetDocument().GetFrame();
  DCHECK(local_frame);
  return *local_frame;
}

bool InsertCommands::ExecuteInsertFragment(LocalFrame& frame,
                                           DocumentFragment* fragment) {
  DCHECK(frame.GetDocument());
  return MakeGarbageCollected<ReplaceSelectionCommand>(
             *frame.GetDocument(), fragment,
             ReplaceSelectionCommand::kPreventNesting,
             InputEvent::InputType::kNone)
      ->Apply();
}

bool InsertCommands::ExecuteInsertElement(LocalFrame& frame,
                                          HTMLElement* content) {
  DCHECK(frame.GetDocument());
  DocumentFragment* const fragment =
      DocumentFragment::Create(*frame.GetDocument());
  DummyExceptionStateForTesting exception_state;
  fragment->AppendChild(content, exception_state);
  if (exception_state.HadException())
    return false;
  return ExecuteInsertFragment(frame, fragment);
}

bool InsertCommands::ExecuteInsertBacktab(LocalFrame& frame,
                                          Event* event,
                                          EditorCommandSource,
                                          const String&) {
  return TargetFrame(frame, event)
      .GetEventHandler()
      .HandleTextInputEvent("\t", event);
}

bool InsertCommands::ExecuteInsertHorizontalRule(LocalFrame& frame,
                                                 Event*,
                                                 EditorCommandSource,
                                                 const String& value) {
  DCHECK(frame.GetDocument());
  auto* const rule = MakeGarbageCollected<HTMLHRElement>(*frame.GetDocument());
  if (!value.empty())
    rule->SetIdAttribute(AtomicString(value));
  return ExecuteInsertElement(frame, rule);
}

bool InsertCommands::ExecuteInsertHTML(LocalFrame& frame,
                                       Event* event,
                                       EditorCommandSource source,
                                       const String& value) {
  DCHECK(frame.GetDocument());
  DocumentFragment* fragment =
      CreateFragmentFromMarkup(*frame.GetDocument(), value, "");
  if (const auto* text_control = EnclosingTextControl(
          frame.Selection().RootEditableElementOrDocumentElement())) {
    if (IsA<HTMLInputElement>(text_control)) {
      UseCounter::Count(frame.GetDocument(),
                        WebFeature::kInsertHTMLCommandOnInput);
      // We'd like to turn off HTML insertion against <input> in order to avoid
      // creating an anonymous block as a child of
      // LayoutNGTextControlInnerEditor. See crbug.com/1174952
      //
      // |textContent()| contains the contents of <style> and <script>.
      // It's not a reasonable behavior, but we think no one cares about
      // the behavior of InsertHTML for <input>.

      // Set convert_brs_to_newlines for fast/forms/8250.html.
      const bool convert_brs_to_newlines = true;
      return ExecuteInsertText(frame, event, source,
                               fragment->textContent(convert_brs_to_newlines));
    } else {
      UseCounter::Count(frame.GetDocument(),
                        WebFeature::kInsertHTMLCommandOnTextarea);
    }
  } else {
    if (Node* anchor =
            frame.Selection().GetSelectionInDOMTree().Base().AnchorNode()) {
      if (IsEditable(*anchor) && !IsRichlyEditable(*anchor)) {
        UseCounter::Count(frame.GetDocument(),
                          WebFeature::kInsertHTMLCommandOnReadWritePlainText);
      }
    }
  }
  return ExecuteInsertFragment(frame, fragment);
}

bool InsertCommands::ExecuteInsertImage(LocalFrame& frame,
                                        Event*,
                                        EditorCommandSource,
                                        const String& value) {
  DCHECK(frame.GetDocument());
  auto* const image =
      MakeGarbageCollected<HTMLImageElement>(*frame.GetDocument());
  if (!value.empty())
    image->setAttribute(html_names::kSrcAttr, AtomicString(value));
  return ExecuteInsertElement(frame, image);
}

bool InsertCommands::ExecuteInsertLineBreak(LocalFrame& frame,
                                            Event* event,
                                            EditorCommandSource source,
                                            const String&) {
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding:
      return TargetFrame(frame, event)
          .GetEventHandler()
          .HandleTextInputEvent("\n", event, kTextEventInputLineBreak);
    case EditorCommandSource::kDOM:
      // Doesn't scroll to make the selection visible, or modify the kill ring.
      // InsertLineBreak is not implemented in IE or Firefox, so this behavior
      // is only needed for backward compatibility with ourselves, and for
      // consistency with other commands.
      DCHECK(frame.GetDocument());
      return TypingCommand::InsertLineBreak(*frame.GetDocument());
  }
  NOTREACHED();
  return false;
}

bool InsertCommands::ExecuteInsertNewline(LocalFrame& frame,
                                          Event* event,
                                          EditorCommandSource,
                                          const String&) {
  const LocalFrame& target_frame = TargetFrame(frame, event);
  return target_frame.GetEventHandler().HandleTextInputEvent(
      "\n", event,
      target_frame.GetEditor().CanEditRichly() ? kTextEventInputKeyboard
                                               : kTextEventInputLineBreak);
}

bool InsertCommands::ExecuteInsertNewlineInQuotedContent(LocalFrame& frame,
                                                         Event*,
                                                         EditorCommandSource,
                                                         const String&) {
  DCHECK(frame.GetDocument());
  return TypingCommand::InsertParagraphSeparatorInQuotedContent(
      *frame.GetDocument());
}

bool InsertCommands::ExecuteInsertOrderedList(LocalFrame& frame,
                                              Event*,
                                              EditorCommandSource,
                                              const String&) {
  DCHECK(frame.GetDocument());
  return MakeGarbageCollected<InsertListCommand>(
             *frame.GetDocument(), InsertListCommand::kOrderedList)
      ->Apply();
}

bool InsertCommands::ExecuteInsertParagraph(LocalFrame& frame,
                                            Event*,
                                            EditorCommandSource,
                                            const String&) {
  DCHECK(frame.GetDocument());
  return TypingCommand::InsertParagraphSeparator(*frame.GetDocument());
}

bool InsertCommands::ExecuteInsertTab(LocalFrame& frame,
                                      Event* event,
                                      EditorCommandSource,
                                      const String&) {
  return TargetFrame(frame, event)
      .GetEventHandler()
      .HandleTextInputEvent("\t", event);
}

bool InsertCommands::ExecuteInsertText(LocalFrame& frame,
                                       Event*,
                                       EditorCommandSource,
                                       const String& value) {
  DCHECK(frame.GetDocument());
  TypingCommand::InsertText(*frame.GetDocument(), value, 0);
  return true;
}

bool InsertCommands::ExecuteInsertUnorderedList(LocalFrame& frame,
                                                Event*,
                                                EditorCommandSource,
                                                const String&) {
  DCHECK(frame.GetDocument());
  return MakeGarbageCollected<InsertListCommand>(
             *frame.GetDocument(), InsertListCommand::kUnorderedList)
      ->Apply();
}

}  // namespace blink
