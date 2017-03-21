// |reftest| skip-if(!this.hasOwnProperty("SIMD"))

/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

var Float32x4 = SIMD.Float32x4;
var Float64x2 = SIMD.Float64x2;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;

var fround = Math.fround;

function boolToSimdLogical(b) {
    return b ? 0xFFFFFFFF | 0 : 0x0;
}

function testEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.equal, (x, y) => boolToSimdLogical(fround(x) == fround(y)), Int32x4);
}
function testNotEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.notEqual, (x, y) => boolToSimdLogical(fround(x) != fround(y)), Int32x4);
}
function testLessThanFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.lessThan, (x, y) => boolToSimdLogical(fround(x) < fround(y)), Int32x4);
}
function testLessThanOrEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(fround(x) <= fround(y)), Int32x4);
}
function testGreaterThanFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.greaterThan, (x, y) => boolToSimdLogical(fround(x) > fround(y)), Int32x4);
}
function testGreaterThanOrEqualFloat32x4(v, w) {
    testBinaryCompare(v, w, Float32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(fround(x) >= fround(y)), Int32x4);
}

function testEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.equal, (x, y) => boolToSimdLogical(x == y), Int32x4);
}
function testNotEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.notEqual, (x, y) => boolToSimdLogical(x != y), Int32x4);
}
function testLessThanFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.lessThan, (x, y) => boolToSimdLogical(x < y), Int32x4);
}
function testLessThanOrEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), Int32x4);
}
function testGreaterThanFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.greaterThan, (x, y) => boolToSimdLogical(x > y), Int32x4);
}
function testGreaterThanOrEqualFloat64x2(v, w) {
    testBinaryCompare(v, w, Float64x2.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), Int32x4);
}

function testEqualInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.equal, (x, y) => boolToSimdLogical(x == y), Int8x16);
}
function testNotEqualInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.notEqual, (x, y) => boolToSimdLogical(x != y), Int8x16);
}
function testLessThanInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.lessThan, (x, y) => boolToSimdLogical(x < y), Int8x16);
}
function testLessThanOrEqualInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), Int8x16);
}
function testGreaterThanInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.greaterThan, (x, y) => boolToSimdLogical(x > y), Int8x16);
}
function testGreaterThanOrEqualInt8x16(v, w) {
    testBinaryCompare(v, w, Int8x16.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), Int8x16);
}

function testEqualInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.equal, (x, y) => boolToSimdLogical(x == y), Int16x8);
}
function testNotEqualInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.notEqual, (x, y) => boolToSimdLogical(x != y), Int16x8);
}
function testLessThanInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.lessThan, (x, y) => boolToSimdLogical(x < y), Int16x8);
}
function testLessThanOrEqualInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), Int16x8);
}
function testGreaterThanInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.greaterThan, (x, y) => boolToSimdLogical(x > y), Int16x8);
}
function testGreaterThanOrEqualInt16x8(v, w) {
    testBinaryCompare(v, w, Int16x8.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), Int16x8);
}

function testEqualInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.equal, (x, y) => boolToSimdLogical(x == y), Int32x4);
}
function testNotEqualInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.notEqual, (x, y) => boolToSimdLogical(x != y), Int32x4);
}
function testLessThanInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.lessThan, (x, y) => boolToSimdLogical(x < y), Int32x4);
}
function testLessThanOrEqualInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.lessThanOrEqual, (x, y) => boolToSimdLogical(x <= y), Int32x4);
}
function testGreaterThanInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.greaterThan, (x, y) => boolToSimdLogical(x > y), Int32x4);
}
function testGreaterThanOrEqualInt32x4(v, w) {
    testBinaryCompare(v, w, Int32x4.greaterThanOrEqual, (x, y) => boolToSimdLogical(x >= y), Int32x4);
}

function test() {
  var Float32x4val = [
      Float32x4(1, 20, 30, 4),
      Float32x4(10, 2, 3, 40),
      Float32x4(9.999, 2.1234, 30.4443, 4),
      Float32x4(10, 2.1233, 30.4444, 4.0001),
      Float32x4(NaN, -Infinity, +Infinity, -0),
      Float32x4(+Infinity, NaN, -0, -Infinity),
      Float32x4(13.37, 42.42, NaN, 0)
  ];

  var v, w;
  for (v of Float32x4val) {
      for (w of Float32x4val) {
          testEqualFloat32x4(v, w);
          testNotEqualFloat32x4(v, w);
          testLessThanFloat32x4(v, w);
          testLessThanOrEqualFloat32x4(v, w);
          testGreaterThanFloat32x4(v, w);
          testGreaterThanOrEqualFloat32x4(v, w);
      }
  }

  var Float64x2val = [
      Float64x2(1, 20),
      Float64x2(10, 2),
      Float64x2(9.999, 2.1234),
      Float64x2(10, 2.1233),
      Float64x2(30.4443, 4),
      Float64x2(30.4444, 4.0001),
      Float64x2(NaN, -Infinity),
      Float64x2(+Infinity, NaN),
      Float64x2(+Infinity, -0),
      Float64x2(-0, -Infinity),
      Float64x2(13.37, 42.42),
      Float64x2(NaN, 0)
  ];

  for (v of Float64x2val) {
      for (w of Float64x2val) {
          testEqualFloat64x2(v, w);
          testNotEqualFloat64x2(v, w);
          testLessThanFloat64x2(v, w);
          testLessThanOrEqualFloat64x2(v, w);
          testGreaterThanFloat64x2(v, w);
          testGreaterThanOrEqualFloat64x2(v, w);
      }
  }

  var Int8x16val = [
      Int8x16(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
      Int8x16(-1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16),
      Int8x16(-1, 2, -3, 4, -5, 6, -7, 8, -9, 10, -11, 12, -13, 14, -15, 16),
      Int8x16(1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16),
      Int8x16(INT8_MAX, INT8_MAX, INT8_MIN, INT8_MIN, INT8_MIN + 1, INT8_MAX - 1, -7, -8, -9, -10, -11, -12, -13, -14, -15, -16),
      Int8x16(INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX - 1, INT8_MIN + 1, 7, 8, 9, 10, 11, 12, 13, 14, 15, -16)
  ];

  for (v of Int8x16val) {
      for (w of Int8x16val) {
          testEqualInt8x16(v, w);
          testNotEqualInt8x16(v, w);
          testLessThanInt8x16(v, w);
          testLessThanOrEqualInt8x16(v, w);
          testGreaterThanInt8x16(v, w);
          testGreaterThanOrEqualInt8x16(v, w);
      }
  }

  var Int16x8val = [
      Int16x8(1, 2, 3, 4, 5, 6, 7, 8),
      Int16x8(-1, -2, -3, -4, -5, -6, -7, -8),
      Int16x8(-1, 2, -3, 4, -5, 6, -7, 8),
      Int16x8(1, -2, 3, -4, 5, -6, 7, -8),
      Int16x8(INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN, INT16_MIN + 1, INT16_MAX - 1, -7, -8),
      Int16x8(INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX - 1, INT16_MIN + 1, 7, -8)
  ];

  for (v of Int16x8val) {
      for (w of Int16x8val) {
          testEqualInt16x8(v, w);
          testNotEqualInt16x8(v, w);
          testLessThanInt16x8(v, w);
          testLessThanOrEqualInt16x8(v, w);
          testGreaterThanInt16x8(v, w);
          testGreaterThanOrEqualInt16x8(v, w);
      }
  }

  var Int32x4val = [
      Int32x4(1, 2, 3, 4),
      Int32x4(-1, -2, -3, -4),
      Int32x4(-1, 2, -3, 4),
      Int32x4(1, -2, 3, -4),
      Int32x4(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN),
      Int32x4(INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN)
  ];

  for (v of Int32x4val) {
      for (w of Int32x4val) {
          testEqualInt32x4(v, w);
          testNotEqualInt32x4(v, w);
          testLessThanInt32x4(v, w);
          testLessThanOrEqualInt32x4(v, w);
          testGreaterThanInt32x4(v, w);
          testGreaterThanOrEqualInt32x4(v, w);
      }
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);
}

test();
