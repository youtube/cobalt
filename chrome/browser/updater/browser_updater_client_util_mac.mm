// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_util.h"

#include <Foundation/Foundation.h>
#import <OpenDirectory/OpenDirectory.h>
#import <ServiceManagement/ServiceManagement.h>

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_authorizationref.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/browser_updater_helper_client_mac.h"
#include "chrome/common/chrome_version.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/updater/updater_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr char kInstallCommand[] = "install";

base::FilePath GetUpdaterExecutablePath() {
  return base::FilePath(base::StrCat({kUpdaterName, ".app"}))
      .Append(FILE_PATH_LITERAL("Contents"))
      .Append(FILE_PATH_LITERAL("MacOS"))
      .Append(kUpdaterName);
}

bool BundleOwnedByUser(uid_t user_uid) {
  const base::FilePath path = base::mac::OuterBundlePath();
  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path.value().c_str(), &stat_info) != 0) {
    VPLOG(2) << "Failed to get information on path " << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    VLOG(2) << "Path " << path.value() << " is a symbolic link.";
    return false;
  }

  return stat_info.st_uid == user_uid;
}

bool BundleOwnedByRoot() {
  return BundleOwnedByUser(0);
}

bool BundleOwnedByCurrentUser() {
  return BundleOwnedByUser(geteuid());
}

bool IsEffectiveUserAdmin() {
  NSError* error;
  ODNode* search_node = [ODNode nodeWithSession:[ODSession defaultSession]
                                           type:kODNodeTypeLocalNodes
                                          error:&error];
  if (!search_node) {
    VLOG(2) << "Error creating ODNode: " << search_node;
    return false;
  }
  ODQuery* query =
      [ODQuery queryWithNode:search_node
              forRecordTypes:kODRecordTypeUsers
                   attribute:kODAttributeTypeUniqueID
                   matchType:kODMatchEqualTo
                 queryValues:[NSString stringWithFormat:@"%d", geteuid()]
            returnAttributes:kODAttributeTypeStandardOnly
              maximumResults:1
                       error:&error];
  if (!query) {
    VLOG(2) << "Error constructing query: " << error;
    return false;
  }

  NSArray<ODRecord*>* results = [query resultsAllowingPartial:NO error:&error];
  if (!results) {
    VLOG(2) << "Error executing query: " << error;
    return false;
  }

  ODRecord* admin_group = [search_node recordWithRecordType:kODRecordTypeGroups
                                                       name:@"admin"
                                                 attributes:nil
                                                      error:&error];
  if (!admin_group) {
    VLOG(2) << "Failed to get 'admin' group: " << error;
    return false;
  }

  bool result = [admin_group isMemberRecord:results.firstObject error:&error];
  VLOG_IF(2, error) << "Failed to get member record: " << error;

  return result;
}

bool ShouldPromoteUpdater() {
  // 1) Should promote if browser is owned by root and not installed. The not
  // installed part of this case is handled in version_updater_mac.mm
  if (BundleOwnedByRoot())
    return true;

  // 2) If the effective user is root and the browser is not owned by root (i.e.
  // if the current user has run with sudo).
  if (geteuid() == 0)
    return true;

  // 3) If effective user is not the owner of the browser and is an
  // administrator.
  return !BundleOwnedByCurrentUser() && IsEffectiveUserAdmin();
}

int RunCommand(const base::FilePath& exe_path, const char* cmd_switch) {
  base::CommandLine command(exe_path);
  command.AppendSwitch(cmd_switch);

  int exit_code = -1;
  auto process = base::LaunchProcess(command, {});
  if (!process.IsValid())
    return exit_code;

  process.WaitForExitWithTimeout(base::Seconds(120), &exit_code);

  return exit_code;
}

// Only works in kUser scope.
void RegisterBrowser(base::OnceClosure complete) {
  BrowserUpdaterClient::Create(updater::UpdaterScope::kUser)
      ->Register(std::move(complete));
}

// Only works in kUser scope.
void InstallUpdaterAndRegisterBrowser(base::OnceClosure complete) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([]() {
        // The updater executable should be in
        // BRANDING.app/Contents/Frameworks/BRANDING.framework/Versions/V/
        // Helpers/Updater.app/Contents/MacOS/Updater
        const base::FilePath updater_executable_path =
            base::mac::FrameworkBundlePath()
                .Append(FILE_PATH_LITERAL("Helpers"))
                .Append(GetUpdaterExecutablePath());

        if (!base::PathExists(updater_executable_path)) {
          VLOG(1) << "The updater does not exist in the bundle.";
          return false;
        }

        int exit_code = RunCommand(updater_executable_path, kInstallCommand);
        if (exit_code != 0) {
          VLOG(1) << "Couldn't install the updater. Exit code: " << exit_code;
          return false;
        }
        return true;
      }),
      base::BindOnce(
          [](base::OnceClosure complete, bool success) {
            if (success) {
              RegisterBrowser(std::move(complete));
            } else {
              std::move(complete).Run();
            }
          },
          std::move(complete)));
}

}  // namespace

std::string CurrentlyInstalledVersion() {
  base::FilePath outer_bundle = base::mac::OuterBundlePath();
  base::FilePath plist_path =
      outer_bundle.Append("Contents").Append("Info.plist");
  NSDictionary* info_plist = [NSDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
  return base::SysNSStringToUTF8(
      base::mac::ObjCCast<NSString>(info_plist[@"CFBundleShortVersionString"]));
}

updater::UpdaterScope GetUpdaterScope() {
  return BundleOwnedByRoot() ? updater::UpdaterScope::kSystem
                             : updater::UpdaterScope::kUser;
}

void EnsureUpdater(base::OnceClosure prompt, base::OnceClosure complete) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetUpdaterScope),
      base::BindOnce(
          [](base::OnceClosure prompt, base::OnceClosure complete,
             updater::UpdaterScope scope) {
            scoped_refptr<BrowserUpdaterClient> client =
                BrowserUpdaterClient::Create(scope);
            client->IsBrowserRegistered(base::BindOnce(
                [](scoped_refptr<BrowserUpdaterClient> client,
                   base::OnceClosure prompt, base::OnceClosure complete,
                   bool registered) {
                  if (registered) {
                    std::move(complete).Run();
                    return;
                  }
                  base::ThreadPool::PostTaskAndReplyWithResult(
                      FROM_HERE, {base::MayBlock()},
                      base::BindOnce(&ShouldPromoteUpdater),
                      base::BindOnce(
                          [](scoped_refptr<BrowserUpdaterClient> client,
                             base::OnceClosure prompt,
                             base::OnceClosure complete, bool promote) {
                            if (promote) {
                              // User intervention is required; prompt.
                              std::move(prompt).Run();
                              std::move(complete).Run();
                              return;
                            }
                            // Check whether an updater exists.
                            client->GetUpdaterVersion(base::BindOnce(
                                [](base::OnceClosure complete,
                                   const base::Version& version) {
                                  if (!version.IsValid()) {
                                    InstallUpdaterAndRegisterBrowser(
                                        std::move(complete));
                                  } else {
                                    RegisterBrowser(std::move(complete));
                                  }
                                },
                                std::move(complete)));
                          },
                          client, std::move(prompt), std::move(complete)));
                },
                client, std::move(prompt), std::move(complete)));
          },
          std::move(prompt), std::move(complete)));
}

void SetupSystemUpdater() {
  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_PROMOTE_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::AuthorizationCreateToRunAsRoot(base::mac::NSToCFCast(prompt));
  if (!authorization.get()) {
    VLOG(0) << "Could not get authorization to run as root.";
    return;
  }

  base::ScopedCFTypeRef<CFErrorRef> error;
  Boolean result = SMJobBless(kSMDomainSystemLaunchd,
                              base::SysUTF8ToCFStringRef(kPrivilegedHelperName),
                              authorization, error.InitializeInto());
  if (!result) {
    base::ScopedCFTypeRef<CFStringRef> desc(CFErrorCopyDescription(error));
    VLOG(0) << "Could not bless the privileged helper. Resulting error: "
            << base::SysCFStringRefToUTF8(desc);
  }

  base::MakeRefCounted<BrowserUpdaterHelperClientMac>()->SetupSystemUpdater(
      base::BindOnce([](int result) {
        VLOG_IF(1, result != 0)
            << "There was a problem with performing the system "
               "updater tasks. Result: "
            << result;
      }));
}
