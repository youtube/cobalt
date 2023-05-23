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

var changeChannelResult = null;
var waitForStatusResult = null;

function changeChannel() {
  const currentChannel = window.h5vcc.updater.getUpdaterChannel();
  const status = window.h5vcc.updater.getUpdateStatus();

  if (currentChannel == "" || status == "") {
    // An update check hasn't happened yet.
    return;
  }

  if (status != "App is up to date" &&
      status != "Update installed, pending restart") {
    // An update is in progress.
    return;
  }

  if (currentChannel == "prod") {
    window.h5vcc.updater.setUpdaterChannel(targetChannel);
    console.log('The channel was changed to ' + targetChannel);
    clearInterval(changeChannelResult);
    waitForStatusResult = setInterval(waitForStatus, 500, targetStatus);
    return;
  }

  if (currentChannel == targetChannel) {
    clearInterval(changeChannelResult);
    waitForStatusResult = setInterval(waitForStatus, 500, targetStatus);
    return;
  }
}

function waitForStatus() {
  const currentStatus = window.h5vcc.updater.getUpdateStatus();

  if (currentStatus == targetStatus) {
    console.log('The expected status was found: ' + targetStatus);
    assertTrue(true);
    clearInterval(waitForStatusResult);
    endTest();
    return;
  }

  return;
}

function endTest() {
  onEndTest();
  setupFinished();
}

var resetInstallations = false;
var encodedStatus = null;
var targetStatus = null;
var targetChannel = null;

var query = window.location.search;

if (query) {
  // Splits each parameter into an array after removing the prepended "?".
  query = query.slice(1).split("&");
}

query.forEach(part => {
  if (part.startsWith("resetInstallations=")) {
    resetInstallations = (part.split("=")[1] === "true")
  }

  if (part.startsWith("status=")) {
    encodedStatus = part.split("=")[1];
    targetStatus = decodeURI(encodedStatus);
  }

  if (part.startsWith("channel=")) {
    targetChannel = part.split("=")[1];
  }
});

if (resetInstallations) {
  window.h5vcc.updater.resetInstallations();
  console.log('Installations have been reset');
  assertTrue(true);
  endTest();
} else {
  ChannelResult = setInterval(changeChannel, 500);
}
