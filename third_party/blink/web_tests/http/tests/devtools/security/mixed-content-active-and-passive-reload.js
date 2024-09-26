// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the active and passive mixed content explanations prompt the user to refresh when there are no recorded requests, and link to the network panel when there are recorded requests.\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  TestRunner.addResult('\nBefore Refresh --------------');

  const pageVisibleSecurityState = new Security.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Neutral, null, null,
    ['displayed-mixed-content', 'ran-mixed-content']);
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  // At this point, the page has mixed content but no mixed requests have been recorded, so the user should be prompted to refresh.
  var explanations =
      Security.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);

  TestRunner.addResult('\nRefresh --------------');

  // Now simulate a refresh.
  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  var passive = SDK.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  passive.mixedContentType = 'optionally-blockable';
  SecurityTestRunner.dispatchRequestFinished(passive);

  var active = SDK.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  active.mixedContentType = 'blockable';
  SecurityTestRunner.dispatchRequestFinished(active);

  var explanations =
      Security.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);
  TestRunner.completeTest();
})();
