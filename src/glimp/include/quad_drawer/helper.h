// Copyright 2019 Google Inc. All Rights Reserved.
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

// The QuadDrawer interface implementation.

#ifndef GLIMP_INCLUDE_QUAD_DRAWER_HELPER_H_
#define GLIMP_INCLUDE_QUAD_DRAWER_HELPER_H_

#include "starboard/ps4/singleton.h"

class QuadDrawerHelper : public starboard::ps4::Singleton<QuadDrawerHelper> {
 public:
  typedef enum color_range {
    kStudioRange = 0,  // Y [16..235], UV [16..240]
    kFullRange = 1     // YUV/RGB [0..255]
  } color_range_t;

  typedef enum primary_id {
    kPrimaryBt709 = 0,
    kPrimaryBt2020 = 1
  } primary_id_t;

  struct QuadDrawerTarget {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
    unsigned int display_width;
    unsigned int display_height;
    unsigned char* planes[3];
    int stride[3];
    unsigned int bit_depth;
    color_range_t color_range;
    primary_id_t primary;
  };

  using QuadDrawerCallBack = void(void* context, void* target);

  void SetCallBack(void* context, QuadDrawerCallBack* quad_drawer_callback);
  QuadDrawerTarget* GetTarget();

 private:
  QuadDrawerTarget quad_drawer_target_ = {};
  QuadDrawerCallBack* quad_drawer_callback_ = nullptr;
  void* context_ = nullptr;
};

#endif  // GLIMP_INCLUDE_QUAD_DRAWER_HELPER_H_
