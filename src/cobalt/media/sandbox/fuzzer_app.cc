/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/media/sandbox/fuzzer_app.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "starboard/directory.h"
#include "starboard/string.h"

namespace cobalt {
namespace media {
namespace sandbox {

FuzzerApp::FuzzerApp() : number_of_iterations_(-1) {}

bool FuzzerApp::Init(int argc, char* argv[], double min_ratio,
                     double max_ratio) {
  file_entries_.clear();

  int initial_seed;
  if (ParseInitialSeedAndNumberOfIterations(argc, argv, &initial_seed)) {
    CollectFiles(argv[argc - 1], min_ratio, max_ratio, initial_seed);
  }
  return !file_entries_.empty() && number_of_iterations_ > 0;
}

void FuzzerApp::RunFuzzingLoop() {
  DCHECK_GT(number_of_iterations_, 0);

  for (int i = 0; i < number_of_iterations_; ++i) {
    for (FileEntries::iterator iter = file_entries_.begin();
         iter != file_entries_.end(); ++iter) {
      LOG(INFO) << "Fuzzing \"" << iter->first << "\" "
                << "with seed " << iter->second.fuzzer.seed();

      Fuzz(iter->first, iter->second.fuzzer.GetFuzzedContent());
      iter->second.fuzzer.AdvanceSeed();
    }
  }
}

const std::vector<uint8>& FuzzerApp::GetFileContent(
    const std::string& filename) const {
  FileEntries::const_iterator iter = file_entries_.find(filename);
  DCHECK(iter != file_entries_.end());
  return iter->second.file_content;
}

bool FuzzerApp::ParseInitialSeedAndNumberOfIterations(int argc, char* argv[],
                                                      int* initial_seed) {
  DCHECK(initial_seed);

  *initial_seed = ZzufFuzzer::kSeedForOriginalContent;

  if (argc != 3 && argc != 4) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " [initial seed (non-negative integer)]"
               << " <number of iterations>"
               << " <file name|directory name contains files to be fuzzed>";
    LOG(ERROR) << "For example: " << argv[0] << " 200000 /data/video-files";
    LOG(ERROR) << "             " << argv[0] << " 115 200000 /data/video-files";
    return false;
  }

  if (argc == 3) {
    *initial_seed = ZzufFuzzer::kSeedForOriginalContent;
    number_of_iterations_ = SbStringParseSignedInteger(argv[1], NULL, 10);

    if (number_of_iterations_ <= 0) {
      LOG(ERROR) << "Invalid 'number of iterations' " << argv[1];
      return false;
    }
  } else {
    DCHECK_EQ(argc, 4);
    *initial_seed = SbStringParseSignedInteger(argv[1], NULL, 10);

    if (*initial_seed < 0) {
      LOG(ERROR) << "Invalid 'initial seed' " << argv[1];
      return false;
    }

    number_of_iterations_ = SbStringParseSignedInteger(argv[2], NULL, 10);

    if (number_of_iterations_ <= 0) {
      LOG(ERROR) << "Invalid 'number of iterations' " << argv[2];
      return false;
    }
  }

  return true;
}

void FuzzerApp::CollectFiles(const std::string& path_name, double min_ratio,
                             double max_ratio, int initial_seed) {
  file_entries_.clear();

  SbDirectory directory = SbDirectoryOpen(path_name.c_str(), NULL);
  if (!SbDirectoryIsValid(directory)) {
    // Assuming it is a file.
    AddFile(path_name, min_ratio, max_ratio, initial_seed);
    return;
  }

  SbDirectoryEntry entry;
  while (SbDirectoryGetNext(directory, &entry)) {
    std::string file_name = path_name + SB_FILE_SEP_STRING + entry.name;
    AddFile(file_name, min_ratio, max_ratio, initial_seed);
  }

  SbDirectoryClose(directory);
}

void FuzzerApp::AddFile(const std::string& file_name, double min_ratio,
                        double max_ratio, int initial_seed) {
  LOG(INFO) << "Loading " << file_name;

  std::string content;
  if (!file_util::ReadFileToString(FilePath(file_name), &content)) {
    LOG(ERROR) << "Failed to load file " << file_name;
    return;
  }
  if (content.empty()) {
    LOG(ERROR) << file_name << " is empty";
    return;
  }
  std::vector<uint8> uint8_content(content.begin(), content.end());
  std::vector<uint8> parsed_content =
      ParseFileContent(file_name, uint8_content);
  if (parsed_content.empty()) {
    return;
  }
  file_entries_.insert(
      std::make_pair(file_name, FileEntry(uint8_content, parsed_content,
                                          min_ratio, max_ratio, initial_seed)));
}

FuzzerApp::FileEntry::FileEntry(const std::vector<uint8>& file_content,
                                const std::vector<uint8>& fuzz_content,
                                double min_ratio, double max_ratio,
                                int initial_seed)
    : file_content(file_content),
      fuzzer(fuzz_content, min_ratio, max_ratio, initial_seed) {}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
