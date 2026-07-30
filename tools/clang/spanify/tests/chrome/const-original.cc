// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

namespace function_params_and_return {

int const* get_buf_east();

int const* get_buf_east() {
  static std::vector<int> buf;
  return &buf[0];
}

void f_east() {
  int const* buf = get_buf_east();
  (void)buf[0];
}

}  // namespace function_params_and_return
