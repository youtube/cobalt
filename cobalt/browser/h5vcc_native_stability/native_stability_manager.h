// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_H5VCC_NATIVE_STABILITY_NATIVE_STABILITY_MANAGER_H_
#define COBALT_BROWSER_H5VCC_NATIVE_STABILITY_NATIVE_STABILITY_MANAGER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/browser/h5vcc_native_stability/public/mojom/h5vcc_native_stability.mojom.h"
#include "starboard/extension/native_stability.h"

namespace h5vcc_native_stability {

// Communicates with the platform's crash reporting system and manages ancillary
// storage in order to provide native stability reports requested by the
// H5vccNativeStability Mojo interface. This is a singleton class and its
// lifetime is scoped to the application.
//
// Threading Model:
// - Calls from Mojo (GetPendingReports, AcknowledgeReports) must originate on
//   the UI sequence, and their response callbacks are posted back to the UI
//   sequence.
// - All other public methods (GetInstance, ArmCrashUuidAnnotation,
//   RecordHangStarted, RecordHangRecovered, PruneStorage) are thread-safe and
//   may be called from any thread.
//
// Blocking disk I/O is offloaded to a background SequencedTaskRunner. Using a
// SequencedTaskRunner guarantees that disk operations execute sequentially,
// preventing concurrent reads and writes.
class NativeStabilityManager {
 public:
  static NativeStabilityManager* GetInstance();

  NativeStabilityManager(const NativeStabilityManager&) = delete;
  NativeStabilityManager& operator=(const NativeStabilityManager&) = delete;

  // Generates a new UUID to identify the crash for this application lifecycle,
  // if one is to occur, and sets the |native_stability_crash_uuid| annotation
  // with it.
  void ArmCrashUuidAnnotation();

  // Asynchronously records that a hang with the given UUID has started,
  // persisting an unrecovered status for the hang to disk. Expected to be
  // called exactly once for a given hang, before any call to
  // RecordHangRecovered.
  void RecordHangStarted(const std::string& hang_uuid);

  // Asynchronously updates the status of a hang with the given UUID to
  // recovered on disk. Expected to be called at most once for a given hang,
  // sometime after a call to RecordHangStarted.
  void RecordHangRecovered(const std::string& hang_uuid);

  // Asynchronously reads and summarizes stability reports stored on disk that
  // have not previously been acknowledged.
  //
  // Because reading files from disk is a blocking operation, the disk I/O is
  // offloaded to a background ThreadPool task runner to prevent blocking the
  // browser's UI message loop. The results are then posted back to the UI
  // sequence to fulfill the Mojo response |callback|.
  void GetPendingReports(
      base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
          callback);

  // Asynchronously acknowledges stability reports to exclude them from future
  // calls to GetPendingReports().
  //
  // The provided UUIDs are persisted to disk so that their acknowledged status
  // persists across application lifecycles.
  //
  // As with GetPendingReports(), disk I/O is offloaded to a background
  // ThreadPool task runner and callbacks are posted back to the UI sequence.
  //
  // Concurrent execution by different Cobalt processes will not produce invalid
  // acknowledged state but could cause read-modify-write races with the last
  // write winning.
  // TODO(b/528362453): Consider implementing mutual process exclusion. Since
  // this is most likely to occur with Cobalt processes running different web
  // applications, we may do this by partitioning the file by web app.
  void AcknowledgeReports(std::vector<std::string> native_stability_event_uuids,
                          base::OnceClosure callback);

  // Asynchronously removes acknowledged report UUIDs and hang attributes from
  // disk if their corresponding stability reports are no longer stored on disk.
  //
  // Note that stability report storage is managed by the platform's crash
  // reporting system, which is expected to implement its own pruning strategy.
  // We prune by reflecting that system's state.
  //
  // As with other disk operations, disk I/O is offloaded to a background
  // ThreadPool task runner.
  void PruneStorage();

  using GetExtensionCallback =
      base::RepeatingCallback<const void*(const char*)>;

  // Injects a custom Starboard extension getter callback for testing.
  void SetGetExtensionForTesting(GetExtensionCallback get_extension_callback);

  // Injects a custom path for acked_event_uuids.json for testing.
  void SetAckedUuidsFilePathForTesting(base::FilePath file_path);

  // Injects a custom path for hang_attributes.json for testing.
  void SetHangAttributesFilePathForTesting(base::FilePath file_path);

  // Resets internal state for unit testing between test cases.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<NativeStabilityManager>;

  NativeStabilityManager();
  ~NativeStabilityManager() = default;

  // Thread-safe helper that lazily creates and returns the background
  // SequencedTaskRunner used for sequential disk I/O.
  scoped_refptr<base::SequencedTaskRunner> GetOrCreateTaskRunner();

  void RecordHangStartedOnTaskRunner(std::string hang_uuid);
  void RecordHangRecoveredOnTaskRunner(std::string hang_uuid);

  void GetPendingReportsOnTaskRunner(
      const StarboardExtensionNativeStabilityApi* native_stability_extension,
      base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
          callback);

  void AcknowledgeReportsOnTaskRunner(
      std::vector<std::string> native_stability_event_uuids,
      base::OnceClosure callback);

  void PruneStorageOnTaskRunner(
      const StarboardExtensionNativeStabilityApi* native_stability_extension);

  base::FilePath GetAckedUuidsFilePath();
  base::FilePath GetHangAttributesFilePath();

  const void* GetExtension(const char* name);

  base::Lock task_runner_lock_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_
      GUARDED_BY(task_runner_lock_);

  GetExtensionCallback get_extension_callback_for_testing_;
  base::FilePath acked_uuids_file_path_for_testing_;
  base::FilePath hang_attributes_file_path_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace h5vcc_native_stability

#endif  // COBALT_BROWSER_H5VCC_NATIVE_STABILITY_NATIVE_STABILITY_MANAGER_H_
