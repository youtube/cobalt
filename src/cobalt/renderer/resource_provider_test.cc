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

#include "base/threading/simple_thread.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/renderer_module.h"

#include "testing/gtest/include/gtest/gtest.h"

using cobalt::render_tree::ResourceProvider;

namespace cobalt {
namespace renderer {

namespace {
render_tree::PixelFormat ChoosePixelFormat(
    ResourceProvider* resource_provider) {
  if (resource_provider->PixelFormatSupported(render_tree::kPixelFormatRGBA8)) {
    return render_tree::kPixelFormatRGBA8;
  } else if (resource_provider->PixelFormatSupported(
                 render_tree::kPixelFormatBGRA8)) {
    return render_tree::kPixelFormatBGRA8;
  } else {
    return render_tree::kPixelFormatInvalid;
  }
}

// This functor/thread is designed to simply allocate many textures as quickly
// in a row as possible.  Many threads should be capable of doing this
// simultaneously.
class CreateImagesThread : public base::SimpleThread {
 public:
  CreateImagesThread(ResourceProvider* resource_provider,
                     int num_images_to_create, const math::Size& image_size)
      : base::SimpleThread("CreateImages"),
        resource_provider_(resource_provider),
        num_images_to_create_(num_images_to_create),
        image_size_(image_size) {}

  void Run() override {
    for (int i = 0; i < num_images_to_create_; ++i) {
      scoped_ptr<render_tree::ImageData> image_data =
          resource_provider_->AllocateImageData(
              image_size_, ChoosePixelFormat(resource_provider_),
              render_tree::kAlphaFormatPremultiplied);
      int num_bytes = image_data->GetDescriptor().pitch_in_bytes *
                      image_data->GetDescriptor().size.height();
      uint8* image_memory = image_data->GetMemory();
      for (int i = 0; i < num_bytes; ++i) {
        image_memory[i] = 0;
      }
      resource_provider_->CreateImage(image_data.Pass());
    }
  }

 private:
  ResourceProvider* resource_provider_;
  int num_images_to_create_;
  const math::Size image_size_;
};

// This functor/thread will spawn the given number of CreateImagesThreads,
// execute them, and then wait for them to complete.  Upon completion, it will
// call the provided callback function.
class CreateImagesSpawnerThread : public base::SimpleThread {
 public:
  CreateImagesSpawnerThread(ResourceProvider* resource_provider,
                            int threads_to_create,
                            int images_to_create_per_thread,
                            const math::Size& image_size,
                            base::Closure finished_callback)
      : base::SimpleThread("CreateImagesSpawner"),
        resource_provider_(resource_provider),
        threads_to_create_(threads_to_create),
        images_to_create_per_thread_(images_to_create_per_thread),
        image_size_(image_size),
        finished_callback_(finished_callback) {}

  void Run() override {
    // First, create our threads.
    typedef std::vector<CreateImagesThread*> ThreadVector;
    ThreadVector image_creating_threads;
    for (int i = 0; i < threads_to_create_; ++i) {
      image_creating_threads.push_back(new CreateImagesThread(
          resource_provider_, images_to_create_per_thread_, image_size_));
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
  const math::Size image_size_;
  base::Closure finished_callback_;
};
}  // namespace

class ResourceProviderTest : public testing::Test {
 public:
  ResourceProviderTest();
  ~ResourceProviderTest();

  // Lets the fixture know that it will run the run loop manually, and the
  // ResourceProviderTest destructor does not need to run it.
  void SetWillRunRunLoopManually() { run_run_loop_manually_ = true; }

 protected:
  MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_ptr<backend::GraphicsSystem> graphics_system_;
  scoped_ptr<backend::GraphicsContext> graphics_context_;
  scoped_ptr<rasterizer::Rasterizer> rasterizer_;

  bool run_run_loop_manually_;
};

ResourceProviderTest::ResourceProviderTest()
    : message_loop_(MessageLoop::TYPE_DEFAULT), run_run_loop_manually_(false) {
  graphics_system_ = backend::CreateDefaultGraphicsSystem();
  graphics_context_ = graphics_system_->CreateGraphicsContext();

  // Create the rasterizer using the platform default RenderModule options.
  RendererModule::Options render_module_options;
  rasterizer_ = render_module_options.create_rasterizer_function.Run(
      graphics_context_.get(), render_module_options);
}

ResourceProviderTest::~ResourceProviderTest() {
  if (!run_run_loop_manually_) {
    message_loop_.PostTask(FROM_HERE, run_loop_.QuitClosure());
    run_loop_.Run();
  }
}

// The following test ensures that any thread can successfully create render
// tree images both at the same time and also while a graphics frame is started.
// This might be a problem in an OpenGL implementation for instance if we
// are naively setting ourselves as the current OpenGL context in order to
// create a texture, in which case another thread holding the context would
// be upset that it was taken away.  This test is essentially attempting to
// cause a crash or DCHECK() to be hit, if the test executes without crashing,
// everything is okay.
TEST_F(ResourceProviderTest, TexturesCanBeCreatedFromSecondaryThread) {
  SetWillRunRunLoopManually();

  // Create a dummy offscreen surface so that we can have a target when we start
  // a frame with the graphics context.
  const math::Size kDummySurfaceDimensions(1, 1);
  scoped_refptr<backend::RenderTarget> dummy_output_surface =
      graphics_context_->CreateOffscreenRenderTarget(kDummySurfaceDimensions);

  // Now that we're inside of a new frame, create images from a separate
  // thread.  This should be perfectly legal and cause no problems.
  const int kNumThreads = 20;
  const int kNumImagesCreatedPerThread = 300;

  // Create a thread to create other threads that will create images.
  CreateImagesSpawnerThread spawner_thread(
      rasterizer_->GetResourceProvider(), kNumThreads,
      kNumImagesCreatedPerThread, math::Size(1, 1),
      base::Bind(&MessageLoop::PostTask, base::Unretained(&message_loop_),
                 FROM_HERE, run_loop_.QuitClosure()));
  spawner_thread.Start();

  // Run our message loop to process backend image creation/destruction
  // requests.
  run_loop_.Run();

  spawner_thread.Join();
}

// If textures never get passed to the renderer, then both allocating and
// releasing them should happen instantly.  In other words, this test ensures
// that we don't have a problem where texture allocations are instantaneous
// but texture deallocations are delayed.  This might be a problem if the
// video decoder is tracking its video frame texture memory usage to maintain
// up to N textures in memory at a time.  If it skips some frames, it might
// release them and then expect that it can allocate N more, but if those
// released frames have a delay on their release, we may have many more than
// N textures allocated at a time.
TEST_F(ResourceProviderTest, ManyTexturesCanBeCreatedAndDestroyedQuickly) {
  SetWillRunRunLoopManually();

  // Create a dummy offscreen surface so that we can have a target when we start
  // a frame with the graphics context.
  const math::Size kDummySurfaceDimensions(1, 1);
  scoped_refptr<backend::RenderTarget> dummy_output_surface =
      graphics_context_->CreateOffscreenRenderTarget(kDummySurfaceDimensions);

  // Now that we're inside of a new frame, create images from a separate
  // thread.  This should be perfectly legal and cause no problems.
  const int kNumThreads = 2;
  const int kNumImagesCreatedPerThread = 500;

  // Create a thread to create other threads that will create images.
  CreateImagesSpawnerThread spawner_thread(
      rasterizer_->GetResourceProvider(), kNumThreads,
      kNumImagesCreatedPerThread, math::Size(256, 256),
      base::Bind(&MessageLoop::PostTask, base::Unretained(&message_loop_),
                 FROM_HERE, run_loop_.QuitClosure()));
  spawner_thread.Start();

  // Run our message loop to process backend image creation/destruction
  // requests.
  run_loop_.Run();

  spawner_thread.Join();
}

// Test that we won't crash even if we attempt to create a very large number
// of textures.
TEST_F(ResourceProviderTest, NoCrashWhenManyTexturesAreAllocated) {
  render_tree::ResourceProvider* resource_provider =
      rasterizer_->GetResourceProvider();

  const int kNumImages = 5000;
  const math::Size kImageSize = math::Size(32, 32);

  scoped_ptr<render_tree::ImageData> image_datas[kNumImages];
  scoped_refptr<render_tree::Image> images[kNumImages];

  for (int i = 0; i < kNumImages; ++i) {
    image_datas[i] = resource_provider->AllocateImageData(
        kImageSize, ChoosePixelFormat(resource_provider),
        render_tree::kAlphaFormatOpaque);
  }

  for (int i = 0; i < kNumImages; ++i) {
    if (image_datas[i]) {
      images[i] = resource_provider->CreateImage(image_datas[i].Pass());
    }
  }

  for (int i = 0; i < kNumImages; ++i) {
    images[i] = NULL;
  }
}

// Test that we can attempt to allocate one massive image without crashing.
// There will likely be an out-of-memory issue when attempting to create the
// image.
TEST_F(ResourceProviderTest, NoCrashWhenMassiveTextureIsAllocated) {
  render_tree::ResourceProvider* resource_provider =
      rasterizer_->GetResourceProvider();

  const math::Size kImageSize = math::Size(16384, 16384);

  scoped_ptr<render_tree::ImageData> image_data =
      resource_provider->AllocateImageData(kImageSize,
                                           ChoosePixelFormat(resource_provider),
                                           render_tree::kAlphaFormatOpaque);
  if (image_data) {
    scoped_refptr<render_tree::Image> image =
        resource_provider->CreateImage(image_data.Pass());
  }
}

// This test is likely not to fail for any out-of-memory reasons, but rather
// maximum texture size reasons.
TEST_F(ResourceProviderTest,
       NoCrashWhenTextureWithOneLargeDimensionIsAllocated) {
  render_tree::ResourceProvider* resource_provider =
      rasterizer_->GetResourceProvider();

  const math::Size kImageSize = math::Size(1024 * 1024, 32);

  scoped_ptr<render_tree::ImageData> image_data =
      resource_provider->AllocateImageData(kImageSize,
                                           ChoosePixelFormat(resource_provider),
                                           render_tree::kAlphaFormatOpaque);
  if (image_data) {
    scoped_refptr<render_tree::Image> image =
        resource_provider->CreateImage(image_data.Pass());
  }
}

}  // namespace renderer
}  // namespace cobalt
