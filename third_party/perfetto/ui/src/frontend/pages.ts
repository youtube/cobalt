// Copyright (C) 2018 The Android Open Source Project
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

import {Actions} from '../common/actions';

import {onClickCopy} from './clipboard';
import {CookieConsent} from './cookie_consent';
import {globals} from './globals';
import {fullscreenModalContainer} from './modal';
import {Sidebar} from './sidebar';
import {Topbar} from './topbar';

function renderPermalink(): m.Children {
  const permalink = globals.state.permalink;
  if (!permalink.requestId || !permalink.hash) return null;
  const url = `${self.location.origin}/#!/?s=${permalink.hash}`;
  const linkProps = {title: 'Click to copy the URL', onclick: onClickCopy(url)};

  return m('.alert-permalink', [
    m('div', 'Permalink: ', m(`a[href=${url}]`, linkProps, url)),
    m('button',
      {
        onclick: () => globals.dispatch(Actions.clearPermalink({})),
      },
      m('i.material-icons.disallow-selection', 'close')),
  ]);
}

class Alerts implements m.ClassComponent {
  view() {
    return m('.alerts', renderPermalink());
  }
}

// Wrap component with common UI elements (nav bar etc).
export function createPage(component: m.Component<PageAttrs>):
    m.Component<PageAttrs> {
  const pageComponent = {
    view({attrs}: m.Vnode<PageAttrs>) {
      const children = [
        m(Sidebar),
        m(Topbar),
        m(Alerts),
        m(component, attrs),
        m(CookieConsent),
        m(fullscreenModalContainer.mithrilComponent),
      ];
      if (globals.state.perfDebug) {
        children.push(m('.perf-stats'));
      }
      return children;
    },
  };

  return pageComponent;
}

export interface PageAttrs {
  subpage?: string;
}
