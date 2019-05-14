// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/blitter.h"
#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/window.h"

#if SB_HAS(BLITTER)

class Application {
 public:
  Application();
  ~Application();

 private:
  // The callback function passed to SbEventSchedule().  Its purpose is to
  // forward to the non-static RenderScene() method.
  static void RenderSceneEventCallback(void* param);

  // Renders one frame of the animated scene, incrementing |frame_| each time
  // it is called.
  void RenderScene();

  static Application* application_;

  // The current frame we are rendering, initialized to 0 and incremented after
  // each frame.
  int frame_;

  // The SbWindow within which we will perform our rendering.
  SbWindow window_;

  // The blitting device we will be targeting.
  SbBlitterDevice device_;

  // The swap chain that represents |window_|'s display.
  SbBlitterSwapChain swap_chain_;
  static const int kOutputWidth = 1920;
  static const int kOutputHeight = 1080;

  // The surface of a simple unchanging RGBA bitmap image we will use while
  // rendering.
  SbBlitterSurface rgba_image_surface_;
  // The surface of a simple unchanging alpha-only bitmap image we will use
  // while rendering.
  SbBlitterSurface alpha_image_surface_;
  static const int kImageWidth = 100;
  static const int kImageHeight = 100;

  // An offscreen surface that we will render to and render from each frame.
  SbBlitterSurface offscreen_surface_;
  static const int kOffscreenWidth = 400;
  static const int kOffscreenHeight = 400;

  // The context within which we will issue all our draw commands.
  SbBlitterContext context_;
};

namespace {
// Populates a region of memory designated as pixel data with a gradient texture
// that includes transparent alpha.
void FillGradientImageData(int width,
                           int height,
                           int pitch_in_bytes,
                           void* pixel_data) {
  uint8_t* pixels = static_cast<uint8_t*>(pixel_data);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      // Setup a vertical gradient in the alpha color channel.
      float alpha = static_cast<float>(y) / height;
      uint8_t alpha_byte = static_cast<uint8_t>(alpha) * 255;

      // Setup a horizontal gradient in the green color channel.
      uint8_t green_byte =
          static_cast<uint8_t>((alpha * static_cast<float>(x) / width) * 255);

      // Assuming BGRA color format.
      pixels[x * 4 + 0] = 0;
      pixels[x * 4 + 1] = green_byte;
      pixels[x * 4 + 0] = 0;
      pixels[x * 4 + 3] = alpha_byte;
    }

    pixels += pitch_in_bytes;
  }
}

// Populates a region of memory designated as pixel data with a checker texture
// using an alpha-only pixel format.
void FillAlphaCheckerImageData(int width,
                               int height,
                               int pitch,
                               void* pixel_data) {
  uint8_t* pixels = static_cast<uint8_t*>(pixel_data);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      bool is_first_vertical_half = y <= height / 2;
      bool is_first_horizontal_half = x <= width / 2;

      uint8_t color =
          is_first_horizontal_half ^ is_first_vertical_half ? 255 : 0;

      pixels[x + y * pitch] = color;
    }
  }
}
}  // namespace

Application::Application() {
  frame_ = 0;
  // In order to access most of the Blitter API, we'll need to create a SbWindow
  // object whose display we can have the Blitter API target.
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  options.size.width = 1920;
  options.size.height = 1080;
  window_ = SbWindowCreate(NULL);
  SB_CHECK(SbWindowIsValid(window_));

  // We start by constructing a SbBlitterDevice which represents the connection
  // to the hardware blitting device (e.g. a GPU).
  device_ = SbBlitterCreateDefaultDevice();

  // Creating the swap chain associates our blitting device to the target
  // window's output display.
  swap_chain_ = SbBlitterCreateSwapChainFromWindow(device_, window_);

  // Let's setup a texture.  We start by creating a SbBlitterPixelData object
  // which can be populated with pixel data by the CPU and then passed on to
  // the device for reference within blit calls.
  SbBlitterPixelData image_data = SbBlitterCreatePixelData(
      device_, kImageWidth, kImageHeight, kSbBlitterPixelDataFormatBGRA8);

  // Once our pixel data object is created, we can extract from it the image
  // data pitch as well as a CPU-accessible pointer to the pixel data.  We
  // pass this information into a function to populate the image.
  int image_data_pitch = SbBlitterGetPixelDataPitchInBytes(image_data);
  FillGradientImageData(kImageHeight, kImageHeight, image_data_pitch,
                        SbBlitterGetPixelDataPointer(image_data));

  // Now that our pixel data is finalized, we create a surface from it.  After
  // this call, our SbBlitterPixelData object is invalid (i.e. it is transformed
  // into a SbBlitterSurface object).
  rgba_image_surface_ =
      SbBlitterCreateSurfaceFromPixelData(device_, image_data);

  // Now setup our alpha-only image.
  SbBlitterPixelData alpha_image_data = SbBlitterCreatePixelData(
      device_, kImageWidth, kImageHeight, kSbBlitterPixelDataFormatA8);
  int alpha_image_data_pitch =
      SbBlitterGetPixelDataPitchInBytes(alpha_image_data);
  FillAlphaCheckerImageData(kImageHeight, kImageHeight, alpha_image_data_pitch,
                            SbBlitterGetPixelDataPointer(alpha_image_data));
  alpha_image_surface_ =
      SbBlitterCreateSurfaceFromPixelData(device_, alpha_image_data);

  // We will also create a (initially blank) surface for use as an offscreen
  // render target.
  offscreen_surface_ = SbBlitterCreateRenderTargetSurface(
      device_, kOffscreenWidth, kOffscreenHeight, kSbBlitterSurfaceFormatRGBA8);

  // Finally, in order to issue draw calls, we need a context that maintains
  // draw state for us.
  context_ = SbBlitterCreateContext(device_);

  RenderScene();
}

Application::~Application() {
  // Cleanup all used resources.
  SbBlitterDestroyContext(context_);
  SbBlitterDestroySurface(offscreen_surface_);
  SbBlitterDestroySurface(alpha_image_surface_);
  SbBlitterDestroySurface(rgba_image_surface_);
  SbBlitterDestroySwapChain(swap_chain_);
  SbBlitterDestroyDevice(device_);
  SbWindowDestroy(window_);
}

void Application::RenderSceneEventCallback(void* param) {
  // Forward the call to the application instance specified as the parameter.
  Application* application = static_cast<Application*>(param);
  application->RenderScene();
}

void Application::RenderScene() {
  // Setup our animation parameter that follows a sawtooth pattern.
  int frame_mod_255 = frame_ % 255;

  // We can get a render target from a swap chain, so that we can target the
  // display with draw calls.
  SbBlitterRenderTarget primary_render_target =
      SbBlitterGetRenderTargetFromSwapChain(swap_chain_);

  // Surfaces created for use as a render target provide both a
  // SbBlitterRenderTarget and SbBlitterTexture objects, for use as target and
  // source in draw calls, respectively.
  SbBlitterRenderTarget offscreen_render_target =
      SbBlitterGetRenderTargetFromSurface(offscreen_surface_);

  // First we set the render target to our offscreen surface.
  SbBlitterSetRenderTarget(context_, offscreen_render_target);

  // And enable alpha blending.
  SbBlitterSetBlending(context_, true);

  // We start by clearing our entire surface to a animating shade of red.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(frame_mod_255, 0, 0, 255));
  SbBlitterFillRect(context_,
                    SbBlitterMakeRect(0, 0, kOffscreenWidth, kOffscreenHeight));
  // We then draw a green rectangle in the top left corner.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 255, 0, 32));
  SbBlitterFillRect(context_, SbBlitterMakeRect(50, 50, 100, 100));

  // Now we disable blending for the next few draw calls, resulting in their
  // alpha channels replacing the alpha channels that already exist in the
  // render target.
  SbBlitterSetBlending(context_, false);

  // Punch a blue half-transparent rectangle out in the top right corner.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 128));
  SbBlitterFillRect(context_, SbBlitterMakeRect(250, 50, 100, 100));

  // Render our surface to the offscreen surface as well, stretched
  // horizontally, in two different locations.
  SbBlitterSetBlending(context_, true);
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterBlitRectToRect(context_, rgba_image_surface_,
                          SbBlitterMakeRect(0, 0, kImageWidth, kImageHeight),
                          SbBlitterMakeRect(frame_mod_255, frame_mod_255,
                                            kImageWidth * 2, kImageHeight));

  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 255, 255, 128));
  SbBlitterBlitRectToRect(context_, rgba_image_surface_,
                          SbBlitterMakeRect(0, 0, kImageWidth, kImageHeight),
                          SbBlitterMakeRect(frame_mod_255, frame_mod_255 + 100,
                                            kImageWidth * 2, kImageHeight));

  // Blit out our alpha checker surface in the color blue.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 255));
  SbBlitterBlitRectToRect(
      context_, alpha_image_surface_,
      SbBlitterMakeRect(0, 0, kImageWidth, kImageHeight),
      SbBlitterMakeRect(50, 200, kImageWidth, kImageHeight));

  // Now switch to the primary display render target.
  SbBlitterSetRenderTarget(context_, primary_render_target);

  // Clear the display to an animating shade of green.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, frame_mod_255, 0, 255));
  SbBlitterFillRect(context_,
                    SbBlitterMakeRect(0, 0, kOutputWidth, kOutputHeight));

  // Render our offscreen surface to the display in three different places
  // and sizes.
  SbBlitterSetColor(context_,
                    SbBlitterColorFromRGBA(255, 255, 255, frame_mod_255));
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterBlitRectToRect(
      context_, offscreen_surface_,
      SbBlitterMakeRect(0, 0, kOffscreenWidth, kOffscreenHeight),
      SbBlitterMakeRect(10, 10, 200, 200));
  SbBlitterBlitRectToRect(
      context_, offscreen_surface_,
      SbBlitterMakeRect(0, 0, kOffscreenWidth, kOffscreenHeight),
      SbBlitterMakeRect(300, 10, 400, 400));
  SbBlitterBlitRectToRect(
      context_, offscreen_surface_,
      SbBlitterMakeRect(0, 0, kOffscreenWidth, kOffscreenHeight),
      SbBlitterMakeRect(10, 500, 800, 200));

  SbBlitterSetModulateBlitsWithColor(context_, false);

  // Blit an animated tiling of the offscreen surface.
  SbBlitterBlitRectToRectTiled(
      context_, offscreen_surface_,
      SbBlitterMakeRect(
          frame_mod_255 * 3, frame_mod_255 * 5,
          1 + static_cast<int>(kOffscreenWidth * (frame_mod_255 / 31.0f)),
          1 + static_cast<int>(kOffscreenWidth * (frame_mod_255 / 63.0f))),
      SbBlitterMakeRect(900, 100, 400, 400));

  // Blit a batch of 4 instances of the offscreen surface in one draw call.
  const int kNumBatchRects = 4;
  SbBlitterRect src_rects[kNumBatchRects];
  SbBlitterRect dst_rects[kNumBatchRects];
  for (int j = 0; j < kNumBatchRects; ++j) {
    src_rects[j].x = 0;
    src_rects[j].y = 0;
    src_rects[j].width = kOffscreenWidth;
    src_rects[j].height = kOffscreenHeight;

    dst_rects[j].x = 900 + j * 220;
    dst_rects[j].y = 600;
    dst_rects[j].width = 200;
    dst_rects[j].height = 300;
  }
  SbBlitterBlitRectsToRects(context_, offscreen_surface_, src_rects, dst_rects,
                            kNumBatchRects);

  // Ensure that all draw commands issued to the context are flushed to
  // the device and guaranteed to eventually be processed.
  SbBlitterFlushContext(context_);

  // Flip the swap chain to reveal our masterpiece to the user.
  SbBlitterFlipSwapChain(swap_chain_);
  ++frame_;

  // Schedule another frame render ASAP.
  SbEventSchedule(&Application::RenderSceneEventCallback, this, 0);
}

Application* s_application = NULL;

// Simple Starboard window event handling to kick off our blitter application.
void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      // Create the application, after which it will use SbEventSchedule()
      // on itself to trigger a frame update until the application is
      // terminated.
      s_application = new Application();
    } break;

    case kSbEventTypeStop: {
      // Shutdown the application.
      delete s_application;
    } break;

    default: {}
  }
}

#else  // SB_HAS(BLITTER)

void SbEventHandle(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart: {
      SB_LOG(ERROR)
          << "Starboard Blitter API is not available on this platform.";

      SbSystemRequestStop(0);
    } break;

    default: {}
  }
}

#endif  // SB_HAS(BLITTER)
