// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/provider_function.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "storage/browser/file_system/watcher_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/file_system_provider_service_ash.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {
namespace {

constexpr const char kInterfaceUnavailable[] = "interface unavailable";

api::file_system_provider::FileSystemInfo ConvertFileSystemInfoMojomToExtension(
    crosapi::mojom::FileSystemInfoPtr info) {
  using api::file_system_provider::OpenedFile;
  using api::file_system_provider::Watcher;
  api::file_system_provider::FileSystemInfo item;
  item.file_system_id = info->metadata->file_system_id->id;
  item.display_name = info->metadata->display_name;
  item.writable = info->metadata->writable;
  item.opened_files_limit = info->metadata->opened_files_limit;
  item.supports_notify_tag = info->metadata->supports_notify;

  for (const auto& watcher : info->watchers) {
    Watcher watcher_item;
    watcher_item.entry_path = watcher->entry_path.value();
    watcher_item.recursive = watcher->recursive;
    if (!watcher->last_tag.empty()) {
      watcher_item.last_tag = watcher->last_tag;
    }
    item.watchers.push_back(std::move(watcher_item));
  }

  for (const auto& opened_file : info->opened_files) {
    OpenedFile opened_file_item;
    opened_file_item.open_request_id = opened_file->open_request_id;
    opened_file_item.file_path = opened_file->file_path;
    switch (opened_file->mode) {
      case crosapi::mojom::OpenFileMode::kRead:
        opened_file_item.mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_READ;
        break;
      case crosapi::mojom::OpenFileMode::kWrite:
        opened_file_item.mode =
            extensions::api::file_system_provider::OPEN_FILE_MODE_WRITE;
        break;
    }
    item.opened_files.push_back(std::move(opened_file_item));
  }
  return item;
}

// Converts the change type from the IDL type to a mojom type. |changed_type|
// must be specified (not CHANGE_TYPE_NONE).
crosapi::mojom::FSPChangeType ParseChangeType(
    const api::file_system_provider::ChangeType& change_type) {
  switch (change_type) {
    case api::file_system_provider::CHANGE_TYPE_CHANGED:
      return crosapi::mojom::FSPChangeType::kChanged;
    case api::file_system_provider::CHANGE_TYPE_DELETED:
      return crosapi::mojom::FSPChangeType::kDeleted;
    default:
      break;
  }
  NOTREACHED();
  return crosapi::mojom::FSPChangeType::kChanged;
}

// Convert the change from the IDL type to mojom type.
crosapi::mojom::FSPChangePtr ParseChange(
    const api::file_system_provider::Change& change) {
  crosapi::mojom::FSPChangePtr result = crosapi::mojom::FSPChange::New();
  result->path = base::FilePath::FromUTF8Unsafe(change.entry_path);
  result->type = ParseChangeType(change.change_type);
  return result;
}

// Converts a list of child changes from the IDL type to mojom type.
std::vector<crosapi::mojom::FSPChangePtr> ParseChanges(
    const std::vector<api::file_system_provider::Change>& changes) {
  std::vector<crosapi::mojom::FSPChangePtr> results;
  for (const auto& change : changes) {
    results.push_back(ParseChange(change));
  }
  return results;
}

}  // namespace

std::string FileSystemProviderBase::GetProviderId() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Terminal app is the only non-extension to use fsp.
  if (!extension()) {
    CHECK(url::IsSameOriginWith(GURL(source_url()),
                                GURL(chrome::kChromeUIUntrustedTerminalURL)));
    return guest_os::kTerminalSystemAppId;
  }
#endif
  return extension_id();
}

void FileSystemProviderBase::RespondWithError(const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool FileSystemProviderBase::MountFinishedInterfaceAvailable() {
  auto* service = chromeos::LacrosService::Get();
  return service->GetInterfaceVersion(
             crosapi::mojom::FileSystemProviderService::Uuid_) >=
         int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
                 kMountFinishedMinVersion};
}

bool FileSystemProviderBase::OperationFinishedInterfaceAvailable() {
  auto* service = chromeos::LacrosService::Get();
  return service->GetInterfaceVersion(
             crosapi::mojom::FileSystemProviderService::Uuid_) >=
         int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
                 kOperationFinishedMinVersion};
}

mojo::Remote<crosapi::mojom::FileSystemProviderService>&
FileSystemProviderBase::GetRemote() {
  auto* service = chromeos::LacrosService::Get();
  return service->GetRemote<crosapi::mojom::FileSystemProviderService>();
}
#endif

ExtensionFunction::ResponseAction FileSystemProviderMountFunction::Run() {
  using api::file_system_provider::Mount::Params;
  const absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the file system Id is empty.
  if (params->options.file_system_id.empty()) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  // It's an error if the display name is empty.
  if (params->options.display_name.empty()) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  // If the opened files limit is set, then it must be larger or equal than 0.
  if (params->options.opened_files_limit &&
      *params->options.opened_files_limit < 0) {
    return RespondNow(
        Error(FileErrorToString(base::File::FILE_ERROR_INVALID_OPERATION)));
  }

  bool persistent =
      params->options.persistent ? *params->options.persistent : true;
  crosapi::mojom::FileSystemMetadataPtr metadata =
      crosapi::mojom::FileSystemMetadata::New();
  metadata->file_system_id = crosapi::mojom::FileSystemId::New();
  metadata->file_system_id->provider = GetProviderId();
  metadata->file_system_id->id = params->options.file_system_id;
  metadata->display_name = params->options.display_name;
  metadata->writable =
      params->options.writable ? *params->options.writable : false;
  metadata->opened_files_limit = base::saturated_cast<uint32_t>(
      params->options.opened_files_limit ? *params->options.opened_files_limit
                                         : 0);
  metadata->supports_notify = params->options.supports_notify_tag
                                  ? *params->options.supports_notify_tag
                                  : false;

  auto callback =
      base::BindOnce(&FileSystemProviderMountFunction::RespondWithError, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->MountWithProfile(std::move(metadata), persistent, std::move(callback),
                         Profile::FromBrowserContext(browser_context()));
#else
  if (!OperationFinishedInterfaceAvailable())
    return RespondNow(Error(kInterfaceUnavailable));
  GetRemote()->Mount(std::move(metadata), persistent, std::move(callback));

#endif
  return RespondLater();
}

ExtensionFunction::ResponseAction FileSystemProviderUnmountFunction::Run() {
  using api::file_system_provider::Unmount::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto id = crosapi::mojom::FileSystemId::New();
  id->provider = GetProviderId();
  id->id = params->options.file_system_id;
  auto callback = base::BindOnce(
      &FileSystemProviderUnmountFunction::RespondWithError, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->UnmountWithProfile(std::move(id), std::move(callback),
                           Profile::FromBrowserContext(browser_context()));
#else
  if (!OperationFinishedInterfaceAvailable())
    return RespondNow(Error(kInterfaceUnavailable));
  GetRemote()->Unmount(std::move(id), std::move(callback));
#endif
  return RespondLater();
}

ExtensionFunction::ResponseAction FileSystemProviderGetAllFunction::Run() {
  auto callback =
      base::BindOnce(&FileSystemProviderGetAllFunction::RespondWithInfos, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->GetAllWithProfile(GetProviderId(), std::move(callback),
                          Profile::FromBrowserContext(browser_context()));
#else
  if (!OperationFinishedInterfaceAvailable())
    return RespondNow(Error(kInterfaceUnavailable));
  GetRemote()->GetAll(GetProviderId(), std::move(callback));
#endif
  return RespondLater();
}

void FileSystemProviderGetAllFunction::RespondWithInfos(
    std::vector<crosapi::mojom::FileSystemInfoPtr> infos) {
  using api::file_system_provider::FileSystemInfo;
  std::vector<FileSystemInfo> items;
  for (auto& info : infos) {
    items.push_back(ConvertFileSystemInfoMojomToExtension(std::move(info)));
  }
  Respond(
      ArgumentList(api::file_system_provider::GetAll::Results::Create(items)));
}

ExtensionFunction::ResponseAction FileSystemProviderGetFunction::Run() {
  using api::file_system_provider::Get::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto id = crosapi::mojom::FileSystemId::New();
  id->provider = GetProviderId();
  id->id = params->file_system_id;
  auto callback =
      base::BindOnce(&FileSystemProviderGetFunction::RespondWithInfo, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->GetWithProfile(std::move(id), std::move(callback),
                       Profile::FromBrowserContext(browser_context()));
#else
  if (!OperationFinishedInterfaceAvailable())
    return RespondNow(Error(kInterfaceUnavailable));
  GetRemote()->Get(std::move(id), std::move(callback));
#endif
  return RespondLater();
}

void FileSystemProviderGetFunction::RespondWithInfo(
    crosapi::mojom::FileSystemInfoPtr info) {
  using api::file_system_provider::FileSystemInfo;
  if (!info) {
    Respond(Error(FileErrorToString(base::File::FILE_ERROR_NOT_FOUND)));
    return;
  }
  auto result = ConvertFileSystemInfoMojomToExtension(std::move(info));
  Respond(ArgumentList(
      api::file_system_provider::Get::Results::Create(std::move(result))));
}

ExtensionFunction::ResponseAction FileSystemProviderNotifyFunction::Run() {
  using api::file_system_provider::Notify::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto callback =
      base::BindOnce(&FileSystemProviderNotifyFunction::RespondWithError, this);
  auto id = crosapi::mojom::FileSystemId::New();
  id->provider = GetProviderId();
  id->id = params->options.file_system_id;

  crosapi::mojom::FSPWatcherPtr watcher = crosapi::mojom::FSPWatcher::New();
  watcher->entry_path =
      base::FilePath::FromUTF8Unsafe(params->options.observed_path);
  watcher->recursive = params->options.recursive;
  watcher->last_tag = params->options.tag ? *params->options.tag : "";
  crosapi::mojom::FSPChangeType type =
      ParseChangeType(params->options.change_type);
  std::vector<crosapi::mojom::FSPChangePtr> changes;
  if (params->options.changes) {
    changes = ParseChanges(*params->options.changes);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->NotifyWithProfile(std::move(id), std::move(watcher), type,
                          std::move(changes), std::move(callback),
                          Profile::FromBrowserContext(browser_context()));
#else
  if (!OperationFinishedInterfaceAvailable())
    return RespondNow(Error(kInterfaceUnavailable));
  GetRemote()->Notify(std::move(id), std::move(watcher), type,
                      std::move(changes), std::move(callback));
#endif
  return RespondLater();
}

void FileSystemProviderNotifyFunction::OnNotifyCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error(FileErrorToString(result)));
    return;
  }

  Respond(NoArguments());
}

bool FileSystemProviderInternal::ForwardMountResult(int64_t request_id,
                                                    base::Value::List& args) {
  auto* profile = Profile::FromBrowserContext(browser_context());
  auto* sw_lifetime_manager =
      extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
          profile);
  sw_lifetime_manager->FinishRequest({
      extension_id(),
      /*file_system_id=*/"",
      request_id,
  });
  auto callback =
      base::BindOnce(&FileSystemProviderInternal::RespondWithError, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->MountFinishedWithProfile(extension_id(), request_id, std::move(args),
                                 std::move(callback), profile);
  return true;
#else
  if (!MountFinishedInterfaceAvailable())
    return false;
  GetRemote()->MountFinished(extension_id(), request_id, std::move(args),
                             std::move(callback));
  return true;
#endif
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalRespondToMountRequestFunction::Run() {
  using api::file_system_provider_internal::RespondToMountRequest::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t request_id = params->request_id;
  bool result = ForwardMountResult(request_id, mutable_args());
  if (!result)
    Respond(Error(kInterfaceUnavailable));
  return RespondLater();
}

bool FileSystemProviderInternal::ForwardOperationResultImpl(
    crosapi::mojom::FSPOperationResponse response,
    crosapi::mojom::FileSystemIdPtr file_system_id,
    int request_id,
    base::Value::List args) {
  auto* profile = Profile::FromBrowserContext(browser_context());
  auto* sw_lifetime_manager =
      extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
          profile);
  sw_lifetime_manager->FinishRequest({
      file_system_id->provider,
      file_system_id->id,
      request_id,
  });
  auto callback =
      base::BindOnce(&FileSystemProviderInternal::RespondWithError, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->file_system_provider_service_ash()
      ->OperationFinishedWithProfile(response, std::move(file_system_id),
                                     request_id, std::move(args),
                                     std::move(callback), profile);
  return true;
#else
  if (!OperationFinishedInterfaceAvailable()) {
    return false;
  }
  GetRemote()->OperationFinished(response, std::move(file_system_id),
                                 request_id, std::move(args),
                                 std::move(callback));
  return true;
#endif
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalUnmountRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kUnmountSuccess);
  if (!result)
    Respond(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetMetadataRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kGetEntryMetadataSuccess);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalGetActionsRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::GetActionsRequestedSuccess::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kGetActionsSuccess);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadDirectoryRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::ReadDirectoryRequestedSuccess::
      Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kReadDirectorySuccess);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalReadFileRequestedSuccessFunction::Run() {
  TRACE_EVENT0("file_system_provider", "ReadFileRequestedSuccess");
  using api::file_system_provider_internal::ReadFileRequestedSuccess::Params;

  // TODO(https://crbug.com/1314397): Improve performance by removing copy.
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kReadFileSuccess);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedSuccessFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedSuccess::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kGenericSuccess);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

ExtensionFunction::ResponseAction
FileSystemProviderInternalOperationRequestedErrorFunction::Run() {
  using api::file_system_provider_internal::OperationRequestedError::Params;
  absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->error == api::file_system_provider::PROVIDER_ERROR_OK) {
    // It's incorrect to pass OK as an error code.
    return ValidationFailure(this);
  }

  bool result = ForwardOperationResult(
      params, mutable_args(),
      crosapi::mojom::FSPOperationResponse::kGenericFailure);
  if (!result)
    return RespondNow(Error(kInterfaceUnavailable));
  return RespondLater();
}

}  // namespace extensions
