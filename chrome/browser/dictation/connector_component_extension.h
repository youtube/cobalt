// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_CONNECTOR_COMPONENT_EXTENSION_H_
#define CHROME_BROWSER_DICTATION_CONNECTOR_COMPONENT_EXTENSION_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

class Profile;

namespace base {
class FilePath;
}

namespace dictation {

// Manages the installation of the Dictation Connector component extension.
class ConnectorComponentExtension {
 public:
  explicit ConnectorComponentExtension(Profile* profile);
  ConnectorComponentExtension(const ConnectorComponentExtension&) = delete;
  ConnectorComponentExtension& operator=(const ConnectorComponentExtension&) =
      delete;
  ~ConnectorComponentExtension();

  // Returns true if still waiting on the installation of the connector
  // component extension.
  bool IsPending() const { return install_pending_; }

 private:
  void InstallConnectorExtension(const base::FilePath& directory);
  void OnManifestLoaded(const base::FilePath& directory,
                        std::optional<base::DictValue> manifest);

  raw_ptr<Profile> profile_;
  bool install_pending_ = true;

  base::CallbackListSubscription get_extension_directory_subscription_;

  base::WeakPtrFactory<ConnectorComponentExtension> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_CONNECTOR_COMPONENT_EXTENSION_H_
