// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" int ContentAppMain(int argc, char* argv[]);

int main(int argc, char* argv[]) {
  return ContentAppMain(argc, argv);
}
