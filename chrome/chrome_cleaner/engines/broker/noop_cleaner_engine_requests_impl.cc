// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/chrome_cleaner/engines/broker/cleaner_engine_requests_impl.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"

namespace chrome_cleaner {

std::unique_ptr<chrome_cleaner::FileRemoverAPI>
CreateFileRemoverWithDigestVerifier(
    std::unique_ptr<ZipArchiver> archiver,
    const base::RepeatingClosure& reboot_needed_callback) {
  return nullptr;
}

CleanerEngineRequestsImpl::CleanerEngineRequestsImpl(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    InterfaceMetadataObserver* metadata_observer,
    std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover)
    : mojo_task_runner_(mojo_task_runner),
      metadata_observer_(metadata_observer),
      file_remover_(std::move(file_remover)) {}

CleanerEngineRequestsImpl::~CleanerEngineRequestsImpl() = default;

void CleanerEngineRequestsImpl::Bind(
    mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests>* remote) {
  receiver_.Bind(remote->InitWithNewEndpointAndPassReceiver());
  // There's no need to call set_disconnect_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
}

void CleanerEngineRequestsImpl::SandboxDeleteFile(
    const base::FilePath& file_name,
    SandboxDeleteFileCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxDeleteFilePostReboot(
    const base::FilePath& file_name,
    SandboxDeleteFilePostRebootCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxNtDeleteRegistryKey(
    const WStringEmbeddedNulls& key,
    SandboxNtDeleteRegistryKeyCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxNtDeleteRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name,
    SandboxNtDeleteRegistryValueCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxNtChangeRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name,
    const WStringEmbeddedNulls& new_value,
    SandboxNtChangeRegistryValueCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxDeleteService(
    const std::wstring& name,
    SandboxDeleteServiceCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxDeleteTask(
    const std::wstring& name,
    SandboxDeleteServiceCallback result_callback) {
  CHECK(false);
}

void CleanerEngineRequestsImpl::SandboxTerminateProcess(
    uint32_t process_id,
    SandboxTerminateProcessCallback result_callback) {
  CHECK(false);
}

}  // namespace chrome_cleaner
