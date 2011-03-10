// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/xdg_util.h"

#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/third_party/xdg_user_dirs/xdg_user_dir_lookup.h"

namespace base {
namespace nix {

FilePath GetXDGDirectory(Environment* env, const char* env_name,
                         const char* fallback_dir) {
  std::string env_value;
  if (env->GetVar(env_name, &env_value) && !env_value.empty())
    return FilePath(env_value);
  return file_util::GetHomeDir().Append(fallback_dir);
}

FilePath GetXDGUserDirectory(Environment* env, const char* dir_name,
                             const char* fallback_dir) {
  char* xdg_dir = xdg_user_dir_lookup(dir_name);
  if (xdg_dir) {
    FilePath rv(xdg_dir);
    free(xdg_dir);
    return rv;
  }
  return file_util::GetHomeDir().Append(fallback_dir);
}

DesktopEnvironment GetDesktopEnvironment(Environment* env) {
  std::string desktop_session;
  if (env->GetVar("DESKTOP_SESSION", &desktop_session)) {
    if (desktop_session == "gnome") {
      return DESKTOP_ENVIRONMENT_GNOME;
    } else if (desktop_session == "kde4") {
      return DESKTOP_ENVIRONMENT_KDE4;
    } else if (desktop_session == "kde") {
      // This may mean KDE4 on newer systems, so we have to check.
      if (env->HasVar("KDE_SESSION_VERSION"))
        return DESKTOP_ENVIRONMENT_KDE4;
      return DESKTOP_ENVIRONMENT_KDE3;
    } else if (desktop_session.find("xfce") != std::string::npos ||
               desktop_session == "xubuntu") {
      return DESKTOP_ENVIRONMENT_XFCE;
    }
  }

  // Fall back on some older environment variables.
  // Useful particularly in the DESKTOP_SESSION=default case.
  if (env->HasVar("GNOME_DESKTOP_SESSION_ID")) {
    return DESKTOP_ENVIRONMENT_GNOME;
  } else if (env->HasVar("KDE_FULL_SESSION")) {
    if (env->HasVar("KDE_SESSION_VERSION"))
      return DESKTOP_ENVIRONMENT_KDE4;
    return DESKTOP_ENVIRONMENT_KDE3;
  }

  return DESKTOP_ENVIRONMENT_OTHER;
}

const char* GetDesktopEnvironmentName(DesktopEnvironment env) {
  switch (env) {
    case DESKTOP_ENVIRONMENT_OTHER:
      return NULL;
    case DESKTOP_ENVIRONMENT_GNOME:
      return "GNOME";
    case DESKTOP_ENVIRONMENT_KDE3:
      return "KDE3";
    case DESKTOP_ENVIRONMENT_KDE4:
      return "KDE4";
    case DESKTOP_ENVIRONMENT_XFCE:
      return "XFCE";
  }
  return NULL;
}

const char* GetDesktopEnvironmentName(Environment* env) {
  return GetDesktopEnvironmentName(GetDesktopEnvironment(env));
}

}  // namespace nix
}  // namespace base
