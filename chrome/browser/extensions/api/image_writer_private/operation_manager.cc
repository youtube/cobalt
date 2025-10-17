// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/image_writer_private/destroy_partitions_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_url_operation.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/file_manager/path_util.h"
#endif

namespace image_writer_api = extensions::api::image_writer_private;

namespace extensions {
namespace image_writer {

using content::BrowserThread;

OperationManager::OperationManager(content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
  process_manager_observation_.Observe(ProcessManager::Get(browser_context_));
}

OperationManager::~OperationManager() = default;

void OperationManager::Shutdown() {
  for (auto& id_and_operation : operations_) {
    scoped_refptr<Operation> operation = id_and_operation.second;
    operation->PostTask(base::BindOnce(&Operation::Abort, operation));
  }
}

void OperationManager::StartWriteFromUrl(
    const ExtensionId& extension_id,
    GURL url,
    const std::string& hash,
    const std::string& device_path,
    Operation::StartWriteCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS can only support a single operation at a time.
  if (operations_.size() > 0) {
#else
  auto existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end()) {
#endif

    std::move(callback).Run(false, error::kOperationAlreadyInProgress);
    return;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      url_loader_factory_remote;
  browser_context_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      ->Clone(url_loader_factory_remote.InitWithNewPipeAndPassReceiver());

  auto operation = base::MakeRefCounted<WriteFromUrlOperation>(
      weak_factory_.GetWeakPtr(), extension_id,
      std::move(url_loader_factory_remote), url, hash, device_path,
      GetAssociatedDownloadFolder());
  operations_[extension_id] = operation;
  operation->PostTask(base::BindOnce(&Operation::Start, operation));

  std::move(callback).Run(true, "");
}

void OperationManager::StartWriteFromFile(
    const ExtensionId& extension_id,
    const base::FilePath& path,
    const std::string& device_path,
    Operation::StartWriteCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS can only support a single operation at a time.
  if (operations_.size() > 0) {
#else
  auto existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end()) {
#endif
    std::move(callback).Run(false, error::kOperationAlreadyInProgress);
    return;
  }

  auto operation = base::MakeRefCounted<WriteFromFileOperation>(
      weak_factory_.GetWeakPtr(), extension_id, path, device_path,
      GetAssociatedDownloadFolder());
  operations_[extension_id] = operation;
  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  std::move(callback).Run(true, "");
}

void OperationManager::CancelWrite(const ExtensionId& extension_id,
                                   Operation::CancelWriteCallback callback) {
  Operation* existing_operation = GetOperation(extension_id);

  if (existing_operation == nullptr) {
    std::move(callback).Run(false, error::kNoOperationInProgress);
  } else {
    existing_operation->PostTask(
        base::BindOnce(&Operation::Cancel, existing_operation));
    DeleteOperation(extension_id);
    std::move(callback).Run(true, "");
  }
}

void OperationManager::DestroyPartitions(
    const ExtensionId& extension_id,
    const std::string& device_path,
    Operation::StartWriteCallback callback) {
  auto existing_operation = operations_.find(extension_id);

  if (existing_operation != operations_.end()) {
    std::move(callback).Run(false, error::kOperationAlreadyInProgress);
    return;
  }

  auto operation = base::MakeRefCounted<DestroyPartitionsOperation>(
      weak_factory_.GetWeakPtr(), extension_id, device_path,
      GetAssociatedDownloadFolder());
  operations_[extension_id] = operation;
  operation->PostTask(base::BindOnce(&Operation::Start, operation));
  std::move(callback).Run(true, "");
}

void OperationManager::OnProgress(const ExtensionId& extension_id,
                                  image_writer_api::Stage stage,
                                  int progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  image_writer_api::ProgressInfo info;
  info.stage = stage;
  info.percent_complete = progress;

  auto args(image_writer_api::OnWriteProgress::Create(info));
  std::unique_ptr<Event> event(new Event(
      events::IMAGE_WRITER_PRIVATE_ON_WRITE_PROGRESS,
      image_writer_api::OnWriteProgress::kEventName, std::move(args)));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void OperationManager::OnComplete(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto args(image_writer_api::OnWriteComplete::Create());
  std::unique_ptr<Event> event(new Event(
      events::IMAGE_WRITER_PRIVATE_ON_WRITE_COMPLETE,
      image_writer_api::OnWriteComplete::kEventName, std::move(args)));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));

  DeleteOperation(extension_id);
}

void OperationManager::OnError(const ExtensionId& extension_id,
                               image_writer_api::Stage stage,
                               int progress,
                               const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  image_writer_api::ProgressInfo info;

  DLOG(ERROR) << "ImageWriter error: " << error_message;

  info.stage = stage;
  info.percent_complete = progress;

  auto args(image_writer_api::OnWriteError::Create(info, error_message));
  std::unique_ptr<Event> event(
      new Event(events::IMAGE_WRITER_PRIVATE_ON_WRITE_ERROR,
                image_writer_api::OnWriteError::kEventName, std::move(args)));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, std::move(event));

  DeleteOperation(extension_id);
}

base::FilePath OperationManager::GetAssociatedDownloadFolder() {
#if BUILDFLAG(IS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  return file_manager::util::GetDownloadsFolderForProfile(profile);
#else
  return base::FilePath();
#endif
}

Operation* OperationManager::GetOperation(const ExtensionId& extension_id) {
  auto existing_operation = operations_.find(extension_id);

  if (existing_operation == operations_.end())
    return nullptr;
  return existing_operation->second.get();
}

void OperationManager::DeleteOperation(const ExtensionId& extension_id) {
  auto existing_operation = operations_.find(extension_id);
  if (existing_operation != operations_.end()) {
    operations_.erase(existing_operation);
  }
}

void OperationManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DeleteOperation(extension->id());
}

void OperationManager::OnShutdown(ExtensionRegistry* registry) {
  DCHECK(extension_registry_observation_.IsObservingSource(registry));
  extension_registry_observation_.Reset();
}

void OperationManager::OnBackgroundHostClose(const ExtensionId& extension_id) {
  DeleteOperation(extension_id);
}

void OperationManager::OnProcessManagerShutdown(ProcessManager* manager) {
  DCHECK(process_manager_observation_.IsObservingSource(manager));
  process_manager_observation_.Reset();
}

void OperationManager::OnExtensionProcessTerminated(
    const Extension* extension) {
  DeleteOperation(extension->id());
}

OperationManager* OperationManager::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<OperationManager>::Get(context);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<OperationManager>>::
    DestructorAtExit g_operation_manager_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<OperationManager>*
OperationManager::GetFactoryInstance() {
  return g_operation_manager_factory.Pointer();
}

}  // namespace image_writer
}  // namespace extensions
