// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SPEECH_MICROPHONE_FAKE_H_
#define COBALT_SPEECH_MICROPHONE_FAKE_H_

#include "cobalt/speech/speech_configuration.h"

#if defined(ENABLE_FAKE_MICROPHONE)

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/speech/microphone.h"

namespace cobalt {
namespace speech {

// Fake microphone to mock the speech input by reading from the pre-recorded
// audio.
class MicrophoneFake : public Microphone {
 public:
  explicit MicrophoneFake(const Options& options);
  ~MicrophoneFake() override {}

  bool Open() override;
  int Read(char* out_data, int data_size) override;
  bool Close() override;
  int MinMicrophoneReadInBytes() override;
  bool IsValid() override { return is_valid_; }
  const char* Label() override { return ""; }

 private:
  base::ThreadChecker thread_checker_;

  bool read_data_from_file_;
  std::vector<FilePath> file_paths_;
  scoped_array<uint8> file_buffer_;
  int file_length_;
  int read_index_;
  bool is_valid_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // COBALT_SPEECH_MICROPHONE_FAKE_H_
