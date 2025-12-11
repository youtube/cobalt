function focusedElementDescription()
{
  var element = accessibilityController.focusedElement;
  var name = element.name;
  var value = '"' + element.valueDescription.substr(20) + '"';
  var description = element.description;
  var placeholder = element.placeholder;
  return name + ', ' + placeholder + ', ' + value + ', ' + description + ', intValue:' + element.intValue + ', range:' + element.minValue + '-' + element.maxValue;
}
