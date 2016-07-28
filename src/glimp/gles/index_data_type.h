/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLIMP_GLES_INDEX_DATA_TYPE_H_
#define GLIMP_GLES_INDEX_DATA_TYPE_H_

namespace glimp {
namespace gles {

// Defines the data type of each index element stored in an index buffer.  This
// will be passed into calls to DrawElements().
enum IndexDataType {
  kIndexDataTypeUnsignedByte,
  kIndexDataTypeUnsignedShort,
  kIndexDataTypeInvalid,
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_INDEX_DATA_TYPE_H_
