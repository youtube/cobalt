// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/uwp/window_internal.h"

// TODO: Make sure the width and height here behave well given that we want
// 1080 video, but perhaps 4k UI where applicable.
SbWindowPrivate::SbWindowPrivate(const SbWindowOptions* /*options*/)
    : width(1920), height(1080) {}

SbWindowPrivate::~SbWindowPrivate() {}
