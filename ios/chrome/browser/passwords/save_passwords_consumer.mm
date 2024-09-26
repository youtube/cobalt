// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/save_passwords_consumer.h"

#import <utility>

#import "base/notreached.h"
#import "components/password_manager/core/browser/password_form.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

SavePasswordsConsumer::SavePasswordsConsumer(
    id<SavePasswordsConsumerDelegate> delegate)
    : delegate_(delegate) {}

SavePasswordsConsumer::~SavePasswordsConsumer() = default;

void SavePasswordsConsumer::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  // Not called because OnGetPasswordStoreResultsFrom() is overridden.
  NOTREACHED_NORETURN();
}

void SavePasswordsConsumer::OnGetPasswordStoreResultsFrom(
    password_manager::PasswordStoreInterface* store,
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  [delegate_ onGetPasswordStoreResults:std::move(results) fromStore:store];
}

base::WeakPtr<password_manager::PasswordStoreConsumer>
SavePasswordsConsumer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ios
