// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"

namespace autofill {

class BnplTosView;

class BnplTosControllerImpl : public BnplTosController {
 public:
  BnplTosControllerImpl();

  BnplTosControllerImpl(const BnplTosControllerImpl&) = delete;
  BnplTosControllerImpl& operator=(const BnplTosControllerImpl&) = delete;

  ~BnplTosControllerImpl() override;

  // BnplTosController:
  void OnViewClosing() override;
  base::WeakPtr<BnplTosController> GetWeakPtr() override;

  // Show the BNPL ToS view. The `create_and_show_view_callback` will
  // be invoked immediately to create the view.
  void Show(base::OnceCallback<std::unique_ptr<BnplTosView>()>
                create_and_show_view_callback);

 private:
  friend class BnplTosControllerImplTest;
  std::unique_ptr<BnplTosView> view_;

  base::WeakPtrFactory<BnplTosControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_IMPL_H_
