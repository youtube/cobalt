// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

namespace media {

void RunFilterCallback(::testing::Unused, FilterCallback* callback) {
  callback->Run();
  delete callback;
}

void DestroyFilterCallback(::testing::Unused, FilterCallback* callback) {
  delete callback;
}

void RunStopFilterCallback(FilterCallback* callback) {
  callback->Run();
  delete callback;
}

MockFilter::MockFilter() :
    requires_message_loop_(false) {
}

MockFilter::MockFilter(bool requires_message_loop) :
    requires_message_loop_(requires_message_loop) {
}

MockFilter::~MockFilter() {}

bool MockFilter::requires_message_loop() const {
  return requires_message_loop_;
}

const char* MockFilter::message_loop_name() const {
  return "MockFilter";
}


}  // namespace media
