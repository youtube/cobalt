// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


// This is the function we set the breakpoint on.
function asyncBreak() {
  foo = "bar";
}

// Tests AsyncTask reporting in WindowTimers.
function testSetTimeout() {
  asyncA(asyncBreak);
}

function asyncA(f) {
  setTimeout(function timeoutA() { asyncB(f) }, 1);
}

function asyncB(f) {
  setTimeout(function timeoutB() { asyncC(f) }, 1);
}

function asyncC(f) {
  f();
}

// Tests AsyncTask reporting in EventTarget.
function testXHR(url) {
  doXHR(url);
}

function doXHR(url) {
  let xhr = new XMLHttpRequest();
  xhr.onload = function fileLoaded() {
    asyncBreak();
  }
  xhr.open('GET', url);
  xhr.send();
}

function testMutate() {
  let target = document.getElementById('test');
  let config = {attributes: true, childList: true, subtree: true};
  let observer = new MutationObserver(mutationCallback);
  observer.observe(target, config);
  doSetAttribute(target, 'foo', 'bar');
}

function mutationCallback(mutationsList, observer) {
  asyncBreak();
}

function doSetAttribute(node, attr, value) {
  node.setAttribute(attr, value);
}

