// Copyright 2015 Google Inc. All Rights Reserved.
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

// Includes all headers in a C context to make sure they compile as C files.

#include "starboard/atomic.h"
#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/directory.h"
#include "starboard/export.h"
#include "starboard/file.h"
#include "starboard/float.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/time_zone.h"
#include "starboard/types.h"
