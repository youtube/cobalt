// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests whether focus is properly remembered on widgets.\n`);


  var outerInput = document.createElement('input');
  outerInput.id = 'Outer';

  var input1 = document.createElement('input');
  input1.id = 'Input1';
  var input2 = document.createElement('input');
  input2.id = 'Input2';
  var input3 = document.createElement('input');
  input3.id = 'Input3';
  var input4 = document.createElement('input');
  input4.id = 'Input4';

  UI.inspectorView.element.appendChild(outerInput);

  var mainWidget = new UI.Widget();
  mainWidget.show(UI.inspectorView.element);

  var widget1 = new UI.Widget();
  widget1.show(mainWidget.element);
  widget1.element.appendChild(input1);
  widget1.setDefaultFocusedElement(input1);

  var widget2 = new UI.Widget();
  widget2.show(mainWidget.element);
  widget2.element.appendChild(input2);
  widget2.setDefaultFocusedElement(input2);

  TestRunner.addResult('Focusing outer input...');
  outerInput.focus();
  dumpFocus();

  TestRunner.addResult('Focusing widget1...');
  widget1.focus();
  dumpFocus();

  TestRunner.addResult('Focusing widget2...');
  input2.focus();
  dumpFocus();

  TestRunner.addResult('Focusing outer input again...');
  outerInput.focus();
  dumpFocus();

  TestRunner.addResult('Focusing main widget...');
  mainWidget.focus();
  dumpFocus();

  TestRunner.addResult('Focusing outer input again...');
  outerInput.focus();
  dumpFocus();

  TestRunner.addResult('Hiding widget2 and focusing main widget...');
  widget2.hideWidget();
  mainWidget.focus();
  dumpFocus();

  var splitWidget = new UI.SplitWidget();
  splitWidget.show(mainWidget.element);

  var widget3 = new UI.Widget();
  widget3.element.appendChild(input3);
  widget3.setDefaultFocusedElement(input3);
  splitWidget.setSidebarWidget(widget3);

  var widget4 = new UI.Widget();
  widget4.element.appendChild(input4);
  widget4.setDefaultFocusedElement(input4);
  splitWidget.setMainWidget(widget4);
  splitWidget.setDefaultFocusedChild(widget4);

  TestRunner.addResult('Focusing split widget in main that has 3 and 4 inputs...');
  splitWidget.focus();
  dumpFocus();

  TestRunner.addResult('Focusing widget 3...');
  widget3.focus();
  dumpFocus();

  TestRunner.addResult('Focusing main widget again...');
  mainWidget.focus();
  dumpFocus();

  TestRunner.completeTest();

  function dumpFocus() {
    var focused = Platform.DOMUtilities.deepActiveElement(document);
    var id = focused ? focused.id : '';
    TestRunner.addResult(id ? id + ' Focused' : 'No focus');
  }
})();
