// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.require('i18n.input.chrome.ElementType');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.EmojiType');
goog.require('i18n.input.chrome.inputview.SpecNodeName');
goog.require('i18n.input.chrome.inputview.content.util');

(function() {

  var viewIdPrefix = 'emoji-k-';
  var emojiKeysPerPage = 27;
  var util = i18n.input.chrome.inputview.content.util;
  var ElementType = i18n.input.chrome.ElementType;
  var EmojiType = i18n.input.chrome.inputview.EmojiType;
  var SpecNodeName = i18n.input.chrome.inputview.SpecNodeName;
  var Css = i18n.input.chrome.inputview.Css;

  var emojiGroups = [
    // Recent
    [''],

    // Hot
    ['\u2665', '\ud83d\ude02', '\u263a',
      '\u2764', '\ud83d\ude12', '\ud83d\ude0d',
      '\ud83d\udc4c', '\ud83d\ude18', '\ud83d\ude0a',
      '\ud83d\ude14', '\ud83d\ude0f', '\ud83d\ude29',
      '\ud83d\ude01', '\ud83d\ude2d', '\ud83d\ude33',
      '\ud83d\udc95', '\u270c', '\ud83d\udc4d',
      '\ud83d\ude09', '\ud83d\udc81', '\ud83d\ude4c',
      '\ud83d\ude0c', '\ud83d\ude0e', '\ud83d\ude48',
      '\ud83d\ude11', '\ud83d\ude1c', '\ud83d\ude0b',
      '\ud83d\ude1e', '\ud83d\ude4f', '\u270b',
      '\ud83d\ude04', '\ud83d\ude4a', '\ud83d\ude15',
      '\ud83d\ude21', '\ud83d\udc4f', '\ud83d\ude22',
      '\ud83d\ude34', '\ud83d\udc40', '\ud83d\ude10',
      '\ud83d\ude31', '\ud83d\ude2b', '\ud83d\ude1d',
      '\ud83d\udc9c', '\ud83d\udc94', '\ud83d\udc8b',
      '\ud83d\ude03', '\ud83d\ude2a', '\ud83d\ude23',
      '\ud83d\udc99', '\ud83d\ude24', '\ud83d\udc4b',
      '\ud83d\udc4a', '\ud83d\ude37', '\ud83d\ude20',
      '\ud83d\ude16', '\ud83d\ude2c', '\ud83d\udc97',
      '\ud83d\ude45', '\ud83d\ude08', '\ud83d\ude05',
      '\ud83d\udc4e', '\ud83d\ude25', '\ud83d\ude4b',
      '\ud83d\ude06', '\ud83d\ude13', '\ud83d\udcaa',
      '\ud83d\udc96', '\ud83d\ude36', '\ud83d\ude1a',
      '\ud83d\udc9b', '\ud83d\udc9a', '\ud83d\ude1b',
      '\ud83d\udc83', '\ud83d\udc9e', '\ud83d\ude00',
      '\ud83d\ude30', '\u270a', '\ud83d\udca9',
      '\ud83d\udc98', '\u261d'],

    // Emotion
    ['\u263a', '\ud83d\ude0a', '\ud83d\ude00',
     '\ud83d\ude01', '\ud83d\ude02', '\ud83d\ude03',
     '\ud83d\ude04', '\ud83d\ude05', '\ud83d\ude06',
     '\ud83d\ude07', '\ud83d\ude08', '\ud83d\ude09',
     '\ud83d\ude2f', '\ud83d\ude10', '\ud83d\ude11',
     '\ud83d\ude15', '\ud83d\ude20', '\ud83d\ude2c',
     '\ud83d\ude21', '\ud83d\ude22', '\ud83d\ude34',
     '\ud83d\ude2e', '\ud83d\ude23', '\ud83d\ude24',
     '\ud83d\ude25', '\ud83d\ude26', '\ud83d\ude27',
     '\ud83d\ude28', '\ud83d\ude29', '\ud83d\ude30',
     '\ud83d\ude1f', '\ud83d\ude31', '\ud83d\ude32',
     '\ud83d\ude33', '\ud83d\ude35', '\ud83d\ude36',
     '\ud83d\ude37', '\ud83d\ude1e', '\ud83d\ude12',
     '\ud83d\ude0d', '\ud83d\ude1b', '\ud83d\ude1c',
     '\ud83d\ude1d', '\ud83d\ude0b', '\ud83d\ude17',
     '\ud83d\ude19', '\ud83d\ude18', '\ud83d\ude1a',
     '\ud83d\ude0e', '\ud83d\ude2d', '\ud83d\ude0c',
     '\ud83d\ude16', '\ud83d\ude14', '\ud83d\ude2a',
     '\ud83d\ude0f', '\ud83d\ude13', '\ud83d\ude2b',
     '\ud83d\ude4b', '\ud83d\ude4c', '\ud83d\ude4d',
     '\ud83d\ude45', '\ud83d\ude46' , '\ud83d\ude47',
     '\ud83d\ude4e', '\ud83d\ude4f', '\ud83d\ude3a',
     '\ud83d\ude3c', '\ud83d\ude38' , '\ud83d\ude39',
     '\ud83d\ude3b', '\ud83d\ude3d', '\ud83d\ude3f',
     '\ud83d\ude3e', '\ud83d\ude40', '\ud83d\ude48',
     '\ud83d\ude49', '\ud83d\ude4a', '\ud83d\udca9',
     '\ud83d\udc76', '\ud83d\udc66', '\ud83d\udc67',
     '\ud83d\udc68', '\ud83d\udc69', '\ud83d\udc74',
     '\ud83d\udc75', '\ud83d\udc8f', '\ud83d\udc91',
     '\ud83d\udc6a', '\ud83d\udc6b', '\ud83d\udc6c',
     '\ud83d\udc6d', '\ud83d\udc64', '\ud83d\udc65',
     '\ud83d\udc6e', '\ud83d\udc77', '\ud83d\udc81',
     '\ud83d\udc82', '\ud83d\udc6f', '\ud83d\udc70',
     '\ud83d\udc78', '\ud83c\udf85', '\ud83d\udc7c',
     '\ud83d\udc71', '\ud83d\udc72', '\ud83d\udc73',
     '\ud83d\udc83', '\ud83d\udc86', '\ud83d\udc87',
     '\ud83d\udc85', '\ud83d\udc7b', '\ud83d\udc79',
     '\ud83d\udc7a', '\ud83d\udc7d', '\ud83d\udc7e',
     '\ud83d\udc7f', '\ud83d\udc80', '\ud83d\udcaa',
     '\ud83d\udc40', '\ud83d\udc42', '\ud83d\udc43',
     '\ud83d\udc63', '\ud83d\udc44', '\ud83d\udc45',
     '\ud83d\udc8b', '\u2764', '\ud83d\udc99',
     '\ud83d\udc9a', '\ud83d\udc9b', '\ud83d\udc9c',
     '\ud83d\udc93', '\ud83d\udc94', '\ud83d\udc95',
     '\ud83d\udc96', '\ud83d\udc97', '\ud83d\udc98',
     '\ud83d\udc9d', '\ud83d\udc9e', '\ud83d\udc9f',
     '\ud83d\udc4d', '\ud83d\udc4e', '\ud83d\udc4c',
     '\u270a', '\u270c', '\u270b',
     '\ud83d\udc4a', '\u261d', '\ud83d\udc46',
     '\ud83d\udc47', '\ud83d\udc48', '\ud83d\udc49',
     '\ud83d\udc4b', '\ud83d\udc4f', '\ud83d\udc50'],

    // Items
    ['\ud83d\udd30', '\ud83d\udc84', '\ud83d\udc5e',
     '\ud83d\udc5f', '\ud83d\udc51', '\ud83d\udc52',
     '\ud83c\udfa9', '\ud83c\udf93', '\ud83d\udc53',
     '\u231a', '\ud83d\udc54', '\ud83d\udc55',
     '\ud83d\udc56', '\ud83d\udc57', '\ud83d\udc58',
     '\ud83d\udc59', '\ud83d\udc60', '\ud83d\udc61',
     '\ud83d\udc62', '\ud83d\udc5a', '\ud83d\udc5c',
     '\ud83d\udcbc', '\ud83c\udf92', '\ud83d\udc5d',
     '\ud83d\udc5b', '\ud83d\udcb0', '\ud83d\udcb3',
     '\ud83d\udcb2', '\ud83d\udcb5', '\ud83d\udcb4',
     '\ud83d\udcb6', '\ud83d\udcb7', '\ud83d\udcb8',
     '\ud83d\udcb1', '\ud83d\udcb9', '\ud83d\udd2b',
     '\ud83d\udd2a', '\ud83d\udca3', '\ud83d\udc89',
     '\ud83d\udc8a', '\ud83d\udeac', '\ud83d\udd14',
     '\ud83d\udd15', '\ud83d\udeaa', '\ud83d\udd2c',
     '\ud83d\udd2d', '\ud83d\udd2e', '\ud83d\udd26',
     '\ud83d\udd0b', '\ud83d\udd0c', '\ud83d\udcdc',
     '\ud83d\udcd7', '\ud83d\udcd8', '\ud83d\udcd9',
     '\ud83d\udcda', '\ud83d\udcd4', '\ud83d\udcd2',
     '\ud83d\udcd1', '\ud83d\udcd3', '\ud83d\udcd5',
     '\ud83d\udcd6', '\ud83d\udcf0', '\ud83d\udcdb',
     '\ud83c\udf83', '\ud83c\udf84', '\ud83c\udf80',
     '\ud83c\udf81', '\ud83c\udf82', '\ud83c\udf88',
     '\ud83c\udf86', '\ud83c\udf87', '\ud83c\udf89',
     '\ud83c\udf8a', '\ud83c\udf8d', '\ud83c\udf8f',
     '\ud83c\udf8c', '\ud83c\udf90', '\ud83c\udf8b',
     '\ud83c\udf8e', '\ud83d\udcf1', '\ud83d\udcf2',
     '\ud83d\udcdf', '\u260e', '\ud83d\udcde',
     '\ud83d\udce0', '\ud83d\udce6', '\u2709',
     '\ud83d\udce8', '\ud83d\udce9', '\ud83d\udcea',
     '\ud83d\udceb', '\ud83d\udced', '\ud83d\udcec',
     '\ud83d\udcee', '\ud83d\udce4', '\ud83d\udce5',
     '\ud83d\udcef', '\ud83d\udce2', '\ud83d\udce3',
     '\ud83d\udce1', '\ud83d\udcac', '\ud83d\udcad',
     '\u2712', '\u270f', '\ud83d\udcdd',
     '\ud83d\udccf', '\ud83d\udcd0', '\ud83d\udccd',
     '\ud83d\udccc', '\ud83d\udcce', '\u2702',
     '\ud83d\udcba', '\ud83d\udcbb', '\ud83d\udcbd',
     '\ud83d\udcbe', '\ud83d\udcbf', '\ud83d\udcc6',
     '\ud83d\udcc5', '\ud83d\udcc7', '\ud83d\udccb',
     '\ud83d\udcc1', '\ud83d\udcc2', '\ud83d\udcc3',
     '\ud83d\udcc4', '\ud83d\udcca', '\ud83d\udcc8',
     '\ud83d\udcc9', '\u26fa', '\ud83c\udfa1',
     '\ud83c\udfa2', '\ud83c\udfa0', '\ud83c\udfaa',
     '\ud83c\udfa8', '\ud83c\udfac', '\ud83c\udfa5',
     '\ud83d\udcf7', '\ud83d\udcf9', '\ud83c\udfa6',
     '\ud83c\udfad', '\ud83c\udfab', '\ud83c\udfae',
     '\ud83c\udfb2', '\ud83c\udfb0', '\ud83c\udccf',
     '\ud83c\udfb4', '\ud83c\udc04', '\ud83c\udfaf',
     '\ud83d\udcfa', '\ud83d\udcfb', '\ud83d\udcc0',
     '\ud83d\udcfc', '\ud83c\udfa7', '\ud83c\udfa4',
     '\ud83c\udfb5', '\ud83c\udfb6', '\ud83c\udfbc',
     '\ud83c\udfbb', '\ud83c\udfb9', '\ud83c\udfb7',
     '\ud83c\udfba', '\ud83c\udfb8', '\u303d'],

    // Nature
    ['\ud83d\udc15', '\ud83d\udc36', '\ud83d\udc29',
     '\ud83d\udc08', '\ud83d\udc31', '\ud83d\udc00',
     '\ud83d\udc01', '\ud83d\udc2d', '\ud83d\udc39',
     '\ud83d\udc22', '\ud83d\udc07', '\ud83d\udc30',
     '\ud83d\udc13', '\ud83d\udc14', '\ud83d\udc23',
     '\ud83d\udc24', '\ud83d\udc25', '\ud83d\udc26',
     '\ud83d\udc0f', '\ud83d\udc11', '\ud83d\udc10',
     '\ud83d\udc3a', '\ud83d\udc03', '\ud83d\udc02',
     '\ud83d\udc04', '\ud83d\udc2e', '\ud83d\udc34',
     '\ud83d\udc17', '\ud83d\udc16', '\ud83d\udc37',
     '\ud83d\udc3d', '\ud83d\udc38', '\ud83d\udc0d',
     '\ud83d\udc3c', '\ud83d\udc27', '\ud83d\udc18',
     '\ud83d\udc28', '\ud83d\udc12', '\ud83d\udc35',
     '\ud83d\udc06', '\ud83d\udc2f', '\ud83d\udc3b',
     '\ud83d\udc2b', '\ud83d\udc2a', '\ud83d\udc0a',
     '\ud83d\udc33', '\ud83d\udc0b', '\ud83d\udc1f',
     '\ud83d\udc20', '\ud83d\udc21', '\ud83d\udc19',
     '\ud83d\udc1a', '\ud83d\udc2c', '\ud83d\udc0c',
     '\ud83d\udc1b', '\ud83d\udc1c', '\ud83d\udc1d',
     '\ud83d\udc1e', '\ud83d\udc32', '\ud83d\udc09',
     '\ud83d\udc3e', '\ud83c\udf78', '\ud83c\udf7a',
     '\ud83c\udf7b', '\ud83c\udf77', '\ud83c\udf79',
     '\ud83c\udf76', '\u2615', '\ud83c\udf75',
     '\ud83c\udf7c', '\ud83c\udf74', '\ud83c\udf68',
     '\ud83c\udf67', '\ud83c\udf66', '\ud83c\udf69',
     '\ud83c\udf70', '\ud83c\udf6a', '\ud83c\udf6b',
     '\ud83c\udf6c', '\ud83c\udf6d', '\ud83c\udf6e',
     '\ud83c\udf6f', '\ud83c\udf73', '\ud83c\udf54',
     '\ud83c\udf5f', '\ud83c\udf5d', '\ud83c\udf55',
     '\ud83c\udf56', '\ud83c\udf57', '\ud83c\udf64',
     '\ud83c\udf63', '\ud83c\udf71', '\ud83c\udf5e',
     '\ud83c\udf5c', '\ud83c\udf59', '\ud83c\udf5a',
     '\ud83c\udf5b', '\ud83c\udf72', '\ud83c\udf65',
     '\ud83c\udf62', '\ud83c\udf61', '\ud83c\udf58',
     '\ud83c\udf60', '\ud83c\udf4c', '\ud83c\udf4e',
     '\ud83c\udf4f', '\ud83c\udf4a', '\ud83c\udf4b',
     '\ud83c\udf44', '\ud83c\udf45', '\ud83c\udf46',
     '\ud83c\udf47', '\ud83c\udf48', '\ud83c\udf49',
     '\ud83c\udf50', '\ud83c\udf51', '\ud83c\udf52',
     '\ud83c\udf53', '\ud83c\udf4d', '\ud83c\udf30',
     '\ud83c\udf31', '\ud83c\udf32', '\ud83c\udf33',
     '\ud83c\udf34', '\ud83c\udf35', '\ud83c\udf37',
     '\ud83c\udf38', '\ud83c\udf39', '\ud83c\udf40',
     '\ud83c\udf41', '\ud83c\udf42', '\ud83c\udf43',
     '\ud83c\udf3a', '\ud83c\udf3b', '\ud83c\udf3c',
     '\ud83c\udf3d', '\ud83c\udf3e', '\ud83c\udf3f',
     '\u2600', '\ud83c\udf08', '\u26c5',
     '\u2601', '\ud83c\udf01', '\ud83c\udf02',
     '\u2614', '\ud83d\udca7', '\u26a1',
     '\ud83c\udf00', '\u2744', '\u26c4',
     '\ud83c\udf19', '\ud83c\udf1e', '\ud83c\udf1d',
     '\ud83c\udf1a', '\ud83c\udf1b', '\ud83c\udf1c',
     '\ud83c\udf11', '\ud83c\udf12', '\ud83c\udf13',
     '\ud83c\udf14', '\ud83c\udf15', '\ud83c\udf16',
     '\ud83c\udf17', '\ud83c\udf18', '\ud83c\udf91',
     '\ud83c\udf04', '\ud83c\udf05', '\ud83c\udf07',
     '\ud83c\udf06', '\ud83c\udf03', '\ud83c\udf0c',
     '\ud83c\udf09', '\ud83c\udf0a', '\ud83c\udf0b',
     '\ud83c\udf0e', '\ud83c\udf0f', '\ud83c\udf0d',
     '\ud83c\udf10'],

    // Places of interests
    ['\ud83c\udfe0', '\ud83c\udfe1', '\ud83c\udfe2',
     '\ud83c\udfe3', '\ud83c\udfe4', '\ud83c\udfe5',
     '\ud83c\udfe6', '\ud83c\udfe7', '\ud83c\udfe8',
     '\ud83c\udfe9', '\ud83c\udfea', '\ud83c\udfeb',
     '\u26ea', '\u26f2', '\ud83c\udfec',
     '\ud83c\udfef', '\ud83c\udff0', '\ud83c\udfed',
     '\ud83d\uddfb', '\ud83d\uddfc', '\ud83d\uddfd',
     '\ud83d\uddfe', '\ud83d\uddff', '\u2693',
     '\ud83c\udfee', '\ud83d\udc88', '\ud83d\udd27',
     '\ud83d\udd28', '\ud83d\udd29', '\ud83d\udebf',
     '\ud83d\udec1', '\ud83d\udec0', '\ud83d\udebd',
     '\ud83d\udebe', '\ud83c\udfbd', '\ud83c\udfa3',
     '\ud83c\udfb1', '\ud83c\udfb3', '\u26be',
     '\u26f3', '\ud83c\udfbe', '\u26bd',
     '\ud83c\udfbf', '\ud83c\udfc0', '\ud83c\udfc1',
     '\ud83c\udfc2', '\ud83c\udfc3', '\ud83c\udfc4',
     '\ud83c\udfc6', '\ud83c\udfc7', '\ud83d\udc0e',
     '\ud83c\udfc8', '\ud83c\udfc9', '\ud83c\udfca',
     '\ud83d\ude82', '\ud83d\ude83', '\ud83d\ude84',
     '\ud83d\ude85', '\ud83d\ude86', '\ud83d\ude87',
     '\u24c2', '\ud83d\ude88', '\ud83d\ude8a',
     '\ud83d\ude8b', '\ud83d\ude8c', '\ud83d\ude8d',
     '\ud83d\ude8e', '\ud83d\ude8f', '\ud83d\ude90',
     '\ud83d\ude91', '\ud83d\ude92', '\ud83d\ude93',
     '\ud83d\ude94', '\ud83d\ude95', '\ud83d\ude96',
     '\ud83d\ude97', '\ud83d\ude98', '\ud83d\ude99',
     '\ud83d\ude9a', '\ud83d\ude9b', '\ud83d\ude9c',
     '\ud83d\ude9d', '\ud83d\ude9e', '\ud83d\ude9f',
     '\ud83d\udea0', '\ud83d\udea1', '\ud83d\udea2',
     '\ud83d\udea3', '\ud83d\ude81', '\u2708',
     '\ud83d\udec2', '\ud83d\udec3', '\ud83d\udec4',
     '\ud83d\udec5', '\u26f5', '\ud83d\udeb2',
     '\ud83d\udeb3', '\ud83d\udeb4', '\ud83d\udeb5',
     '\ud83d\udeb7', '\ud83d\udeb8', '\ud83d\ude89',
     '\ud83d\ude80', '\ud83d\udea4', '\ud83d\udeb6',
     '\u26fd', '\ud83c\udd7f', '\ud83d\udea5',
     '\ud83d\udea6', '\ud83d\udea7', '\ud83d\udea8',
     '\u2668', '\ud83d\udc8c', '\ud83d\udc8d',
     '\ud83d\udc8e', '\ud83d\udc90', '\ud83d\udc92'],

    // Special characters
    ['\ud83d\udd1d', '\ud83d\udd19', '\ud83d\udd1b',
     '\ud83d\udd1c', '\ud83d\udd1a', '\u23f3',
     '\u231b', '\u23f0', '\u2648',
     '\u2649', '\u264a', '\u264b',
     '\u264c', '\u264d', '\u264e',
     '\u264f', '\u2650', '\u2651',
     '\u2652', '\u2653', '\u26ce',
     '\ud83d\udd31', '\ud83d\udd2f', '\ud83d\udebb',
     '\ud83d\udeae', '\ud83d\udeaf', '\ud83d\udeb0',
     '\ud83d\udeb1', '\ud83c\udd70', '\ud83c\udd71',
     '\ud83c\udd8e', '\ud83c\udd7e', '\ud83d\udcae',
     '\ud83d\udcaf', '\ud83d\udd20', '\ud83d\udd21',
     '\ud83d\udd22', '\ud83d\udd23', '\ud83d\udd24',
     '\u27bf', '\ud83d\udcf6', '\ud83d\udcf3',
     '\ud83d\udcf4', '\ud83d\udcf5', '\ud83d\udeb9',
     '\ud83d\udeba', '\ud83d\udebc', '\u267f',
     '\u267b', '\ud83d\udead', '\ud83d\udea9',
     '\u26a0', '\ud83c\ude01', '\ud83d\udd1e',
     '\u26d4', '\ud83c\udd92', '\ud83c\udd97',
     '\ud83c\udd95', '\ud83c\udd98', '\ud83c\udd99',
     '\ud83c\udd93', '\ud83c\udd96', '\ud83c\udd9a',
     '\ud83c\ude32', '\ud83c\ude33', '\ud83c\ude34',
     '\ud83c\ude35', '\ud83c\ude36', '\ud83c\ude37',
     '\ud83c\ude38', '\ud83c\ude39', '\ud83c\ude02',
     '\ud83c\ude3a', '\ud83c\ude50', '\ud83c\ude51',
     '\u3299', '\u00ae', '\u00a9',
     '\u2122', '\ud83c\ude1a', '\ud83c\ude2f',
     '\u3297', '\u2b55', '\u274c',
     '\u274e', '\u2139', '\ud83d\udeab',
     '\u2705', '\u2714', '\ud83d\udd17',
     '\u2734', '\u2733', '\u2795',
     '\u2796', '\u2716', '\u2797',
     '\ud83d\udca0', '\ud83d\udca1', '\ud83d\udca4',
     '\ud83d\udca2', '\ud83d\udd25', '\ud83d\udca5',
     '\ud83d\udca8', '\ud83d\udca6', '\ud83d\udcab',
     '\ud83d\udd5b', '\ud83d\udd67', '\ud83d\udd50',
     '\ud83d\udd5c', '\ud83d\udd51', '\ud83d\udd5d',
     '\ud83d\udd52', '\ud83d\udd5e', '\ud83d\udd53',
     '\ud83d\udd5f', '\ud83d\udd54', '\ud83d\udd60',
     '\ud83d\udd55', '\ud83d\udd61', '\ud83d\udd56',
     '\ud83d\udd62', '\ud83d\udd57', '\ud83d\udd63',
     '\ud83d\udd58', '\ud83d\udd64', '\ud83d\udd59',
     '\ud83d\udd65', '\ud83d\udd5a', '\ud83d\udd66',
     '\u2195', '\u2b06', '\u2197',
     '\u27a1', '\u2198', '\u2b07',
     '\u2199', '\u2b05', '\u2196',
     '\u2194', '\u2934', '\u2935',
     '\u23ea', '\u23eb', '\u23ec',
     '\u23e9', '\u25c0', '\u25b6',
     '\ud83d\udd3d', '\ud83d\udd3c', '\u2747',
     '\u2728', '\ud83d\udd34', '\ud83d\udd35',
     '\u26aa', '\u26ab', '\ud83d\udd33',
     '\ud83d\udd32', '\u2b50', '\ud83c\udf1f',
     '\ud83c\udf20', '\u25ab', '\u25aa',
     '\u25fd', '\u25fe', '\u25fb',
     '\u25fc', '\u2b1c', '\u2b1b',
     '\ud83d\udd38', '\ud83d\udd39', '\ud83d\udd36',
     '\ud83d\udd37', '\ud83d\udd3a', '\ud83d\udd3b',
     '\u2754', '\u2753', '\u2755',
     '\u2757', '\u203c', '\u2049',
     '\u3030', '\u27b0', '\u2660',
     '\u2665', '\u2663', '\u2666',
     '\ud83c\udd94', '\ud83d\udd11', '\u21a9',
     '\ud83c\udd91', '\ud83d\udd0d', '\ud83d\udd12',
     '\ud83d\udd13', '\u21aa', '\ud83d\udd10',
     '\u2611', '\ud83d\udd18', '\ud83d\udd0e',
     '\ud83d\udd16', '\ud83d\udd0f', '\ud83d\udd03',
     '\ud83d\udd00', '\ud83d\udd01', '\ud83d\udd02',
     '\ud83d\udd04', '\ud83d\udce7', '\ud83d\udd05',
     '\ud83d\udd06', '\ud83d\udd07', '\ud83d\udd08',
     '\ud83d\udd09', '\ud83d\udd0a'],

    // Emoticon
    [':)',
     ';-)',
     ':-D',
     ':P',
     ':-(',
     ':\'(',
     ':-)',
     ':-*',
     ':-$',
     ':-\\',
     ':-[',
     ':-!',
     ':S',
     ':O',
     ':-O',
     'B-)',
     'o_O',
     'O_o',
     '^O^',
     '-.-',
     '^_^',
     '^﹏^',
     '^m^',
     '^/^',
     '~_~',
     '-_-',
     '-_-||',
     '>_<',
     '><',
     '>﹏<',
     '_#',
     '\#_#',
     '*-*',
     '(^^)',
     '(^_^)',
     '(^.^)',
     '(^!^)',
     '(^J^)',
     '(^m^)',
     '(^\'^)',
     '(^_-)',
     '(^O^)',
     '(^o^)',
     '(^q^)',
     '(^○^)',
     '(^O^;)',
     '(^m^;)',
     '(^Q^)',
     '!(^^)!',
     'T_T',
     '(ToT)',
     '(T_T)',
     '\@_\@',
     '=.=',
     '=.=!',
     '=_=',
     '╰_╯',
     '-_-z',
     '^_-',
     '囧rz',
     'Orz',
     '→_→',
     '←_←',
     '≧◇≦',
     '(x_x)',
     '(′o`)',
     '(′ェ`)',
     '(?_?)',
     '(′θ`)',
     '(*_*)',
     '(@@)',
     '⊙▽⊙',
     '⊙△⊙',
     '⊙_⊙',
     '⊙﹏⊙',
     '◑﹏◐',
     '◑︿◐',
     '◑__◐',
     '∩__∩',
     '∩﹏∩',
     '（ˇˍˇ）',
     '(′▽`〃)',
     '(′0ノ`*)',
     '(^_^;)',
     '(@_@)',
     '(*^^*)',
     '(´・ω・`)',
     '(=θωθ=)',
     '（°ο°）',
     '^(oo)^',
     '(#^.^#)',
     '（*^_^*）',
     '(¯(●●)¯)',
     '>"<|||',
     '(′～`；)',
     '(=′?`=)',
     '(○’ω’○)',
     'o(≧o≦)o',
     '(??_??)?',
     '└(^o^)┘',
     '(︶^︶)',
     '(>.<*)',
     '(⊙ｏ⊙)',
     '(⊙﹏⊙)',
     '=^_^=',
     '::>_<::',
     '↖(^ω^)↗',
     '~w_w~']
  ];
  var keyList = [];
  var mapping = {};
  var viewId = 0;
  keyList.push(util.createTabBarKey('Tabbar0', EmojiType.RECENT,
      Css.EMOJI_TABBAR_RECENT));
  mapping['Tabbar0'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar1', EmojiType.HOT,
      Css.EMOJI_TABBAR_HOT));
  mapping['Tabbar1'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar2', EmojiType.EMOTION,
      Css.EMOJI_TABBAR_EMOTION));
  mapping['Tabbar2'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar3', EmojiType.ITEMS,
      Css.EMOJI_TABBAR_ITEMS));
  mapping['Tabbar3'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar4', EmojiType.NATURE,
      Css.EMOJI_TABBAR_NATURE));
  mapping['Tabbar4'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar5', EmojiType.PLACES_OF_INTERESTS,
      Css.EMOJI_TABBAR_PLACES_OF_INTERESTS));
  mapping['Tabbar5'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar6',
      EmojiType.SPECIAL_CHARACTERS,
      Css.EMOJI_TABBAR_SPECIAL_CHARACTERS));
  mapping['Tabbar6'] = viewIdPrefix + viewId++;
  keyList.push(util.createTabBarKey('Tabbar7', EmojiType.EMOTICON,
      Css.EMOJI_TABBAR_EMOTICON));
  mapping['Tabbar7'] = viewIdPrefix + viewId++;

  // Tab bar layout has 10 keys but we only need 8 keys here, skip two keys.
  viewId += 2;
  for (var i = 0, count = 0; i < emojiGroups.length; i++) {
    var pages = Math.ceil(emojiGroups[i].length / emojiKeysPerPage);
    for (var j = 0; j < pages * emojiKeysPerPage; j++) {
      var spec = {};
      spec[SpecNodeName.ID] = 'emojikey' + count;
      spec[SpecNodeName.TYPE] = ElementType.EMOJI_KEY;
      spec[SpecNodeName.TEXT] =
          j < emojiGroups[i].length ? emojiGroups[i][j] : '';
      spec[SpecNodeName.IS_EMOTICON] = (i == EmojiType.EMOTICON);
      var key = i18n.input.chrome.inputview.content.util.createKey(spec);
      mapping[key['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;
      keyList.push(key);
      count++;
    }
  }
  var tmp = util.createBackspaceKey();
  keyList.push(tmp);
  mapping[tmp['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;
  tmp = util.createEnterKey();
  keyList.push(tmp);
  mapping[tmp['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;
  // SideKeys layout has 3 keys but we only need 2 keys here, skip one key.
  viewId++;

  //The space row.
  tmp = util.createBackToKeyboardKey();
  keyList.push(tmp);
  mapping[tmp['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;
  tmp = util.createSpaceKey();
  keyList.push(tmp);
  mapping[tmp['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;
  tmp = util.createHideKeyboardKey();
  keyList.push(tmp);
  mapping[tmp['spec'][SpecNodeName.ID]] = viewIdPrefix + viewId++;

  var result = {};
  result[SpecNodeName.TEXT] = emojiGroups;
  result[SpecNodeName.KEY_LIST] = keyList;
  result[SpecNodeName.MAPPING] = mapping;
  result[SpecNodeName.LAYOUT] = 'emoji';
  result['id'] = 'emoji';
  google.ime.chrome.inputview.onConfigLoaded(result);
}) ();
