// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_SPLASH_SPLASH_H_
#define COBALT_SPLASH_SPLASH_H_

// #include <memory>
// #include <string>
// #include <vector>

// #include "base/functional/callback_forward.h"
// #include "base/memory/ref_counted.h"
// #include "base/memory/scoped_refptr.h"
// #include "base/strings/string_piece.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace cobalt {

class Splash {
 public:
  static std::unique_ptr<content::WebContents> Show(
      content::BrowserContext* browser_context,
      gfx::NativeView container,
      const GURL& url);
};

}  // namespace cobalt

#endif  // COBALT_SPLASH_SPLASH_H_
