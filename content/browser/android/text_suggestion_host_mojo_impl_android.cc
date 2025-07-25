// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/text_suggestion_host_mojo_impl_android.h"

#include "content/browser/android/text_suggestion_host_android.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
std::unique_ptr<TextSuggestionHostMojoImplAndroid>
TextSuggestionHostMojoImplAndroid::Create(
    TextSuggestionHostAndroid* text_suggestion_host,
    mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver) {
  return std::make_unique<TextSuggestionHostMojoImplAndroid>(
      text_suggestion_host, std::move(receiver));
}

TextSuggestionHostMojoImplAndroid::TextSuggestionHostMojoImplAndroid(
    TextSuggestionHostAndroid* text_suggestion_host,
    mojo::PendingReceiver<blink::mojom::TextSuggestionHost> receiver)
    : text_suggestion_host_(text_suggestion_host),
      receiver_(this, std::move(receiver)) {}

TextSuggestionHostMojoImplAndroid::~TextSuggestionHostMojoImplAndroid() =
    default;

void TextSuggestionHostMojoImplAndroid::StartSuggestionMenuTimer() {
  text_suggestion_host_->StartSuggestionMenuTimer();
}

void TextSuggestionHostMojoImplAndroid::ShowSpellCheckSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    std::vector<blink::mojom::SpellCheckSuggestionPtr> suggestions) {
  text_suggestion_host_->ShowSpellCheckSuggestionMenu(caret_x, caret_y,
                                                      marked_text, suggestions);
}

void TextSuggestionHostMojoImplAndroid::ShowTextSuggestionMenu(
    double caret_x,
    double caret_y,
    const std::string& marked_text,
    std::vector<blink::mojom::TextSuggestionPtr> suggestions) {
  text_suggestion_host_->ShowTextSuggestionMenu(caret_x, caret_y, marked_text,
                                                suggestions);
}

}  // namespace content
