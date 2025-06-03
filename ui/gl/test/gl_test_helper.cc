// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/test/gl_test_helper.h"

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <wingdi.h>

#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gl/direct_composition_support.h"
#endif

namespace gl {
// static
GLuint GLTestHelper::CreateTexture(GLenum target) {
  // Create the texture object.
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return texture;
}

// static
GLuint GLTestHelper::SetupFramebuffer(int width, int height) {
  GLuint color_buffer_texture = CreateTexture(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, color_buffer_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint framebuffer = 0;
  glGenFramebuffersEXT(1, &framebuffer);
  glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            color_buffer_texture, 0);
  if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        << "Error setting up framebuffer";
    glDeleteFramebuffersEXT(1, &framebuffer);
    framebuffer = 0;
  }
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  glDeleteTextures(1, &color_buffer_texture);
  return framebuffer;
}

// static
bool GLTestHelper::CheckPixels(int x,
                               int y,
                               int width,
                               int height,
                               const uint8_t expected_color[4]) {
  return CheckPixelsWithError(x, y, width, height, 0, expected_color);
}

// static
bool GLTestHelper::CheckPixelsWithError(int x,
                                        int y,
                                        int width,
                                        int height,
                                        int error,
                                        const uint8_t expected_color[4]) {
  int size = width * height * 4;
  std::unique_ptr<uint8_t[]> pixels(new uint8_t[size]);
  const uint8_t kCheckClearValue = 123u;
  memset(pixels.get(), kCheckClearValue, size);
  glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
  int bad_count = 0;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      int offset = yy * width * 4 + xx * 4;
      for (int jj = 0; jj < 4; ++jj) {
        uint8_t actual = pixels[offset + jj];
        uint8_t expected = expected_color[jj];
        EXPECT_NEAR(expected, actual, error)
            << " at " << (xx + x) << ", " << (yy + y) << " channel " << jj;
        bad_count += actual != expected;
        // Exit early just so we don't spam the log but we print enough to
        // hopefully make it easy to diagnose the issue.
        if (bad_count > 16)
          return false;
      }
    }
  }

  return !bad_count;
}

#if BUILDFLAG(IS_WIN)

// static
SkColor GLTestHelper::GetColorAtPoint(const SkBitmap& bitmap,
                                      const gfx::Point& location) {
  CHECK_GE(location.x(), 0);
  CHECK_LT(location.x(), bitmap.width());
  CHECK_GE(location.y(), 0);
  CHECK_LT(location.y(), bitmap.height());
  return bitmap.getColor(location.x(), location.y());
}

// static
SkBitmap GLTestHelper::ReadBackWindow(HWND window, const gfx::Size& size) {
  {
    // Ensure that the previous commit has been processed before trying to read
    // back the window contents.
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
        GetDirectCompositionDevice();
    if (dcomp_device) {
      CHECK_EQ(S_OK, dcomp_device->WaitForCommitCompletion());
    }
  }

  base::win::ScopedCreateDC mem_hdc(::CreateCompatibleDC(nullptr));
  DCHECK(mem_hdc.IsValid());

  BITMAPV4HEADER hdr;
  gfx::CreateBitmapV4HeaderForARGB888(size.width(), size.height(), &hdr);

  void* bits = nullptr;
  base::win::ScopedBitmap bitmap(
      ::CreateDIBSection(mem_hdc.Get(), reinterpret_cast<BITMAPINFO*>(&hdr),
                         DIB_RGB_COLORS, &bits, nullptr, 0));
  DCHECK(bitmap.is_valid());

  base::win::ScopedSelectObject select_object(mem_hdc.Get(), bitmap.get());

  // Grab a copy of the window. Use PrintWindow because it works even when the
  // window's partially occluded. The PW_RENDERFULLCONTENT flag is undocumented,
  // but works starting in Windows 8.1. It allows for capturing the contents of
  // the window that are drawn using DirectComposition.
  UINT flags = PW_CLIENTONLY | PW_RENDERFULLCONTENT;

  BOOL result = PrintWindow(window, mem_hdc.Get(), flags);
  if (!result)
    PLOG(ERROR) << "Failed to print window";

  GdiFlush();

  SkBitmap sk_bitmap;
  CHECK(sk_bitmap.tryAllocPixels(SkImageInfo::Make(
      SkISize::Make(size.width(), size.height()),
      SkColorInfo(SkColorType::kBGRA_8888_SkColorType,
                  SkAlphaType::kPremul_SkAlphaType, nullptr))));
  memcpy(sk_bitmap.getAddr(0, 0), bits, sk_bitmap.computeByteSize());

  return sk_bitmap;
}

// static
SkColor GLTestHelper::ReadBackWindowPixel(HWND window,
                                          const gfx::Point& point) {
  gfx::Size size(point.x() + 1, point.y() + 1);
  auto pixels = ReadBackWindow(window, size);
  return GetColorAtPoint(pixels, point);
}
#endif

}  // namespace gl
