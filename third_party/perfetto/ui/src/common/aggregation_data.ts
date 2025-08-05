// Copyright (C) 2019 The Android Open Source Project
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

export type Column = (StringColumn|TimestampColumn|NumberColumn|StateColumn)&
    {title: string, columnId: string};

export interface StringColumn {
  kind: 'STRING';
  data: Uint16Array;
}

export interface TimestampColumn {
  kind: 'TIMESTAMP_NS';
  data: Float64Array;
}

export interface NumberColumn {
  kind: 'NUMBER';
  data: Uint16Array;
}

export interface StateColumn {
  kind: 'STATE';
  data: Uint16Array;
}

type TypedArrayConstructor =
    Uint16ArrayConstructor|Float64ArrayConstructor|Uint32ArrayConstructor;
export interface ColumnDef {
  title: string;
  kind: string;
  sum?: boolean;
  columnConstructor: TypedArrayConstructor;
  columnId: string;
}

export interface AggregateData {
  tabName: string;
  columns: Column[];
  columnSums: string[];
  // For string interning.
  strings: string[];
  // Some aggregations will have extra info to display;
  extra?: ThreadStateExtra;
}

export function isEmptyData(data: AggregateData) {
  return data.columns.length === 0 || data.columns[0].data.length === 0;
}

export interface ThreadStateExtra {
  kind: 'THREAD_STATE';
  states: string[];
  values: Float64Array;
  totalMs: number;
}
