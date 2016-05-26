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

#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "starboard/configuration.h"
#include "starboard/log.h"

#if SB_IS(PLAYER_PUNCHED_OUT)
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

namespace {

const int kWindowWidth = 1920;
const int kWindowHeight = 1080;

#if SB_IS(PLAYER_PUNCHED_OUT)
bool HasNeededExtensionsForPunchedOutVideo(Display* display) {
  int dummy;
  return (XRenderQueryExtension(display, &dummy, &dummy) &&
          XCompositeQueryExtension(display, &dummy, &dummy));
}
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

}  // namespace

SbWindowPrivate::SbWindowPrivate(Display* display,
                                 const SbWindowOptions* options)
    : window(None)
#if SB_IS(PLAYER_PUNCHED_OUT)
    , window_picture(None)
    , composition_pixmap(None)
    , gl_window(None)
#endif
    , display(display) {
  XSynchronize(display, True);

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
  swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;
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

#if SB_IS(PLAYER_PUNCHED_OUT)
  SB_CHECK(HasNeededExtensionsForPunchedOutVideo(display));
  gl_window = XCreateSimpleWindow(display, window, 0, 0, width, height, 0,
                                  WhitePixel(display, DefaultScreen(display)),
                                  BlackPixel(display, DefaultScreen(display)));
  SB_CHECK(gl_window != None);
  XMapWindow(display, gl_window);
  // Manual redirection means that this window will only draw to its pixmap, and
  // won't be automatically rendered onscreen. This is important, because the GL
  // graphics will punch through any layers of windows up to, but not including,
  // the root window. We must composite manually, in Composite(), to avoid that.
  XCompositeRedirectWindow(display, gl_window, CompositeRedirectManual);

  // Create the picture for compositing onto. This can persist across frames.
  XRenderPictFormat* pict_format =
      XRenderFindVisualFormat(display, x_visual_info.visual);
  window_picture = XRenderCreatePicture(display, window, pict_format, 0, NULL);
  SB_CHECK(window_picture != None);
#endif  // SB_IS(PLAYER_PUNCHED_OUT)

  XMapWindow(display, window);
  XSynchronize(display, False);
}

SbWindowPrivate::~SbWindowPrivate() {
#if SB_IS(PLAYER_PUNCHED_OUT)
  XRenderFreePicture(display, window_picture);
  if (composition_pixmap != None) {
    XFreePixmap(display, composition_pixmap);
  }
  XDestroyWindow(display, gl_window);
#endif  // SB_IS(PLAYER_PUNCHED_OUT)
  XDestroyWindow(display, window);
}

#if SB_IS(PLAYER_PUNCHED_OUT)
void SbWindowPrivate::Composite(
    ::starboard::shared::starboard::VideoFrame* frame) {
  if (composition_pixmap == None) {
    composition_pixmap = XCreatePixmap(display, window, width, height, 32);
  }
  SB_DCHECK(composition_pixmap != None);
  GC composition_pixmap_gc = XCreateGC(display, composition_pixmap, 0, NULL);
  SB_DCHECK(composition_pixmap_gc != None);

  // TODO: Once frame is solid, only do this if there is no frame.
  XFillRectangle(display, composition_pixmap, composition_pixmap_gc, 0, 0,
                 width, height);

  if (frame != NULL &&
      frame->format() == starboard::shared::starboard::VideoFrame::kBGRA32 &&
      frame->GetPlaneCount() > 0 && frame->width() > 0 && frame->height() > 0) {
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

    // Replace the composition pixmap with the video plane.
    // TODO: Scale frame to video bounds dimensions.
    XPutImage(display, composition_pixmap, composition_pixmap_gc, &image, 0, 0,
              (width - image.width) / 2, (height - image.height) / 2,
              image.width, image.height);
  }
  XFreeGC(display, composition_pixmap_gc);

  Picture composition_picture = XRenderCreatePicture(
      display, composition_pixmap,
      XRenderFindStandardFormat(display, PictStandardARGB32), 0, NULL);
  SB_CHECK(composition_picture != None);

  Picture gl_picture = XRenderCreatePicture(
      display, gl_window,
      XRenderFindStandardFormat(display, PictStandardARGB32), 0, NULL);
  SB_CHECK(gl_picture != None);

  // Composite (with blending) the GL output on top of the composition pixmap
  // that already has the current video frame if video is playing.
  XRenderComposite(display, PictOpOver, gl_picture, NULL, composition_picture,
                   0, 0, 0, 0, 0, 0, width, height);
  XRenderFreePicture(display, gl_picture);

  // Now that we have a fully-composited frame in composition_pixmap, render it
  // to the window, which acts as our front buffer.
  XRenderComposite(display, PictOpSrc, composition_picture, NULL,
                   window_picture, 0, 0, 0, 0, 0, 0, width, height);
  XRenderFreePicture(display, composition_picture);
}
#endif  // SB_IS(PLAYER_PUNCHED_OUT)
