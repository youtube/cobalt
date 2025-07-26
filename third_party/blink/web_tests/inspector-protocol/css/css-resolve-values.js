(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const relativeLengthUnits = ["em", "ex", "cap", "ch", "ic", "lh", "rcap", "rch", "rem", "rex", "ric", "rlh", "vw", "vh ", "vi", "vb", "vmin", "vmax"];
  const lengthExpressions = ["calc(1em + 10px)", "calc(1em + 3em)", "calc(3px + 2.54cm)", "clamp(10px, calc(10rem + 10rex), 30px)", "max(100px, 30em)", "calc(100px * cos(60deg))"];
  const invalidLengthValues = ["calc(", "em", "calc(10 + 20)", "red", "calc(10ms + 5s)"];
  const testValues = ["invalid", "1em", "1rem", "calc(3px + 3px)", "calc(1em + 1px)"];
  const cssWideKeywords = ["initial", "inherit", "unset"];
  const validColorValues = ["aqua", "peachpuff", "blanchedalmond", "rgb(255, 0, 0)", "#0f5ffe", "color-mix(in srgb, plum, #f00)"];

  var {page, session, dp} = await testRunner.startURL('resources/css-resolve-values.html', 'Test css.resolveValue method');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  function print(inputValues, response) {
    testRunner.log("{\"input\": [" + inputValues.toString() + "]}");
    if (response.error) {
      testRunner.log(JSON.stringify(response.error));
    } else {
      testRunner.log(JSON.stringify(response.result));
    }
  }

  async function testResolveValues(querySelector, values, propertyName) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, propertyName: propertyName});
    print(values, response);
  }

  async function testResolveValuesWithoutProperty(querySelector, values) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId});
    print(values, response);
  }

  async function testResolveValuesForPseudoElement(querySelector, values, propertyName, pseudoType) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, propertyName: propertyName, pseudoType: pseudoType});
    print(values, response);
  }

  async function testResolveValuesForPseudoElementWithoutProperty(querySelector, values, pseudoType) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, pseudoType: pseudoType});
    print(values, response);
  }

  testRunner.runTestSuite([
    async function testNotExistentNode() {
      testRunner.log('Test non existent node');
      await testResolveValues('.notexistent', testValues, "height");
    },
    async function testInvalidProperty() {
      testRunner.log('Test invalid property name');
      await testResolveValues('.outer', testValues, "invalid");
    },
    async function testResolveValuesSimple() {
      testRunner.log('Test resolveValues for width property');
      await testResolveValues('.inner', testValues, "width");
    },
    async function testResolveValuesSimpleNoProperty() {
      testRunner.log('Test resolveValues no property specified');
      await testResolveValuesWithoutProperty('.inner', testValues);
    },
    async function testRelativeUnits() {
      testRunner.log('Relative length units to absolute test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.inner', relativeValues, "width");
    },
    async function testRelativeUnitsNoProperty() {
      testRunner.log('Relative length units to absolute no property specified test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValuesWithoutProperty('.inner', relativeValues);
    },
    async function testRelativeUnitsOuter() {
      testRunner.log('Relative length units to absolute for parent element test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.outer', relativeValues, "width");
    },
    async function testRelativeUnitsOuterNoProperty() {
      testRunner.log('Relative length units to absolute for parent element no property specified test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValuesWithoutProperty('.outer', relativeValues);
    },
    async function testRelativeUnitsFontSize() {
      testRunner.log('Relative length units to absolute font-size property test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.inner', relativeValues, "font-size");
    },
    async function testExpressionsEvaluation() {
      testRunner.log('Evaluate expression test');
      await testResolveValues('.inner', lengthExpressions, "height");
    },
    async function testExpressionsEvaluationNoProperty() {
      testRunner.log('Evaluate expression with no property specified test');
      await testResolveValuesWithoutProperty('.inner', lengthExpressions);
    },
    async function testCSSWideKeywords() {
      testRunner.log('Test CSS-wide keywords');
      await testResolveValues('.inner', cssWideKeywords, "font-size");
    },
    async function testCSSWideKeywordsNoProperty() {
      testRunner.log('Test CSS-wide keywords with no property specified');
      await testResolveValues('.inner', cssWideKeywords);
    },
    async function testColorValues() {
      testRunner.log('Test <color> values');
      await testResolveValues('.inner', validColorValues, "background-color");
    },
    async function testColorValuesNoProperty() {
      testRunner.log('Test <color> values with no property specified');
      await testResolveValuesWithoutProperty('.inner', validColorValues);
    },
    async function testOnlyInvalidValues() {
      testRunner.log('Invalid values test');
      await testResolveValues('.inner', invalidLengthValues, "width");
    },
    async function testCustomProperty() {
      testRunner.log('Test resolveValues on custom property');
      await testResolveValues('div', testValues, "--prop");
    },
    async function testRegisterCustomProperty() {
      testRunner.log('Test resolveValues on register custom property');
      await testResolveValues('div', testValues, "--reg-prop");
    },
    async function testPseudoElement() {
      testRunner.log('Pseudo element test');
      await testResolveValuesForPseudoElement('div', testValues, "height", 'after');
    },
    async function testPseudoElementNoProperty() {
      testRunner.log('Pseudo element with no property specified test');
      await testResolveValuesForPseudoElementWithoutProperty('div', testValues, 'after');
    },
    async function testNonExistentPseudoElement() {
      testRunner.log('Non existent pseudo element test');
      await testResolveValuesForPseudoElement('div', testValues, "height", 'marker');
    },
    async function testPseudoElementsNoElement() {
      testRunner.log('Pseudo element with no content');
      await testResolveValuesForPseudoElement('div', testValues, "width", 'before');
    },
    async function testElementDisplayNone() {
      testRunner.log('Test on element without computed style');
      await testResolveValues('.display-none', testValues, "height");
    }
  ]);
});