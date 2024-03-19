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
import {v4 as uuidv4} from 'uuid';

import {assertExists} from '../base/logging';
import {QueryResponse, runQuery} from '../common/queries';
import {QueryError} from '../common/query_result';
import {
  AddDebugTrackMenu,
  uuidToViewName,
} from '../tracks/debug/add_debug_track_menu';

import {
  addTab,
  BottomTab,
  bottomTabRegistry,
  closeTab,
  NewBottomTabArgs,
} from './bottom_tab';
import {globals} from './globals';
import {QueryTable} from './query_table';
import {Button} from './widgets/button';
import {Popup, PopupPosition} from './widgets/popup';


export function runQueryInNewTab(query: string, title: string, tag?: string) {
  return addTab({
    kind: QueryResultTab.kind,
    tag,
    config: {
      query,
      title,
    },
  });
}

interface QueryResultTabConfig {
  readonly query: string;
  readonly title: string;
  // Optional data to display in this tab instead of fetching it again
  // (e.g. when duplicating an existing tab which already has the data).
  readonly prefetchedResponse?: QueryResponse;
}

export class QueryResultTab extends BottomTab<QueryResultTabConfig> {
  static readonly kind = 'org.perfetto.QueryResultTab';

  queryResponse?: QueryResponse;
  sqlViewName?: string;

  static create(args: NewBottomTabArgs): QueryResultTab {
    return new QueryResultTab(args);
  }

  constructor(args: NewBottomTabArgs) {
    super(args);

    this.initTrack(args);
  }

  async initTrack(args: NewBottomTabArgs) {
    let uuid = '';
    if (this.config.prefetchedResponse !== undefined) {
      this.queryResponse = this.config.prefetchedResponse;
      uuid = args.uuid;
    } else {
      const result = await runQuery(this.config.query, this.engine);
      this.queryResponse = result;
      globals.rafScheduler.scheduleFullRedraw();
      if (result.error !== undefined) {
        return;
      }

      uuid = uuidv4();
    }

    if (uuid !== '') {
      this.sqlViewName = await this.createViewForDebugTrack(uuid);
      if (this.sqlViewName) {
        globals.rafScheduler.scheduleFullRedraw();
      }
    }
  }

  getTitle(): string {
    const suffix =
        this.queryResponse ? ` (${this.queryResponse.rows.length})` : '';
    return `${this.config.title}${suffix}`;
  }

  viewTab(): m.Child {
    return m(QueryTable, {
      query: this.config.query,
      resp: this.queryResponse,
      onClose: () => closeTab(this.uuid),
      contextButtons: [
        this.sqlViewName === undefined ?
            null :
            m(Popup,
              {
                trigger: m(Button, {label: 'Show debug track', minimal: true}),
                position: PopupPosition.Top,
              },
              m(AddDebugTrackMenu, {
                sqlViewName: this.sqlViewName,
                columns: assertExists(this.queryResponse).columns,
                engine: this.engine,
              })),
      ],
    });
  }

  isLoading() {
    return this.queryResponse === undefined;
  }

  renderTabCanvas() {}

  async createViewForDebugTrack(uuid: string): Promise<string> {
    const viewId = uuidToViewName(uuid);
    // Assuming that the query results come from a SELECT query, try creating a
    // view to allow us to reuse it for further queries.
    // TODO(altimin): This should get the actual query that was used to
    // generate the results from the SQL query iterator.
    try {
      const createViewResult = await this.engine.query(
          `create view ${viewId} as ${this.config.query}`);
      if (createViewResult.error()) {
        // If it failed, do nothing.
        return '';
      }
    } catch (e) {
      if (e instanceof QueryError) {
        // If it failed, do nothing.
        return '';
      }
      throw e;
    }
    return viewId;
  }
}

bottomTabRegistry.register(QueryResultTab);
