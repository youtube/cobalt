// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_MANAGER_H_

#include <memory>
#include <string>

#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {
class BaseBlockingPage;
class SafeBrowsingNetworkContext;
}  // namespace safe_browsing

namespace android_webview {

// The Safe Browsing service is responsible for checking URLs against
// anti-phishing and anti-malware tables. This is an Android WebView-specific UI
// manager.
class AwSafeBrowsingUIManager : public safe_browsing::BaseUIManager {
 public:
  class UIManagerClient {
   public:
    static UIManagerClient* FromWebContents(content::WebContents* web_contents);

    // Whether this web contents can show any sort of interstitial
    virtual bool CanShowInterstitial() = 0;

    // Returns the appropriate BaseBlockingPage::ErrorUiType
    virtual int GetErrorUiType() = 0;
  };

  // Construction needs to happen on the UI thread.
  AwSafeBrowsingUIManager();

  AwSafeBrowsingUIManager(const AwSafeBrowsingUIManager&) = delete;
  AwSafeBrowsingUIManager& operator=(const AwSafeBrowsingUIManager&) = delete;

  // Gets the correct ErrorUiType for the web contents
  int GetErrorUiType(content::WebContents* web_contents) const;

  // BaseUIManager methods:
  void DisplayBlockingPage(const UnsafeResource& resource) override;

  // Called on the UI thread by the ThreatDetails with the report, so the
  // service can send it over.
  void SendThreatDetails(
      content::BrowserContext* browser_context,
      std::unique_ptr<safe_browsing::ClientSafeBrowsingReportRequest> report)
      override;

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Called on the IO thread to get a SharedURLLoaderFactory that can be used on
  // the IO thread. If kSafeBrowsingOnUIThread is enabled it's called on the UI
  // thread.
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryOnSBThread();

 protected:
  ~AwSafeBrowsingUIManager() override;

 private:
  safe_browsing::BaseBlockingPage* CreateBlockingPageForSubresource(
      content::WebContents* contents,
      const GURL& blocked_url,
      const UnsafeResource& unsafe_resource) override;

  // Called on the UI thread to create a URLLoaderFactory interface ptr for
  // the SB thread.
  void CreateURLLoaderFactoryForSB(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // This is what owns the URLRequestContext inside the network service. This is
  // used by SimpleURLLoader for Safe Browsing requests.
  std::unique_ptr<safe_browsing::SafeBrowsingNetworkContext> network_context_;

  // A SharedURLLoaderFactory and its interfaceptr used on the SB thread.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_on_sb_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_on_sb_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_UI_MANAGER_H_
