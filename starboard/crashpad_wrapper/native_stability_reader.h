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

#ifndef STARBOARD_CRASHPAD_WRAPPER_NATIVE_STABILITY_READER_H_
#define STARBOARD_CRASHPAD_WRAPPER_NATIVE_STABILITY_READER_H_

#include "base/files/file_path.h"
#include "starboard/extension/native_stability.h"

namespace crashpad {

// Reads (pending and completed) crash and hang reports from the Crashpad
// database located at |database_directory_path| and populates |reports| with up
// to |max_num_reports| stability reports.
//
// Returns the number of reports populated, or -1 on failure.
int ReadReportsFromDatabase(const base::FilePath& database_directory_path,
                            SbNativeStabilityReport* reports,
                            int max_num_reports);

}  // namespace crashpad

#endif  // STARBOARD_CRASHPAD_WRAPPER_NATIVE_STABILITY_READER_H_
