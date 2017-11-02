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

#ifndef COBALT_BUILD_BUILD_CONFIG_H_
#define COBALT_BUILD_BUILD_CONFIG_H_

#if COBALT_MEDIA_BUFFER_INITIAL_CAPACITY < 0
#error cobalt_media_buffer_initial_capacity has to be greater than or equal to 0
#endif  // COBALT_MEDIA_BUFFER_INITIAL_CAPACITY < 0

#if COBALT_MEDIA_BUFFER_ALLOCATION_UNIT < 0
#error cobalt_media_buffer_allocation_unit has to be greater than or equal to 0
#endif  // COBALT_MEDIA_BUFFER_ALLOCATION_UNIT < 0

#if COBALT_MEDIA_BUFFER_ALIGNMENT < 0
#error "cobalt_media_buffer_alignment has to be greater than or equal to 0."
#endif  // COBALT_MEDIA_BUFFER_ALIGNMENT < 0

#if COBALT_MEDIA_BUFFER_PADDING < 0
#error "cobalt_media_buffer_padding has to be greater than or equal to 0."
#endif  // COBALT_MEDIA_BUFFER_PADDING < 0

#if COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET < 8 * 1024 * 1024
#error cobalt_media_buffer_progressive_budget has to be greater than or equal \
           to 8 MB.
#endif  // COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET < 0

#if COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET <= 0
#error "cobalt_media_buffer_non_video_budget has to be greater than 0."
#endif  // COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET < 0

#if COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P <= 0
#error "cobalt_media_buffer_video_budget_1080p has to be greater than 0."
#endif  // COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P < 0

#if COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K < COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P
#error cobalt_media_buffer_video_budget_4k has to be greater than or equal to \
           cobalt_media_buffer_video_budget_1080p.
#endif  // COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K <
        //     COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P

#if COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P != 0
#if COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P <  \
    (COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P + \
     COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)
#error cobalt_media_buffer_max_capacity_1080p has to be greater than the sum \
           of cobalt_media_buffer_video_budget_1080p + \
           cobalt_media_buffer_non_video_budget
#endif  // COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P <
        // (COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P +
        // COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)
#endif  // COBALT_MEDIA_BUFFER_MAX_CAPACITY_1080P != 0

#if COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K != 0
#if COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K <  \
    (COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K + \
     COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)
#error cobalt_media_buffer_max_capacity_4k has to be greater than the sum of \
           cobalt_media_buffer_video_budget_4k + \
           cobalt_media_buffer_non_video_budget
#endif  // COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K <
        // (COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K +
        // COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET)
#endif  // COBALT_MEDIA_BUFFER_MAX_CAPACITY_4K != 0

#endif  // COBALT_BUILD_BUILD_CONFIG_H_
