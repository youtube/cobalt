// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests workspace mappings\n`);


  var projects = {};
  var workspace = new Workspace.Workspace();

  function createUISourceCode(projectId, relativePath) {
    var project = projects[projectId];
    if (!projects[projectId]) {
      var projectType =
          projectId.startsWith('file') ? Workspace.projectTypes.FileSystem : Workspace.projectTypes.Network;
      project = new Workspace.ProjectStore(workspace, projectId, projectType, '');
      workspace.addProject(project);
      projects[projectId] = project;
    }
    var uiSourceCode = project.createUISourceCode(projectId + '/' + relativePath, Common.resourceTypes.Script);
    project.addUISourceCode(uiSourceCode);
  }

  function dumpUISourceCodeForURL(url) {
    var uiSourceCode = workspace.uiSourceCodeForURL(url);
    TestRunner.addResult('uiSourceCode for url ' + url + ': ' + (uiSourceCode ? 'EXISTS' : 'null'));
  }

  createUISourceCode('file:///var/www', 'localhost/index.html');

  createUISourceCode('http://www.example.com', 'index.html');
  createUISourceCode('http://localhost', 'index.html');
  createUISourceCode('http://localhost', 'foo/index.html');
  createUISourceCode('https://localhost', 'index.html');

  dumpUISourceCodeForURL('http://www.example.com/index.html');
  dumpUISourceCodeForURL('http://localhost/index.html');
  dumpUISourceCodeForURL('http://localhost/foo/index.html');
  dumpUISourceCodeForURL('https://localhost/index.html');
  dumpUISourceCodeForURL('file:///var/www/localhost/index.html');
  dumpUISourceCodeForURL('file:///var/www/localhost/index2.html');

  TestRunner.completeTest();
})();
