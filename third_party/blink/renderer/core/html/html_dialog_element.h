/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/closewatcher/close_watcher.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class Document;
class ExceptionState;

class CORE_EXPORT HTMLDialogElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLDialogElement(Document&);

  void Trace(Visitor*) const override;

  void close(const String& return_value = String());
  void show(ExceptionState&);
  void showModal(ExceptionState&);
  void RemovedFrom(ContainerNode&) override;

  bool IsModal() const { return is_modal_; }

  String returnValue() const { return return_value_; }
  void setReturnValue(const String& return_value) {
    return_value_ = return_value;
  }

  void CloseWatcherFiredCancel(Event*);
  void CloseWatcherFiredClose();

  bool IsMouseFocusable() const override {
    return isConnected() && IsFocusableStyleAfterUpdate();
  }

  // https://html.spec.whatwg.org/C/#the-dialog-element
  // Chooses the focused element when show() or showModal() is invoked.
  void SetFocusForDialog(bool is_modal);

  // This is the old dialog initial focus behavior which is currently being
  // replaced by SetFocusForDialog.
  // TODO(http://crbug.com/383230): Remove this when DialogNewFocusBehavior gets
  // to stable with no issues.
  static void SetFocusForDialogLegacy(HTMLDialogElement* dialog);

 private:
  void DefaultEventHandler(Event&) override;

  void SetIsModal(bool is_modal);
  void ScheduleCloseEvent();

  bool is_modal_;
  String return_value_;
  WeakMember<Element> previously_focused_element_;

  Member<CloseWatcher> close_watcher_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DIALOG_ELEMENT_H_
