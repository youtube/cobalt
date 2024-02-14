// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LogReader, parseString, parseVarArgs} from '../logreader.mjs';
import {Profile} from '../profile.mjs';

import {DeoptLogEntry} from './log/deopt.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {Edge, MapLogEntry} from './log/map.mjs';
import {Timeline} from './timeline.mjs';

// ===========================================================================

export class Processor extends LogReader {
  _profile = new Profile();
  _mapTimeline = new Timeline();
  _icTimeline = new Timeline();
  _deoptTimeline = new Timeline();
  _formatPCRegexp = /(.*):[0-9]+:[0-9]+$/;
  MAJOR_VERSION = 7;
  MINOR_VERSION = 6;
  constructor(logString) {
    super();
    this.propertyICParser = [
      parseInt, parseInt, parseInt, parseInt, parseString, parseString,
      parseString, parseString, parseString, parseString
    ];
    this.dispatchTable_ = {
      __proto__: null,
      'code-creation': {
        parsers: [
          parseString, parseInt, parseInt, parseInt, parseInt, parseString,
          parseVarArgs
        ],
        processor: this.processCodeCreation
      },
      'code-deopt': {
        parsers: [
          parseInt, parseInt, parseInt, parseInt, parseInt, parseString,
          parseString, parseString
        ],
        processor: this.processCodeDeopt
      },
      'v8-version': {
        parsers: [
          parseInt,
          parseInt,
        ],
        processor: this.processV8Version
      },
      'script-source': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processScriptSource
      },
      'code-move':
          {parsers: [parseInt, parseInt], processor: this.processCodeMove},
      'code-delete': {parsers: [parseInt], processor: this.processCodeDelete},
      'sfi-move':
          {parsers: [parseInt, parseInt], processor: this.processFunctionMove},
      'map-create':
          {parsers: [parseInt, parseString], processor: this.processMapCreate},
      'map': {
        parsers: [
          parseString, parseInt, parseString, parseString, parseInt, parseInt,
          parseInt, parseString, parseString
        ],
        processor: this.processMap
      },
      'map-details': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processMapDetails
      },
      'LoadGlobalIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'LoadGlobalIC')
      },
      'StoreGlobalIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreGlobalIC')
      },
      'LoadIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'LoadIC')
      },
      'StoreIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreIC')
      },
      'KeyedLoadIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'KeyedLoadIC')
      },
      'KeyedStoreIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'KeyedStoreIC')
      },
      'StoreInArrayLiteralIC': {
        parsers: this.propertyICParser,
        processor: this.processPropertyIC.bind(this, 'StoreInArrayLiteralIC')
      },
    };
    if (logString) this.processString(logString);
  }

  printError(str) {
    console.error(str);
    throw str
  }

  processString(string) {
    let end = string.length;
    let current = 0;
    let next = 0;
    let line;
    let i = 0;
    let entry;
    try {
      while (current < end) {
        next = string.indexOf('\n', current);
        if (next === -1) break;
        i++;
        line = string.substring(current, next);
        current = next + 1;
        this.processLogLine(line);
      }
    } catch (e) {
      console.error(`Error occurred during parsing, trying to continue: ${e}`);
    }
    this.finalize();
  }

  processLogFile(fileName) {
    this.collectEntries = true;
    this.lastLogFileName_ = fileName;
    let i = 1;
    let line;
    try {
      while (line = readline()) {
        this.processLogLine(line);
        i++;
      }
    } catch (e) {
      console.error(
          `Error occurred during parsing line ${i}` +
          ', trying to continue: ' + e);
    }
    this.finalize();
  }

  finalize() {
    // TODO(cbruni): print stats;
    this._mapTimeline.transitions = new Map();
    let id = 0;
    this._mapTimeline.forEach(map => {
      if (map.isRoot()) id = map.finalizeRootMap(id + 1);
      if (map.edge && map.edge.name) {
        const edge = map.edge;
        const list = this._mapTimeline.transitions.get(edge.name);
        if (list === undefined) {
          this._mapTimeline.transitions.set(edge.name, [edge]);
        } else {
          list.push(edge);
        }
      }
    });
  }

  /**
   * Parser for dynamic code optimization state.
   */
  parseState(s) {
    switch (s) {
      case '':
        return Profile.CodeState.COMPILED;
      case '~':
        return Profile.CodeState.OPTIMIZABLE;
      case '*':
        return Profile.CodeState.OPTIMIZED;
    }
    throw new Error(`unknown code state: ${s}`);
  }

  processCodeCreation(type, kind, timestamp, start, size, name, maybe_func) {
    if (maybe_func.length) {
      const funcAddr = parseInt(maybe_func[0]);
      const state = this.parseState(maybe_func[1]);
      this._profile.addFuncCode(
          type, name, timestamp, start, size, funcAddr, state);
    } else {
      this._profile.addCode(type, name, timestamp, start, size);
    }
  }

  processCodeDeopt(
      timestamp, codeSize, instructionStart, inliningId, scriptOffset,
      deoptKind, deoptLocation, deoptReason) {
    this._deoptTimeline.push(new DeoptLogEntry(deoptKind, timestamp));
  }

  processV8Version(majorVersion, minorVersion) {
    if ((majorVersion == this.MAJOR_VERSION &&
         minorVersion <= this.MINOR_VERSION) ||
        (majorVersion < this.MAJOR_VERSION)) {
      window.alert(
          `Unsupported version ${majorVersion}.${minorVersion}. \n` +
          `Please use the matching tool for given the V8 version.`);
    }
  }

  processScriptSource(scriptId, url, source) {
    this._profile.addScriptSource(scriptId, url, source);
  }

  processCodeMove(from, to) {
    this._profile.moveCode(from, to);
  }

  processCodeDelete(start) {
    this._profile.deleteCode(start);
  }

  processFunctionMove(from, to) {
    this._profile.moveFunc(from, to);
  }

  formatName(entry) {
    if (!entry) return '<unknown>';
    let name = entry.func.getName();
    let re = /(.*):[0-9]+:[0-9]+$/;
    let array = re.exec(name);
    if (!array) return name;
    return entry.getState() + array[1];
  }

  processPropertyIC(
      type, pc, time, line, column, old_state, new_state, map, key, modifier,
      slow_reason) {
    let fnName = this.functionName(pc);
    let parts = fnName.split(' ');
    let fileName = parts[parts.length - 1];
    let script = this.getScript(fileName);
    // TODO: Use SourcePosition here directly
    let entry = new IcLogEntry(
        type, fnName, time, line, column, key, old_state, new_state, map,
        slow_reason, script, modifier);
    if (script) {
      entry.sourcePosition = script.addSourcePosition(line, column, entry);
    }
    this._icTimeline.push(entry);
  }

  functionName(pc) {
    let entry = this._profile.findEntry(pc);
    return this.formatName(entry);
  }
  formatPC(pc, line, column) {
    let entry = this._profile.findEntry(pc);
    if (!entry) return '<unknown>'
      if (entry.type === 'Builtin') {
        return entry.name;
      }
    let name = entry.func.getName();
    let array = this._formatPCRegexp.exec(name);
    if (array === null) {
      entry = name;
    } else {
      entry = entry.getState() + array[1];
    }
    return entry + ':' + line + ':' + column;
  }

  processFileName(filePositionLine) {
    if (!filePositionLine.includes(' ')) return;
    // Try to handle urls with file positions: https://foo.bar.com/:17:330"
    filePositionLine = filePositionLine.split(' ');
    let parts = filePositionLine[1].split(':');
    if (parts[0].length <= 5) return parts[0] + ':' + parts[1];
    return parts[1];
  }

  processMap(type, time, from, to, pc, line, column, reason, name) {
    let time_ = parseInt(time);
    if (type === 'Deprecate') return this.deprecateMap(type, time_, from);
    let from_ = this.getExistingMapEntry(from, time_);
    let to_ = this.getExistingMapEntry(to, time_);
    // TODO: use SourcePosition directly.
    let edge = new Edge(type, name, reason, time, from_, to_);
    to_.filePosition = this.formatPC(pc, line, column);
    let fileName = this.processFileName(to_.filePosition);
    // TODO: avoid undefined source positions.
    if (fileName !== undefined) {
      to_.script = this.getScript(fileName);
    }
    if (to_.script) {
      to_.sourcePosition = to_.script.addSourcePosition(line, column, to_)
    }
    edge.finishSetup();
  }

  deprecateMap(type, time, id) {
    this.getExistingMapEntry(id, time).deprecate();
  }

  processMapCreate(time, id) {
    // map-create events might override existing maps if the addresses get
    // recycled. Hence we do not check for existing maps.
    let map = this.createMapEntry(id, time);
  }

  processMapDetails(time, id, string) {
    // TODO(cbruni): fix initial map logging.
    let map = this.getExistingMapEntry(id, time);
    map.description = string;
  }

  createMapEntry(id, time) {
    let map = new MapLogEntry(id, time);
    this._mapTimeline.push(map);
    return map;
  }

  getExistingMapEntry(id, time) {
    if (id === '0x000000000000') return undefined;
    let map = MapLogEntry.get(id, time);
    if (map === undefined) {
      console.error(`No map details provided: id=${id}`);
      // Manually patch in a map to continue running.
      return this.createMapEntry(id, time);
    };
    return map;
  }

  getScript(url) {
    const script = this._profile.getScript(url);
    // TODO create placeholder script for empty urls.
    if (script === undefined) {
      console.error(`Could not find script for url: '${url}'`)
    }
    return script;
  }

  get icTimeline() {
    return this._icTimeline;
  }

  get mapTimeline() {
    return this._mapTimeline;
  }

  get deoptTimeline() {
    return this._deoptTimeline;
  }

  get scripts() {
    return this._profile.scripts_.filter(script => script !== undefined);
  }
}
