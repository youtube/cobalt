// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests UISourceCode class.\n`);
  await TestRunner.showPanel('sources');

  var MockProject = class extends Workspace.Workspace.ProjectStore {
    requestFileContent(uri) {
      TestRunner.addResult('Content is requested from SourceCodeProvider.');
      return new Promise(resolve => {
        setTimeout(() => resolve({ content: 'var x = 0;', error: null, isEncoded: false }));
      });
    }

    mimeType() {
      return 'text/javascript';
    }

    isServiceProject() {
      return false;
    }

    type() {
      return Workspace.Workspace.projectTypes.Debugger;
    }

    url() {
      return 'mock://debugger-ui/';
    }
  };

  TestRunner.runTestSuite([function testUISourceCode(next) {
    var uiSourceCode = new Workspace.UISourceCode.UISourceCode(new MockProject(), 'url', Common.ResourceType.resourceTypes.Script);
    function didRequestContent(callNumber, { content, error, isEncoded }) {
      TestRunner.addResult('Callback ' + callNumber + ' is invoked.');
      TestRunner.assertEquals('text/javascript', uiSourceCode.mimeType());
      TestRunner.assertEquals('var x = 0;', content);

      if (callNumber === 3) {
        // Check that sourceCodeProvider.requestContent won't be called anymore.
        uiSourceCode.requestContent().then(function({ content, error, isEncoded }) {
          TestRunner.assertEquals('text/javascript', uiSourceCode.mimeType());
          TestRunner.assertEquals('var x = 0;', content);
          next();
        });
      }
    }
    // Check that all callbacks will be invoked.
    uiSourceCode.requestContent().then(didRequestContent.bind(null, 1));
    uiSourceCode.requestContent().then(didRequestContent.bind(null, 2));
    uiSourceCode.requestContent().then(didRequestContent.bind(null, 3));
  }]);
})();
