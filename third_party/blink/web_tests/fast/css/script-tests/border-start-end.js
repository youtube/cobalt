description('Tests border-start and border-end');

function test(dir, css, queryProp)
{
    var div = document.createElement('div');
    div.setAttribute('style', css);
    div.dir = dir;
    document.body.appendChild(div);

    var result = getComputedStyle(div).getPropertyValue(queryProp);
    document.body.removeChild(div);
    return result;
}

function testSame(dir, prop, altProp, value)
{
    shouldBeEqualToString('test("' + dir + '", "' + prop + ': ' + value + '", "' + altProp + '")', value);
    shouldBeEqualToString('test("' + dir + '", "' + altProp + ': ' + value + '", "' + prop + '")', value);
}

function testWidth(dir, style)
{
    var div = document.createElement('div');
    div.setAttribute('style', 'width:100px;' + style);
    div.dir = dir;
    document.body.appendChild(div);

    var result = div.offsetWidth;
    document.body.removeChild(div);
    return result;
}

shouldBe('testWidth("ltr", "border-inline-start-width: 10px; border-inline-start-style: solid")', '110');
shouldBe('testWidth("ltr", "border-inline-end-width: 20px; border-inline-end-style:  solid")', '120');
shouldBe('testWidth("rtl", "border-inline-start-width: 10px; border-inline-start-style:  solid")', '110');
shouldBe('testWidth("rtl", "border-inline-end-width: 20px; border-inline-end-style:  solid")', '120');

testSame('ltr', 'border-inline-start-color', 'border-left-color', 'rgb(255, 0, 0)');
testSame('ltr', 'border-inline-end-color', 'border-right-color', 'rgb(255, 0, 0)');
testSame('rtl', 'border-inline-start-color', 'border-right-color', 'rgb(255, 0, 0)');
testSame('rtl', 'border-inline-end-color', 'border-left-color', 'rgb(255, 0, 0)');

testSame('ltr', 'border-inline-start-style', 'border-left-style', 'dotted');
testSame('ltr', 'border-inline-end-style', 'border-right-style', 'dotted');
testSame('rtl', 'border-inline-start-style', 'border-right-style', 'dotted');
testSame('rtl', 'border-inline-end-style', 'border-left-style', 'dotted');

shouldBeEqualToString('test("ltr", "border-inline-start-width: 10px; border-inline-start-style: solid", "border-left-width")', '10px');
shouldBeEqualToString('test("ltr", "border-inline-end-width: 10px; border-inline-end-style: solid", "border-right-width")', '10px');
shouldBeEqualToString('test("rtl", "border-inline-start-width: 10px; border-inline-start-style: solid", "border-right-width")', '10px');
shouldBeEqualToString('test("rtl", "border-inline-end-width: 10px; border-inline-end-style: solid", "border-left-width")', '10px');

shouldBeEqualToString('test("ltr", "border-left: 10px solid", "border-inline-start-width")', '10px');
shouldBeEqualToString('test("ltr", "border-right: 10px solid", "border-inline-end-width")', '10px');
shouldBeEqualToString('test("rtl", "border-left: 10px solid", "border-inline-end-width")', '10px');
shouldBeEqualToString('test("rtl", "border-right: 10px solid", "border-inline-start-width")', '10px');

shouldBeEqualToString('test("ltr", "border-inline-start: 10px solid red", "border-left-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("ltr", "border-inline-start: 10px solid red", "border-left-style")', 'solid');
shouldBeEqualToString('test("ltr", "border-inline-start: 10px solid red", "border-left-width")', '10px');

shouldBeEqualToString('test("rtl", "border-inline-start: 10px solid red", "border-right-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("rtl", "border-inline-start: 10px solid red", "border-right-style")', 'solid');
shouldBeEqualToString('test("rtl", "border-inline-start: 10px solid red", "border-right-width")', '10px');

shouldBeEqualToString('test("ltr", "border-inline-end: 10px solid red", "border-right-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("ltr", "border-inline-end: 10px solid red", "border-right-style")', 'solid');
shouldBeEqualToString('test("ltr", "border-inline-end: 10px solid red", "border-right-width")', '10px');

shouldBeEqualToString('test("rtl", "border-inline-end: 10px solid red", "border-left-color")', 'rgb(255, 0, 0)');
shouldBeEqualToString('test("rtl", "border-inline-end: 10px solid red", "border-left-style")', 'solid');
shouldBeEqualToString('test("rtl", "border-inline-end: 10px solid red", "border-left-width")', '10px');
