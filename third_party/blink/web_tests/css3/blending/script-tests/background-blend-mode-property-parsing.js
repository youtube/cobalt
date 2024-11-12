description("Test the parsing of the background-blend-mode property.");

// These have to be global for the test helpers to see them.
var stylesheet, cssRule, declaration, blendModeRule, subRule;
var styleElement = document.createElement("style");
document.head.appendChild(styleElement);
stylesheet = styleElement.sheet;

function testBlendModeRule(description, rule, expectedLength, expectedValue, expectedTypes, expectedTexts)
{
    debug("");
    debug(description + " : " + rule);

    stylesheet.insertRule("body { background-blend-mode: " + rule + "; }", 0);
    cssRule = stylesheet.cssRules.item(0);

    shouldBe("cssRule.type", "1");

    declaration = cssRule.style;
    shouldBe("declaration.length", "1");
    shouldBe("declaration.getPropertyValue('background-blend-mode')", "'" + expectedValue + "'");
}

var blendmodes = ["normal", "multiply, screen", "screen, hue", "overlay, normal", "darken, lighten, normal, luminosity", "lighten", "color-dodge", "color-burn", "hard-light", "soft-light", "difference", "exclusion", "hue", "saturation", "color", "luminosity"];

for(x in blendmodes)
   testBlendModeRule("Basic reference", blendmodes[x], 1, blendmodes[x]);


successfullyParsed = true;
