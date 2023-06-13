// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/strings/string_number_conversions.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/serialize_script_value_interface.h"

using ::testing::ContainsRegex;

namespace cobalt {
namespace bindings {
namespace testing {

class SerializeScriptValueTest
    : public InterfaceBindingsTest<SerializeScriptValueInterface> {
 public:
  void ExpectTrue(const std::string& script) {
    std::string result;
    EvaluateScript("`${" + script + "}`", &result);
    EXPECT_EQ("true", result)
        << "Expect \"" + script + "\" to evaluate to true.";
  }
};

TEST_F(SerializeScriptValueTest, Serialize) {
  // Stores serialized result that is then used by |deserializeTest()|.
  EvaluateScript(R"(
window.arrayBuffer = new ArrayBuffer(2);
window.sharedArrayBuffer = new SharedArrayBuffer(4);
{
  new Uint8Array(window.arrayBuffer).fill(10);
  new Uint8Array(window.sharedArrayBuffer).fill(20);
}
test.serializeTest({
  a: ['something'],
  b: new Uint8Array([42]),
  c: window.arrayBuffer,
  d: window.sharedArrayBuffer,
});
    )");
  ExpectTrue("!(test.deserializeTest() instanceof Array)");
  ExpectTrue("test.deserializeTest() instanceof Object");
  ExpectTrue("test.deserializeTest().a instanceof Array");
  ExpectTrue("1 === test.deserializeTest().a.length");
  ExpectTrue("'something' === test.deserializeTest().a[0]");
  ExpectTrue("test.deserializeTest().b instanceof Uint8Array");
  ExpectTrue("!(test.deserializeTest().b instanceof Uint16Array)");
  ExpectTrue("42 === test.deserializeTest().b[0]");
  ExpectTrue("test.deserializeTest().c instanceof ArrayBuffer");
  ExpectTrue("test.deserializeTest().d instanceof SharedArrayBuffer");
  EvaluateScript(R"(
window.otherArrayBuffer = test.deserializeTest().c;
window.otherSharedArrayBuffer = test.deserializeTest().d;
const arrayBufferToString = arrayBuffer =>
    new Uint8Array(arrayBuffer).toString();
window.string = arrayBufferToString(window.arrayBuffer);
window.otherString = arrayBufferToString(window.otherArrayBuffer);
window.sharedString = arrayBufferToString(window.sharedArrayBuffer);
window.otherSharedString = arrayBufferToString(window.otherSharedArrayBuffer);
    )");
  ExpectTrue("2 === window.arrayBuffer.byteLength");
  ExpectTrue("2 === window.otherArrayBuffer.byteLength");
  ExpectTrue("4 === window.sharedArrayBuffer.byteLength");
  ExpectTrue("4 === window.otherSharedArrayBuffer.byteLength");
  ExpectTrue("'10,10' === window.string");
  ExpectTrue("'10,10' === window.otherString");
  ExpectTrue("'20,20,20,20' === window.sharedString");
  ExpectTrue("'20,20,20,20' === window.otherSharedString");
  EvaluateScript(R"(
const updateArrayBuffer = arrayBuffer => {
 const view = new Uint8Array(arrayBuffer);
 view.forEach((x, i) => {
  view[i] = x + i * 10;
 });
};
updateArrayBuffer(window.otherArrayBuffer);
updateArrayBuffer(window.otherSharedArrayBuffer);
window.string = arrayBufferToString(window.arrayBuffer);
window.otherString = arrayBufferToString(window.otherArrayBuffer);
window.sharedString = arrayBufferToString(window.sharedArrayBuffer);
window.otherSharedString = arrayBufferToString(window.otherSharedArrayBuffer);
    )");
  ExpectTrue("'10,10' === window.string");
  ExpectTrue("'10,20' === window.otherString");
  ExpectTrue("'20,30,40,50' === window.sharedString");
  ExpectTrue("'20,30,40,50' === window.otherSharedString");
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
