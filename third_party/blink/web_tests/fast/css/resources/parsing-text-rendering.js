description("This tests checks that all of the input values for text-rendering parse correctly.");

function test(value)
{
    var div = document.createElement("div");
    div.setAttribute("style", value);
    document.body.appendChild(div);

    var result = div.style.getPropertyValue("text-rendering");
    document.body.removeChild(div);
    return result;
}

shouldBe('test("text-rendering: auto;")', '"auto"');
shouldBe('test("text-rendering: optimizespeed;")', '"optimizespeed"');
shouldBe('test("text-rendering: optimizelegibility;")', '"optimizelegibility"');
shouldBe('test("text-rendering: geometricprecision;")', '"geometricprecision"');
shouldBe('test("text-rendering: OptIMizESpEEd;")', '"optimizespeed"');

shouldBeEqualToString('test("text-rendering: auto auto;")', '');
shouldBeEqualToString('test("text-rendering: optimizeCoconuts;")', '');
shouldBeEqualToString('test("text-rendering: 15;")', '');
