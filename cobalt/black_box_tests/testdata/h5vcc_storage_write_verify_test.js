// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
'use strict';

function failTest() {
  notReached();
  onEndTest();
}

function assertWrite(bytes, test_string) {
  let response = h5vcc.storage.writeTest(bytes, test_string);
  if (response.error != "") {
    console.log(`assertWrite(${bytes}, ${test_string}), error: ${response.error}`);
    failTest();
    return false;
  }
  if (response.bytes_written != bytes) {
    console.log(`assertWrite(${bytes}, ${test_string}), bytes_written (${response.bytes_written}) not equal to bytes`);
    failTest();
    return false;
  }
  return true;
}

function assertVerify(bytes, test_string) {
  let response = h5vcc.storage.verifyTest(bytes, test_string);
  if (response.error != "") {
    console.log(`assertVerify(${bytes}, ${test_string}), error: ${response.error}`);
    failTest();
    return false;
  }
  if (response.bytes_read != bytes) {
    console.log(`assertVerify(${bytes}, ${test_string}), bytes_read (${response.bytes_read}) not equal to bytes`);
    failTest();
    return false;
  }
  if (!response.verified) {
    console.log(`assertVerify(${bytes}, ${test_string}), response not verified`);
    failTest();
    return false;
  }
  return true;
}

function h5vccStorageWriteVerifyTest() {
  if (!h5vcc.storage) {
    console.log("h5vcc.storage does not exist");
    return;
  }

  if (!h5vcc.storage.writeTest || !h5vcc.storage.verifyTest) {
    console.log("writeTest and verifyTest do not exist");
    return;
  }

  if (!assertWrite(1, "a"))
    return;
  if (!assertVerify(1, "a"))
    return;

  if (!assertWrite(100, "a"))
    return;
  if (!assertVerify(100, "a"))
    return;

  if (!assertWrite(24 * 1024 * 1024, "foobar"))
    return;
  if (!assertVerify(24 * 1024 * 1024, "foobar"))
    return;
}

h5vccStorageWriteVerifyTest();
onEndTest();
