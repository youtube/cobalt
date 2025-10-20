// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/component.h"
#include "components/update_client/protocol_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/extension/installation_manager.h"
#endif

namespace update_client {

class Configurator;
class PersistedData;
struct UpdateContext;

class UpdateChecker {
 public:
  using UpdateCheckCallback = base::OnceCallback<void(
      const absl::optional<ProtocolParser::Results>& results,
      ErrorCategory error_category,
      int error,
      int retry_after_sec)>;

  using Factory =
      std::unique_ptr<UpdateChecker> (*)(scoped_refptr<Configurator> config,
                                         PersistedData* persistent);

  UpdateChecker(const UpdateChecker&) = delete;
  UpdateChecker& operator=(const UpdateChecker&) = delete;

  virtual ~UpdateChecker() = default;

  // Initiates an update check for the components specified by their ids.
  // `update_context` contains the updateable apps. `additional_attributes`
  // specifies any extra request data to send. On completion, the state of
  // `components` is mutated as required by the server response received.
  virtual void CheckForUpdates(
      scoped_refptr<UpdateContext> update_context,
      const base::flat_map<std::string, std::string>& additional_attributes,
      UpdateCheckCallback update_check_callback) = 0;

#if BUILDFLAG(IS_STARBOARD)
  virtual void Cancel() = 0;
  virtual bool SkipUpdate(const CobaltExtensionInstallationManagerApi* installation_api) = 0;
#endif

  static std::unique_ptr<UpdateChecker> Create(
      scoped_refptr<Configurator> config,
      PersistedData* persistent);
#if BUILDFLAG(IS_STARBOARD)
  virtual PersistedData* GetPersistedData() = 0;
#endif
 protected:
  UpdateChecker() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CHECKER_H_
