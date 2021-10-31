// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SANDBOX_FUZZER_APP_H_
#define COBALT_MEDIA_SANDBOX_FUZZER_APP_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/media/sandbox/zzuf_fuzzer.h"

namespace cobalt {
namespace media {
namespace sandbox {

// This class provides common functionalities required by fuzzer applications
// including command line parsing, file data collecting, and the fuzzing loop.
class FuzzerApp {
 public:
  FuzzerApp();

  bool Init(int argc, char* argv[], double min_ratio = 0.01f,
            double max_ratio = 0.05f);
  void RunFuzzingLoop();

  // This function parse |file_content| and returns the content used to fuzz.
  // The return value may be different than |file_content|.  It will be used to
  // generate the |fuzzing_content| parameter passed to Fuzz().
  // The derived class may also take this opportunity to initialize any other
  // data required by the fuzzing process and associate with |file_name|.
  // If the return value is empty, the file will be excluded during the fuzzing
  // process.
  virtual std::vector<uint8> ParseFileContent(
      const std::string& file_name, const std::vector<uint8>& file_content) = 0;
  virtual void Fuzz(const std::string& file_name,
                    const std::vector<uint8>& fuzzing_content) = 0;

 protected:
  ~FuzzerApp() {}

  const std::vector<uint8>& GetFileContent(const std::string& filename) const;

 private:
  struct FileEntry {
    std::vector<uint8> file_content;
    ZzufFuzzer fuzzer;

    FileEntry(const std::vector<uint8>& file_content,
              const std::vector<uint8>& fuzz_content, double min_ratio,
              double max_ratio, int initial_seed);
  };

  typedef std::map<std::string, FileEntry> FileEntries;

  bool ParseInitialSeedAndNumberOfIterations(int argc, char* argv[],
                                             int* initial_seed);
  void CollectFiles(const std::string& path_name, double min_ratio,
                    double max_ratio, int initial_seed);
  void AddFile(const std::string& file_name, double min_ratio, double max_ratio,
               int initial_seed);

  int number_of_iterations_;
  // Map from filename to file data.
  FileEntries file_entries_;
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_FUZZER_APP_H_
