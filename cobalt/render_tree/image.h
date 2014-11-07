/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef RENDER_TREE_IMAGE_H_
#define RENDER_TREE_IMAGE_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace cobalt {
namespace render_tree {

// The Image type is an abstract base class that represents a stored image
// and all of its pixel information.  When constructing a render tree,
// external images can be introduced by adding an ImageNode and associating it
// with a specific Image object.  Examples of concrete Image objects include
// an Image that stores its pixel data in a CPU memory buffer, or one that
// stores its image data as a GPU texture.  Regardless, the concrete type of
// an Image objects is not relevant unless the Image is being constructed or
// it is being read by a rasterizer reading a submitted render tree.  Since
// the rasterizer may only be compatible with specific concrete Image types,
// functionality is provided through Downcast to downcast the image to a
// particular subtype.  In debug builds, RTTI is used to ensure the cast
// is safe.
class Image : public base::RefCountedThreadSafe<Image> {
 public:
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;

  // Provide functionality for downcasting Image
#if !defined(__LB_SHELL__FOR_RELEASE__)
  template <typename Derived>
  Derived* Downcast() {
    Derived* casted = dynamic_cast<Derived*>(this);
    DCHECK(casted);
    return casted;
  }
  template <typename Derived>
  const Derived* Downcast() const {
    const Derived* casted = dynamic_cast<const Derived*>(this);
    DCHECK(casted);
    return casted;
  }
#else
  // RTTI may not be enabled in release builds, so use a standard
  // static_cast.
  template <typename Derived>
  Derived* Downcast() {
    return static_cast<Derived*>(this);
  }
  template <typename Derived>
  const Derived* Downcast() const {
    return static_cast<const Derived*>(this);
  }
#endif

 protected:
  virtual ~Image() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Image>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_IMAGE_H_
