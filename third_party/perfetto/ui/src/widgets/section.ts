// Copyright (C) 2023 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import m from 'mithril';
import {HTMLAttrs} from './common';

export interface SectionAttrs extends HTMLAttrs {
  // The name of the section, displayed in the title bar
  title: string;
}

export class Section implements m.ClassComponent<SectionAttrs> {
  view({attrs, children}: m.CVnode<SectionAttrs>) {
    const {title, ...htmlAttrs} = attrs;
    return m(
      'section.pf-section',
      htmlAttrs,
      m('header', m('h1', title)),
      m('article', children),
    );
  }
}
