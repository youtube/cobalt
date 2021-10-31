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

namespace glimp {
namespace gles {

// An enum for the type of data stored for a specific vertex attribute.
enum VertexAttributeType {
  kVertexAttributeTypeByte,
  kVertexAttributeTypeUnsignedByte,
  kVertexAttributeTypeShort,
  kVertexAttributeTypeUnsignedShort,
  kVertexAttributeTypeHalfFloat,
  kVertexAttributeTypeFloat,
  kVertexAttributeTypeFixed,
  kVertexAttributeTypeInvalid,
};

// Stores all information that can be set about a vertex attribute through
// a call to VertexAttribPointer().
//   https://www.khronos.org/opengles/sdk/docs/man/xhtml/glVertexAttribPointer.xml
struct VertexAttributeArray {
  // Initialize with default values defined by the specification for
  // glVertexAttribPointer().
  VertexAttributeArray()
      : size(4),
        type(kVertexAttributeTypeFloat),
        normalized(false),
        stride_in_bytes(0),
        offset(0) {}

  VertexAttributeArray(int size,
                       VertexAttributeType type,
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
  VertexAttributeType type;

  // Whether each component should be normalized from their type's min/max range
  // to the range [-1, 1] for signed values and [0, 1] for unsigned values.
  bool normalized;

  // Number of bytes between subsequent vertices.
  int stride_in_bytes;

  // Derived from the void* ptr parameter of glVertexAttribPointer(), but stored
  // as an integer as only offsets into array buffers are supported in glimp.
  int offset;
};

// Constant vertex attributes are specified through the glVertexAttribXfv()
// functions.  When set for a specific attribute location, all vertices will
// be passed this constant attribute value.  Constant vertex attributes are
// enabled whenever vertex attribute arrays are disabled.
struct VertexAttributeConstant {
  VertexAttributeConstant() : type(kVertexAttributeTypeInvalid), size(0) {}

  VertexAttributeType type;
  int size;

  // We currently only support floating point constant vertex attributes,
  // but if needed this can be modified to be an arbitrary data store, and
  // methods added to convert to the specified type.
  float data[4];
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_VERTEX_ATTRIBUTE_H_
