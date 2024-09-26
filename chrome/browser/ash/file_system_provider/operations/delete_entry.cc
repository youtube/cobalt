// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/delete_entry.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace ash {
namespace file_system_provider {
namespace operations {

DeleteEntry::DeleteEntry(RequestDispatcher* dispatcher,
                         const ProvidedFileSystemInfo& file_system_info,
                         const base::FilePath& entry_path,
                         bool recursive,
                         storage::AsyncFileUtil::StatusCallback callback)
    : Operation(dispatcher, file_system_info),
      entry_path_(entry_path),
      recursive_(recursive),
      callback_(std::move(callback)) {}

DeleteEntry::~DeleteEntry() {
}

bool DeleteEntry::Execute(int request_id) {
  using extensions::api::file_system_provider::DeleteEntryRequestedOptions;

  if (!file_system_info_.writable())
    return false;

  DeleteEntryRequestedOptions options;
  options.file_system_id = file_system_info_.file_system_id();
  options.request_id = request_id;
  options.entry_path = entry_path_.AsUTF8Unsafe();
  options.recursive = recursive_;

  return SendEvent(
      request_id,
      extensions::events::FILE_SYSTEM_PROVIDER_ON_DELETE_ENTRY_REQUESTED,
      extensions::api::file_system_provider::OnDeleteEntryRequested::kEventName,
      extensions::api::file_system_provider::OnDeleteEntryRequested::Create(
          options));
}

void DeleteEntry::OnSuccess(int /* request_id */,
                            const RequestValue& /* result */,
                            bool has_more) {
  DCHECK(callback_);
  std::move(callback_).Run(base::File::FILE_OK);
}

void DeleteEntry::OnError(int /* request_id */,
                          const RequestValue& /* result */,
                          base::File::Error error) {
  DCHECK(callback_);
  std::move(callback_).Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
