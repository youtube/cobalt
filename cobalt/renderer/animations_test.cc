// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/submission.h"

#include "testing/gtest/include/gtest/gtest.h"

using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::Image;
using cobalt::render_tree::ImageNode;
using cobalt::render_tree::ResourceProvider;

namespace cobalt {
namespace renderer {

namespace {
scoped_refptr<Image> CreateDummyImage(ResourceProvider* resource_provider) {
  // Initialize the image data and store a predictable, testable pattern
  // of image data into it.
  math::Size image_size(16, 16);

  int rgba_mapping[4] = {0, 1, 2, 3};
  render_tree::PixelFormat pixel_format = render_tree::kPixelFormatInvalid;
  if (resource_provider->PixelFormatSupported(render_tree::kPixelFormatRGBA8)) {
    pixel_format = render_tree::kPixelFormatRGBA8;
  } else if (resource_provider->PixelFormatSupported(
                 render_tree::kPixelFormatBGRA8)) {
    pixel_format = render_tree::kPixelFormatBGRA8;
    rgba_mapping[0] = 2;
    rgba_mapping[2] = 0;
  } else {
    NOTREACHED() << "Unsupported pixel format.";
  }

  scoped_ptr<render_tree::ImageData> image_data =
      resource_provider->AllocateImageData(
          image_size, pixel_format, render_tree::kAlphaFormatPremultiplied);
  for (int i = 0; i < image_size.width() * image_size.height(); ++i) {
    image_data->GetMemory()[i * 4 + rgba_mapping[0]] = 1;
    image_data->GetMemory()[i * 4 + rgba_mapping[1]] = 2;
    image_data->GetMemory()[i * 4 + rgba_mapping[2]] = 3;
    image_data->GetMemory()[i * 4 + rgba_mapping[3]] = 255;
  }

  // Create and return the new image.
  return resource_provider->CreateImage(image_data.Pass());
}

void AnimateImageNode(base::WaitableEvent* animate_has_started,
                      base::WaitableEvent* image_ready,
                      scoped_refptr<Image>* image, bool* first_animate,
                      ImageNode::Builder* image_node, base::TimeDelta time) {
  UNREFERENCED_PARAMETER(time);

  if (!*first_animate) {
    // We only do the test the first time this animation runs, ignore the
    // subsequent animate calls.
    return;
  }
  *first_animate = false;

  // Time to animate the image!  First signal that we are in the animation
  // callback which will prompt the CreateImageThread to create the image.
  animate_has_started->Signal();
  // Wait for the CreateImageThread to finish creating the image.
  image_ready->Wait();

  DCHECK(*image);

  // Animate the image node by setting its image to the newly created image.
  image_node->source = *image;

  // Reset the image reference to NULL so that it doesn't live on past the
  // lifetime of the pipeline within the main thread's reference.
  *image = NULL;
}

class CreateImageThread : public base::SimpleThread {
 public:
  CreateImageThread(base::WaitableEvent* animate_has_started,
                    base::WaitableEvent* image_ready,
                    scoped_refptr<Image>* image,
                    ResourceProvider* resource_provider)
      : base::SimpleThread("CreateImage"),
        animate_has_started_(animate_has_started),
        image_ready_(image_ready),
        image_(image),
        resource_provider_(resource_provider) {}

  void Run() override {
    // Wait until we receive the signal that the renderer has called the
    // animation callback.
    animate_has_started_->Wait();

    *image_ = CreateDummyImage(resource_provider_);

    DCHECK(*image_);

    // Signal to the animation callback that it can now reference the newly
    // created image.
    image_ready_->Signal();
  }

 private:
  base::WaitableEvent* animate_has_started_;
  base::WaitableEvent* image_ready_;
  scoped_refptr<Image>* image_;
  ResourceProvider* resource_provider_;
};

}  // namespace

// This test checks that while a client's animation callback code is being
// executed, there will be no problems in animating an ImageNode's Image such
// that it is set to an Image object that was created while the animation
// is executed.  Depending on the implementation of the renderer for the
// specific platform, there is a possibility of threading issues to manifest
// in this case, but it really should be working, as the media engine
// relies on this mechanism to deliver responsive video with minimal frame
// drops.
TEST(AnimationsTest, FreshlyCreatedImagesCanBeUsedInAnimations) {
  scoped_ptr<backend::GraphicsSystem> graphics_system =
      backend::CreateDefaultGraphicsSystem();
  scoped_ptr<backend::GraphicsContext> graphics_context =
      graphics_system->CreateGraphicsContext();

  // Initialize the debug context. This will cause GraphicsContextEGL to compute
  // whether or not the pixels require a vertical flip, fixing a crash when the
  // vertical flip requirement is determined after the pipelines are created.
  // The root cause of the crash has yet to be discovered.
  graphics_context->InitializeDebugContext();

  // Create a dummy offscreen surface so that we can have a target when we start
  // a frame with the graphics context.
  const math::Size kDummySurfaceDimensions(1, 1);
  scoped_refptr<backend::RenderTarget> dummy_output_surface =
      graphics_context->CreateDownloadableOffscreenRenderTarget(
          kDummySurfaceDimensions);

  const int kNumTestTrials = 5;
  for (int i = 0; i < kNumTestTrials; ++i) {
    // Setup some synchronization objects so we can ensure the image is created
    // while the animation callback is being executed, and also that the image
    // is referenced only after it is created.  It is important that these
    // objects outlive the Pipeline object (created below), since they will
    // be referenced from another thread created within the Pipeline object.
    base::WaitableEvent animate_has_started(true, false);
    base::WaitableEvent image_ready(true, false);
    scoped_refptr<Image> image;

    // Create the rasterizer using the platform default RenderModule options.
    RendererModule::Options render_module_options;
    Pipeline pipeline(
        base::Bind(render_module_options.create_rasterizer_function,
                   graphics_context.get(), render_module_options),
        dummy_output_surface, NULL, true, Pipeline::kNoClear);

    // Our test render tree will consist of only a single ImageNode.
    scoped_refptr<ImageNode> test_node = new ImageNode(
        scoped_refptr<Image>(CreateDummyImage(pipeline.GetResourceProvider())),
        math::RectF(1.0f, 1.0f));

    // Animate the ImageNode and pass in our callback function to be executed
    // upon render_tree animation.
    AnimateNode::Builder animations;
    bool first_animate = true;
    animations.Add(test_node,
                   base::Bind(&AnimateImageNode, &animate_has_started,
                              &image_ready, &image, &first_animate));

    // Setup a separate thread to be responsible for creating the image, so that
    // we can guarantee that this operation will occur on a separate thread.
    CreateImageThread create_image_thread(&animate_has_started, &image_ready,
                                          &image,
                                          pipeline.GetResourceProvider());
    create_image_thread.Start();

    // Submit the render tree and animation to the rendering pipeline for
    // rasterization (and the execution of our animation callback).
    pipeline.Submit(Submission(new AnimateNode(animations, test_node),
                               base::Time::Now() - base::Time::UnixEpoch()));

    // Wait for all events that we have planned to occur.
    animate_has_started.Wait();
    image_ready.Wait();

    create_image_thread.Join();
  }

  // Verify that the image data was read and rendered correctly.
  scoped_array<uint8_t> rendered_data =
      graphics_context->DownloadPixelDataAsRGBA(dummy_output_surface);
  const int kNumRenderedPixels =
      kDummySurfaceDimensions.width() * kDummySurfaceDimensions.height();
  for (int i = 0; i < kNumRenderedPixels; ++i) {
    EXPECT_EQ(1, rendered_data[i * 4 + 0]);
    EXPECT_EQ(2, rendered_data[i * 4 + 1]);
    EXPECT_EQ(3, rendered_data[i * 4 + 2]);
    EXPECT_EQ(255, rendered_data[i * 4 + 3]);
  }
}

}  // namespace renderer
}  // namespace cobalt
