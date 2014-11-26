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

  // Given raw pixel data, this method will wrap it in a render_tree::Image
  // that is returned and which can be inserted into a render_tree.
  virtual scoped_refptr<render_tree::Image> CreateImage(
      int width, int height, int pitch, scoped_array<uint8_t> pixel_data) = 0;

  // Used as a parameter to GetSystemFont() to describe the font style the
  // caller is seeking.
  enum FontStyle {
    kNormal,
    kBold,
    kItalic,
    kBoldItalic,
  };

  // Given a set of font information, this method will return a font pre-loaded
  // on the system that fits the specified font parameters.
  virtual scoped_refptr<render_tree::Font> GetPreInstalledFont(
      const char* font_family_name, FontStyle font_style, int font_size) = 0;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RESOURCE_PROVIDER_H_
