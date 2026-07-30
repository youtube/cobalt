// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
#define CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/private_verification_tokens/common/private_verification_tokens_store.h"
#include "components/private_verification_tokens/mojom/private_verification_tokens_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrivateVerificationTokensService
    : public KeyedService,
      public private_verification_tokens::mojom::
          PrivateVerificationTokensProvider {
 public:
  static std::unique_ptr<PrivateVerificationTokensService> Create(
      const base::FilePath& data_directory);
  ~PrivateVerificationTokensService() override;
  void Shutdown() override;
  void BindReceiver(
      mojo::PendingReceiver<
          private_verification_tokens::mojom::PrivateVerificationTokensProvider>
          pending_receiver);

  // mojom implementation
  void GetTokens(
      private_verification_tokens::mojom::PrivateVerificationTokensProvider::
          GetTokensCallback callback) override;
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInitializationComplete() = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool is_initialized() const;

 private:
  PrivateVerificationTokensService();

  void OnStoreInitialized();

  mojo::ReceiverSet<
      private_verification_tokens::mojom::PrivateVerificationTokensProvider>
      receivers_;
  std::unique_ptr<private_verification_tokens::PrivateVerificationTokensStore>
      store_;
  bool is_shutting_down_ = false;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<PrivateVerificationTokensService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_H_
