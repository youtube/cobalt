// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_PATCHER_H_
#define COBALT_UPDATER_PATCHER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/update_client/patcher.h"

// TODO(b/174699165): Investigate differential updates later.
// This is a skeleton class for now.

namespace cobalt {
namespace updater {

class PatcherFactory : public update_client::PatcherFactory {
 public:
  PatcherFactory();
  PatcherFactory(const PatcherFactory&) = delete;
  PatcherFactory& operator=(const PatcherFactory&) = delete;

  scoped_refptr<update_client::Patcher> Create() const override;

 protected:
  ~PatcherFactory() override;
};

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_PATCHER_H_
