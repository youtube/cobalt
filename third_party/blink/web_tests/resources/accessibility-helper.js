function buildAccessibilityTree(accessibilityObject, indent, allAttributesRequired, rolesToIgnore, targetObject, targetString) {
    // Progressively appending to a string is slow (https://crbug.com/900098),
    // instead we build a list of lines and join that at the end.
    var consoleLines = [];
    var result = _buildAccessibilityTreeInner(accessibilityObject, indent, consoleLines, allAttributesRequired, rolesToIgnore, targetObject, targetString);
    document.getElementById("console").innerText += consoleLines.join("");
    return result;
}

function _buildAccessibilityTreeInner(accessibilityObject, indent, consoleLines, allAttributesRequired, rolesToIgnore, targetObject, targetString) {
    if (rolesToIgnore) {
        for (var i = 0; i < rolesToIgnore.length; i++) {
            if (accessibilityObject.role  == 'AXRole: ' + rolesToIgnore[i])
                return true;
        }
    }

    var str = "";
    for (var i = 0; i < indent; i++)
        str += "    ";
    str += accessibilityObject.role;
    if (accessibilityObject.value)
        str += " AXValue: " + accessibilityObject.value;
    else if (accessibilityObject.name)
        str += " \"" + accessibilityObject.name + "\"";
    str += allAttributesRequired && accessibilityObject.role == '' ? accessibilityObject.allAttributes() : '';
    str += targetObject && accessibilityObject.isEqual(targetObject) ? "     " + targetString : '';
    str += "\n";

    consoleLines.push(str)

    if (accessibilityObject.name.indexOf('End of test') >= 0)
        return false;

    var count = accessibilityObject.childrenCount;
    for (var i = 0; i < count; i++) {
        if (!_buildAccessibilityTreeInner(accessibilityObject.childAtIndex(i), indent + 1, consoleLines, allAttributesRequired, rolesToIgnore, targetObject, targetString))
            return false;
    }

    return true;
}

function traverseAccessibilityTree(accessibilityObject) {
    var count = accessibilityObject.childrenCount;
    for (var i = 0; i < count; i++)
        traverseAccessibilityTree(accessibilityObject.childAtIndex(i));
}
