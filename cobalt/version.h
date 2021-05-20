// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_VERSION_H_
#define COBALT_VERSION_H_

// The Cobalt version string.  Its components are:
//
// <feature year>.<purpose>.<update number>
//
// where,
//
//   feature year:  Cobalt features are tied to a yearly release cycle and this
//                  number indicates the yearly feature set that this version
//                  of Cobalt supports.
//
//   purpose:       The purpose of this build, usually named after the reason
//                  that a branch is cut.  On trunk it will be "master", and on
//                  LTS branches for example it will be "lts".
//
//   update number: The current update revision number (e.g. release number)
//                  for a given pair of values above.  This will always be 0 on
//                  trunk.  When a release branch is cut, this should be
//                  modified to start at 1, and be incremented each time a
//                  release is cut.
//.

#define COBALT_VERSION "22.master.0"

#endif  // COBALT_VERSION_H_
