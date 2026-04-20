// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_TVOS_SHARED_MEDIA_DECODE_TARGET_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"

struct SbDecodeTargetPrivate {
  class Data : public starboard::RefCountedThreadSafe<Data> {
   public:
    virtual const SbDecodeTargetInfo& info() const = 0;

   protected:
    friend class starboard::RefCountedThreadSafe<Data>;
    virtual ~Data() {}
  };

  SbDecodeTargetPrivate(const starboard::scoped_refptr<Data>& data,
                        int64_t timestamp)
      : data(data), timestamp(timestamp) {}
  explicit SbDecodeTargetPrivate(const SbDecodeTargetPrivate& that)
      : data(that.data) {}

  starboard::scoped_refptr<Data> data;
  int64_t timestamp;
};

#endif  // STARBOARD_TVOS_SHARED_MEDIA_DECODE_TARGET_INTERNAL_H_
