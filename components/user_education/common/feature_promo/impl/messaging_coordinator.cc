// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/messaging_coordinator.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "components/user_education/product_messaging/product_messaging_controller.h"
#include "components/user_education/product_messaging/product_messaging_types.h"

namespace user_education::internal {

DEFINE_CLASS_PRODUCT_MESSAGE_KEY(MessagingCoordinator, kLowPriorityNoticeId);
DEFINE_CLASS_PRODUCT_MESSAGE_KEY(MessagingCoordinator, kHighPriorityNoticeId);

MessagingCoordinator::MessagingCoordinator(
    ProductMessagingController& controller)
    : controller_(controller) {}

MessagingCoordinator::~MessagingCoordinator() = default;

bool MessagingCoordinator::ReadyToShow(bool high_priority) const {
  // Must always be holding the handle.
  if (!handle_) {
    return false;
  }

  if (high_priority) {
    return promo_state_ == PromoState::kHighPriorityPending;
  } else {
    return promo_state_ == PromoState::kLowPriorityPending;
  }
}

bool MessagingCoordinator::IsBlockedByExternalPromo() const {
  return !handle_ &&
         !controller_
              ->GetAllMessages(
                  {ProductMessageStatus::kWaiting, ProductMessageStatus::kReady,
                   ProductMessageStatus::kShowing},
                  /*priority_higher_than=*/ProductMessageType::kHighPriorityIph)
              .empty();
}

void MessagingCoordinator::TransitionToState(PromoState promo_state) {
  switch (promo_state) {
    case PromoState::kNone:
      ReleaseAll();
      break;
    case PromoState::kLowPriorityShowing:
      CHECK(ReadyToShow(/*high_priority=*/false));
      handle_->SetShown();
      break;
    case PromoState::kHighPriorityShowing:
      CHECK(ReadyToShow(/*high_priority=*/true));
      handle_->SetShown();
      break;
    case PromoState::kLowPriorityPending:
      RequestPriority(/*high_priority=*/false);
      break;
    case PromoState::kHighPriorityPending:
      RequestPriority(/*high_priority=*/true);
      break;
  }

  promo_state_ = promo_state;
}

base::CallbackListSubscription MessagingCoordinator::AddPromoPreemptedCallback(
    base::RepeatingClosure callback) {
  return promo_preempted_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription MessagingCoordinator::AddPromoReadyCallback(
    base::RepeatingClosure callback) {
  return promo_ready_callbacks_.Add(std::move(callback));
}

void MessagingCoordinator::RequestPriority(bool high_priority) {
  CHECK(!ReadyToShow(high_priority));

  // If the handle is held but it's being held for the wrong reason, release it.
  if (handle_) {
    handle_.reset();
  }

  auto cb = base::BindOnce(&MessagingCoordinator::OnPriorityReceived,
                           weak_ptr_factory_.GetWeakPtr());
  if (high_priority) {
    controller_->UnqueueMessage(kLowPriorityNoticeId);
    controller_->QueueMessage(kHighPriorityNoticeId, std::move(cb));
  } else {
    controller_->UnqueueMessage(kHighPriorityNoticeId);
    controller_->QueueMessage(kLowPriorityNoticeId, std::move(cb));
  }
}

void MessagingCoordinator::ReleaseAll() {
  handle_.reset();
  controller_->UnqueueMessage(kLowPriorityNoticeId);
  controller_->UnqueueMessage(kHighPriorityNoticeId);
}

void MessagingCoordinator::OnPriorityReceived(ProductMessagingHandle handle) {
  handle_ = std::move(handle);
  handle_->SetSupersededCallback(base::BindRepeating(
      &MessagingCoordinator::OnStatusChange, base::Unretained(this)));
  promo_ready_callbacks_.Notify();
}

void MessagingCoordinator::OnStatusChange(ProductMessageKey message_key,
                                          ProductMessageStatus status) {
  if (message_key == kLowPriorityNoticeId ||
      message_key == kHighPriorityNoticeId) {
    return;
  }
  if (status != ProductMessageStatus::kShowing) {
    return;
  }
  if (message_key.type() <= ProductMessageType::kHighPriorityIph) {
    return;
  }
  if (promo_state_ == PromoState::kLowPriorityShowing) {
    promo_preempted_callbacks_.Notify();
  }
}

}  // namespace user_education::internal
