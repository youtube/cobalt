// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"

#include "chrome/services/cups_proxy/test/libcups_test_suite.h"

int main(int argc, char** argv) {
  cups_proxy::LibCupsTestSuite test_suite(argc, argv);
  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&cups_proxy::LibCupsTestSuite::Run,
                     base::Unretained(&test_suite)));
}
