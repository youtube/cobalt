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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/test/scenes/all_scenes_combined_scene.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/benchmark.h"

using cobalt::math::Size;
using cobalt::math::SizeF;
using cobalt::render_tree::AlphaFormat;
using cobalt::render_tree::animations::AnimateNode;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::Node;
using cobalt::render_tree::PixelFormat;
using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::backend::Display;
using cobalt::renderer::backend::GraphicsContext;
using cobalt::renderer::backend::GraphicsSystem;
using cobalt::renderer::backend::RenderTarget;
using cobalt::renderer::backend::SurfaceInfo;
using cobalt::renderer::rasterizer::Rasterizer;
using cobalt::renderer::RendererModule;
using cobalt::renderer::test::scenes::AddBlankBackgroundToScene;
using cobalt::renderer::test::scenes::CreateAllScenesCombinedScene;
using cobalt::system_window::SystemWindow;

namespace {
const int kViewportWidth = 1920;
const int kViewportHeight = 1080;

scoped_ptr<Rasterizer> CreateDefaultRasterizer(
    GraphicsContext* graphics_context) {
  RendererModule::Options render_module_options;
  return render_module_options.create_rasterizer_function.Run(
      graphics_context, render_module_options);
}

// Allow test writers to choose whether the rasterizer results should be drawn
// on-screen or off-screen.  The major difference between the two is that when
// drawing to a display, we must wait for v-sync, and thus the framerate will
// be capped at 60fps.
enum OutputSurfaceType {
  kOutputSurfaceTypeDisplay,
  kOutputSurfaceTypeOffscreen,
};

typedef base::Callback<scoped_refptr<Node>(
    ResourceProvider*, const SizeF&, base::TimeDelta)> SceneCreateFunction;

// RunRenderTreeSceneBenchmark serves as a framework for render tree scene
// based benchmarks.  In other words, it makes it easy to test performance
// metrics given different render tree scenes.
void RunRenderTreeSceneBenchmark(SceneCreateFunction scene_create_function,
                                 OutputSurfaceType output_surface_type) {
  // Disable tracing so that we can iterate one round without recording
  // results.  This is to trigger any lazy initialization that may need to
  // be done.
  base::debug::TraceLog::GetInstance()->SetEnabled(false);

  // Setup our graphics system.
  scoped_ptr<GraphicsSystem> graphics_system =
      cobalt::renderer::backend::CreateDefaultGraphicsSystem();
  scoped_ptr<GraphicsContext> graphics_context =
      graphics_system->CreateGraphicsContext();

  // Create the rasterizer using the platform default RenderModule options.
  scoped_ptr<Rasterizer> rasterizer =
      CreateDefaultRasterizer(graphics_context.get());

  base::EventDispatcher event_dispatcher;
  scoped_ptr<SystemWindow> test_system_window;
  scoped_ptr<Display> test_display;
  scoped_refptr<RenderTarget> test_surface;
  if (output_surface_type == kOutputSurfaceTypeDisplay) {
    test_system_window.reset(new cobalt::system_window::SystemWindow(
        &event_dispatcher,
        cobalt::math::Size(kViewportWidth, kViewportHeight)));
    test_display = graphics_system->CreateDisplay(test_system_window.get());
    test_surface = test_display->GetRenderTarget();
  } else if (output_surface_type == kOutputSurfaceTypeOffscreen) {
    // Create our offscreen surface that will be the target of our test
    // rasterizations.
    const Size kTestOffscreenDimensions(1920, 1080);
    test_surface = graphics_context->CreateDownloadableOffscreenRenderTarget(
        kTestOffscreenDimensions);
  } else {
    DLOG(FATAL) << "Unknown output surface type.";
  }

  scoped_refptr<Node> scene =
      scene_create_function.Run(rasterizer->GetResourceProvider(),
                                test_surface->GetSize(), base::TimeDelta());

  const int kRenderIterationCount = 100;
  const float kFixedTimeStepInSecondsPerFrame = 0.016f;
  for (int i = 0; i < kRenderIterationCount; ++i) {
    AnimateNode* animate_node =
        base::polymorphic_downcast<AnimateNode*>(scene.get());
    scoped_refptr<Node> animated = animate_node
                                       ->Apply(base::TimeDelta::FromSecondsD(
                                           i * kFixedTimeStepInSecondsPerFrame))
                                       .animated->source();

    // Submit the render tree to be rendered.
    rasterizer->Submit(animated, test_surface);
    graphics_context->Finish();

    if (i == 0) {
      // Enable tracing again after one iteration has passed and any lazy
      // initializations are out of the way.
      base::debug::TraceLog::GetInstance()->SetEnabled(true);
    }
  }
}
}  // namespace

// Setup a quick macro so that we can measure the same events for each
// render tree builder benchmark.
#define RENDER_TREE_BUILDER_BENCHMARK(test_name)                              \
  TRACE_EVENT_BENCHMARK5(                                                     \
      test_name, "BuildRenderTree", cobalt::trace_event::IN_SCOPE_DURATION,   \
      "AnimateNode::Apply()", cobalt::trace_event::IN_SCOPE_DURATION,         \
      "Rasterizer::Submit()", cobalt::trace_event::FLOW_DURATION,             \
      "Rasterizer::Submit()", cobalt::trace_event::TIME_BETWEEN_EVENT_STARTS, \
      "VisitRenderTree", cobalt::trace_event::IN_SCOPE_DURATION)

// A catch-all test that excercises every different render tree node at the
// same time.  This is the same render tree builder used by the renderer
// sandbox.
RENDER_TREE_BUILDER_BENCHMARK(AllScenesCombinedOffscreenBenchmark) {
  RunRenderTreeSceneBenchmark(base::Bind(&CreateAllScenesCombinedScene),
                              kOutputSurfaceTypeOffscreen);
}

RENDER_TREE_BUILDER_BENCHMARK(AllScenesCombinedOnscreenBenchmark) {
  RunRenderTreeSceneBenchmark(base::Bind(&CreateAllScenesCombinedScene),
                              kOutputSurfaceTypeDisplay);
}

// This benchmark tracks how long it takes to load image data into a image
// via the render_tree::ResourceProvider interface provided by our default
// rasterizer.
namespace {
void SynthesizeImageData(ImageData* image_data) {
  TRACE_EVENT0("rasterizer_benchmark", "SynthesizeImageData");
  // Simply fill the entire image with a single arbitrarily chosen byte value.
  const uint8_t kFillValue = 127;
  int height = image_data->GetDescriptor().size.height();
  int pitch_in_bytes = image_data->GetDescriptor().pitch_in_bytes;
  for (int row = 0; row < height; ++row) {
    uint8_t* pixel_data = image_data->GetMemory() + row * pitch_in_bytes;
    for (int row_byte = 0; row_byte < pitch_in_bytes; ++row_byte) {
      pixel_data[row_byte] = kFillValue;
    }
  }
}

void RunCreateImageViaResourceProviderBenchmark(AlphaFormat alpha_format) {
  scoped_ptr<GraphicsSystem> graphics_system =
      cobalt::renderer::backend::CreateDefaultGraphicsSystem();
  scoped_ptr<GraphicsContext> graphics_context =
      graphics_system->CreateGraphicsContext();

  // Create the rasterizer using the platform default RenderModule options.
  scoped_ptr<Rasterizer> rasterizer =
      CreateDefaultRasterizer(graphics_context.get());

  ResourceProvider* resource_provider = rasterizer->GetResourceProvider();
  if (!resource_provider->AlphaFormatSupported(alpha_format)) {
    // Only run the test if the alpha format is supported.
    return;
  }

  const int kIterationCount = 20;
  const Size kImageSize(400, 400);
  for (int i = 0; i < kIterationCount; ++i) {
    // Repeatedly allocate memory for an image, write to that memory, and then
    // submit the image data back to the ResourceProvider to have it create
    // an image out of it.
    scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
        kImageSize, cobalt::render_tree::kPixelFormatRGBA8, alpha_format);

    SynthesizeImageData(image_data.get());

    resource_provider->CreateImage(image_data.Pass());
  }
}
}  // namespace

TRACE_EVENT_BENCHMARK3(
    CreateUnpremultipliedAlphaImageViaResourceProviderBenchmark,
    "ResourceProvider::AllocateImageData()",
    cobalt::trace_event::IN_SCOPE_DURATION, "ResourceProvider::CreateImage()",
    cobalt::trace_event::IN_SCOPE_DURATION, "SynthesizeImageData",
    cobalt::trace_event::IN_SCOPE_DURATION) {
  RunCreateImageViaResourceProviderBenchmark(
      cobalt::render_tree::kAlphaFormatUnpremultiplied);
}

TRACE_EVENT_BENCHMARK3(
    CreatePremultipliedAlphaImageViaResourceProviderBenchmark,
    "ResourceProvider::AllocateImageData()",
    cobalt::trace_event::IN_SCOPE_DURATION, "ResourceProvider::CreateImage()",
    cobalt::trace_event::IN_SCOPE_DURATION, "SynthesizeImageData",
    cobalt::trace_event::IN_SCOPE_DURATION) {
  RunCreateImageViaResourceProviderBenchmark(
      cobalt::render_tree::kAlphaFormatPremultiplied);
}
