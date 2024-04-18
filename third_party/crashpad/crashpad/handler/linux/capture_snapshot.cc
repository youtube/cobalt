// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "handler/linux/capture_snapshot.h"

#include <utility>

#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/sanitized/sanitization_information.h"
#include "util/misc/metrics.h"
#include "util/misc/tri_state.h"

namespace crashpad {

bool CaptureSnapshot(
    PtraceConnection* connection,
    const ExceptionHandlerProtocol::ClientInformation& info,
    const std::map<std::string, std::string>& process_annotations,
    uid_t client_uid,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    std::unique_ptr<ProcessSnapshotLinux>* snapshot,
    std::unique_ptr<ProcessSnapshotSanitized>* sanitized_snapshot) {
  std::unique_ptr<ProcessSnapshotLinux> process_snapshot(
      new ProcessSnapshotLinux());
#if defined(STARBOARD) || defined(NATIVE_TARGET_BUILD)
  if (!process_snapshot->Initialize(connection,
                                    info.evergreen_information_address,
                                    info.serialized_annotations_address,
                                    info.serialized_annotations_size,
                                    info.handler_start_type)) {
#else
  if (!process_snapshot->Initialize(connection)) {
#endif
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSnapshotFailed);
    return false;
  }

  pid_t local_requesting_thread_id = -1;
  if (requesting_thread_stack_address) {
    local_requesting_thread_id = process_snapshot->FindThreadWithStackAddress(
        requesting_thread_stack_address);
  }

  if (requesting_thread_id) {
    *requesting_thread_id = local_requesting_thread_id;
  }

  if (!process_snapshot->InitializeException(info.exception_information_address,
                                             local_requesting_thread_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kExceptionInitializationFailed);
    return false;
  }

  Metrics::ExceptionCode(process_snapshot->Exception()->Exception());

  CrashpadInfoClientOptions client_options;
  process_snapshot->GetCrashpadOptions(&client_options);
  if (client_options.crashpad_handler_behavior == TriState::kDisabled) {
    return false;
  }

  for (auto& p : process_annotations) {
    process_snapshot->AddAnnotation(p.first, p.second);
  }

  if (info.sanitization_information_address) {
    SanitizationInformation sanitization_info;
    ProcessMemoryRange range;
    if (!range.Initialize(connection->Memory(), connection->Is64Bit()) ||
        !range.Read(info.sanitization_information_address,
                    sizeof(sanitization_info),
                    &sanitization_info)) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kSanitizationInitializationFailed);
      return false;
    }

    auto annotations_whitelist = std::make_unique<std::vector<std::string>>();
    auto memory_range_whitelist =
        std::make_unique<std::vector<std::pair<VMAddress, VMAddress>>>();
    if (!ReadAnnotationsWhitelist(
            range,
            sanitization_info.annotations_whitelist_address,
            annotations_whitelist.get()) ||
        !ReadMemoryRangeWhitelist(
            range,
            sanitization_info.memory_range_whitelist_address,
            memory_range_whitelist.get())) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kSanitizationInitializationFailed);
      return false;
    }

    std::unique_ptr<ProcessSnapshotSanitized> sanitized(
        new ProcessSnapshotSanitized());
    if (!sanitized->Initialize(process_snapshot.get(),
                               sanitization_info.annotations_whitelist_address
                                   ? std::move(annotations_whitelist)
                                   : nullptr,
                               std::move(memory_range_whitelist),
                               sanitization_info.target_module_address,
                               sanitization_info.sanitize_stacks)) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kSkippedDueToSanitization);
      return false;
    }
    *sanitized_snapshot = std::move(sanitized);
  }

  *snapshot = std::move(process_snapshot);
  return true;
}

}  // namespace crashpad
