(async function() {
  TestRunner.addResult(
      'Verifies that BinaryResourceViewFactory interprets base64 data correctly');
  TestRunner.addResult('');

  await TestRunner.loadLegacyModule('source_frame');
  const base64content =
      'c2VuZGluZyB0aGlzIHV0Zi04IHN0cmluZyBhcyBhIGJpbmFyeSBtZXNzYWdlLi4u';
  const factory = new SourceFrame.BinaryResourceViewFactory(
      base64content, 'http://example.com', Common.resourceTypes.WebSocket);

  TestRunner.addResult('Base64View:');
  TestRunner.addResult((await factory.createBase64View().lazyContent()).content);
  TestRunner.addResult('');

  TestRunner.addResult('HexView:');
  TestRunner.addResult((await factory.createHexView().lazyContent()).content);
  TestRunner.addResult('');

  TestRunner.addResult('Utf8View:');
  TestRunner.addResult((await factory.createUtf8View().lazyContent()).content);
  TestRunner.addResult('');

  TestRunner.completeTest();
})();
