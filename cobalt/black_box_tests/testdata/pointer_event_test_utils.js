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

// This file provides test utility functions for pointer event related
// tests.

// Fail if the test is not finished within 30 seconds.
const kTimeout = 30 * 1000;
setTimeout(fail, kTimeout);

function fail() {
    console.log("Failing due to timeout!");
    assertTrue(false);
}

function phaseName(phase) {
    switch (phase) {
        case Event.NONE: return 'none';
        case Event.CAPTURING_PHASE: return 'capturing';
        case Event.AT_TARGET: return 'at target';
        case Event.BUBBLING_PHASE: return 'bubbling';
    }
    return `[unknown: '${phase}']`;
}

let current_event_number = 0
let failure_count = 0;

function checkEvent(e) {
    let matched = false;
    while (current_event_number < expected_events.length) {
      const {name, id, phase} = expected_events[current_event_number]
      current_event_number++;
      if (name === e.name && id === e.id && phase === e.phase) {
        matched = true;
        break;
      }
      console.error(`Missing Event ['${name}', '${id}', '${phaseName(phase)}']`);
      failure_count++;
    }
    if (!matched) {
      console.error(`Unexpected Event ['${e.name}', '${e.id}', '${phaseName(e.phase)}']`);
      failure_count++;
    }
}

function logEvent(e) {
    const pointertype = e.pointerType ? e.pointerType + ' ' : '';
    const id = this.getAttribute('id');
    checkEvent({ name: e.type, id: id, phase: e.eventPhase });
    console.log(`${e.type} ${pointertype}${id} (${this.getAttribute('class')}) \
        [${phaseName(e.eventPhase)}] (${e.screenX} ${e.screenY})`);
}

function setHandlers(event, selector, callback) {
    var elements = document.querySelectorAll(selector);
    for (var i = 0; i < elements.length; ++i) {
        elements[i].addEventListener(event, callback);
    }
}
