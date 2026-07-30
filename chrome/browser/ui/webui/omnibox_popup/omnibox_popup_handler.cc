// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include "ui/base/models/menu_model.h"

OmniboxPopupHandler::OmniboxPopupHandler(
    mojo::PendingReceiver<omnibox_popup::mojom::PageHandler> receiver,
    mojo::PendingRemote<omnibox_popup::mojom::Page> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents) {}

OmniboxPopupHandler::~OmniboxPopupHandler() = default;

void OmniboxPopupHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, nullptr);
  }
}

void OmniboxPopupHandler::CloseUI() {
  if (embedder_) {
    embedder_->CloseUI();
  }
}

void OmniboxPopupHandler::OnSelectionChanged(const gfx::Range& selection,
                                             uint32_t sequence_number) {
  if (sequence_number < current_sequence_number_) {
    return;
  }
  latest_selection_ = selection;
}

void OmniboxPopupHandler::OnShow() {
  page_->OnShow();
}

void OmniboxPopupHandler::OnContextMenuClosed() {
  page_->OnContextMenuClosed();
}

void OmniboxPopupHandler::SetInputState(const std::string& text,
                                        const gfx::Range& selection,
                                        bool user_input_in_progress,
                                        bool is_double_click,
                                        const std::string& full_url) {
  latest_selection_ = selection;
  current_sequence_number_++;
  auto state = omnibox_popup::mojom::OmniboxInputState::New();
  state->sequence_number = current_sequence_number_;
  state->text = text;
  state->selection = selection;
  state->user_input_in_progress = user_input_in_progress;
  state->is_double_click = is_double_click;
  state->full_url = full_url;
  page_->SetInputState(std::move(state));
}
