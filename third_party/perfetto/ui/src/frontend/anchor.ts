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

interface AnchorAttrs {
  // Optional icon to show at the end of the content.
  icon?: string;
  // Remaining items.
  [htmlAttrs: string]: any;
}

export class Anchor implements m.ClassComponent<AnchorAttrs> {
  view({attrs, children}: m.CVnode<AnchorAttrs>) {
    const {icon, ...htmlAttrs} = attrs;

    return m(
        'a.pf-anchor',
        htmlAttrs,
        icon && m('i.material-icons', icon),
        children,
    );
  }
}
