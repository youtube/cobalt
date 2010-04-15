// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINUX_UTIL_H_
#define BASE_LINUX_UTIL_H_

#include <stdint.h>
#include <sys/types.h>

#include <string>

class FilePath;

namespace base {

class EnvVarGetter;

static const char kFindInodeSwitch[] = "--find-inode";

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0,
// it's assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

// Get the Linux Distro if we can, or return "Unknown", similar to
// GetWinVersion() in base/win_util.h.
std::string GetLinuxDistro();

// Get the home directory.
FilePath GetHomeDir(EnvVarGetter* env);

// Utility function for getting XDG directories.
// |env_name| is the name of an environment variable that we want to use to get
// a directory path. |fallback_dir| is the directory relative to $HOME that we
// use if |env_name| cannot be found or is empty. |fallback_dir| may be NULL.
// Examples of |env_name| are XDG_CONFIG_HOME and XDG_DATA_HOME.
FilePath GetXDGDirectory(EnvVarGetter* env, const char* env_name,
                         const char* fallback_dir);

// Wrapper around xdg_user_dir_lookup() from src/base/third_party/xdg-user-dirs
// This looks up "well known" user directories like the desktop and music
// folder. Examples of |dir_name| are DESKTOP and MUSIC.
FilePath GetXDGUserDirectory(EnvVarGetter* env, const char* dir_name,
                             const char* fallback_dir);

enum DesktopEnvironment {
  DESKTOP_ENVIRONMENT_OTHER,
  DESKTOP_ENVIRONMENT_GNOME,
  // KDE3 and KDE4 are sufficiently different that we count
  // them as two different desktop environments here.
  DESKTOP_ENVIRONMENT_KDE3,
  DESKTOP_ENVIRONMENT_KDE4,
  DESKTOP_ENVIRONMENT_XFCE,
};

// Return an entry from the DesktopEnvironment enum with a best guess
// of which desktop environment we're using.  We use this to know when
// to attempt to use preferences from the desktop environment --
// proxy settings, password manager, etc.
DesktopEnvironment GetDesktopEnvironment(EnvVarGetter* env);

// Return a string representation of the given desktop environment.
// May return NULL in the case of DESKTOP_ENVIRONMENT_OTHER.
const char* GetDesktopEnvironmentName(DesktopEnvironment env);
// Convenience wrapper that calls GetDesktopEnvironment() first.
const char* GetDesktopEnvironmentName(EnvVarGetter* env);

// Return the inode number for the UNIX domain socket |fd|.
bool FileDescriptorGetInode(ino_t* inode_out, int fd);

// Find the process which holds the given socket, named by inode number. If
// multiple processes hold the socket, this function returns false.
bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H_
