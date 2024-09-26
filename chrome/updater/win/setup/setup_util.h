// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <ios>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/work_item_list.h"

namespace base {
class FilePath;
}  // namespace base

namespace updater {

enum class UpdaterScope;

std::wstring GetTaskName(UpdaterScope scope);
void UnregisterWakeTask(UpdaterScope scope);

std::wstring GetProgIdForClsid(REFCLSID clsid);
std::wstring GetComProgIdRegistryPath(const std::wstring& progid);
std::wstring GetComServerClsidRegistryPath(REFCLSID clsid);
std::wstring GetComServerAppidRegistryPath(REFGUID appid);
std::wstring GetComIidRegistryPath(REFIID iid);
std::wstring GetComTypeLibRegistryPath(REFIID iid);

// Returns the resource index for the type library where the interface specified
// by the `iid` is defined. For encapsulation reasons, the updater interfaces
// are segregated in multiple IDL files, which get compiled to multiple type
// libraries. The type libraries are inserted in the compiled binary as
// resources with different resource indexes. The resource index becomes a
// suffix of the path to where the type library exists, such as
// `...\updater.exe\\1`. See the Windows SDK documentation for LoadTypeLib for
// details.
std::wstring GetComTypeLibResourceIndex(REFIID iid);

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can be installed side-by-side with other instances of the updater.
std::vector<IID> GetSideBySideInterfaces(UpdaterScope scope);

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can only be installed for the active instance of the updater.
std::vector<IID> GetActiveInterfaces(UpdaterScope scope);

// Returns the interfaces ids of all interfaces declared in IDL of the updater
// that can be installed side-by-side (if `is_internal` is `true`) or for the
// active instance (if `is_internal` is `false`) .
std::vector<IID> GetInterfaces(bool is_internal, UpdaterScope scope);

// Returns the CLSIDs of servers that can be installed side-by-side with other
// instances of the updater.
std::vector<CLSID> GetSideBySideServers(UpdaterScope scope);

// Returns the CLSIDs of servers that can only be installed for the active
// instance of the updater.
std::vector<CLSID> GetActiveServers(UpdaterScope scope);

// Returns the CLSIDs of servers that can be installed side-by-side (if
// `is_internal` is `true`) or for the active instance (if `is_internal` is
// `false`) .
std::vector<CLSID> GetServers(bool is_internal, UpdaterScope scope);

// Helper function that joins two vectors and returns the resultant vector.
template <typename T>
std::vector<T> JoinVectors(const std::vector<T>& vector1,
                           const std::vector<T>& vector2) {
  std::vector<T> joined_vector = vector1;
  joined_vector.insert(joined_vector.end(), vector2.begin(), vector2.end());
  return joined_vector;
}

// Adds work items to register the per-user COM server.
void AddComServerWorkItems(const base::FilePath& com_server_path,
                           bool is_internal,
                           WorkItemList* list);

// Adds work items to register the COM service.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            bool internal_service,
                            WorkItemList* list);

// Adds a worklist item to set a value in the Run key in the user registry under
// the value `run_value_name` to start the specified `command`.
void RegisterUserRunAtStartup(const std::wstring& run_value_name,
                              const base::CommandLine& command,
                              WorkItemList* list);

// Deletes the value in the Run key in the user registry under the value
// `run_value_name`.
bool UnregisterUserRunAtStartup(const std::wstring& run_value_name);

class RegisterWakeTaskWorkItem : public WorkItem {
 public:
  RegisterWakeTaskWorkItem(const base::CommandLine& run_command,
                           UpdaterScope scope);

  RegisterWakeTaskWorkItem(const RegisterWakeTaskWorkItem&) = delete;
  RegisterWakeTaskWorkItem& operator=(const RegisterWakeTaskWorkItem&) = delete;

  ~RegisterWakeTaskWorkItem() override;

 private:
  // Overrides of WorkItem.
  bool DoImpl() override;
  void RollbackImpl() override;

  const base::CommandLine run_command_;
  const UpdaterScope scope_;
  std::wstring task_name_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
