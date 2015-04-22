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

#ifndef RENDER_TREE_RESOURCE_PROVIDER_H_
#define RENDER_TREE_RESOURCE_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/image.h"

namespace cobalt {
namespace render_tree {

// A ResourceProvider is a thread-safe class that is usually provided by
// a specific render_tree consumer.  Its purpose is to generate render_tree
// resources that can be attached to a render_tree that would subsequently be
// submitted to that specific render_tree consumer.  While it depends on the
// details of the ResourceProvider, it is very likely that resources created by
// a ResourceProvider that came from a specific render_tree consumer should only
// be submitted back to that same render_tree consumer.  The object should be
// thread-safe since it will be very common for resources to be created on one
// thread, but consumed on another.
class ResourceProvider {
 public:
  virtual ~ResourceProvider() {}

  // This method can be used to create an ImageData object.
  virtual scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                                  PixelFormat pixel_format,
                                                  AlphaFormat alpha_format) = 0;

  // This function will consume an ImageData object produced by a call to
  // AllocateImageData(), wrap it in a render_tree::Image that can be
  // used in a render tree, and return it to the caller.
  virtual scoped_refptr<Image> CreateImage(
      scoped_ptr<ImageData> pixel_data) = 0;

  // Given a set of font information, this method will return a font pre-loaded
  // on the system that fits the specified font parameters.
  virtual scoped_refptr<Font> GetPreInstalledFont(const char* font_family_name,
                                                  FontStyle font_style,
                                                  float font_size) = 0;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RESOURCE_PROVIDER_H_
