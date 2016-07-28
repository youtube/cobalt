/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// FIXME: This file is supposed to be automagically generated as an action step
// in the yarr build target.  For some reason it is not being generated.  I
// generated it from third_party/webkit/Source/JavaScriptCore using the command
// line: python create_regex_tables --no-tables RegExpJitTables.h

CharacterClass* digitsCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x30, 0x39));
    return characterClass;
}

CharacterClass* nondigitsCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x00, 0x2f));
    characterClass->m_ranges.append(CharacterRange(0x3a, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0xffff));
    return characterClass;
}

CharacterClass* newlineCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_matches.append(0x0a);
    characterClass->m_matches.append(0x0d);
    characterClass->m_matchesUnicode.append(0x2028);
    characterClass->m_matchesUnicode.append(0x2029);
    return characterClass;
}

CharacterClass* spacesCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x09, 0x0d));
    characterClass->m_matches.append(0x20);
    characterClass->m_matchesUnicode.append(0x00a0);
    characterClass->m_matchesUnicode.append(0x1680);
    characterClass->m_matchesUnicode.append(0x180e);
    characterClass->m_rangesUnicode.append(CharacterRange(0x2000, 0x200a));
    characterClass->m_matchesUnicode.append(0x2028);
    characterClass->m_matchesUnicode.append(0x2029);
    characterClass->m_matchesUnicode.append(0x202f);
    characterClass->m_matchesUnicode.append(0x205f);
    characterClass->m_matchesUnicode.append(0x3000);
    characterClass->m_matchesUnicode.append(0xfeff);
    return characterClass;
}

CharacterClass* nonspacesCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x00, 0x08));
    characterClass->m_ranges.append(CharacterRange(0x0e, 0x1f));
    characterClass->m_ranges.append(CharacterRange(0x21, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0x009f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x00a1, 0x167f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x1681, 0x180d));
    characterClass->m_rangesUnicode.append(CharacterRange(0x180f, 0x1fff));
    characterClass->m_rangesUnicode.append(CharacterRange(0x200b, 0x2027));
    characterClass->m_rangesUnicode.append(CharacterRange(0x202a, 0x202e));
    characterClass->m_rangesUnicode.append(CharacterRange(0x2030, 0x205e));
    characterClass->m_rangesUnicode.append(CharacterRange(0x2060, 0x2fff));
    characterClass->m_rangesUnicode.append(CharacterRange(0x3001, 0xfefe));
    characterClass->m_rangesUnicode.append(CharacterRange(0xff00, 0xffff));
    return characterClass;
}

CharacterClass* nonwordcharCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x00, 0x2f));
    characterClass->m_ranges.append(CharacterRange(0x3a, 0x40));
    characterClass->m_ranges.append(CharacterRange(0x5b, 0x5e));
    characterClass->m_matches.append(0x60);
    characterClass->m_ranges.append(CharacterRange(0x7b, 0x7f));
    characterClass->m_rangesUnicode.append(CharacterRange(0x0080, 0xffff));
    return characterClass;
}

CharacterClass* wordcharCreate()
{
    CharacterClass* characterClass = new CharacterClass(0);
    characterClass->m_ranges.append(CharacterRange(0x30, 0x39));
    characterClass->m_ranges.append(CharacterRange(0x41, 0x5a));
    characterClass->m_matches.append(0x5f);
    characterClass->m_ranges.append(CharacterRange(0x61, 0x7a));
    return characterClass;
}
