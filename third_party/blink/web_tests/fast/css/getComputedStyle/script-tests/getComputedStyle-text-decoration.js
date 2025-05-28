function testComputedStyle(propertyJS, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
}

description("Test to make sure text-decoration property returns values properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';
debug("Initial value:");
e = document.getElementById('test');
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Initial value (explicit):");
e.style.textDecoration = 'initial';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Value 'none':");
e.style.textDecoration = 'none';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Value 'underline':");
e.style.textDecoration = 'underline';
testComputedStyle("textDecoration", "underline solid rgb(0, 0, 0)");
debug('');

debug("Value 'overline':");
e.style.textDecoration = 'overline';
testComputedStyle("textDecoration", "overline solid rgb(0, 0, 0)");
debug('');

debug("Value 'line-through':");
e.style.textDecoration = 'line-through';
testComputedStyle("textDecoration", "line-through solid rgb(0, 0, 0)");
debug('');

debug("Value 'underline overline line-through':");
e.style.textDecoration = 'underline overline line-through';
testComputedStyle("textDecoration", "underline overline line-through solid rgb(0, 0, 0)");
debug('');

debug("Value 'blink' (valid but ignored):");
e.style.textDecoration = 'blink';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Value 'overline overline' (invalid):");
e.style.textDecoration = '';
e.style.textDecoration = 'overline overline';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Value 'underline blank' (invalid):");
e.style.textDecoration = '';
e.style.textDecoration = 'underline blank';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

debug("Value '':");
e.style.textDecoration = '';
testComputedStyle("textDecoration", "none solid rgb(0, 0, 0)");
debug('');

testContainer.innerHTML = '<div id="test-parent" style="text-decoration: underline;">hello <span id="test-ancestor" style="text-decoration: inherit;">world</span></div>';
debug("Parent gets 'underline' value:");
e = document.getElementById('test-parent');
testComputedStyle("textDecoration", "underline solid rgb(0, 0, 0)");
debug('');

debug("Ancestor should explicitly inherit value from parent when 'inherit' value is used:");
e = document.getElementById('test-ancestor');
testComputedStyle("textDecoration", "underline solid rgb(0, 0, 0)");
debug('');

debug("Ancestor should not implicitly inherit value from parent (i.e. when value is void):");
e.style.textDecoration = '';
testComputedStyle("textDecoration", "none");
debug('');

document.body.removeChild(testContainer);
