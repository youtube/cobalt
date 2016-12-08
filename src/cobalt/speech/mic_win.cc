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

#include "cobalt/speech/mic.h"

namespace cobalt {
namespace speech {

class MicWin : public Mic {
 public:
  MicWin(int sample_rate, const DataReceivedCallback& data_received,
         const CompletionCallback& completion, const ErrorCallback& error)
      : Mic(sample_rate, data_received, completion, error) {}

  void Start() OVERRIDE { NOTIMPLEMENTED(); }
  void Stop() OVERRIDE { NOTIMPLEMENTED(); }
};

// static
scoped_ptr<Mic> Mic::Create(int sample_rate,
                            const DataReceivedCallback& data_received,
                            const CompletionCallback& completion,
                            const ErrorCallback& error) {
  return make_scoped_ptr<Mic>(
      new MicWin(sample_rate, data_received, completion, error));
}

}  // namespace speech
}  // namespace cobalt
