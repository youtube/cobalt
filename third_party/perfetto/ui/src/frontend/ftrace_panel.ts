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
import {StringListPatch} from 'src/common/state';

import {assertExists} from '../base/logging';
import {Actions} from '../common/actions';
import {colorForString} from '../common/colorizer';
import {formatTimestamp} from '../common/time';

import {globals} from './globals';
import {Panel} from './panel';
import {
  MultiSelect,
  MultiSelectDiff,
  Option as MultiSelectOption,
} from './widgets/multiselect';
import {PopupPosition} from './widgets/popup';

const ROW_H = 20;
const PAGE_SIZE = 250;

// This class is quite a weird one. The state looks something like this:
//
// view() -> renders the panel from the data, for now we have no idea what size
// the scroll window is going to be so we don't know how many rows to ask for,
// and the number of rendered rows in our state is likely going to be 0 or wrong
//
// oncreate() -> we now know how many rows we need to display and our scroll
// offset. This is where we as our controller to update the rows, which could
// take some time. Record the first and last row we can see. Attach scroll
// handler to the scrolly window here.
//
// onScroll() -> we know the window has been scrolled, we need to see if things
// have changed enough to constitute a redraw.

// Another call to view() can come at any time, as a reusult of the controller
// giving us some data.
//
export class FtracePanel extends Panel<{}> {
  private page: number = 0;
  private pageCount: number = 0;

  view(_: m.CVnode<{}>) {
    return m(
        '.ftrace-panel',
        m(
            '.sticky',
            [
              this.renderRowsLabel(),
              this.renderFilterPanel(),
            ],
            ),
        this.renderRows(),
    );
  }

  private scrollContainer(dom: Element): HTMLElement {
    const el = dom.parentElement!.parentElement!.parentElement;
    return assertExists(el);
  }

  oncreate({dom}: m.CVnodeDOM) {
    const sc = this.scrollContainer(dom);
    sc.addEventListener('scroll', this.onScroll);
    this.recomputeVisibleRowsAndUpdate(sc);
  }

  onupdate({dom}: m.CVnodeDOM) {
    const sc = this.scrollContainer(dom);
    this.recomputeVisibleRowsAndUpdate(sc);
  }

  recomputeVisibleRowsAndUpdate(scrollContainer: HTMLElement) {
    const prevPage = this.page;
    const prevPageCount = this.pageCount;

    const visibleRowOffset = Math.floor(scrollContainer.scrollTop / ROW_H);
    const visibleRowCount = Math.ceil(scrollContainer.clientHeight / ROW_H);

    // Work out which "page" we're on
    this.page = Math.floor(visibleRowOffset / PAGE_SIZE) - 1;
    this.pageCount = Math.ceil(visibleRowCount / PAGE_SIZE) + 2;

    if (this.page !== prevPage || this.pageCount !== prevPageCount) {
      globals.dispatch(Actions.updateFtracePagination({
        offset: Math.max(0, this.page) * PAGE_SIZE,
        count: this.pageCount * PAGE_SIZE,
      }));
    }
  }

  onremove({dom}: m.CVnodeDOM) {
    const sc = this.scrollContainer(dom);
    sc.removeEventListener('scroll', this.onScroll);

    globals.dispatch(Actions.updateFtracePagination({
      offset: 0,
      count: 0,
    }));
  }

  onScroll = (e: Event) => {
    const scrollContainer = e.target as HTMLElement;
    this.recomputeVisibleRowsAndUpdate(scrollContainer);
  };

  onRowOver(ts: number) {
    globals.dispatch(Actions.setHoverCursorTimestamp({ts}));
  }

  onRowOut() {
    globals.dispatch(Actions.setHoverCursorTimestamp({ts: -1}));
  }

  private renderRowsLabel() {
    if (globals.ftracePanelData) {
      const {numEvents} = globals.ftracePanelData;
      return m('.ftrace-rows-label', `Ftrace Events (${numEvents})`);
    } else {
      return m('.ftrace-rows-label', 'Ftrace Rows');
    }
  }

  private renderFilterPanel() {
    if (!globals.ftraceCounters) {
      return null;
    }

    const options: MultiSelectOption[] =
        globals.ftraceCounters.map(({name, count}) => {
          return {
            id: name,
            name: `${name} (${count})`,
            checked: !globals.state.ftraceFilter.excludedNames.some(
                (excluded: string) => excluded === name),
          };
        });

    return m(
        MultiSelect,
        {
          label: 'Filter by name',
          icon: 'filter_list_alt',
          popupPosition: PopupPosition.Top,
          options,
          onChange: (diffs: MultiSelectDiff[]) => {
            const excludedNames: StringListPatch[] = diffs.map(
                ({id, checked}) => [checked ? 'remove' : 'add', id],
            );
            globals.dispatchMultiple([
              Actions.updateFtraceFilter({excludedNames}),
              Actions.requestTrackReload({}),
            ]);
          },
        },
    );
  }

  // Render all the rows including the first title row
  private renderRows() {
    const data = globals.ftracePanelData;
    const rows: m.Children = [];

    rows.push(m(
        `.row`,
        m('.cell.row-header', 'Timestamp'),
        m('.cell.row-header', 'Name'),
        m('.cell.row-header', 'CPU'),
        m('.cell.row-header', 'Process'),
        m('.cell.row-header', 'Args'),
        ));

    if (data) {
      const {events, offset, numEvents} = data;
      for (let i = 0; i < events.length; i++) {
        const {ts, name, cpu, process, args} = events[i];

        const timestamp =
            formatTimestamp(ts / 1e9 - globals.state.traceTime.startSec);

        const rank = i + offset;

        const color = colorForString(name);
        const hsl = `hsl(
          ${color.h},
          ${color.s - 20}%,
          ${Math.min(color.l + 10, 60)}%
        )`;

        rows.push(m(
            `.row`,
            {
              style: {top: `${(rank + 1.0) * ROW_H}px`},
              onmouseover: this.onRowOver.bind(this, ts / 1e9),
              onmouseout: this.onRowOut.bind(this),
            },
            m('.cell', timestamp),
            m('.cell', m('span.colour', {style: {background: hsl}}), name),
            m('.cell', cpu),
            m('.cell', process),
            m('.cell', args),
            ));
      }
      return m('.rows', {style: {height: `${numEvents * ROW_H}px`}}, rows);
    } else {
      return m('.rows', rows);
    }
  }

  renderCanvas() {}
}
