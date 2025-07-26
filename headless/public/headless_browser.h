// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
#define HEADLESS_PUBLIC_HEADLESS_BROWSER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_export.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
struct UserAgentMetadata;
}

namespace headless {

class HeadlessWebContents;

// This class represents the global headless browser instance. To get a pointer
// to one, call |HeadlessBrowserMain| to initiate the browser main loop. An
// instance of |HeadlessBrowser| will be passed to the callback given to that
// function.
class HEADLESS_EXPORT HeadlessBrowser {
 public:
  struct Options;

  HeadlessBrowser(const HeadlessBrowser&) = delete;
  HeadlessBrowser& operator=(const HeadlessBrowser&) = delete;

  // Create a new browser context which can be used to create tabs and isolate
  // them from one another.
  // Pointer to HeadlessBrowserContext becomes invalid after:
  // a) Calling HeadlessBrowserContext::Close or
  // b) Calling HeadlessBrowser::Shutdown
  virtual HeadlessBrowserContext::Builder CreateBrowserContextBuilder() = 0;

  virtual std::vector<HeadlessBrowserContext*> GetAllBrowserContexts() = 0;

  // Returns HeadlessBrowserContext associated with the given id if any.
  // Otherwise returns null.
  virtual HeadlessBrowserContext* GetBrowserContextForId(
      const std::string& id) = 0;

  // Allows setting and getting the browser context that DevTools will create
  // new targets in by default.
  virtual void SetDefaultBrowserContext(
      HeadlessBrowserContext* browser_context) = 0;
  virtual HeadlessBrowserContext* GetDefaultBrowserContext() = 0;

  // Returns a task runner for submitting work to the browser main thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BrowserMainThread()
      const = 0;

  // Requests browser to stop as soon as possible. |Run| will return as soon as
  // browser stops.
  // IMPORTANT: All pointers to HeadlessBrowserContexts and HeadlessWebContents
  // become invalid after calling this function.
  virtual void Shutdown() = 0;

  static std::string GetProductNameAndVersion();
  static blink::UserAgentMetadata GetUserAgentMetadata();

 protected:
  HeadlessBrowser() {}
  virtual ~HeadlessBrowser() {}
};

// Embedding API overrides for the headless browser.
struct HEADLESS_EXPORT HeadlessBrowser::Options {
  Options();
  Options(Options&& options);

  Options(const Options&) = delete;
  Options& operator=(const Options&) = delete;

  ~Options();

  Options& operator=(Options&& options);

  // Port at which DevTools should listen for connections on localhost.
  std::optional<int> devtools_port;

  // Enables remote debug over stdio pipes [in=3, out=4].
  bool devtools_pipe_enabled = false;

  // A single way to test whether the devtools server has been requested.
  bool DevtoolsServerEnabled();

  // Default per-context options, can be specialized on per-context basis.
  std::string accept_language;
  std::string user_agent;

  // The ProxyConfig to use. The system proxy settings are used by default.
  std::unique_ptr<net::ProxyConfig> proxy_config;

  // Default window size. This is also used to create the window tree host and
  // as the headless screen size. Defaults to 800x600.
  gfx::Size window_size = gfx::Size(800, 600);

  // Headless screen info specification.
  std::string screen_info_spec;

  // Path to user data directory, where browser will look for its state.
  // If empty, default directory (where the binary is located) will be used.
  base::FilePath user_data_dir;

  // Path to disk cache directory. If emppty, 'Cache' subdirectory of the
  // user data directory will be used.
  base::FilePath disk_cache_dir;

  // Run a browser context in an incognito mode. Enabled by default.
  bool incognito_mode = true;

  // If true, then all pop-ups and calls to window.open will fail.
  bool block_new_web_contents = false;

  // Whether or not BeginFrames will be issued over DevTools protocol
  // (experimental).
  bool enable_begin_frame_control = false;

  // Font render hinting value to override any default settings
  gfx::FontRenderParams::Hinting font_render_hinting =
      gfx::FontRenderParams::Hinting::HINTING_FULL;

  // Whether lazy loading of images and frames is enabled.
  bool lazy_load_enabled = true;

  // Forces each navigation to use a new BrowsingInstance.
  bool force_new_browsing_instance = false;

  // Reminder: when adding a new field here, do not forget to add it to
  // HeadlessBrowserContextOptions (where appropriate).
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_BROWSER_H_
