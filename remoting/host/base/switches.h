// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_SWITCHES_H_
#define REMOTING_HOST_BASE_SWITCHES_H_

#include "build/build_config.h"

namespace remoting {

// "--elevate=<binary>" requests |binary| to be launched elevated (possibly
// causing a UAC prompt).
extern const char kElevateSwitchName[];

// "--help" prints the usage message.
extern const char kHelpSwitchName[];

// Used to specify the type of the process. kProcessType* constants specify
// possible values.
extern const char kProcessTypeSwitchName[];

// "--?" prints the usage message.
extern const char kQuestionSwitchName[];

// The command line switch used to get version of the daemon.
extern const char kVersionSwitchName[];

// Values for kProcessTypeSwitchName.
extern const char kProcessTypeController[];
extern const char kProcessTypeDaemon[];
extern const char kProcessTypeDesktop[];
extern const char kProcessTypeHost[];
extern const char kProcessTypeRdpDesktopSession[];
extern const char kProcessTypeEvaluateCapability[];
extern const char kProcessTypeFileChooser[];
extern const char kProcessTypeUrlForwarderConfigurator[];
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
extern const char kProcessTypeXSessionChooser[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

extern const char kEvaluateCapabilitySwitchName[];

#if BUILDFLAG(IS_LINUX)
// Used to record client sessions to utmp/wtmp.
extern const char kEnableUtempter[];
#endif

// Values for kEvaluateCapabilitySwitchName.
#if BUILDFLAG(IS_WIN)
// Executes EvaluateD3D() function.
extern const char kEvaluateD3D[];
// Executes Evaluate3dDisplayMode() function.
extern const char kEvaluate3dDisplayMode[];
// Used in combination with |kProcessTypeUrlForwarderConfigurator| to set up
// the URL forwarder on Windows.
extern const char kSetUpUrlForwarderSwitchName[];
#endif

// Used to pass the HWND for the parent process to a child process.
extern const char kParentWindowSwitchName[];

// Name of the pipe used to communicate from the parent to the child process.
extern const char kInputSwitchName[];

// Name of the pipe used to communicate from the child to the parent process.
extern const char kOutputSwitchName[];

// Token used to create a message pipe between a pair of child and parent
// processes.
extern const char kMojoPipeToken[];

#if BUILDFLAG(IS_APPLE)
// NativeMessagingHost switch to check for required OS permissions and request
// them if necessary.
extern const char kCheckPermissionSwitchName[];

// Command line switch to check for Accessibility permission.
extern const char kCheckAccessibilityPermissionSwitchName[];

// Command line switch to check for Screen Recording permission.
extern const char kCheckScreenRecordingPermissionSwitchName[];

// Command line switch to list all audio devices and their UID.
extern const char kListAudioDevicesSwitchName[];
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_SWITCHES_H_
