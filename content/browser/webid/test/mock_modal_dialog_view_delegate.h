// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_MODAL_DIALOG_VIEW_DELEGATE_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_MODAL_DIALOG_VIEW_DELEGATE_H_

#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockModalDialogViewDelegate
    : public FederatedIdentityModalDialogViewDelegate {
 public:
  MockModalDialogViewDelegate();

  ~MockModalDialogViewDelegate() override;

  MockModalDialogViewDelegate(const MockModalDialogViewDelegate&) = delete;
  MockModalDialogViewDelegate& operator=(const MockModalDialogViewDelegate&) =
      delete;

  MOCK_METHOD0(NotifyClose, void());
  MOCK_METHOD1(NotifyResolve, bool(const std::string&));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_MODAL_DIALOG_VIEW_DELEGATE_H_
