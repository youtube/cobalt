// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_model_setter_impl.h"

#include "components/safe_browsing/content/renderer/phishing_classifier/flatbuffer_scorer.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/protobuf_scorer.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace safe_browsing {

PhishingModelSetterImpl::PhishingModelSetterImpl() = default;
PhishingModelSetterImpl::~PhishingModelSetterImpl() = default;

void PhishingModelSetterImpl::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface<mojom::PhishingModelSetter>(
      base::BindRepeating(&PhishingModelSetterImpl::OnRendererAssociatedRequest,
                          base::Unretained(this)));
}

void PhishingModelSetterImpl::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(mojom::PhishingModelSetter::Name_);
}

void PhishingModelSetterImpl::SetPhishingModel(const std::string& model,
                                               base::File tflite_visual_model) {
  std::unique_ptr<Scorer> scorer;

  // An empty model string means we should disable client-side phishing
  // detection.
  if (!model.empty()) {
    scorer = safe_browsing::ProtobufModelScorer::Create(
        model, std::move(tflite_visual_model));
    if (!scorer)
      return;
  }
  ScorerStorage::GetInstance()->SetScorer(std::move(scorer));

  if (observer_for_testing_.is_bound()) {
    observer_for_testing_->PhishingModelUpdated();
  }
}

void PhishingModelSetterImpl::SetPhishingFlatBufferModel(
    base::ReadOnlySharedMemoryRegion flatbuffer_region,
    base::File tflite_visual_model) {
  std::unique_ptr<Scorer> scorer;
  // An invalid region means we should disable client-side phishing detection.
  if (flatbuffer_region.IsValid()) {
    scorer = safe_browsing::FlatBufferModelScorer::Create(
        std::move(flatbuffer_region), std::move(tflite_visual_model));
    if (!scorer)
      return;
  }
  ScorerStorage::GetInstance()->SetScorer(std::move(scorer));

  if (observer_for_testing_.is_bound()) {
    observer_for_testing_->PhishingModelUpdated();
  }
}

void PhishingModelSetterImpl::SetTestObserver(
    mojo::PendingRemote<mojom::PhishingModelSetterTestObserver> observer,
    SetTestObserverCallback callback) {
  if (observer_for_testing_.is_bound())
    observer_for_testing_.reset();
  observer_for_testing_.Bind(std::move(observer));
  std::move(callback).Run();
}

void PhishingModelSetterImpl::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::PhishingModelSetter> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace safe_browsing
