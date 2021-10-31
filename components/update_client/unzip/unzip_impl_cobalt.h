// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_
#define COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/unzipper.h"

namespace update_client {

class UnzipCobaltFactory : public UnzipperFactory {
 public:
  UnzipCobaltFactory();

  std::unique_ptr<Unzipper> Create() const override;

 protected:
  ~UnzipCobaltFactory() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnzipCobaltFactory);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UNZIP_UNZIP_IMPL_COBALT_H_
