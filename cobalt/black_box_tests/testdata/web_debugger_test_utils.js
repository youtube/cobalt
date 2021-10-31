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

// Tests AsyncTask reporting in a Promise using its 'then' method.
function testPromise() {
  let p = makePromise();
  waitPromise(p);
}

function makePromise() {
  return new Promise(function promiseExecutor(resolve, reject) {
        setTimeout(function promiseTimeout() {
          resolve();
        }, 1)
      });
}

function waitPromise(p) {
  p.then(promiseThen);
}

function promiseThen() {
  asyncBreak();
}

// Tests AsyncTask reporting in a JS async function that awaits a promise.
function testAsyncFunction() {
  let p = makePromise();
  asyncAwait(p);
}

async function asyncAwait(p) {
  await p;
  asyncBreak();
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

// Tests AsyncTask reporting in a MutationObserver.
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

function testAnimationFrame() {
  doRequestAnimationFrame();
}

function doRequestAnimationFrame() {
  window.requestAnimationFrame(function animationFrameCallback() {
    asyncBreak();
  });
}

// Tests AsyncTask reporting in a MediaSource (that uses an EventQueue).
function testMediaSource(){
  let elem = document.createElement('video');
  let ms = new MediaSource;
  setSourceListener(ms);
  attachMediaSource(elem, ms);
}

function setSourceListener(source) {
  source.addEventListener('sourceopen', function sourceOpenCallback() {
    asyncBreak();
  });
}

function attachMediaSource(elem, ms) {
  let url = window.URL.createObjectURL(ms);
  elem.src = url;
}
