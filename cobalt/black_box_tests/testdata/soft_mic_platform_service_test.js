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

var SOFT_MIC_SERVICE_NAME = "com.google.youtube.tv.SoftMic";

function failTest() {
  notReached();
  onEndTest();
}

/**
* @param {ArrayBuffer} data to be converted to a String.
*/
function ab2str(data) {
  try {
    var string_data = new TextDecoder("utf-8").decode(data);
    return string_data;
  } catch(error) {
    console.log(`ab2str() error: ${error}, decoding data: ${data}`);
    failTest();
  }
}

/**
* @param {String} data to be converted to an ArrayBuffer.
*/
function str2ab(data) {
  try {
    var array_buffer_data = new TextEncoder().encode(data).buffer;
    return array_buffer_data;
  } catch(error) {
    console.log(`str2ab() error: ${error}, decoding data: ${data}`);
    failTest();
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

function testService(assertCallback) {
  /**
  * @param {ArrayBuffer} data
  */
   function receiveCallback(service, data) {
    var str_response = ab2str(data);

    try {
      var response = JSON.parse(str_response);
      var has_hard_mic = response["hasHardMicSupport"];
      var has_soft_mic = response["hasSoftMicSupport"];
      console.log(`receiveCallback, has_hard_mic: ${has_hard_mic}, has_soft_mic: ${has_soft_mic}`);

      assertCallback(has_hard_mic, has_soft_mic);
    } catch (error) {
      console.log(`receiveCallback() error: ${error}`);
      failTest();
    }

    soft_mic_service.close();
    onEndTest();
  }

  if (!H5vccPlatformService) {
    console.log("H5vccPlatformService is not implemented");
    failTest();
    return;
  }

  if (!H5vccPlatformService.has(SOFT_MIC_SERVICE_NAME)) {
    console.log(`H5vccPlatformService.Has(${SOFT_MIC_SERVICE_NAME}) returned false.`);
    failTest();
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

  soft_mic_service.send(str2ab(JSON.stringify("getMicSupport")));
}

window.onkeydown = function(event) {
  if (event.keyCode === 97) {
    testService(bothUndefined);
  } else if (event.keyCode === 98) {
    testService(hardMicUndefinedSoftMicTrue);
  } else if (event.keyCode === 99) {
    testService(hardMicUndefinedSoftMicFalse);
  } else if (event.keyCode === 100) {
    testService(hardMicTrueSoftMicUndefined);
  } else if (event.keyCode === 101) {
    testService(hardMicTrueSoftMicTrue);
  } else if (event.keyCode === 102) {
    testService(hardMicTrueSoftMicFalse);
  } else if (event.keyCode === 103) {
    testService(hardMicFalseSoftMicUndefined);
  } else if (event.keyCode === 104) {
    testService(hardMicFalseSoftMicTrue);
  } else if (event.keyCode === 105) {
    testService(hardMicFalseSoftMicFalse);
  }
}
