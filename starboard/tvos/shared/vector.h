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

#ifndef STARBOARD_TVOS_SHARED_VECTOR_H_
#define STARBOARD_TVOS_SHARED_VECTOR_H_

/**
 *  @brief A 2D vector.
 */
typedef struct SBDVector {
  /**
   *  @brief The horizontal (X axis) component of the 2D vector.
   */
  float x;

  /**
   *  @brief The vertical (Y axis) component of the 2D vector.
   */
  float y;
} SBDVector;

/**
 *  @brief Constructs a new vector given both components.
 *  @param x The horizontal component of the vector.
 *  @param y the vertical component of the vector.
 */
static inline SBDVector SBDVectorMake(float x, float y) {
  SBDVector vector = {x, y};
  return vector;
}

#endif  // STARBOARD_TVOS_SHARED_VECTOR_H_
