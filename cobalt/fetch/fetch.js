// ==ClosureCompiler==
// @output_file_name fetch.js
// @compilation_level SIMPLE_OPTIMIZATIONS
// @language_out ES5_STRICT
// ==/ClosureCompiler==

// Copyright (c) 2014-2016 GitHub, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// Contact GitHub API Training Shop Blog About

// Fetch API: https://fetch.spec.whatwg.org/

(function(self) {
  'use strict';

  if (self.fetch) {
    return
  }

  const Array = self.Array
  const ArrayBuffer = self.ArrayBuffer
  const create = self.Object.create
  const defineProperties = self.Object.defineProperties
  const defineProperty = self.Object.defineProperty
  const Error = self.Error
  const Symbol = self.Symbol
  const Symbol_iterator = Symbol.iterator
  const Map = self.Map
  const RangeError = self.RangeError
  const TypeError = self.TypeError
  const Uint8Array = self.Uint8Array

  const Promise = self.Promise

  const ReadableStream = self.ReadableStream
  const ReadableStreamTee = self.ReadableStreamTee
  const IsReadableStreamDisturbed = self.IsReadableStreamDisturbed
  const IsReadableStreamLocked = self.IsReadableStreamLocked

  const ABORT_ERROR = 'AbortError'
  const ABORT_MESSAGE = 'Aborted'

  const ERROR_INVALID_HEADERS_INIT =
      'Constructing Headers with invalid parameters'
  const ERROR_NETWORK_REQUEST_FAILED = 'Network request failed'

  // Internal slots for objects.
  const BODY_SLOT = Symbol('body')
  const BODY_USED_SLOT = Symbol('bodyUsed')
  const CACHE_SLOT = Symbol('cache')
  const CREDENTIALS_SLOT = Symbol('credentials')
  const GUARD_CALLBACK_SLOT = Symbol('guardCallback')
  const HEADERS_SLOT = Symbol('headers')
  const INTEGRITY_SLOT = Symbol('integrity')
  const MAP_SLOT = Symbol('map')
  const METHOD_SLOT = Symbol('method')
  const MODE_SLOT = Symbol('mode')
  const OK_SLOT = Symbol('ok')
  const REDIRECT_SLOT = Symbol('redirect')
  const STATUS_SLOT = Symbol('status')
  const STATUS_TEXT_SLOT = Symbol('statusText')
  const TYPE_SLOT = Symbol('type')
  const URL_SLOT = Symbol('url')
  const IS_ABORTED_SLOT = Symbol('is_aborted')
  const SIGNAL_SLOT = Symbol('signal')

  // Forbidden headers corresponding to various header guard types.
  const INVALID_HEADERS_REQUEST = [
    'accept-charset',
    'accept-encoding',
    'access-control-request-headers',
    'access-control-request-method',
    'connection',
    'content-length',
    'cookie',
    'cookie2',
    'date',
    'dnt',
    'expect',
    'host',
    'keep-alive',
    'origin',
    'referer',    // [sic]
    'te',
    'trailer',
    'transfer-encoding',
    'upgrade',
    'via'
  ]

  const INVALID_HEADERS_RESPONSE = [
    'set-cookie',
    'set-cookie2'
  ]

  // The header guard for no-cors requests is special. Only these headers are
  // allowed. And only certain values for content-type are allowed.
  const VALID_HEADERS_NOCORS = [
    'accept',
    'accept-language',
    'content-language'
    // 'content-type' is treated specially.
  ]

  const VALID_HEADERS_NOCORS_CONTENT_TYPE = [
    'application/x-www-form-urlencoded',
    'multipart/form-data',
    'text/plain'
  ]

  // Values that are allowed for Request cache mode.
  const VALID_CACHE_MODES = [
    'default',
    'no-store',
    'reload',
    'no-cache',
    'force-cache',
    'only-if-cached'
  ]

  // Values that are allowed for Request credentials.
  const VALID_CREDENTIALS = ['omit', 'same-origin', 'include']

  // HTTP methods whose capitalization should be normalized.
  const VALID_METHODS = ['DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST', 'PUT']

  // Methods that are allowed for Request mode: no-cors.
  const VALID_METHODS_NOCORS = ['GET', 'HEAD', 'POST']

  // Modes that are allowed for RequestInit. Although 'navigate' is a valid
  // request mode, it is not allowed in the RequestInit parameter.
  const VALID_MODES = ['same-origin', 'no-cors', 'cors']

  // Values that are allowed for Request redirect mode.
  const VALID_REDIRECT_MODES = ['follow', 'error', 'manual']

  // Body is not allowed in responses with a null body status.
  const NULL_BODY_STATUSES = [101, 204, 205, 304 ]

  // Statuses that are allowed for Response.redirect.
  const REDIRECT_STATUSES = [301, 302, 303, 307, 308]

  const ARRAY_BUFFER_VIEW_CLASSES = [
    '[object Int8Array]',
    '[object Uint8Array]',
    '[object Uint8ClampedArray]',
    '[object Int16Array]',
    '[object Uint16Array]',
    '[object Int32Array]',
    '[object Uint32Array]',
    '[object Float32Array]',
    '[object Float64Array]'
  ]

  var isArrayBufferView = ArrayBuffer.isView || function(obj) {
    return obj && ARRAY_BUFFER_VIEW_CLASSES.indexOf(
        Object.prototype.toString.call(obj)) > -1
  }

  function normalizeName(name) {
    if (typeof name !== 'string') {
      name = String(name)
    }
    if (/[^a-z0-9\-#$%&'*+.\^_`|~]/i.test(name)) {
      throw new TypeError('Invalid character in header field name')
    }
    return name.toLowerCase()
  }

  function normalizeValue(value) {
    if (typeof value !== 'string') {
      value = String(value)
    }

    // https://fetch.spec.whatwg.org/#concept-header-value
    // The web platform tests don't quite abide by the current spec. Use a
    // permissive approach that passes the tests while following the spirit
    // of the spec. Essentially, leading & trailing HTTP whitespace bytes
    // are trimmed first before checking validity.
    var c, first, last, i, len

    //value = value.replace(/^[ \t\n\r]+|[ \t\n\r]+$/g, '')
    for (first = 0, len = value.length; first < len; first++) {
      c = value.charCodeAt(first)
      if (c !== 9 && c !== 10 && c !== 13 && c !== 32) {
        break
      }
    }
    for (last = value.length - 1; last > first; last--) {
      c = value.charCodeAt(last)
      if (c !== 9 && c !== 10 && c !== 13 && c !== 32) {
        break
      }
    }
    value = value.substring(first, last + 1)

    // Follow the chromium implementation of IsValidHTTPHeaderValue(). This
    // should be updated once the web platform test abides by the fetch spec.
    for (i = 0, len = value.length; i < len; i++) {
      c = value.charCodeAt(i)
      if (c >= 256 || c === 0 || c === 10 || c === 13) {
        throw new TypeError('Invalid character in header field value')
      }
    }
    return value
  }

  // Callbacks to determine whether a given header name & value are forbidden
  // for various header guard types.
  function guardImmutableCallback(name, value) {
    throw new TypeError('Immutable header cannot be modified')
  }

  function guardNoneCallback(name, value) {
    return false
  }

  function guardRequestCallback(name, value) {
    var nameLower = name.toLowerCase()
    if (INVALID_HEADERS_REQUEST.indexOf(nameLower) > -1 ||
        nameLower.startsWith('proxy-') || nameLower.startsWith('sec-')) {
      return true
    }
    return false
  }

  function guardRequestNoCorsCallback(name, value) {
    var nameLower = name.toLowerCase()
    if (VALID_HEADERS_NOCORS.indexOf(nameLower) > -1) {
      return false
    }
    if (nameLower === 'content-type') {
      var mimeType = value.split(';')[0].toLowerCase()
      if (VALID_HEADERS_NOCORS_CONTENT_TYPE.indexOf(mimeType) > -1) {
        return false
      }
    }
    return true
  }

  function guardResponseCallback(name, value) {
    if (INVALID_HEADERS_RESPONSE.indexOf(name.toLowerCase()) > -1) {
      return true
    }
    return false
  }

  // https://fetch.spec.whatwg.org/#headers-class
  function Headers(init) {
    this[MAP_SLOT] = new Map();
    if (this[GUARD_CALLBACK_SLOT] === undefined) {
      this[GUARD_CALLBACK_SLOT] = guardNoneCallback
    }

    if (init === undefined) {
      return
    } else if (init === null || typeof init !== 'object') {
      throw new TypeError(ERROR_INVALID_HEADERS_INIT)
    } else if (init instanceof Headers) {
      // TODO: Use for..of in case |init| has a custom Symbol.iterator.
      // However, this results in the ClosureCompiler polyfilling Symbol, so
      // use forEach until ClosureCompiler supports ES6 output.
      init.forEach(function(value, name) {
        this.append(name, value)
      }, this)
    } else if (Array.isArray(init)) {
      init.forEach(function(header) {
        if (header.length !== 2) {
          throw new TypeError(ERROR_INVALID_HEADERS_INIT)
        }
        this.append(header[0], header[1])
      }, this)
    } else {
      Object.getOwnPropertyNames(init).forEach(function(name) {
        this.append(name, init[name])
      }, this)
    }
  }

  function CreateHeadersWithGuard(headersInit, guardCallback) {
    var headers = create(Headers.prototype)
    headers[GUARD_CALLBACK_SLOT] = guardCallback
    Headers.call(headers, headersInit)
    return headers
  }

  Headers.prototype.append = function(name, value) {
    if (arguments.length !== 2) {
      throw TypeError('Invalid parameters to append')
    }
    name = normalizeName(name)
    value = normalizeValue(value)
    if (this[GUARD_CALLBACK_SLOT](name, value)) {
      return
    }
    if (this[MAP_SLOT].has(name)) {
      this[MAP_SLOT].set(name, this[MAP_SLOT].get(name) + ', ' + value)
    } else {
      this[MAP_SLOT].set(name, value)
    }
  }

  Headers.prototype['delete'] = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to delete')
    }
    if (this[GUARD_CALLBACK_SLOT](name, 'invalid')) {
      return
    }
    this[MAP_SLOT].delete(normalizeName(name))
  }

  Headers.prototype.get = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to get')
    }
    name = normalizeName(name)
    var value = this[MAP_SLOT].get(name)
    return value !== undefined ? value : null
  }

  Headers.prototype.has = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to has')
    }
    return this[MAP_SLOT].has(normalizeName(name))
  }

  Headers.prototype.set = function(name, value) {
    if (arguments.length !== 2) {
      throw TypeError('Invalid parameters to set')
    }
    name = normalizeName(name)
    value = normalizeValue(value)
    if (this[GUARD_CALLBACK_SLOT](name, value)) {
      return
    }
    this[MAP_SLOT].set(name, value)
  }

  Headers.prototype.forEach = function(callback, thisArg) {
    var sorted_array = Array.from(this[MAP_SLOT].entries()).sort()
    var that = this
    sorted_array.forEach(function(value) {
      callback.call(thisArg, value[1], value[0], that)
    })
  }

  Headers.prototype.keys = function() {
    var sorted_map = new Map(Array.from(this[MAP_SLOT].entries()).sort())
    return sorted_map.keys()
  }

  Headers.prototype.values = function() {
    var sorted_map = new Map(Array.from(this[MAP_SLOT].entries()).sort())
    return sorted_map.values()
  }

  Headers.prototype.entries = function() {
    var sorted_map = new Map(Array.from(this[MAP_SLOT].entries()).sort())
    return sorted_map.entries()
  }

  Headers.prototype[Symbol_iterator] = Headers.prototype.entries

  function consumeBodyAsUint8Array(body) {
    if (body.bodyUsed) {
      return Promise.reject(new TypeError('Body was already read'))
    }

    if (body.body === null) {
      return Promise.resolve(new Uint8Array(0))
    }

    if (IsReadableStreamLocked(body.body)) {
      return Promise.reject(new TypeError('ReadableStream was already locked'))
    }

    var reader = body.body.getReader()
    var results = []
    var resultsLength = 0
    return reader.read().then(function addResult(result) {
      if (result.done) {
        var data
        if (results.length === 0) {
          data = new Uint8Array(0)
        } else if (results.length === 1) {
          // Create a view of the ArrayBuffer. No copy is made.
          data = new Uint8Array(results[0].buffer)
        } else {
          data = new Uint8Array(resultsLength)
          for (var i = 0, len = results.length, offset = 0; i < len; i++) {
            data.set(results[i], offset)
            offset += results[i].length
          }
        }
        return data
      } else if (result.value instanceof Uint8Array) {
        resultsLength += result.value.length
        results.push(result.value)
        return reader.read().then(addResult)
      } else {
        return Promise.reject(new TypeError('Invalid stream data type'))
      }
    })
  }

  // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
  function extractBody(controller, data, errorString) {
    // Copy the incoming data. This is to prevent subsequent changes to |data|
    // from impacting the Body's contents, and also to prevent changes to the
    // Body's contents from impacting the original data.
    if (!data) {
    } else if (typeof data === 'string') {
      controller.enqueue(FetchInternal.encodeToUTF8(data))
    } else if (ArrayBuffer.prototype.isPrototypeOf(data)) {
      controller.enqueue(new Uint8Array(data.slice(0)))
    } else if (isArrayBufferView(data)) {
      // View as bytes
      const asBytes = new Uint8Array(data.buffer);
      // slice and copy
      var byteSlice = Uint8Array.from(asBytes.slice(data.byteOffset, data.byteLength + 1));
      controller.enqueue(byteSlice);
    } else if (data instanceof Blob) {
      controller.enqueue(new Uint8Array(FetchInternal.blobToArrayBuffer(data)))
    } else {
      throw new TypeError(errorString)
    }
  }

  // https://fetch.spec.whatwg.org/#body-mixin
  // However, our engine does not fully support URLSearchParams, FormData, nor
  //   Blob types. So only support text(), arrayBuffer(), and json().
  function Body() {
    this._initBody = function(body) {
      this[BODY_USED_SLOT] = false
      if (body === null || body === undefined) {
        this[BODY_SLOT] = null
      } else if (body instanceof ReadableStream) {
        this[BODY_SLOT] = body
      } else {
        this[BODY_SLOT] = new ReadableStream({
          start(controller) {
            extractBody(controller, body, 'Unsupported BodyInit type')
            controller.close()
          }
        })
      }

      if (!this[HEADERS_SLOT].get('content-type')) {
        if (typeof body === 'string') {
          this[HEADERS_SLOT].set('content-type', 'text/plain;charset=UTF-8')
        } else if (body instanceof Blob && body.type !== "") {
          this[HEADERS_SLOT].set('content-type', body.type)
        }
      }
    }

    defineProperties(this, {
      'body': { get: function() { return this[BODY_SLOT] } },
      'bodyUsed': {
        get: function() {
          if (this[BODY_USED_SLOT]) {
            return true
          } else if (this[BODY_SLOT]) {
            return !!IsReadableStreamDisturbed(this[BODY_SLOT])
          } else {
            return false
          }
        }
      }
    })

    this.arrayBuffer = function() {
      if (this[IS_ABORTED_SLOT]) {
        return Promise.reject(new DOMException(ABORT_MESSAGE, ABORT_ERROR))
      }
      return consumeBodyAsUint8Array(this).then(function(data) {
        return data.buffer
      })
    }

    this.text = function() {
      if (this[IS_ABORTED_SLOT]) {
        return Promise.reject(new DOMException(ABORT_MESSAGE, ABORT_ERROR))
      }
      return consumeBodyAsUint8Array(this).then(function(data) {
        return FetchInternal.decodeFromUTF8(data)
      })
    }

    this.json = function() {
      if (this[IS_ABORTED_SLOT]) {
        return Promise.reject(new DOMException(ABORT_MESSAGE, ABORT_ERROR))
      }
      return this.text().then(JSON.parse)
    }

    return this
  }

  function normalizeMethod(method) {
    var upcased = method.toUpperCase()
    if (VALID_METHODS.indexOf(upcased) === -1) {
      throw new TypeError('Invalid request method')
    }
    return upcased
  }

  // https://fetch.spec.whatwg.org/#request-class
  function Request(input, init) {
    // When cloning a request, |init| will have the non-standard member
    // |cloneBody|. This signals that the "!hasInit" path should be used.
    var hasInit = init !== undefined && init !== null &&
                  init.cloneBody === undefined
    init = init || {}
    var body = init.body || init.cloneBody
    if(init.body === '') body = '';
    var headersInit = init.headers

    // AbortSignal cannot be constructed directly, so create a temporary
    // AbortController and steal its signal. This signal is only used to follow
    // another signal, so the controller isn't needed.
    var tempController = new AbortController()
    this[SIGNAL_SLOT] = tempController.signal

    // Let signal be null.
    var signal = null

    if (input instanceof Request) {
      this[URL_SLOT] = input.url
      this[CACHE_SLOT] = input.cache
      this[CREDENTIALS_SLOT] = input.credentials
      if (headersInit === undefined) {
        headersInit = input.headers
      }
      this[INTEGRITY_SLOT] = input.integrity
      this[METHOD_SLOT] = input.method
      this[MODE_SLOT] = input.mode
      if (hasInit && this[MODE_SLOT] === 'navigate') {
        this[MODE_SLOT] = 'same-origin'
      }
      this[REDIRECT_SLOT] = input.redirect
      if (!body && input.body !== null) {
        // Take ownership of the stream and mark |input| as disturbed.
        body = input.body
        input[BODY_USED_SLOT] = true
      }

      // Set signal to input's signal.
      signal = input[SIGNAL_SLOT]
    } else {
      this[URL_SLOT] = String(input)
      if (!FetchInternal.isUrlValid(this[URL_SLOT],
                                    false /* allowCredentials */)) {
        throw new TypeError('Invalid request URL')
      }
      this[MODE_SLOT] = 'cors'
      this[CREDENTIALS_SLOT] = 'same-origin'
    }

    if (init.window !== undefined && init.window !== null) {
      throw new TypeError('Invalid request window')
    }

    this[CACHE_SLOT] = init.cache || this[CACHE_SLOT] || 'default'
    if (VALID_CACHE_MODES.indexOf(this[CACHE_SLOT]) === -1) {
      throw new TypeError('Invalid request cache mode')
    }

    this[CREDENTIALS_SLOT] = init.credentials || this[CREDENTIALS_SLOT] ||
                             'same-origin'
    if (VALID_CREDENTIALS.indexOf(this[CREDENTIALS_SLOT]) === -1) {
      throw new TypeError('Invalid request credentials')
    }

    if (init.integrity !== undefined) {
      this[INTEGRITY_SLOT] = init.integrity
    } else if (this[INTEGRITY_SLOT] === undefined) {
      this[INTEGRITY_SLOT] = ''
    }
    this[METHOD_SLOT] = normalizeMethod(init.method || this[METHOD_SLOT] ||
                                        'GET')

    if (init.mode && VALID_MODES.indexOf(init.mode) === -1) {
      throw new TypeError('Invalid request mode')
    }
    this[MODE_SLOT] = init.mode || this[MODE_SLOT] || 'no-cors'
    if (this[MODE_SLOT] === 'no-cors') {
      if (VALID_METHODS_NOCORS.indexOf(this[METHOD_SLOT]) === -1) {
        throw new TypeError('Invalid request method for no-cors')
      }
      if (this[INTEGRITY_SLOT] !== '') {
        throw new TypeError(
            'Request integrity data is not allowed with no-cors')
      }
    }
    if (this[MODE_SLOT] !== 'same-origin' &&
        this[CACHE_SLOT] === 'only-if-cached') {
      throw new TypeError(
          'Request mode must be same-origin for only-if-cached')
    }

    this[REDIRECT_SLOT] = init.redirect || this[REDIRECT_SLOT] || 'follow'
    if (VALID_REDIRECT_MODES.indexOf(this[REDIRECT_SLOT]) === -1) {
      throw new TypeError('Invalid request redirect mode')
    }

    if (this[MODE_SLOT] === 'no-cors') {
      this[HEADERS_SLOT] =
          CreateHeadersWithGuard(headersInit, guardRequestNoCorsCallback)
    } else {
      this[HEADERS_SLOT] =
          CreateHeadersWithGuard(headersInit, guardRequestCallback)
    }

    if ((this[METHOD_SLOT] === 'GET' || this[METHOD_SLOT] === 'HEAD') && body) {
      throw new TypeError('Request body is not allowed for GET or HEAD')
    }

    // If init["signal"] exists, then set signal to it.
    if ('signal' in init) {
      signal = init.signal
    }

    // If signal is not null, then make r's signal follow signal.
    if (signal) {
      this[SIGNAL_SLOT].follow(signal)
    }

    this._initBody(body)
  }

  Request.prototype.clone = function() {
    var cloneBody = null
    if (this[BODY_SLOT] !== null) {
      var streams = ReadableStreamTee(this[BODY_SLOT],
                                      true /* cloneForBranch2 */)
      this[BODY_SLOT] = streams[0]
      cloneBody = streams[1]
    }
    return new Request(this, { cloneBody: cloneBody, signal: this[SIGNAL_SLOT] })
  }

  defineProperties(Request.prototype, {
    'cache': { get: function() { return this[CACHE_SLOT] } },
    'credentials': { get: function() { return this[CREDENTIALS_SLOT] } },
    'headers': { get: function() { return this[HEADERS_SLOT] } },
    'integrity': { get: function() { return this[INTEGRITY_SLOT] } },
    'method': { get: function() { return this[METHOD_SLOT] } },
    'mode': { get: function() { return this[MODE_SLOT] } },
    'redirect': { get: function() { return this[REDIRECT_SLOT] } },
    'url': { get: function() { return this[URL_SLOT] } },
    'signal': { get: function() { return this[SIGNAL_SLOT] } }
  })

  function parseHeaders(rawHeaders, guardCallback) {
    var headers = CreateHeadersWithGuard(undefined, guardCallback)
    // Replace instances of \r\n and \n followed by at least one space or
    // horizontal tab with a space.
    // https://tools.ietf.org/html/rfc7230#section-3.2
    var preProcessedHeaders = rawHeaders.replace(/\r?\n[\t ]+/g, ' ')
    preProcessedHeaders.split(/\r?\n/).forEach(function(line) {
      var parts = line.split(':')
      var key = parts.shift().trim()
      if (key) {
        var value = parts.join(':').trim()
        headers.append(key, value)
      }
    })
    return headers
  }

  Body.call(Request.prototype)

  // Status text must be a reason-phrase token.
  // https://tools.ietf.org/html/rfc7230#section-3.1.2
  function parseStatusText(text) {
    for (var i = 0, len = text.length, c; i < len; i++) {
      c = text.charCodeAt(i)
      if (c !== 9 && (c < 32 || c > 255 || c === 127)) {
        throw TypeError('Invalid response status text')
      }
    }
    return text
  }

  // https://fetch.spec.whatwg.org/#response-class
  function Response(body, init) {
    if (!init) {
      init = {}
    }

    this[TYPE_SLOT] = 'default'
    this[STATUS_SLOT] = 'status' in init ? init.status : 200
    if (this[STATUS_SLOT] < 200 || this[STATUS_SLOT] > 599) {
      throw new RangeError('Invalid response status')
    }
    this[OK_SLOT] = this[STATUS_SLOT] >= 200 && this[STATUS_SLOT] < 300
    this[STATUS_TEXT_SLOT] = 'statusText' in init ?
                             parseStatusText(init.statusText) : 'OK'
    this[HEADERS_SLOT] =
        CreateHeadersWithGuard(init.headers, guardResponseCallback)
    this[URL_SLOT] = init.url || ''
    if (body && NULL_BODY_STATUSES.indexOf(this[STATUS_SLOT]) > -1) {
      throw new TypeError(
          'Response body is not allowed with a null body status')
    }
    this[IS_ABORTED_SLOT] = init.is_aborted || false
    this._initBody(body)
  }

  Body.call(Response.prototype)

  Response.prototype.clone = function() {
    var cloneBody = null
    if (this[BODY_SLOT] !== null) {
      var streams = ReadableStreamTee(this[BODY_SLOT],
                                      true /* cloneForBranch2 */)
      this[BODY_SLOT] = streams[0]
      cloneBody = streams[1]
    }
    return new Response(cloneBody, {
      status: this[STATUS_SLOT],
      statusText: this[STATUS_TEXT_SLOT],
      headers: CreateHeadersWithGuard(this[HEADERS_SLOT],
                                      guardResponseCallback),
      url: this[URL_SLOT],
      is_aborted: this[IS_ABORTED_SLOT]
    })
  }

  defineProperties(Response.prototype, {
    'headers': { get: function() { return this[HEADERS_SLOT] } },
    'ok': { get: function() { return this[OK_SLOT] } },
    'status': { get: function() { return this[STATUS_SLOT] } },
    'statusText': { get: function() { return this[STATUS_TEXT_SLOT] } },
    'type': { get: function() { return this[TYPE_SLOT] } },
    'url': { get: function() { return this[URL_SLOT] } }
  })

  Response.error = function() {
    var response = new Response(null)
    response[HEADERS_SLOT][GUARD_CALLBACK_SLOT] = guardImmutableCallback
    response[TYPE_SLOT] = 'error'
    response[STATUS_SLOT] = 0
    response[STATUS_TEXT_SLOT] = ''
    return response
  }

  Response.redirect = function(url, status) {
    if (!FetchInternal.isUrlValid(url, true /* allowCredentials */)) {
      throw new TypeError('Invalid URL for response redirect')
    }
    if (status === undefined) {
      status = 302
    }
    if (REDIRECT_STATUSES.indexOf(status) === -1) {
      throw new RangeError('Invalid status code for response redirect')
    }

    return new Response(null, {status: status, headers: {location: url}})
  }

  self.Headers = Headers
  self.Request = Request
  self.Response = Response

  // Algorithm steps for https://fetch.spec.whatwg.org/#fetch-method
  self.fetch = function(input, init) {
    // 1 Let p be a new promise.
    // ..
    // 10. Return p
    return new Promise(function(resolve, reject) {
      var cancelled = false
      var isCORSMode = false
      // 2. Let requestObject be the result of invoking the initial value of Request
      //    as constructor
      // 3.  Let request be requestObjects request.
      var request = new Request(input, init)
      var xhr = new XMLHttpRequest()
      var responseStreamController = null

      // 4. If requestObjects signals aborted flag is set, then:
      //    Abort fetch with p, request, and null.
      //    Return p.
      if (request.signal.aborted) {
        return reject(new DOMException(ABORT_MESSAGE, ABORT_ERROR))
      }

      // 5. If requests clients global object is a ServiceWorkerGlobalScope object,
      //   then set requests service-workers mode to "none".
      //   This step is skipped, as serviceWorkers are not supported

      var responseStream = new ReadableStream({
        start(controller) {
          responseStreamController = controller
        },
        cancel(controller) {
          cancelled = true
          xhr.abort()
        }
      })

      var handleAbort = function() {
        if (!cancelled) {
          cancelled = true
          responseStream.cancel()
          if(responseStreamController) {
            try {
              ReadableStreamDefaultControllerError(responseStreamController,
                  new DOMException(ABORT_MESSAGE, ABORT_ERROR))
            } catch(_) {}
          }
          setTimeout(function() {
              try {
                xhr.abort()
              } catch(_) {}
            }, 0)
        }
      }

      xhr.onload = function() {
        responseStreamController.close()
      }

      xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.HEADERS_RECEIVED) {
          var init = {
            status: xhr.status,
            statusText: xhr.statusText,
            headers: parseHeaders(xhr.getAllResponseHeaders() || '',
                                  guardResponseCallback)
          }
          init.url = 'responseURL' in xhr ?
                     xhr.responseURL : init.headers.get('X-Request-URL')
          try {
            // 6. Let responseObject be a new Response object
            var response = new Response(responseStream, init)
            // 7. Let locallyAborted be false - done in response constructor

            request[SIGNAL_SLOT].addEventListener('abort',() => {
              // 8.1 Set locallyAborted to true.
              // 8.2 Abort fetch with p, request, and responseObject.
              // 8.3 Terminate the ongoing fetch with the aborted flag set.
              response[IS_ABORTED_SLOT] = true
              handleAbort()
              reject(new DOMException(ABORT_MESSAGE, ABORT_ERROR))
            })

            response[TYPE_SLOT] = isCORSMode ? 'cors' : 'basic'
            resolve(response)
          } catch (err) {
            reject(err)
          }
        }
      }

      // 9. Run the following in parallel:
      //    Fetch request.
      //    To process response for response, run these substeps:
      //    9.1 If locallyAborted is true, terminate these substeps
      //    9.2 If responses aborted flag is set, then abort fetch
      //       - Above are handled in responseBody methods
      xhr.onerror = function() {
        // 9.3 If response is a network error, then reject p with a TypeError and terminate these substeps.
        responseStreamController.error(
            new TypeError(ERROR_NETWORK_REQUEST_FAILED))
        reject(new TypeError(ERROR_NETWORK_REQUEST_FAILED))
      }

      xhr.ontimeout = function() {
        responseStreamController.error(
            new TypeError(ERROR_NETWORK_REQUEST_FAILED))
        reject(new TypeError(ERROR_NETWORK_REQUEST_FAILED))
      }

      xhr.open(request.method, request.url, true)

      if (request.credentials === 'include') {
        xhr.withCredentials = true
      }

      request.headers.forEach(function(value, name) {
        xhr.setRequestHeader(name, value)
      })

      var fetchUpdate = function(data) {
        if (!cancelled) {
          // Data is already a Uint8Array. Enqueue a reference to this, rather
          // than make a copy of it, since caller no longer references it.
          responseStreamController.enqueue(data)
        }
      }

      var modeUpdate = function(iscorsmode) {
        isCORSMode = iscorsmode
      }

      if (request.body === null) {
        xhr.fetch(fetchUpdate, modeUpdate, null)
      } else {
        consumeBodyAsUint8Array(request).then(function(data) {
          xhr.fetch(fetchUpdate, modeUpdate, data)
        })
      }
    })
  }

  self.fetch.polyfill = true
})(this);
