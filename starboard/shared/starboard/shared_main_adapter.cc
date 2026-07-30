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

// Imports and calls into StarboardMain() from main(). Can be used to
// link into a shared Starboard executable to turn it into a traditional
// executable.

#include "starboard/configuration.h"

#undef main

extern "C" {

SB_IMPORT_PLATFORM int StarboardMain(int argc, char** argv);

int main(int argc, char** argv) {
  return StarboardMain(argc, argv);
}

}  // extern "C"
