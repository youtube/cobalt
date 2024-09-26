// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/mac/uninstaller/remoting_uninstaller.h"

#import <Cocoa/Cocoa.h>
#include <sys/stat.h>

#include "base/mac/authorization_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "remoting/host/mac/constants_mac.h"

void logOutput(FILE* pipe) {
  char readBuffer[128];
  for (;;) {
    long bytesRead = read(fileno(pipe), readBuffer, sizeof(readBuffer) - 1);
    if (bytesRead < 1)
      break;
    readBuffer[bytesRead] = '\0';
    NSLog(@"%s", readBuffer);
  }
}

NSArray* convertToNSArray(const char** array) {
  NSMutableArray* ns_array = [[[NSMutableArray alloc] init] autorelease];
  int i = 0;
  const char* element = array[i++];
  while (element != nullptr) {
    [ns_array addObject:@(element)];
    element = array[i++];
  }
  return ns_array;
}

@implementation RemotingUninstaller

- (void)runCommand:(const char*)cmd
     withArguments:(const char**)args {
  NSTask* task;
  NSPipe* output = [NSPipe pipe];
  NSString* result;

  NSArray* arg_array = convertToNSArray(args);
  NSLog(@"Executing: %s %@", cmd, [arg_array componentsJoinedByString:@" "]);

  @try {
    task = [[[NSTask alloc] init] autorelease];
    [task setLaunchPath:@(cmd)];
    [task setArguments:arg_array];
    [task setStandardInput:[NSPipe pipe]];
    [task setStandardOutput:output];
    [task launch];

    NSData* data = [[output fileHandleForReading] readDataToEndOfFile];

    [task waitUntilExit];

    if ([task terminationStatus] != 0) {
      // TODO(garykac): When we switch to sdk_10.6, show the
      // [task terminationReason] as well.
      NSLog(@"Command terminated status=%d", [task terminationStatus]);
    }

    result = [[[NSString alloc] initWithData:data
                                    encoding:NSUTF8StringEncoding]
              autorelease];
    if ([result length] != 0) {
      NSLog(@"Result: %@", result);
    }
  }
  @catch (NSException* exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
  }
}

- (void)sudoCommand:(const char*)cmd
      withArguments:(const char**)args
          usingAuth:(AuthorizationRef)authRef  {
  NSArray* arg_array = convertToNSArray(args);
  NSLog(@"Executing (as Admin): %s %@", cmd,
        [arg_array componentsJoinedByString:@" "]);
  FILE* pipe = nullptr;
  OSStatus status = base::mac::ExecuteWithPrivilegesAndGetPID(
      authRef, cmd, kAuthorizationFlagDefaults, args, &pipe, nullptr);

  if (status == errAuthorizationToolExecuteFailure) {
    NSLog(@"Error errAuthorizationToolExecuteFailure");
  } else if (status != errAuthorizationSuccess) {
    NSLog(@"Error while executing %s. Status=%d",
          cmd, static_cast<int>(status));
  } else {
    logOutput(pipe);
  }

  if (pipe != nullptr)
    fclose(pipe);
}

- (void)sudoDelete:(const char*)filename
         usingAuth:(AuthorizationRef)authRef  {
  const char* args[] = { "-rf", filename, nullptr };
  [self sudoCommand:"/bin/rm" withArguments:args usingAuth:authRef];
}

- (void)shutdownService {
  const char* launchCtl = "/bin/launchctl";
  const char* argsStop[] = { "stop", remoting::kServiceName, nullptr };
  [self runCommand:launchCtl withArguments:argsStop];

  if ([[NSFileManager defaultManager]
          fileExistsAtPath:@(remoting::kServicePlistPath)]) {
    const char* argsUnload[] = { "unload", "-w", "-S", "Aqua",
                                remoting::kServicePlistPath, nullptr };
    [self runCommand:launchCtl withArguments:argsUnload];
  }
}

- (void)keystoneUnregisterUsingAuth:(AuthorizationRef)authRef {
  // ksadmin moved from MacOS to Helpers in Keystone 1.2.13.112, 2019-11-12. A
  // symbolic link from the old location was left in place, but may not remain
  // indefinitely. Try the new location first, falling back to the old if
  // needed.
  static const char kKSAdminPath[] =
      "/Library/Google/GoogleSoftwareUpdate/"
      "GoogleSoftwareUpdate.bundle/Contents/Helpers/"
      "ksadmin";
  static const char kKSAdminOldPath[] =
      "/Library/Google/GoogleSoftwareUpdate/"
      "GoogleSoftwareUpdate.bundle/Contents/MacOS/"
      "ksadmin";

  struct stat statbuf;
  const char* ksadminPath =
      (stat(kKSAdminPath, &statbuf) == 0 && (statbuf.st_mode & S_IXUSR))
          ? kKSAdminPath
          : kKSAdminOldPath;

  static const char kKSProductID[] = "com.google.chrome_remote_desktop";

  const char* args[] = {"--delete", "--productid", kKSProductID, "-S", nullptr};
  [self sudoCommand:ksadminPath withArguments:args usingAuth:authRef];
}

- (void)remotingUninstallUsingAuth:(AuthorizationRef)authRef {
  // Remove the enabled file before shutting down the service or else it might
  // restart itself.
  [self sudoDelete:remoting::kHostEnabledPath usingAuth:authRef];

  [self shutdownService];

  [self sudoDelete:remoting::kServicePlistPath usingAuth:authRef];
  [self sudoDelete:remoting::kHostBinaryPath usingAuth:authRef];
  [self sudoDelete:remoting::kHostLegacyBinaryPath usingAuth:authRef];
  [self sudoDelete:remoting::kOldHostHelperScriptPath usingAuth:authRef];
  [self sudoDelete:remoting::kHostConfigFilePath usingAuth:authRef];
  [self sudoDelete:remoting::kHostSettingsFilePath usingAuth:authRef];
  [self sudoDelete:remoting::kLogFilePath usingAuth:authRef];
  [self sudoDelete:remoting::kLogFileConfigPath usingAuth:authRef];
  for (const char* path : remoting::kNativeMessagingManifestPaths) {
    [self sudoDelete:path usingAuth:authRef];
  }
  [self sudoDelete:remoting::kBrandedUninstallerPath usingAuth:authRef];
  [self sudoDelete:remoting::kUnbrandedUninstallerPath usingAuth:authRef];

  [self keystoneUnregisterUsingAuth:authRef];
}

- (OSStatus)remotingUninstall {
  base::mac::ScopedAuthorizationRef authRef;
  OSStatus status =
      AuthorizationCreate(nullptr, kAuthorizationEmptyEnvironment,
                          kAuthorizationFlagDefaults, authRef.InitializeInto());
  if (status != errAuthorizationSuccess) {
    [NSException raise:@"AuthorizationCreate Failure"
                format:@"Error during AuthorizationCreate status=%d",
                           static_cast<int>(status)];
  }

  AuthorizationItem right = {kAuthorizationRightExecute, 0, nullptr, 0};
  AuthorizationRights rights = {1, &right};
  AuthorizationFlags flags = kAuthorizationFlagDefaults |
                             kAuthorizationFlagInteractionAllowed |
                             kAuthorizationFlagPreAuthorize |
                             kAuthorizationFlagExtendRights;
  status = AuthorizationCopyRights(authRef, &rights, nullptr, flags, nullptr);
  if (status == errAuthorizationSuccess) {
    RemotingUninstaller* uninstaller =
        [[[RemotingUninstaller alloc] init] autorelease];
    [uninstaller remotingUninstallUsingAuth:authRef];
  }
  return status;
}

@end
