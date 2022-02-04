// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

const SOFT_MIC_SERVICE_NAME = "com.google.youtube.tv.SoftMic";

function failTest() {
  notReached();
  onEndTest();
}

/**
* @param {ArrayBuffer} data to be converted to a String.
*/
function ab2str(data) {
  try {
    return String.fromCharCode.apply(null, new Uint8Array(data));
  } catch(error) {
    console.error(`ab2str() error: ${error}, decoding data: ${data}`);
  }
}

/**
* @param {String} data to be converted to an ArrayBuffer.
*/
function str2ab(data) {
  try {
    return Uint8Array.from(data.split(''), (s) => {return s.charCodeAt(0)}).buffer;
  } catch(error) {
    console.error(`str2ab() error: ${error}, decoding data: ${data}`);
  }
}

function bothUndefined(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertTrue(soft_mic);
}

function hardMicUndefinedSoftMicTrue(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertTrue(soft_mic);
}

function hardMicUndefinedSoftMicFalse(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertFalse(soft_mic);
}

function hardMicTrueSoftMicUndefined(hard_mic, soft_mic) {
  assertTrue(hard_mic);
  assertTrue(soft_mic);
}

function hardMicTrueSoftMicTrue(hard_mic, soft_mic) {
  assertTrue(hard_mic);
  assertTrue(soft_mic);
}

function hardMicTrueSoftMicFalse(hard_mic, soft_mic) {
  assertTrue(hard_mic);
  assertFalse(soft_mic);
}

function hardMicFalseSoftMicUndefined(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertTrue(soft_mic);
}

function hardMicFalseSoftMicTrue(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertTrue(soft_mic);
}

function hardMicFalseSoftMicFalse(hard_mic, soft_mic) {
  assertFalse(hard_mic);
  assertFalse(soft_mic);
}

function micGestureNull(micGesture) {
  assertFalse(micGesture);
  assertTrue(micGesture == null);
}

function micGestureHold(micGesture) {
  assertTrue(micGesture);
  assertTrue(micGesture == "HOLD");
}

function micGestureTap(micGesture) {
  assertTrue(micGesture);
  assertTrue(micGesture == "TAP");
}

/**
* @param {function} assertCallback
* @param {boolean} testMicGestureOnly
*/
function testService(assertCallback, testMicGestureOnly = false) {
  var service_send_done = false;
  var service_response_received = false;

  /**
  * @param {ArrayBuffer} data
  */
  function testResponseIsTrue(data) {
    try {
      assertTrue(new Int8Array(data)[0]);
    } catch (error) {
      console.log(`Error in testResponseIsTrue: ${error}`);
      notReached();
    }
  }

  /**
  * @param {ArrayBuffer} data
  */
  function receiveCallback(service, data) {
    var str_response = ab2str(data);

    try {
      var response = JSON.parse(str_response);
      var has_hard_mic = response["hasHardMicSupport"];
      var has_soft_mic = response["hasSoftMicSupport"];
      var mic_gesture = response["micGesture"];

      console.log(`receiveCallback() response:
                  has_hard_mic: ${has_hard_mic},
                  has_soft_mic: ${has_soft_mic},
                  micGesture: ${mic_gesture}`);

      if (testMicGestureOnly)
        assertCallback(mic_gesture);
      else
        assertCallback(has_hard_mic, has_soft_mic);
    } catch (error) {
      console.log(`receiveCallback() error: ${error}`);
      failTest();
    }

    service_response_received = true;

    if (service_send_done) {
      soft_mic_service.close();
      onEndTest();
    }
  }

  if (!H5vccPlatformService) {
    console.log("H5vccPlatformService is not implemented");
    onEndTest();
    return;
  }

  if (!H5vccPlatformService.has(SOFT_MIC_SERVICE_NAME)) {
    console.log(`H5vccPlatformService.Has(${SOFT_MIC_SERVICE_NAME}) returned false.`);
    onEndTest();
    return;
  }

  // Open the service and pass the receive_callback.
  var soft_mic_service = H5vccPlatformService.open(SOFT_MIC_SERVICE_NAME,
                              receiveCallback);

  if (soft_mic_service === null) {
    console.log("H5vccPlatformService.open() returned null");
    failTest();
    return;
  }

  // Send "getMicSupport" message and test the sync response here and the async platform
  // response in the receiveCallback()
  testResponseIsTrue(soft_mic_service.send(str2ab(JSON.stringify("getMicSupport"))));

  // Test the sync response for "notifySearchActive".
  testResponseIsTrue(soft_mic_service.send(str2ab(JSON.stringify("notifySearchActive"))));

  // Test the sync response for "notifySearchInactive".
  testResponseIsTrue(soft_mic_service.send(str2ab(JSON.stringify("notifySearchInactive"))));

  service_send_done = true;

  if (service_response_received) {
    soft_mic_service.close();
    onEndTest();
  }
}

function testIncorrectRequests() {
  /**
  * @param {ArrayBuffer} data
  */
  function testResponseIsFalse(data) {
    try {
      assertFalse(new Int8Array(data)[0]);
    } catch (error) {
      console.log(`Error in testResponseIsFalse: ${error}`);
      notReached();
    }
  }

  if (!H5vccPlatformService) {
    console.log("H5vccPlatformService is not implemented");
    onEndTest();
    return;
  }

  if (!H5vccPlatformService.has(SOFT_MIC_SERVICE_NAME)) {
    console.log(`H5vccPlatformService.Has(${SOFT_MIC_SERVICE_NAME}) returned false.`);
    onEndTest();
    return;
  }

  // Open the service and pass the receive_callback.
  var soft_mic_service = H5vccPlatformService.open(SOFT_MIC_SERVICE_NAME, (service, data) => { });

  if (soft_mic_service === null) {
    console.log("H5vccPlatformService.open() returned null");
    failTest();
    return;
  }

  // Send "getMicSupport" without JSON.stringify.
  testResponseIsFalse(soft_mic_service.send(str2ab("getMicSupport")));

  // Send "notifySearchActive" without JSON.stringify.
  testResponseIsFalse(soft_mic_service.send(str2ab("notifySearchActive")));

  // Send "notifySearchInactive" without JSON.stringify.
  testResponseIsFalse(soft_mic_service.send(str2ab("notifySearchInactive")));

  // Send "" empty string.
  testResponseIsFalse(soft_mic_service.send(str2ab("")));

  // Send "foo" invalid message string.
  testResponseIsFalse(soft_mic_service.send(str2ab("foo")));

  // Complete the test.
  soft_mic_service.close();
  onEndTest();
}

/**
* @param {KeyboardEvent} event
*/
window.onkeydown = function(event) {
  if (event.shiftKey) {
    testMicGesture(event)
    return;
  }

  if (event.key == 0) {
    testIncorrectRequests();
  } else if (event.key == 1) {
    testService(bothUndefined);
  } else if (event.key == 2) {
    testService(hardMicUndefinedSoftMicTrue);
  } else if (event.key == 3) {
    testService(hardMicUndefinedSoftMicFalse);
  } else if (event.key == 4) {
    testService(hardMicTrueSoftMicUndefined);
  } else if (event.key == 5) {
    testService(hardMicTrueSoftMicTrue);
  } else if (event.key == 6) {
    testService(hardMicTrueSoftMicFalse);
  } else if (event.key == 7) {
    testService(hardMicFalseSoftMicUndefined);
  } else if (event.key == 8) {
    testService(hardMicFalseSoftMicTrue);
  } else if (event.key == 9) {
    testService(hardMicFalseSoftMicFalse);
  }
}

/**
* @param {KeyboardEvent} event
*/
function testMicGesture(event) {
  if (event.key == 0) {
    testService(micGestureNull, true);
  } else if (event.key == 1) {
    testService(micGestureHold, true);
  } else if (event.key == 2) {
    testService(micGestureTap, true);
  }
}
