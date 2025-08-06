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

#include "cobalt/shell/browser/shell_browser_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "cobalt/shell/browser/shell_content_index_provider.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"

namespace content {

ShellBrowserContext::ShellResourceContext::ShellResourceContext() {}

ShellBrowserContext::ShellResourceContext::~ShellResourceContext() {}

ShellBrowserContext::ShellBrowserContext(bool off_the_record,
                                         bool delay_services_creation)
    : resource_context_(std::make_unique<ShellResourceContext>()),
      off_the_record_(off_the_record) {
  InitWhileIOAllowed();

  if (!delay_services_creation) {
    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServices(this);
  }
}

ShellBrowserContext::~ShellBrowserContext() {
  NotifyWillBeDestroyed();

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

  // Need to destruct the ResourceContext before posting tasks which may delete
  // the URLRequestContext because ResourceContext's destructor will remove any
  // outstanding request while URLRequestContext's destructor ensures that there
  // are no more outstanding requests.
  if (resource_context_) {
    GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                          resource_context_.release());
  }
  ShutdownStoragePartitions();
}

void ShellBrowserContext::InitWhileIOAllowed() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kIgnoreCertificateErrors)) {
    ignore_certificate_errors_ = true;
  }
  if (cmd_line->HasSwitch(switches::kContentShellDataPath)) {
    path_ = cmd_line->GetSwitchValuePath(switches::kContentShellDataPath);
    if (base::DirectoryExists(path_) || base::CreateDirectory(path_)) {
      // BrowserContext needs an absolute path, which we would normally get via
      // PathService. In this case, manually ensure the path is absolute.
      if (!path_.IsAbsolute()) {
        path_ = base::MakeAbsoluteFilePath(path_);
      }
      if (!path_.empty()) {
        FinishInitWhileIOAllowed();
        base::PathService::OverrideAndCreateIfNeeded(
            SHELL_DIR_USER_DATA, path_, /*is_absolute=*/true, /*create=*/false);
        return;
      }
    } else {
      LOG(WARNING) << "Unable to create data-path directory: " << path_.value();
    }
  }

  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path_));

  FinishInitWhileIOAllowed();
}

void ShellBrowserContext::FinishInitWhileIOAllowed() {
  key_ = std::make_unique<SimpleFactoryKey>(path_, off_the_record_);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());
}

std::unique_ptr<ZoomLevelDelegate> ShellBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath&) {
  return nullptr;
}

base::FilePath ShellBrowserContext::GetPath() {
  return path_;
}

bool ShellBrowserContext::IsOffTheRecord() {
  return off_the_record_;
}

DownloadManagerDelegate* ShellBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

ResourceContext* ShellBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

PlatformNotificationService*
ShellBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

PushMessagingService* ShellBrowserContext::GetPushMessagingService() {
  return nullptr;
}

StorageNotificationService*
ShellBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

SSLHostStateDelegate* ShellBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

PermissionControllerDelegate*
ShellBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

ClientHintsControllerDelegate*
ShellBrowserContext::GetClientHintsControllerDelegate() {
  return client_hints_controller_delegate_;
}

BackgroundFetchDelegate* ShellBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* ShellBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

BrowsingDataRemoverDelegate*
ShellBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

ContentIndexProvider* ShellBrowserContext::GetContentIndexProvider() {
  if (!content_index_provider_) {
    content_index_provider_ = std::make_unique<ShellContentIndexProvider>();
  }
  return content_index_provider_.get();
}

FederatedIdentityApiPermissionContextDelegate*
ShellBrowserContext::GetFederatedIdentityApiPermissionContext() {
  return nullptr;
}

FederatedIdentityAutoReauthnPermissionContextDelegate*
ShellBrowserContext::GetFederatedIdentityAutoReauthnPermissionContext() {
  return nullptr;
}

FederatedIdentityPermissionContextDelegate*
ShellBrowserContext::GetFederatedIdentityPermissionContext() {
  return nullptr;
}

ReduceAcceptLanguageControllerDelegate*
ShellBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

OriginTrialsControllerDelegate*
ShellBrowserContext::GetOriginTrialsControllerDelegate() {
  return nullptr;
}

}  // namespace content
