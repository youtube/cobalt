// Copyright 2025 The Cobalt Authors. All Rights Reserved.

//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define COBALT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"

class SimpleFactoryKey;

namespace content {

class BackgroundSyncController;
class ContentIndexProvider;
class ClientHintsControllerDelegate;
class DownloadManagerDelegate;
class OriginTrialsControllerDelegate;
class PermissionControllerDelegate;
class ReduceAcceptLanguageControllerDelegate;
class ZoomLevelDelegate;

class ShellBrowserContext : public BrowserContext {
 public:
  // If |delay_services_creation| is true, the owner is responsible for calling
  // CreateBrowserContextServices() for this BrowserContext.
  ShellBrowserContext(bool off_the_record,
                      bool delay_services_creation = false);

  ShellBrowserContext(const ShellBrowserContext&) = delete;
  ShellBrowserContext& operator=(const ShellBrowserContext&) = delete;

  ~ShellBrowserContext() override;

  void set_client_hints_controller_delegate(
      ClientHintsControllerDelegate* delegate) {
    client_hints_controller_delegate_ = delegate;
  }

  // BrowserContext implementation.
  base::FilePath GetPath() override;
  std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  bool IsOffTheRecord() override;
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  PlatformNotificationService* GetPlatformNotificationService() override;
  PushMessagingService* GetPushMessagingService() override;
  StorageNotificationService* GetStorageNotificationService() override;
  SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  PermissionControllerDelegate* GetPermissionControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate() override;
  ContentIndexProvider* GetContentIndexProvider() override;
  ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;
  ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate() override;

 protected:
  bool ignore_certificate_errors() const { return ignore_certificate_errors_; }

  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<ContentIndexProvider> content_index_provider_;
  std::unique_ptr<ReduceAcceptLanguageControllerDelegate>
      reduce_accept_lang_controller_delegate_;

 private:
  // Performs initialization of the ShellBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();
  void FinishInitWhileIOAllowed();

  const bool off_the_record_;
  bool ignore_certificate_errors_ = false;
  base::FilePath path_;
  std::unique_ptr<SimpleFactoryKey> key_;
  raw_ptr<ClientHintsControllerDelegate> client_hints_controller_delegate_ =
      nullptr;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
