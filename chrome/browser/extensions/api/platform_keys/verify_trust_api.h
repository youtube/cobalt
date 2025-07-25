// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/common/extensions/api/platform_keys.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// This keyed service is used by the platformKeys.verifyTLSServerCertificate for
// caching and to reuse objects between multiple API calls (e.g. the
// net::CertVerifier).
class VerifyTrustAPI : public BrowserContextKeyedAPI,
                       public ExtensionRegistryObserver {
 public:
  // Will be called with |return_value| set to the verification result (net::OK
  // if the certificate is trusted, otherwise a net error code) and
  // |cert_status| to the bitwise-OR of CertStatus flags. If an error occured
  // during processing the parameters, |error| is set to an english error
  // message and |return_value| and |cert_status| must be ignored.
  using VerifyCallback = base::OnceCallback<
      void(const std::string& error, int return_value, int cert_status)>;
  using Params = api::platform_keys::VerifyTLSServerCertificate::Params;

  // Consumers should use the factory instead of this constructor.
  explicit VerifyTrustAPI(content::BrowserContext* context);

  VerifyTrustAPI(const VerifyTrustAPI&) = delete;
  VerifyTrustAPI& operator=(const VerifyTrustAPI&) = delete;

  ~VerifyTrustAPI() override;

  // Verifies the server certificate as described by |params| for the
  // extension with id |extension_id|. When verification is complete
  // (successful or not), the result will be passed to |callback|.
  //
  // Note: It is safe to delete this object while there are still
  // outstanding operations. However, if this happens, |callback|
  // will NOT be called.
  void Verify(absl::optional<Params> params,
              const std::string& extension_id,
              VerifyCallback callback);

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<VerifyTrustAPI>* GetFactoryInstance();

 protected:
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  class IOPart;
  friend class BrowserContextKeyedAPIFactory<VerifyTrustAPI>;

  // Calls |ui_callback| with the given parameters.
  void FinishedVerificationOnUI(VerifyCallback ui_callback,
                                const std::string& error,
                                int return_value,
                                int cert_status);

  // Calls |ui_callback| on the UIThread with the given arguments.
  static void CallBackOnUI(VerifyCallback ui_callback,
                           const std::string& error,
                           int return_value,
                           int cert_status);

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VerifyTrustAPI"; }

  // Created on the UIThread but must be used and destroyed only on the
  // IOThread.
  std::unique_ptr<IOPart, content::BrowserThread::DeleteOnIOThread> io_part_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};

  base::WeakPtrFactory<VerifyTrustAPI> weak_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<
    VerifyTrustAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_H_
