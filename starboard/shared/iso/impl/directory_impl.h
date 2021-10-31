// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_ISO_IMPL_DIRECTORY_IMPL_H_
#define STARBOARD_SHARED_ISO_IMPL_DIRECTORY_IMPL_H_

#include "starboard/configuration.h"
#include "starboard/directory.h"

#include "starboard/shared/internal_only.h"

// Ensure SbDirectory is typedef'd to a SbDirectoryPrivate* that has a directory
// field.
SB_COMPILE_ASSERT(sizeof(((SbDirectory)0)->directory), \
                  SbDirectoryPrivate_must_have_directory);

#endif  // STARBOARD_SHARED_ISO_IMPL_DIRECTORY_IMPL_H_
