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

#include "starboard/configuration.h"
#include "starboard/stub/application_stub.h"

int main(int argc, char** argv) {
#if SB_API_VERSION >= 15
  return SbRunStarboardMain(argc, argv, SbEventHandle);
#else
  starboard::stub::ApplicationStub application;
  return application.Run(argc, argv);
#endif  // SB_API_VERSION >= 15
}

#if SB_API_VERSION >= 15
int SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
  starboard::stub::ApplicationStub application(callback);
  return application.Run(argc, argv);
}
#endif  // SB_API_VERSION >= 15
