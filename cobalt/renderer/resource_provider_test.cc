/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/threading/simple_thread.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/renderer_module.h"

#include "testing/gtest/include/gtest/gtest.h"

using cobalt::render_tree::ResourceProvider;

namespace cobalt {
namespace renderer {

namespace {
// This functor/thread is designed to simply allocate many textures as quickly
// in a row as possible.  Many threads should be capable of doing this
// simultaneously.
class CreateImagesThread : public base::SimpleThread {
 public:
  CreateImagesThread(ResourceProvider* resource_provider,
                     int num_images_to_create)
      : base::SimpleThread("CreateImages"),
        resource_provider_(resource_provider),
        num_images_to_create_(num_images_to_create) {}

  void Run() OVERRIDE {
    for (int i = 0; i < num_images_to_create_; ++i) {
      scoped_ptr<render_tree::ImageData> image_data =
          resource_provider_->AllocateImageData(
              math::Size(1, 1), render_tree::kPixelFormatRGBA8,
              render_tree::kAlphaFormatPremultiplied);

      resource_provider_->CreateImage(image_data.Pass());
    }
  }

 private:
  ResourceProvider* resource_provider_;
  int num_images_to_create_;
};

// This functor/thread will spawn the given number of CreateImagesThreads,
// execute them, and then wait for them to complete.  Upon completion, it will
// call the provided callback function.
class CreateImagesSpawnerThread : public base::SimpleThread {
 public:
  CreateImagesSpawnerThread(ResourceProvider* resource_provider,
                            int threads_to_create,
                            int images_to_create_per_thread,
                            base::Closure finished_callback)
      : base::SimpleThread("CreateImagesSpawner"),
        resource_provider_(resource_provider),
        threads_to_create_(threads_to_create),
        images_to_create_per_thread_(images_to_create_per_thread),
        finished_callback_(finished_callback) {}

  void Run() OVERRIDE {
    // First, create our threads.
    typedef std::vector<CreateImagesThread*> ThreadVector;
    ThreadVector image_creating_threads;
    for (int i = 0; i < threads_to_create_; ++i) {
      image_creating_threads.push_back(new CreateImagesThread(
          resource_provider_, images_to_create_per_thread_));
    }

    // Second, tell them all to begin executing at the same time.
    for (ThreadVector::iterator iter = image_creating_threads.begin();
         iter != image_creating_threads.end(); ++iter) {
      (*iter)->Start();
    }

    // Thirdly, wait for them to all finish executing and then destroy them.
    for (ThreadVector::iterator iter = image_creating_threads.begin();
         iter != image_creating_threads.end(); ++iter) {
      (*iter)->Join();
      delete *iter;
    }

    finished_callback_.Run();
  }

 private:
  ResourceProvider* resource_provider_;
  int threads_to_create_;
  int images_to_create_per_thread_;
  base::Closure finished_callback_;
};
}  // namespace

// The following test ensures that any thread can successfully create render
// tree images both at the same time and also while a graphics frame is started.
// This might be a problem in an OpenGL implementation for instance if we
// are naively setting ourselves as the current OpenGL context in order to
// create a texture, in which case another thread holding the context would
// be upset that it was taken away.  This test is essentially attempting to
// cause a crash or DCHECK() to be hit, if the test executes without crashing,
// everything is okay.
TEST(ResourceProviderTest, TexturesCanBeCreatedFromSecondaryThread) {
  scoped_ptr<backend::GraphicsSystem> graphics_system =
      backend::CreateDefaultGraphicsSystem();
  scoped_ptr<backend::GraphicsContext> graphics_context =
      graphics_system->CreateGraphicsContext();

  // When creating images from another thread, the rasterizer must be
  // constructed from a thread with a message loop.  This message loop will
  // be where all functions that access backend graphics resources are run.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  // Create the rasterizer using the platform default RenderModule options.
  RendererModule::Options render_module_options;
  scoped_ptr<Rasterizer> rasterizer =
      render_module_options.create_rasterizer_function.Run(
          graphics_context.get());

  // Create a dummy offscreen surface so that we can have a target when we start
  // a frame with the graphics context.
  const math::Size kDummySurfaceDimensions(1, 1);
  scoped_refptr<backend::RenderTarget> dummy_output_surface =
      graphics_context->CreateOffscreenRenderTarget(kDummySurfaceDimensions);

  scoped_ptr<backend::GraphicsContext::Frame> frame =
      graphics_context->StartFrame(dummy_output_surface);

  // Now that we're inside of a new frame, create images from a separate
  // thread.  This should be perfectly legal and cause no problems.
  const int kNumThreads = 20;
  const int kNumImagesCreatedPerThread = 300;

  // Create a thread to create other threads that will create images.
  CreateImagesSpawnerThread spawner_thread(
      rasterizer->GetResourceProvider(), kNumThreads,
      kNumImagesCreatedPerThread,
      base::Bind(&MessageLoop::PostTask, base::Unretained(&message_loop),
                 FROM_HERE, run_loop.QuitClosure()));
  spawner_thread.Start();

  // Run our message loop to process backend image creation/destruction
  // requests.
  run_loop.Run();

  spawner_thread.Join();
}

}  // namespace renderer
}  // namespace cobalt
