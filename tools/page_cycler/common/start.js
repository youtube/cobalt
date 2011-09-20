// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script runs the page cycler.  It assumes that __pages is an array
// containing the directories for various pages to exercise.

document.title = "page cycler";

var initialPage = __pages[0];

document.cookie = "__navigated_to_report=0; path=/";
document.cookie = "__pc_done=0; path=/";
document.cookie = "__pc_pages=" + __pages + "; path=/";
document.cookie = "__pc_timings=; path=/";

var options = location.search.substring(1).split('&');

function getopt(name) {
  var r = new RegExp("^" + name + "=");
  for (i = 0; i < options.length; ++i) {
    if (options[i].match(r)) {
      return options[i].substring(name.length + 1);
    }
  }
  return null;
}

function start() {
  var iterations = document.getElementById("iterations").value;
  window.resizeTo(800, 800);
  var ts = (new Date()).getTime();
  var url = initialPage + "/index.html?n=" + iterations + "&i=0&p=0&ts=" + ts + "&td=0";
  window.location = url;
}

function render_form() {
  var form = document.createElement("FORM");
  form.setAttribute("action", "javascript:start()");

  var label = document.createTextNode("Iterations: ");
  form.appendChild(label);

  var input = document.createElement("INPUT");
  input.setAttribute("id", "iterations");
  var iterations = getopt("iterations");
  input.setAttribute("value", iterations ? iterations : "5");
  form.appendChild(input);

  input = document.createElement("INPUT");
  input.setAttribute("type", "submit");
  input.setAttribute("value", "Start");
  form.appendChild(input);

  document.body.appendChild(form);
}

render_form();

// should we start automatically?
if (location.search.match("auto=1")) {
  start();
} else {
  document.write("<h3>Note: You must have started chrome with <tt>--enable-file-cookies --js-flags=\"--expose_gc\"</tt> for this test to work manually.</h3>");
  if (!window.gc)
    document.write("<h3 style='color:red'>WARNING: window.gc is not defined.  Test results may be unreliable!</h3>");
}
