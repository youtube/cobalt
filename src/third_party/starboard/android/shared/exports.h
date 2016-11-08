// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_EXPORTS_H_
#define STARBOARD_ANDROID_SHARED_EXPORTS_H_

#include <stdlib.h>
#include <android/input.h>

struct args {
    const char* external_files_dir;
    const char* external_cache_dir;
    ANativeWindow *main_window;
    ANativeWindow *video_window;
    int sbToAppMsgRead;
    int sbToAppMsgWrite;
};

extern "C" {
SB_EXPORT_PLATFORM int starboard_initialize(void *args);
SB_EXPORT_PLATFORM void starboard_queue_input_event(AInputEvent* event);
}

#endif
