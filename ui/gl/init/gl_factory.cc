// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_initializer.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gl {
namespace init {

namespace {

bool g_is_angle_enabled = true;

bool ShouldFallbackToSoftwareGL() {
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string requested_implementation_gl_name =
      cmd->GetSwitchValueASCII(switches::kUseGL);

  if (cmd->HasSwitch(switches::kUseGL) &&
      requested_implementation_gl_name == "any") {
    return true;
  } else {
    return false;
  }
}

GLImplementationParts GetRequestedGLImplementation(
    bool* fallback_to_software_gl) {
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string requested_implementation_gl_name =
      cmd->GetSwitchValueASCII(switches::kUseGL);

  // If --use-angle was specified but --use-gl was not, assume --use-gl=angle
  if (cmd->HasSwitch(switches::kUseANGLE) &&
      !cmd->HasSwitch(switches::kUseGL)) {
    requested_implementation_gl_name = kGLImplementationANGLEName;
  }

  if (requested_implementation_gl_name == kGLImplementationDisabledName) {
    return GLImplementationParts(kGLImplementationDisabled);
  }

  std::vector<GLImplementationParts> allowed_impls =
      GetAllowedGLImplementations();

  if (GetGlWorkarounds().disable_es3gl_context_for_testing) {
    GLVersionInfo::DisableES3ForTesting();
  }

  // If the passthrough command decoder is enabled, put ANGLE first if allowed
  if (g_is_angle_enabled && UsePassthroughCommandDecoder(cmd)) {
    std::vector<GLImplementationParts> angle_impls = {};
    bool software_gl_allowed = false;
    auto iter = allowed_impls.begin();
    while (iter != allowed_impls.end()) {
      if ((*iter) == GetSoftwareGLImplementation()) {
        software_gl_allowed = true;
        allowed_impls.erase(iter);
      } else if (iter->gl == kGLImplementationEGLANGLE) {
        angle_impls.emplace_back(*iter);
        allowed_impls.erase(iter);
      } else {
        iter++;
      }
    }
    allowed_impls.insert(allowed_impls.begin(), angle_impls.begin(),
                         angle_impls.end());
    // Insert software implementations at the end, after all other hardware
    // implementations
    if (software_gl_allowed) {
      allowed_impls.emplace_back(GetSoftwareGLImplementation());
    }
  }

  if (allowed_impls.empty()) {
    LOG(ERROR) << "List of allowed GL implementations is empty.";
    return GLImplementationParts(kGLImplementationNone);
  }

  *fallback_to_software_gl = false;
  absl::optional<GLImplementationParts> impl_from_cmdline =
      GetRequestedGLImplementationFromCommandLine(cmd, fallback_to_software_gl);

  // The default implementation is always the first one in list.
  if (!impl_from_cmdline)
    return allowed_impls[0];

  if (IsSoftwareGLImplementation(*impl_from_cmdline))
    return *impl_from_cmdline;

  if (impl_from_cmdline->IsAllowed(allowed_impls))
    return *impl_from_cmdline;

  std::vector<std::string> allowed_impl_strs;
  for (const auto& allowed_impl : allowed_impls) {
    allowed_impl_strs.push_back(allowed_impl.ToString());
  }
  LOG(ERROR) << "Requested GL implementation " << impl_from_cmdline->ToString()
             << " not found in allowed implementations: ["
             << base::JoinString(allowed_impl_strs, ",") << "].";
  return GLImplementationParts(kGLImplementationNone);
}

GLDisplay* InitializeGLOneOffPlatformHelper(bool init_extensions,
                                            gl::GpuPreference gpu_preference) {
  TRACE_EVENT1("gpu,startup", "gl::init::InitializeGLOneOffPlatformHelper",
               "init_extensions", init_extensions);

  bool fallback_to_software_gl = ShouldFallbackToSoftwareGL();
  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  bool disable_gl_drawing = cmd->HasSwitch(switches::kDisableGLDrawingForTests);

  return InitializeGLOneOffPlatformImplementation(
      fallback_to_software_gl, disable_gl_drawing, init_extensions,
      gpu_preference);
}

}  // namespace

GLDisplay* InitializeGLOneOff(gl::GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu,startup", "gl::init::InitializeOneOff");

  if (!InitializeStaticGLBindingsOneOff())
    return nullptr;
  if (GetGLImplementation() == kGLImplementationDisabled) {
    return GetDefaultDisplayEGL();
  }

  return InitializeGLOneOffPlatformHelper(true, gpu_preference);
}

GLDisplay* InitializeGLNoExtensionsOneOff(bool init_bindings,
                                          gl::GpuPreference gpu_preference) {
  TRACE_EVENT1("gpu,startup", "gl::init::InitializeNoExtensionsOneOff",
               "init_bindings", init_bindings);
  if (init_bindings) {
    if (!InitializeStaticGLBindingsOneOff())
      return nullptr;
    if (GetGLImplementation() == kGLImplementationDisabled) {
      return GetDefaultDisplayEGL();
    }
  }

  return InitializeGLOneOffPlatformHelper(false, gpu_preference);
}

bool InitializeStaticGLBindingsOneOff() {
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  bool fallback_to_software_gl = false;
  GLImplementationParts impl =
      GetRequestedGLImplementation(&fallback_to_software_gl);
  if (impl.gl == kGLImplementationDisabled) {
    SetGLImplementation(kGLImplementationDisabled);
    return true;
  } else if (impl.gl == kGLImplementationNone) {
    return false;
  }

  return InitializeStaticGLBindingsImplementation(impl,
                                                  fallback_to_software_gl);
}

bool InitializeStaticGLBindingsImplementation(GLImplementationParts impl,
                                              bool fallback_to_software_gl) {
  if (IsSoftwareGLImplementation(impl))
    fallback_to_software_gl = false;

  bool initialized = InitializeStaticGLBindings(impl);
  if (!initialized && fallback_to_software_gl) {
    ShutdownGL(nullptr, /*due_to_fallback*/ true);
    initialized = InitializeStaticGLBindings(GetSoftwareGLImplementation());
  }
  if (!initialized) {
    ShutdownGL(nullptr, /*due_to_fallback*/ false);
    return false;
  }
  return true;
}

GLDisplay* InitializeGLOneOffPlatformImplementation(
    bool fallback_to_software_gl,
    bool disable_gl_drawing,
    bool init_extensions,
    gl::GpuPreference gpu_preference) {
  if (IsSoftwareGLImplementation(GetGLImplementationParts()))
    fallback_to_software_gl = false;

  GLDisplay* display = InitializeGLOneOffPlatform(gpu_preference);
  bool initialized = !!display;

  if (!initialized) {
    DVLOG(1) << "Initialization failed. Attempting to initialize default "
                "GLDisplayEGL.";
    RemoveGpuPreferenceEGL(gpu_preference);
    display = InitializeGLOneOffPlatform(gl::GpuPreference::kDefault);
    initialized = !!display;
  }
  if (!initialized && fallback_to_software_gl) {
    ShutdownGL(nullptr, /*due_to_fallback=*/true);
    if (InitializeStaticGLBindings(GetSoftwareGLImplementation())) {
      display = InitializeGLOneOffPlatform(gpu_preference);
      initialized = !!display;
    }
  }
  if (initialized && init_extensions) {
    initialized = InitializeExtensionSettingsOneOffPlatform(display);
  }

  if (!initialized) {
    ShutdownGL(display, false);
    return nullptr;
  }

  DVLOG(1) << "Using " << GetGLImplementationGLName(GetGLImplementationParts())
           << " GL implementation.";
  SetNullDrawGLBindings(disable_gl_drawing);
  return display;
}

GLDisplay* GetOrInitializeGLOneOffPlatformImplementation(
    bool fallback_to_software_gl,
    bool disable_gl_drawing,
    bool init_extensions,
    gl::GpuPreference gpu_preference) {
  gl::GLDisplay* display = gl::GetDisplay(gpu_preference);
  DCHECK(display);

  if (display->IsInitialized()) {
    return display;
  }

  display = gl::init::InitializeGLOneOffPlatformImplementation(
      /*fallback_to_software_gl=*/false, /*disable_gl_drawing=*/false,
      /*init_extensions=*/true,
      /*gpu_preference=*/gpu_preference);

  return display;
}

void ShutdownGL(GLDisplay* display, bool due_to_fallback) {
  ShutdownGLPlatform(display);

  UnloadGLNativeLibraries(due_to_fallback);
  SetGLImplementation(kGLImplementationNone);
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(gl::GLDisplay* display,
                                                  const gfx::Size& size) {
  return CreateOffscreenGLSurfaceWithFormat(display, size, GLSurfaceFormat());
}

void DisableANGLE() {
  DCHECK_NE(GetGLImplementation(), kGLImplementationEGLANGLE);
  g_is_angle_enabled = false;
}

}  // namespace init
}  // namespace gl
