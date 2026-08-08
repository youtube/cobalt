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

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
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
// Calls must originate on the UI sequence. Offloads blocking disk I/O to a
// background task runner and posts response callbacks back to the UI sequence.
class NativeStabilityManager {
 public:
  static NativeStabilityManager* GetInstance();

  NativeStabilityManager(const NativeStabilityManager&) = delete;
  NativeStabilityManager& operator=(const NativeStabilityManager&) = delete;

  // Generates a new UUID to identify the crash for this application lifecycle,
  // if one is to occur, and sets the |native_stability_crash_uuid| annotation
  // with it.
  void ArmCrashUuidAnnotation();

  void RecordHangStarted(const std::string& hang_uuid);
  void RecordHangRecovered(const std::string& hang_uuid);

  // Asynchronously reads and summarizes stability reports stored on disk.
  //
  // Because reading files from disk is a blocking operation, the disk I/O is
  // offloaded to a background ThreadPool task runner to prevent blocking the
  // browser's UI message loop. The results are then posted back to the UI
  // sequence to fulfill the Mojo response |callback|.
  void GetPendingReports(
      base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
          callback);

  using GetExtensionCallback =
      base::RepeatingCallback<const void*(const char*)>;

  // Inject a custom Starboard extension getter callback for testing.
  void SetGetExtensionForTesting(GetExtensionCallback get_extension_callback);

  // Resets internal state for unit testing between test cases.
  void ResetForTesting();

 private:
  friend class base::NoDestructor<NativeStabilityManager>;

  NativeStabilityManager();
  ~NativeStabilityManager() = default;

  void GetPendingReportsOnTaskRunner(
      const StarboardExtensionNativeStabilityApi* native_stability_extension,
      base::OnceCallback<void(std::vector<mojom::NativeStabilityReportPtr>)>
          callback);

  const void* GetExtension(const char* name);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  GetExtensionCallback get_extension_callback_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace h5vcc_native_stability

#endif  // COBALT_BROWSER_H5VCC_NATIVE_STABILITY_NATIVE_STABILITY_MANAGER_H_
