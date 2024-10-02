// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/untrusted_annotator_page_handler_impl.h"

#include <memory>

#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/webui/projector_app/mojom/untrusted_annotator.mojom.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/mojom/annotator_structs.mojom.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace ash {

UntrustedAnnotatorPageHandlerImpl::UntrustedAnnotatorPageHandlerImpl(
    mojo::PendingReceiver<annotator::mojom::UntrustedAnnotatorPageHandler>
        annotator_handler,
    mojo::PendingRemote<annotator::mojom::UntrustedAnnotatorPage> annotator,
    content::WebUI* web_ui)
    : annotator_remote_(std::move(annotator)),
      annotator_handler_receiver_(this, std::move(annotator_handler)),
      web_ui_(web_ui) {
  ProjectorAppClient::Get()->SetAnnotatorPageHandler(this);
}

UntrustedAnnotatorPageHandlerImpl::~UntrustedAnnotatorPageHandlerImpl() {
  ProjectorAppClient::Get()->ResetAnnotatorPageHandler(this);
}

void UntrustedAnnotatorPageHandlerImpl::SetTool(const AnnotatorTool& tool) {
  auto mojo_tool = annotator::mojom::AnnotatorTool::New();
  mojo_tool->color = tool.GetColorHexString();
  mojo_tool->tool = tool.GetToolString();
  mojo_tool->size = tool.size;
  annotator_remote_->SetTool(std::move(mojo_tool));
}

void UntrustedAnnotatorPageHandlerImpl::Undo() {
  annotator_remote_->Undo();
}

void UntrustedAnnotatorPageHandlerImpl::Redo() {
  annotator_remote_->Redo();
}

void UntrustedAnnotatorPageHandlerImpl::Clear() {
  annotator_remote_->Clear();
}

void UntrustedAnnotatorPageHandlerImpl::OnUndoRedoAvailabilityChanged(
    bool undo_available,
    bool redo_available) {
  // ProjectorController is created when ash::Shell::Init is called and is
  // destroyed when ash::Shell is destroyed. Therefore, ProjectorController
  // is available when this WebUI is showing.
  ProjectorController::Get()->OnUndoRedoAvailabilityChanged(undo_available,
                                                            redo_available);
}

void UntrustedAnnotatorPageHandlerImpl::OnCanvasInitialized(bool success) {
  // ProjectorController is created when ash::Shell::Init is called and is
  // destroyed when ash::Shell is destroyed. Therefore, ProjectorController
  // is available when this WebUI is showing.
  ProjectorController::Get()->OnCanvasInitialized(success);
}

}  // namespace ash
