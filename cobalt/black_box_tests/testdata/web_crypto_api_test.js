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

// Tests borrowed from Web Platform Tests.
// TODO(b/237798928): remove these tests and enable web platform tests. This
//                    requires supporting sending/receiving objects using
//                    |postMessage()|.

const getHmacImportedTestVectors = (() => {
  const plaintext = new Uint8Array([95, 77, 186, 79, 50, 12, 12, 232, 118, 114, 90, 252, 229, 251, 210, 91, 248, 62, 90, 113, 37, 160, 140, 175, 231, 60, 62, 186, 196, 33, 119, 157, 249, 213, 93, 24, 12, 58, 233, 148, 38, 69, 225, 216, 47, 238, 140, 157, 41, 75, 60, 177, 160, 138, 153, 49, 32, 27, 60, 14, 129, 252, 71, 202, 207, 131, 21, 162, 175, 102, 50, 65, 19, 195, 182, 98, 48, 195, 70, 8, 196, 244, 89, 54, 52, 206, 2, 178, 103, 54, 34, 119, 240, 168, 64, 202, 116, 188, 61, 26, 98, 54, 149, 44, 94, 215, 170, 248, 168, 254, 203, 221, 250, 117, 132, 230, 151, 140, 234, 93, 42, 91, 159, 183, 241, 180, 140, 139, 11, 229, 138, 48, 82, 2, 117, 77, 131, 118, 16, 115, 116, 121, 60, 240, 38, 170, 238, 83, 0, 114, 125, 131, 108, 215, 30, 113, 179, 69, 221, 178, 228, 68, 70, 255, 197, 185, 1, 99, 84, 19, 137, 13, 145, 14, 163, 128, 152, 74, 144, 25, 16, 49, 50, 63, 22, 219, 204, 157, 107, 225, 104, 184, 72, 133, 56, 76, 160, 62, 18, 96, 10, 193, 194, 72, 2, 138, 243, 114, 108, 201, 52, 99, 136, 46, 168, 192, 42, 171]);
  const raw = {
    'SHA-1': new Uint8Array([71, 162, 7, 70, 209, 113, 121, 219, 101, 224, 167, 157, 237, 255, 199, 253, 241, 129, 8, 27]),
    'SHA-256': new Uint8Array([229, 136, 236, 8, 17, 70, 61, 118, 114, 65, 223, 16, 116, 180, 122, 228, 7, 27, 81, 242, 206, 54, 83, 123, 166, 156, 205, 195, 253, 194, 183, 168]),
    'SHA-384': new Uint8Array([107, 29, 162, 142, 171, 31, 88, 42, 217, 113, 142, 255, 224, 94, 35, 213, 253, 44, 152, 119, 162, 217, 68, 63, 144, 190, 192, 147, 190, 206, 46, 167, 210, 53, 76, 208, 189, 197, 225, 71, 210, 233, 0, 147, 115, 73, 68, 136]),
    'SHA-512': new Uint8Array([93, 204, 53, 148, 67, 170, 246, 82, 250, 19, 117, 214, 179, 230, 31, 220, 242, 155, 180, 162, 139, 213, 211, 220, 250, 64, 248, 47, 144, 107, 178, 128, 4, 85, 219, 3, 181, 211, 31, 185, 114, 161, 90, 109, 1, 3, 162, 78, 86, 209, 86, 161, 25, 192, 229, 161, 233, 42, 68, 195, 197, 101, 124, 249])
  };
  const signatures = {
    'SHA-1': new Uint8Array([5, 51, 144, 42, 153, 248, 82, 78, 229, 10, 240, 29, 56, 222, 220, 225, 51, 217, 140, 160]),
    'SHA-256': new Uint8Array([133, 164, 12, 234, 46, 7, 140, 40, 39, 163, 149, 63, 251, 102, 194, 123, 41, 26, 71, 43, 13, 112, 160, 0, 11, 69, 216, 35, 128, 62, 235, 84]),
    'SHA-384': new Uint8Array([33, 124, 61, 80, 240, 186, 154, 109, 110, 174, 30, 253, 215, 165, 24, 254, 46, 56, 128, 181, 130, 164, 13, 6, 30, 144, 153, 193, 224, 38, 239, 88, 130, 84, 139, 93, 92, 236, 221, 85, 152, 217, 155, 107, 111, 48, 87, 255]),
    'SHA-512': new Uint8Array([97, 251, 39, 140, 63, 251, 12, 206, 43, 241, 207, 114, 61, 223, 216, 239, 31, 147, 28, 12, 97, 140, 37, 144, 115, 36, 96, 89, 57, 227, 249, 162, 198, 244, 175, 105, 11, 218, 52, 7, 220, 47, 87, 112, 246, 160, 164, 75, 149, 77, 100, 163, 50, 227, 238, 8, 33, 171, 248, 43, 127, 62, 153, 193])
  };
  return key_usage => Object.keys(raw).map(hash =>
    crypto.subtle.importKey(
      'raw',
      raw[hash],
      {name: 'HMAC', hash},
      false,
      key_usage,
    ).then(key => ({
      name: `HMAC with ${hash}`,
      algorithm: {name: 'HMAC', hash},
      key,
      plaintext,
      signature: signatures[hash],
    })));
})();

const copyBuffer = sourceBuffer => {
  const source = new Uint8Array(sourceBuffer);
  const copy = new Uint8Array(sourceBuffer.byteLength)
  for (let i = 0; i < source.byteLength; i++) {
    copy[i] = source[i];
  }
  return copy;
};

const copyAndAlterBuffer = buffer => {
  const copy = copyBuffer(buffer);
  copy[0] = 255 - copy[0];
  return copy;
};

const runSubtleImportKeyVerifySignTests = () => {
  const test_cases = [
    {test_name: 'verification'},
    {
      test_name: 'verification with altered signature after call',
      modify_vector: v => {
        v.signature = copyBuffer(v.signature);
        return v;
      },
    },
    {
      test_name: 'with altered plaintext after call',
      modify_vector: v => {
        v.plaintext = copyBuffer(v.plaintext);
        return v;
      },
    },
    {
      test_name: 'no verify usage',
      key_usage: ['sign'],
      expected_is_verified: null,
      expected_err: err => err.name === 'InvalidAccessError',
    },
    {
      test_name: 'round trip',
      sign: true,
    },
    {
      test_name: 'verification failure due to wrong plaintext',
      modify_vector: v => {
        v.plaintext = copyAndAlterBuffer(v.plaintext);
        return v;
      },
      expected_is_verified: false,
    },
    {
      test_name: 'verification failure due to wrong signature',
      modify_vector: v => {
        v.signature = copyAndAlterBuffer(v.signature);
        return v;
      },
      expected_is_verified: false,
    },
    {
      test_name: 'verification failure due to wrong signature length',
      modify_vector: v => {
        v.signature = v.signature.slice(1);
        return v;
      },
      expected_is_verified: false,
    },
  ];
  return Promise.all(test_cases.flatMap(({
    key_usage=['verify', 'sign'],
    modify_vector=(v => v),
    sign=false,
    expected_is_verified=true,
    expected_err=(err => false),
    test_name,
  }) =>
    getHmacImportedTestVectors(key_usage).map(promise_vector =>
      promise_vector
        .then(modify_vector)
        .then(vector =>
          (sign ? crypto.subtle.sign(vector.algorithm, vector.key, vector.plaintext) : Promise.resolve(vector.signature))
            .then(signature => crypto.subtle.verify(vector.algorithm, vector.key, signature, vector.plaintext))
            .then(is_verified => [is_verified === expected_is_verified, 'is_verified'])
            .catch(err => [expected_err(err), 'error for test ' + vector.name + ': ' + err.message + '\''])
            .then(result => [result[0], vector.name + ' ' + test_name + ': ' + result[1]]))
        .catch(err => [false, test_name + ': importVectorKeys failed'])
    )));
};

const runGetRandomValuesTests = () =>
  Promise.resolve([
    Int8Array,
    Int16Array,
    Int32Array,
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
  ].map(ctor => [crypto.getRandomValues(new ctor(8)).constructor === ctor, `${ctor.name} mismatch`]));

const getAesCtrImportedTestVectors = (() => {
  const plaintext = new Uint8Array([84, 104, 105, 115, 32, 115,
      112, 101, 99, 105, 102, 105, 99, 97, 116, 105, 111, 110,
      32, 100, 101, 115, 99, 114, 105, 98, 101, 115, 32, 97, 32,
      74, 97, 118, 97, 83, 99, 114, 105, 112, 116, 32, 65, 80,
      73, 32, 102, 111, 114, 32, 112, 101, 114, 102, 111, 114,
      109, 105, 110, 103, 32, 98, 97, 115, 105, 99, 32, 99, 114,
      121, 112, 116, 111, 103, 114, 97, 112, 104, 105, 99, 32,
      111, 112, 101, 114, 97, 116, 105, 111, 110, 115, 32, 105,
      110, 32, 119, 101, 98, 32, 97, 112, 112, 108, 105, 99, 97,
      116, 105, 111, 110, 115, 44, 32, 115, 117, 99, 104, 32, 97,
      115, 32, 104, 97, 115, 104, 105, 110, 103, 44, 32, 115,
      105, 103, 110, 97, 116, 117, 114, 101, 32, 103, 101, 110,
      101, 114, 97, 116, 105, 111, 110, 32, 97, 110, 100, 32,
      118, 101, 114, 105, 102, 105, 99, 97, 116, 105, 111, 110,
      44, 32, 97, 110, 100, 32, 101, 110, 99, 114, 121, 112,
      116, 105, 111, 110, 32, 97, 110, 100, 32, 100, 101, 99,
      114, 121, 112, 116, 105, 111, 110, 46, 32, 65, 100, 100,
      105, 116, 105, 111, 110, 97, 108, 108, 121, 44, 32, 105,
      116, 32, 100, 101, 115, 99, 114, 105, 98, 101, 115, 32, 97,
      110, 32, 65, 80, 73, 32, 102, 111, 114, 32, 97, 112, 112,
      108, 105, 99, 97, 116, 105, 111, 110, 115, 32, 116, 111,
      32, 103, 101, 110, 101, 114, 97, 116, 101, 32, 97, 110,
      100, 47, 111, 114, 32, 109, 97, 110, 97, 103, 101, 32, 116,
      104, 101, 32, 107, 101, 121, 105, 110, 103, 32, 109, 97,
      116, 101, 114, 105, 97, 108, 32, 110, 101, 99, 101, 115,
      115, 97, 114, 121, 32, 116, 111, 32, 112, 101, 114, 102,
      111, 114, 109, 32, 116, 104, 101, 115, 101, 32, 111, 112,
      101, 114, 97, 116, 105, 111, 110, 115, 46, 32, 85, 115,
      101, 115, 32, 102, 111, 114, 32, 116, 104, 105, 115, 32,
      65, 80, 73, 32, 114, 97, 110, 103, 101, 32, 102, 114, 111,
      109, 32, 117, 115, 101, 114, 32, 111, 114, 32, 115, 101,
      114, 118, 105, 99, 101, 32, 97, 117, 116, 104, 101, 110,
      116, 105, 99, 97, 116, 105, 111, 110, 44, 32, 100, 111,
      99, 117, 109, 101, 110, 116, 32, 111, 114, 32, 99, 111,
      100, 101, 32, 115, 105, 103, 110, 105, 110, 103, 44, 32,
      97, 110, 100, 32, 116, 104, 101, 32, 99, 111, 110, 102,
      105, 100, 101, 110, 116, 105, 97, 108, 105, 116, 121, 32,
      97, 110, 100, 32, 105, 110, 116, 101, 103, 114, 105, 116,
      121, 32, 111, 102, 32, 99, 111, 109, 109, 117, 110, 105,
      99, 97, 116, 105, 111, 110, 115, 46]);

  // We want some random key bytes of various sizes.
  // These were randomly generated from a script.
  const keyBytes = {
      128: new Uint8Array([222, 192, 212, 252, 191, 60, 71,
          65, 200, 146, 218, 189, 28, 212, 192, 78]),
      192: new Uint8Array([208, 238, 131, 65, 63, 68, 196, 63, 186, 208,
          61, 207, 166, 18, 99, 152, 29, 109, 221, 95, 240, 30, 28, 246]),
      256: new Uint8Array([103, 105, 56, 35, 251, 29, 88, 7, 63, 145, 236,
          233, 204, 58, 249, 16, 229, 83, 38, 22, 164, 210, 123, 19, 235, 123, 116,
          216, 0, 11, 191, 48])
  };

  // AES-CTR needs a 16 byte (128 bit) counter.
  const counter = new Uint8Array([85, 170, 248, 155, 168, 148, 19, 213, 78, 167, 39,
      167, 108, 39, 162, 132]);

  // Results. These were created using the Python cryptography module.

  // AES-CTR produces ciphertext
  const ciphertext = {
    128: new Uint8Array([233, 17, 117, 253, 164, 245, 234, 87, 197, 43, 13, 0, 11, 190, 152, 175, 104, 192, 165, 144, 88, 174, 237, 138, 181, 183, 6, 53, 3, 161, 206, 71, 13, 121, 218, 209, 116, 249, 10, 170, 250, 165, 68, 157, 132, 141, 200, 178, 197, 87, 209, 231, 250, 75, 154, 65, 162, 251, 30, 159, 234, 20, 20, 181, 147, 218, 180, 12, 4, 241, 75, 79, 129, 64, 15, 228, 60, 147, 153, 1, 129, 176, 150, 161, 85, 97, 22, 154, 234, 23, 127, 16, 4, 22, 226, 11, 104, 16, 176, 14, 225, 176, 79, 239, 103, 243, 190, 222, 40, 186, 244, 212, 29, 57, 125, 175, 21, 17, 233, 2, 13, 119, 102, 233, 230, 4, 16, 222, 56, 225, 67, 45, 191, 250, 15, 153, 45, 193, 240, 212, 117, 101, 68, 232, 199, 101, 175, 125, 247, 6, 249, 14, 0, 157, 185, 56, 76, 51, 228, 77, 234, 84, 60, 42, 119, 187, 213, 32, 34, 222, 65, 231, 215, 26, 73, 141, 231, 254, 185, 118, 14, 180, 126, 80, 51, 102, 200, 141, 204, 45, 26, 56, 119, 136, 222, 45, 143, 120, 231, 44, 43, 221, 136, 21, 188, 138, 84, 232, 208, 238, 226, 117, 104, 60, 165, 4, 18, 144, 240, 49, 173, 90, 68, 84, 239, 161, 124, 196, 144, 119, 24, 243, 239, 75, 117, 254, 219, 209, 53, 131, 37, 79, 68, 26, 21, 168, 163, 50, 59, 18, 244, 11, 143, 190, 188, 129, 108, 249, 180, 104, 216, 215, 165, 160, 251, 84, 132, 152, 195, 154, 110, 216, 70, 21, 248, 148, 146, 152, 56, 174, 248, 227, 1, 102, 15, 118, 182, 50, 73, 63, 35, 112, 159, 237, 253, 94, 16, 127, 120, 38, 127, 51, 27, 96, 163, 140, 20, 111, 151, 16, 72, 74, 74, 205, 239, 241, 16, 179, 183, 116, 95, 248, 58, 168, 203, 93, 233, 225, 91, 17, 226, 10, 120, 85, 114, 4, 31, 40, 82, 161, 152, 17, 86, 237, 207, 7, 228, 110, 182, 65, 68, 68, 156, 206, 116, 185, 204, 148, 22, 58, 111, 218, 138, 225, 146, 25, 114, 29, 96, 183, 87, 181, 181, 236, 113, 141, 171, 213, 9, 84, 182, 230, 163, 147, 246, 86, 246, 52, 111, 64, 34, 157, 12, 80, 224, 28, 21, 112, 31, 42, 79, 229, 210, 90, 23, 78, 223, 155, 144, 238, 12, 14, 191, 158, 6, 181, 254, 0, 85, 134, 56, 161, 234, 55, 129, 64, 59, 12, 146, 6, 217, 232, 20, 214, 167, 159, 183, 165, 96, 96, 225, 199, 23, 106, 243, 108, 106, 26, 214, 53, 152, 26, 155, 253, 128, 7, 216, 207, 109, 159, 147, 240, 232, 226, 43, 147, 169, 162, 204, 215, 9, 10, 177, 223, 99, 206, 163, 240, 64]),
    192: new Uint8Array([98, 123, 235, 65, 14, 86, 80, 133, 88, 104, 244, 125, 165, 185, 163, 4, 3, 230, 62, 58, 113, 222, 46, 210, 17, 155, 95, 19, 125, 125, 70, 234, 105, 54, 23, 246, 114, 9, 237, 191, 9, 194, 34, 254, 156, 11, 50, 216, 80, 178, 185, 221, 132, 154, 27, 85, 82, 49, 241, 123, 23, 106, 119, 134, 203, 0, 151, 66, 149, 218, 124, 247, 227, 233, 236, 184, 88, 234, 174, 250, 83, 168, 33, 15, 122, 26, 96, 213, 210, 4, 52, 92, 20, 12, 64, 12, 209, 197, 69, 100, 15, 56, 60, 63, 241, 52, 18, 189, 93, 146, 47, 60, 33, 200, 218, 243, 43, 169, 17, 108, 19, 199, 174, 33, 107, 186, 57, 95, 167, 138, 180, 187, 53, 113, 208, 148, 190, 48, 167, 53, 209, 52, 153, 184, 231, 63, 168, 54, 179, 238, 93, 130, 125, 3, 149, 119, 60, 25, 142, 150, 183, 193, 29, 18, 3, 219, 235, 219, 26, 116, 217, 196, 108, 6, 96, 103, 212, 48, 227, 91, 124, 77, 181, 169, 18, 111, 123, 83, 26, 169, 230, 88, 103, 185, 153, 93, 143, 152, 142, 231, 41, 226, 226, 156, 179, 206, 212, 67, 18, 193, 187, 53, 252, 214, 15, 228, 246, 131, 170, 101, 134, 212, 100, 170, 146, 47, 57, 125, 50, 230, 51, 246, 74, 175, 129, 196, 178, 206, 176, 52, 153, 39, 77, 24, 186, 99, 137, 83, 105, 111, 168, 35, 176, 24, 29, 170, 223, 74, 160, 138, 247, 12, 102, 233, 136, 59, 172, 228, 242, 84, 13, 34, 155, 80, 80, 87, 180, 143, 129, 61, 213, 54, 41, 8, 183, 102, 126, 179, 127, 77, 55, 176, 152, 41, 131, 85, 86, 225, 87, 216, 139, 226, 196, 195, 210, 34, 33, 161, 249, 153, 205, 197, 128, 41, 28, 121, 6, 159, 25, 211, 168, 137, 26, 217, 249, 113, 81, 141, 18, 1, 250, 228, 68, 238, 74, 54, 99, 167, 236, 176, 199, 148, 161, 143, 156, 51, 189, 204, 59, 240, 151, 170, 85, 63, 23, 38, 152, 199, 12, 81, 217, 244, 178, 231, 249, 159, 224, 107, 214, 58, 127, 116, 143, 219, 155, 80, 55, 213, 171, 80, 127, 235, 20, 247, 12, 104, 228, 147, 202, 124, 143, 110, 223, 76, 221, 154, 175, 143, 185, 237, 222, 189, 104, 218, 72, 244, 55, 253, 138, 183, 92, 231, 68, 176, 239, 171, 100, 10, 63, 61, 194, 228, 15, 133, 216, 45, 60, 135, 203, 142, 127, 153, 172, 223, 213, 230, 220, 189, 223, 234, 156, 134, 238, 220, 251, 104, 209, 117, 175, 47, 46, 148, 6, 61, 216, 215, 39, 30, 116, 212, 45, 112, 202, 227, 198, 98, 253, 97, 177, 120, 74, 238, 68, 99, 240, 96, 43, 88, 166]),
    256: new Uint8Array([55, 82, 154, 67, 47, 80, 186, 78, 83, 56, 95, 130, 102, 236, 61, 236, 204, 236, 234, 222, 122, 226, 147, 149, 233, 41, 16, 118, 201, 91, 185, 162, 79, 71, 146, 252, 221, 110, 165, 137, 75, 129, 94, 219, 93, 94, 64, 34, 250, 190, 5, 90, 6, 177, 167, 224, 25, 121, 85, 91, 87, 152, 56, 100, 191, 35, 1, 156, 177, 179, 127, 253, 173, 176, 87, 247, 40, 207, 178, 175, 10, 51, 209, 70, 52, 76, 251, 160, 172, 203, 77, 191, 97, 58, 123, 238, 82, 60, 166, 214, 134, 14, 71, 74, 156, 15, 77, 6, 141, 76, 10, 205, 148, 204, 85, 203, 242, 30, 66, 133, 202, 21, 17, 108, 151, 2, 15, 44, 51, 180, 88, 80, 8, 248, 254, 151, 201, 226, 156, 6, 39, 197, 212, 124, 72, 217, 75, 232, 139, 155, 22, 199, 242, 223, 116, 10, 141, 42, 7, 85, 99, 5, 184, 43, 145, 159, 122, 135, 202, 46, 209, 157, 178, 114, 98, 194, 119, 194, 19, 242, 167, 236, 162, 94, 90, 106, 219, 234, 67, 11, 162, 225, 6, 17, 152, 23, 16, 84, 40, 90, 255, 158, 8, 105, 198, 56, 220, 213, 36, 203, 241, 242, 85, 218, 103, 90, 202, 214, 215, 134, 121, 169, 149, 139, 122, 143, 155, 178, 29, 217, 197, 128, 173, 25, 111, 154, 14, 76, 106, 101, 0, 215, 187, 33, 223, 116, 205, 89, 52, 206, 60, 77, 141, 31, 57, 211, 74, 42, 219, 88, 210, 36, 196, 128, 151, 136, 124, 222, 157, 59, 225, 70, 163, 234, 59, 173, 228, 198, 134, 76, 249, 228, 69, 181, 196, 194, 179, 239, 78, 43, 143, 94, 234, 10, 177, 192, 185, 171, 231, 164, 254, 91, 44, 11, 29, 148, 223, 107, 18, 149, 61, 50, 115, 38, 14, 128, 189, 9, 77, 236, 151, 163, 23, 122, 156, 236, 11, 80, 66, 190, 24, 4, 4, 12, 148, 57, 64, 59, 143, 114, 247, 66, 111, 167, 86, 173, 98, 102, 207, 44, 134, 89, 231, 64, 50, 157, 208, 210, 79, 159, 133, 73, 118, 98, 202, 215, 57, 247, 29, 97, 116, 1, 28, 119, 248, 243, 31, 180, 66, 38, 40, 141, 251, 134, 129, 126, 241, 113, 22, 50, 28, 113, 187, 158, 217, 125, 182, 233, 144, 246, 32, 88, 88, 15, 0, 102, 131, 67, 31, 34, 150, 98, 241, 213, 227, 205, 175, 254, 3, 53, 70, 124, 167, 38, 53, 104, 140, 147, 158, 200, 179, 45, 100, 101, 246, 81, 166, 53, 247, 60, 10, 78, 127, 10, 173, 176, 232, 31, 91, 203, 250, 236, 38, 113, 172, 151, 253, 194, 253, 50, 242, 76, 148, 23, 117, 195, 122, 104, 16, 212, 177, 113, 188, 138, 186, 144, 168, 102, 3])
  };

  const keyLengths = [128, 192, 256];
  return key_usage => {
    const keyLength = keyLengths[0];
    return {
    passing: keyLengths.map(keyLength =>
      crypto.subtle.importKey(
          'raw',
          keyBytes[keyLength],
          {name: 'AES-CTR'},
          false,
          key_usage,
        )
        .then(key => ({
          name: "AES-CTR " + keyLength + "-bit key",
          algorithm: {name: "AES-CTR", counter, length: 64},
          key,
          plaintext,
          result: ciphertext[keyLength]
        }))),
    failing: keyLengths.flatMap(keyLength =>
      [
        {
          name: "AES-CTR " + keyLength + "-bit key, 0-bit counter",
          algorithm: {name: "AES-CTR", counter, length: 0},
          plaintext,
          result: ciphertext[keyLength],
        },
        {
          name: "AES-CTR " + keyLength + "-bit key, 129-bit counter",
          algorithm: {name: "AES-CTR", counter, length: 129},
          plaintext,
          result: ciphertext[keyLength],
        },
      ].map(vector =>
        crypto.subtle.importKey(
          'raw',
          keyBytes[keyLength],
          {name: 'AES-CTR'},
          false,
          key_usage,
        ).then(key => Object.assign(vector, {key}))
      )
    )
  }};
})();

const equalBuffers = (a, b) => {
  if (a.byteLength !== b.byteLength) {
    return false;
  }
  const aBytes = new Uint8Array(a);
  const bBytes = new Uint8Array(b);

  for (let i = 0; i < a.byteLength; i++) {
    if (aBytes[i] !== bBytes[i]) {
      return false;
    }
  }

  return true;
};

const runSubtleEncryptDecrptTests = () => {
  const test_cases = [
    {test_name: 'encrypt'},
    {
      test_name: 'altered plaintext',
      modify_vector: vector => {
        vector.plaintext = copyBuffer(vector.plaintext);
        return vector;
      },
    },
    {
      test_name: 'decrypt',
      encrypt_or_decrypt: 'decrypt',
    },
    {
      test_name: 'decrypt with altered ciphertext',
      modify_vector: vector => {
        vector.result = copyBuffer(vector.result);
        return vector;
      },
      encrypt_or_decrypt: 'decrypt',
    },
    {
      test_name: 'without encrypt usage',
      key_usage: ['decrypt'],
      expected_result: false,
      expected_err: err => err.name === 'InvalidAccessError',
    },
    {
      test_name: 'mismatched key and algorithm',
      modify_vector: vector => {
        vector.algorithm.name = 'AES-CBC';
        vector.algorithm.iv = new Uint8Array(16); // Need syntactically valid parameter to get to error being checked.
        return vector;
      },
      expected_result: false,
      expected_err: err => err.name === 'InvalidAccessError',
    },
    {
      test_name: 'without decrypt usage',
      key_usage: ['encrypt'],
      encrypt_or_decrypt: 'decrypt',
      expected_result: false,
      expected_err: err => err.name === 'InvalidAccessError',
    },
    {
      test_name: 'OperationError due to data lengths',
      vector_type: 'failing',
      expected_result: false,
      expected_err: err => err.name === 'OperationError',
    },
    {
      test_name: 'OperationError due to data lengths for decryption',
      vector_type: 'failing',
      encrypt_or_decrypt: 'decrypt',
      expected_result: false,
      expected_err: err => err.name === 'OperationError',
    },
  ];
  return Promise.all([test_cases].flatMap(({
    key_usage=['encrypt', 'decrypt'],
    modify_vector=(v => v),
    vector_type='passing',
    encrypt_or_decrypt='encrypt',
    expected_result=true,
    expected_err=(err => false),
    test_name,
  }) =>
    getAesCtrImportedTestVectors(key_usage)[vector_type].map(promiseVector =>
      promiseVector
        .then(modify_vector)
        .then(vector =>
          (encrypt_or_decrypt === 'encrypt' ?
            crypto.subtle.encrypt(vector.algorithm, vector.key, vector.plaintext)
              .then(result => [equalBuffers(result, vector.result) === expected_result, 'Should return expected result'])
              .catch(err => [expected_err(err), 'encrypt error for test ' + vector.name + ': ' + err.message]) :
            crypto.subtle.decrypt(vector.algorithm, vector.key, vector.result)
              .then(plaintext => [equalBuffers(plaintext, vector.plaintext) === expected_result, 'Should return expected result'])
              .catch(err => [expected_err(err), 'decrypt error for test ' + vector.name + ': ' + err.message]))
            .then(result => [result[0], vector.name + ' ' + test_name + ': ' + result[1]])
          .catch(err => [false, 'importKey failed for ' + vector.name])))));
};

const postResults = (promise_results, suite_name) =>
  promise_results
    .then(results => {
      const failures = results.filter(([status, message]) => !status).map(([status, message]) => message);
      if (failures.length === 0) {
        postMessage('pass');
      } else {
        postMessage('\n' + `Failures for ${suite_name}:` + '\n' + failures.join('\n'));
      }
    })
    .catch(err => postMessage(err + ': ' + err.message));

onmessage = event => {
  if (event.data === 'crypto_defined') {
    postResults(Promise.resolve([
      [!!crypto, 'crypto is not defined.'],
      [!!crypto.subtle, 'crypto.subtle is not defined.'],
      [!!crypto.subtle.importKey, 'crypto.subtle.importKey is not defined.'],
      [!!crypto.subtle.sign, 'crypto.subtle.sign is not defined.'],
      [!!crypto.subtle.verify, 'crypto.subtle.verify is not defined.'],
      [!!crypto.getRandomValues, 'crypto.getRandomValues is not defined.'],
    ]), event.data);
  } else if (event.data === 'subtle_encrypt_decrypt') {
    postResults(runSubtleEncryptDecrptTests(), event.data);
  } else if (event.data === 'subtle_importKey_verify_sign') {
    postResults(runSubtleImportKeyVerifySignTests(), event.data);
  } else if (event.data === 'getRandomValues') {
    postResults(runGetRandomValuesTests(), event.data);
  } else {
    postMessage(`Unexpected message received: ${event.data}.`);
  }
};
