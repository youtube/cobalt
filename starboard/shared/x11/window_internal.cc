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

#include "starboard/shared/x11/window_internal.h"

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>

#include "starboard/configuration.h"
#include "starboard/double.h"
#include "starboard/log.h"
#include "starboard/player.h"

namespace {

const int kWindowWidth = 1920;
const int kWindowHeight = 1080;

bool HasNeededExtensionsForPunchedOutVideo(Display* display) {
  int dummy;
  return (XRenderQueryExtension(display, &dummy, &dummy) &&
          XCompositeQueryExtension(display, &dummy, &dummy));
}

}  // namespace

SbWindowPrivate::SbWindowPrivate(Display* display,
                                 const SbWindowOptions* options)
    : window(None),
      window_picture(None),
      composition_pixmap(None),
      composition_picture(None),
      video_pixmap(None),
      video_pixmap_width(0),
      video_pixmap_height(0),
      video_pixmap_gc(None),
      video_picture(None),
      gl_window(None),
      gl_picture(None),
      display(display),
      unhandled_resize(false) {
  // Request a 32-bit depth visual for our Window.
  XVisualInfo x_visual_info = {0};
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor,
                   &x_visual_info);

  Window root_window = RootWindow(display, x_visual_info.screen);

  width = kWindowWidth;
  height = kWindowHeight;
  if (options && options->size.width > 0 && options->size.height > 0) {
    width = options->size.width;
    height = options->size.height;
  }

  XSetWindowAttributes swa = {0};
  swa.event_mask =
      KeyPressMask | KeyReleaseMask | StructureNotifyMask | FocusChangeMask;
  swa.colormap =
      XCreateColormap(display, root_window, x_visual_info.visual, AllocNone);
  // Setting border_pixel to 0 is required if the requested window depth (e.g.
  // 32) is not equal to the parent window's depth (e.g. 24).
  swa.border_pixel = 0;
  int attribute_flags = CWBorderPixel | CWEventMask | CWColormap;

  window = XCreateWindow(display, root_window, 0, 0, width, height, 0,
                         x_visual_info.depth, InputOutput, x_visual_info.visual,
                         attribute_flags, &swa);
  SB_CHECK(window != None) << "Failed to create the X window.";

  const char* name = "Cobalt";
  if (options && options->name) {
    name = options->name;
  }
  XStoreName(display, window, name);

  Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(display, window, &wm_delete, 1);

  SB_CHECK(HasNeededExtensionsForPunchedOutVideo(display));
  gl_window = XCreateSimpleWindow(display, window, 0, 0, width, height, 0,
                                  WhitePixel(display, DefaultScreen(display)),
                                  BlackPixel(display, DefaultScreen(display)));
  SB_CHECK(gl_window != None);
  XMapWindow(display, gl_window);
  // Manual redirection means that this window will only draw to its pixmap,
  // and won't be automatically rendered onscreen. This is important, because
  // the GL graphics will punch through any layers of windows up to, but not
  // including, the root window. We must composite manually, in Composite(),
  // to avoid that.
  XCompositeRedirectWindow(display, gl_window, CompositeRedirectManual);

  gl_picture = XRenderCreatePicture(
      display, gl_window,
      XRenderFindStandardFormat(display, PictStandardARGB32), 0, NULL);
  SB_CHECK(gl_picture != None);

  // Create the picture for compositing onto. This can persist across frames.
  XRenderPictFormat* pict_format =
      XRenderFindVisualFormat(display, x_visual_info.visual);
  window_picture = XRenderCreatePicture(display, window, pict_format, 0, NULL);
  SB_CHECK(window_picture != None);

  XSelectInput(display, window,
               VisibilityChangeMask | ExposureMask | FocusChangeMask |
                   StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
  XMapWindow(display, window);
}

SbWindowPrivate::~SbWindowPrivate() {
  if (composition_pixmap != None) {
    XFreePixmap(display, composition_pixmap);
    XRenderFreePicture(display, composition_picture);
  }
  if (video_pixmap != None) {
    XRenderFreePicture(display, video_picture);
    XFreeGC(display, video_pixmap_gc);
    XFreePixmap(display, video_pixmap);
  }
  XRenderFreePicture(display, gl_picture);
  XDestroyWindow(display, gl_window);
  XRenderFreePicture(display, window_picture);
  XDestroyWindow(display, window);
}

void SbWindowPrivate::BeginComposite() {
  XSynchronize(display, True);
  XWindowAttributes window_attributes;
  XGetWindowAttributes(display, window, &window_attributes);
  if (window_attributes.width != width ||
      window_attributes.height != height) {
    width = window_attributes.width;
    height = window_attributes.height;
    unhandled_resize = true;
    if (composition_pixmap != None) {
      XFreePixmap(display, composition_pixmap);
      composition_pixmap = None;
      XRenderFreePicture(display, composition_picture);
      composition_picture = None;
    }
  }

  if (composition_pixmap == None) {
    composition_pixmap = XCreatePixmap(display, window, width, height, 32);
    SB_DCHECK(composition_pixmap != None);

    composition_picture = XRenderCreatePicture(
        display, composition_pixmap,
        XRenderFindStandardFormat(display, PictStandardARGB32), 0, NULL);
  }
  SB_CHECK(composition_picture != None);

  XRenderColor black = {0x0000, 0x0000, 0x0000, 0xFFFF};
  XRenderFillRectangle(display, PictOpSrc, composition_picture, &black, 0, 0,
                       width, height);
}

void SbWindowPrivate::CompositeVideoFrame(
    int bounds_x,
    int bounds_y,
    int bounds_width,
    int bounds_height,
    const starboard::scoped_refptr<CpuVideoFrame>& frame) {
  if (frame != NULL && frame->format() == CpuVideoFrame::kBGRA32 &&
      frame->GetPlaneCount() > 0 && frame->width() > 0 && frame->height() > 0) {
    if (frame->width() != video_pixmap_width ||
        frame->height() != video_pixmap_height) {
      if (video_pixmap != None) {
        XRenderFreePicture(display, video_picture);
        video_picture = None;
        XFreeGC(display, video_pixmap_gc);
        video_pixmap_gc = None;
        XFreePixmap(display, video_pixmap);
        video_pixmap = None;
      }
    }

    if (video_pixmap == None) {
      video_pixmap_width = frame->width();
      video_pixmap_height = frame->height();
      video_pixmap = XCreatePixmap(display, window, video_pixmap_width,
                                   video_pixmap_height, 32);
      SB_DCHECK(video_pixmap != None);

      video_pixmap_gc = XCreateGC(display, video_pixmap, 0, NULL);
      SB_DCHECK(video_pixmap_gc != None);

      video_picture = XRenderCreatePicture(
          display, video_pixmap,
          XRenderFindStandardFormat(display, PictStandardARGB32), 0, NULL);
    }
    SB_CHECK(video_picture != None);

    XImage image = {0};
    image.width = frame->width();
    image.height = frame->height();
    image.format = ZPixmap;
    image.data = const_cast<char*>(
        reinterpret_cast<const char*>(frame->GetPlane(0).data));
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.bitmap_pad = 32;
    image.bytes_per_line = frame->GetPlane(0).pitch_in_bytes;
    Status status = XInitImage(&image);
    SB_DCHECK(status);

    // Upload the video frame to the X server.
    XPutImage(display, video_pixmap, video_pixmap_gc, &image, 0, 0,
              0, 0, image.width, image.height);

    // Initially assume we don't have to center or scale.
    int video_width = frame->width();
    int video_height = frame->height();
    if (bounds_width != width || bounds_height != height ||
        frame->width() != width || frame->height() != height) {
      // Scale to fit the smallest dimension of the frame into the window.
      double scale =
          std::min(bounds_width / static_cast<double>(frame->width()),
                   bounds_height / static_cast<double>(frame->height()));
      // Center the scaled frame within the window.
      video_width = scale * frame->width();
      video_height = scale * frame->height();
      XTransform transform = {{
          { XDoubleToFixed(1), XDoubleToFixed(0), XDoubleToFixed(0) },
          { XDoubleToFixed(0), XDoubleToFixed(1), XDoubleToFixed(0) },
          { XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(scale) }
        }};
      XRenderSetPictureTransform(display, video_picture, &transform);
    }

    int dest_x = bounds_x + (bounds_width - video_width) / 2;
    int dest_y = bounds_y + (bounds_height - video_height) / 2;
    XRenderComposite(display, PictOpSrc, video_picture, None,
                     composition_picture, 0, 0, 0, 0, dest_x, dest_y,
                     video_width, video_height);
  }
}

void SbWindowPrivate::EndComposite() {
  // Composite (with blending) the GL output on top of the composition pixmap
  // that already has the current video frame if video is playing.
  XRenderComposite(display, PictOpOver, gl_picture, None, composition_picture,
                   0, 0, 0, 0, 0, 0, width, height);

  // Now that we have a fully-composited frame in composition_pixmap, render it
  // to the window, which acts as our front buffer.
  XRenderComposite(display, PictOpSrc, composition_picture, None,
                   window_picture, 0, 0, 0, 0, 0, 0, width, height);
  XSynchronize(display, False);
}
