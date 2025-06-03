// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_invalidation_handler.h"

namespace invalidation {

FakeInvalidationHandler::FakeInvalidationHandler(const std::string& owner_name)
    : owner_name_(owner_name) {}

FakeInvalidationHandler::~FakeInvalidationHandler() = default;

InvalidatorState FakeInvalidationHandler::GetInvalidatorState() const {
  return state_;
}

const std::map<Topic, Invalidation>&
FakeInvalidationHandler::GetReceivedInvalidations() const {
  return received_invalidations_;
}

void FakeInvalidationHandler::ClearReceivedInvalidations() {
  invalidation_count_ = 0;
  received_invalidations_.clear();
}

int FakeInvalidationHandler::GetInvalidationCount() const {
  return invalidation_count_;
}

void FakeInvalidationHandler::OnInvalidatorStateChange(InvalidatorState state) {
  state_ = state;
}

void FakeInvalidationHandler::OnIncomingInvalidation(
    const Invalidation& invalidation) {
  received_invalidations_.emplace(invalidation.topic(), invalidation);
  ++invalidation_count_;
}

std::string FakeInvalidationHandler::GetOwnerName() const {
  return owner_name_;
}

bool FakeInvalidationHandler::IsPublicTopic(const Topic& topic) const {
  return topic == "PREFERENCE";
}

}  // namespace invalidation
