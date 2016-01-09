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

#ifndef GLIMP_GLES_VERTEX_ATTRIBUTE_H_
#define GLIMP_GLES_VERTEX_ATTRIBUTE_H_

// Stores all information that can be set about a vertex attribute through
// a call to VertexAttribPointer().
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glVertexAttribPointer.xml
struct VertexAttribute {
  // An enum for the type of data stored for a specific vertex attribute.
  enum Type {
    kTypeByte,
    kTypeUnsignedByte,
    kTypeShort,
    kTypeUnsignedShort,
    kTypeFloat,
    kTypeFixed,
    kTypeInvalid,
  };

  // Initialize with default values defined by the specification for
  // glVertexAttribPointer().
  VertexAttribute()
      : size(4),
        type(kTypeFloat),
        normalized(false),
        stride_in_bytes(0),
        offset(0) {}

  VertexAttribute(int size,
                  Type type,
                  bool normalized,
                  int stride_in_bytes,
                  int offset)
      : size(size),
        type(type),
        normalized(normalized),
        stride_in_bytes(stride_in_bytes),
        offset(offset) {}

  // Number of components per vertex
  int size;

  // The data type of each component.
  Type type;

  // Whether each component should be normalized from their type's min/max range
  // to the range [-1, 1] for signed values and [0, 1] for unsigned values.
  bool normalized;

  // Number of bytes between subsequent vertices.
  int stride_in_bytes;

  // Derived from the void* ptr parameter of glVertexAttribPointer(), but stored
  // as an integer as only offsets into array buffers are supported in glimp.
  int offset;
};

#endif  // GLIMP_GLES_VERTEX_ATTRIBUTE_H_
