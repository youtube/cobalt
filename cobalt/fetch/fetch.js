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
  const Error = self.Error
  const Symbol_iterator = self.Symbol.iterator
  const Map = self.Map
  const RangeError = self.RangeError
  const TypeError = self.TypeError
  const Uint8Array = self.Uint8Array

  const Promise = self.Promise
  const Promise_reject = Promise.reject
  const Promise_resolve = Promise.resolve

  const ReadableStream = self.ReadableStream
  const ReadableStreamTee = self.ReadableStreamTee
  const IsReadableStreamDisturbed = self.IsReadableStreamDisturbed
  const IsReadableStreamLocked = self.IsReadableStreamLocked

  const err_InvalidHeadersInit = 'Constructing Headers with invalid parameters'
  const err_NetworkRequestFailed = 'Network request failed'

  const viewClasses = [
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
    return obj && viewClasses.indexOf(Object.prototype.toString.call(obj)) > -1
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

  // https://fetch.spec.whatwg.org/#headers-class
  function Headers(headers) {
    this.map = new Map();

    if (headers === undefined) {
      return
    } else if (headers === null || typeof headers !== 'object') {
      throw new TypeError(err_InvalidHeadersInit)
    } else if (headers instanceof Headers) {
      // Should use for..of in case |headers| has a custom Symbol.iterator.
      // However, this results in the ClosureCompiler polyfilling Symbol.
      headers.forEach(function(value, name) {
        this.append(name, value)
      }, this)
    } else if (Array.isArray(headers)) {
      headers.forEach(function(header) {
        if (header.length !== 2) {
          throw new TypeError(err_InvalidHeadersInit)
        }
        this.append(header[0], header[1])
      }, this)
    } else {
      Object.getOwnPropertyNames(headers).forEach(function(name) {
        this.append(name, headers[name])
      }, this)
    }
  }

  Headers.prototype.append = function(name, value) {
    if (arguments.length !== 2) {
      throw TypeError('Invalid parameters to append')
    }
    name = normalizeName(name)
    value = normalizeValue(value)
    if (this.map.has(name)) {
      this.map.set(name, this.map.get(name) + ', ' + value)
    } else {
      this.map.set(name, value)
    }
  }

  Headers.prototype['delete'] = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to delete')
    }
    this.map.delete(normalizeName(name))
  }

  Headers.prototype.get = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to get')
    }
    name = normalizeName(name)
    var value = this.map.get(name)
    return value !== undefined ? value : null
  }

  Headers.prototype.has = function(name) {
    if (arguments.length !== 1) {
      throw TypeError('Invalid parameters to has')
    }
    return this.map.has(normalizeName(name))
  }

  Headers.prototype.set = function(name, value) {
    if (arguments.length !== 2) {
      throw TypeError('Invalid parameters to set')
    }
    this.map.set(normalizeName(name), normalizeValue(value))
  }

  Headers.prototype.forEach = function(callback, thisArg) {
    var sorted_array = Array.from(this.map.entries()).sort()
    var that = this
    sorted_array.forEach(function(value) {
      callback.call(thisArg, value[1], value[0], that)
    })
  }

  Headers.prototype.keys = function() {
    var sorted_map = new Map(Array.from(this.map.entries()).sort())
    return sorted_map.keys()
  }

  Headers.prototype.values = function() {
    var sorted_map = new Map(Array.from(this.map.entries()).sort())
    return sorted_map.values()
  }

  Headers.prototype.entries = function() {
    var sorted_map = new Map(Array.from(this.map.entries()).sort())
    return sorted_map.entries()
  }

  Headers.prototype[Symbol_iterator] = Headers.prototype.entries

  function consumeBodyAsUint8Array(body) {
    if (body.bodyUsed) {
      return Promise_reject(new TypeError('Body already read'))
    }

    if (body.body === null) {
      return Promise_resolve(new Uint8Array(0))
    }

    if (IsReadableStreamLocked(body.body)) {
      return Promise_reject(new TypeError('ReadableStream already locked'))
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
          data = new Uint8Array(results[0])
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
        return Promise_reject(new TypeError('Invalid stream read value type'))
      }
    })
  }

  function encodeStringToUint8Array(str) {
    // Encode string to UTF-8 then store it in an Uint8Array.
    var utf8 = unescape(encodeURIComponent(str))
    var uint8 = new Uint8Array(utf8.length)
    for (var i = 0, len = utf8.length; i < len; i++) {
      uint8[i] = utf8.charCodeAt(i)
    }
    return uint8
  }

  function decodeStringFromUint8Array(uint8) {
    // Decode string from UTF-8 that is stored in the Uint8Array.
    return decodeURIComponent(escape(String.fromCharCode.apply(null, uint8)))
  }

  // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
  function extractBody(controller, data, errorString) {
    if (!data) {
    } else if (typeof data === 'string') {
      controller.enqueue(encodeStringToUint8Array(data))
    } else if (ArrayBuffer.prototype.isPrototypeOf(data)) {
      controller.enqueue(new Uint8Array(data))
    } else if (isArrayBufferView(data)) {
      controller.enqueue(new Uint8Array(data.buffer))
    } else {
      throw new TypeError(errorString)
    }
  }

  // https://fetch.spec.whatwg.org/#body-mixin
  // However, our engine does not fully support URLSearchParams, FormData, nor
  //   Blob types. So only support text(), arrayBuffer(), and json().
  function Body() {
    this._initBody = function(body) {
      this._bodyUsed = false
      if (body === null || body === undefined) {
        this.body = null
      } else if (body instanceof ReadableStream) {
        this.body = body
      } else {
        this.body = new ReadableStream({
          start(controller) {
            extractBody(controller, body, 'Unsupported BodyInit type')
            controller.close()
          }
        })
      }

      if (!this.headers.get('content-type')) {
        if (typeof body === 'string') {
          this.headers.set('content-type', 'text/plain;charset=UTF-8')
        }
      }
    }

    Object.defineProperty(this, 'bodyUsed', {
      get: function() {
        if (this._bodyUsed) {
          return true
        } else if (this.body) {
          return !!IsReadableStreamDisturbed(this.body)
        } else {
          return false
        }
      }
    })

    this.arrayBuffer = function() {
      return consumeBodyAsUint8Array(this).then(function(data) {
        return data.buffer
      })
    }

    this.text = function() {
      return consumeBodyAsUint8Array(this).then(function(data) {
        if (data.length === 0) {
          return ''
        } else {
          return decodeStringFromUint8Array(data)
        }
      })
    }

    this.json = function() {
      return this.text().then(JSON.parse)
    }

    return this
  }

  // HTTP methods whose capitalization should be normalized
  const methods = ['DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST', 'PUT']

  function normalizeMethod(method) {
    var upcased = method.toUpperCase()
    return (methods.indexOf(upcased) > -1) ? upcased : method
  }

  // https://fetch.spec.whatwg.org/#request-class
  function Request(input, init) {
    init = init || {}
    var body = init.body

    if (input instanceof Request) {
      if (input.bodyUsed) {
        throw new TypeError('Request body already read')
      }
      this.url = input.url
      this.credentials = input.credentials
      if (!init.headers) {
        this.headers = new Headers(input.headers)
      }
      this.method = input.method
      this.mode = input.mode
      if (!body && input.body !== null) {
        // Take ownership of the stream and mark |input| as disturbed.
        body = input.body
        input._bodyUsed = true
      }
    } else {
      this.url = String(input)
    }

    this.credentials = init.credentials || this.credentials || 'omit'
    if (init.headers || !this.headers) {
      this.headers = new Headers(init.headers)
    }
    this.method = normalizeMethod(init.method || this.method || 'GET')
    this.mode = init.mode || this.mode || null
    this.referrer = null

    if ((this.method === 'GET' || this.method === 'HEAD') && body) {
      throw new TypeError('Body not allowed for GET or HEAD requests')
    }
    this._initBody(body)
  }

  Request.prototype.clone = function() {
    var cloneBody = null
    if (this.body !== null) {
      var streams = ReadableStreamTee(this.body, true /* cloneForBranch2 */)
      this.body = streams[0]
      cloneBody = streams[1]
    }
    return new Request(this, { body: cloneBody })
  }

  function parseHeaders(rawHeaders) {
    var headers = new Headers()
    // Replace instances of \r\n and \n followed by at least one space or horizontal tab with a space
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
        throw TypeError('Invalid status text')
      }
    }
    return text
  }

  // Body is not allowed in responses with a null body status.
  const nullBodyStatuses = [ 101, 204, 205, 304 ]

  // https://fetch.spec.whatwg.org/#response-class
  function Response(body, init) {
    if (!init) {
      init = {}
    }

    this.type = 'default'
    this.status = 'status' in init ? init.status : 200
    if (this.status < 200 || this.status > 599) {
      throw new RangeError('Invalid response status')
    }
    this.ok = this.status >= 200 && this.status < 300
    this.statusText = 'statusText' in init ? parseStatusText(init.statusText) : 'OK'
    this.headers = new Headers(init.headers)
    this.url = init.url || ''
    if (body && nullBodyStatuses.indexOf(this.status) > -1) {
      throw new TypeError('Body not allowed with a null body status')
    }
    this._initBody(body)
  }

  Body.call(Response.prototype)

  Response.prototype.clone = function() {
    var cloneBody = null
    if (this.body !== null) {
      var streams = ReadableStreamTee(this.body, true /* cloneForBranch2 */)
      this.body = streams[0]
      cloneBody = streams[1]
    }
    return new Response(cloneBody, {
      status: this.status,
      statusText: this.statusText,
      headers: new Headers(this.headers),
      url: this.url
    })
  }

  Response.error = function() {
    var response = new Response(null)
    response.type = 'error'
    response.status = 0
    response.statusText = ''
    return response
  }

  var redirectStatuses = [301, 302, 303, 307, 308]

  Response.redirect = function(url, status) {
    if (!FetchInternal.IsUrlValid(url)) {
      throw new TypeError('Invalid URL')
    }
    if (status === undefined) {
      status = 302
    }
    if (redirectStatuses.indexOf(status) === -1) {
      throw new RangeError('Invalid status code')
    }

    return new Response(null, {status: status, headers: {location: url}})
  }

  self.Headers = Headers
  self.Request = Request
  self.Response = Response

  self.fetch = function(input, init) {
    return new Promise(function(resolve, reject) {
      var cancelled = false
      var request = new Request(input, init)
      var xhr = new XMLHttpRequest()

      var responseStreamController = null
      var responseStream = new ReadableStream({
        start(controller) {
          responseStreamController = controller
        },
        cancel(controller) {
          cancelled = true
          xhr.abort()
        }
      })

      xhr.onload = function() {
        responseStreamController.close()
      }

      xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.HEADERS_RECEIVED) {
          var init = {
            status: xhr.status,
            statusText: xhr.statusText,
            headers: parseHeaders(xhr.getAllResponseHeaders() || '')
          }
          init.url = 'responseURL' in xhr ? xhr.responseURL : init.headers.get('X-Request-URL')
          resolve(new Response(responseStream, init))
        }
      }

      xhr.onerror = function() {
        responseStreamController.error(new TypeError(err_NetworkRequestFailed))
        reject(new TypeError(err_NetworkRequestFailed))
      }

      xhr.ontimeout = function() {
        responseStreamController.error(new TypeError(err_NetworkRequestFailed))
        reject(new TypeError(err_NetworkRequestFailed))
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
          // Data is already an Uint8Array.
          responseStreamController.enqueue(data)
        }
      }

      if (request.body === null) {
        xhr.fetch(fetchUpdate, null)
      } else {
        consumeBodyAsUint8Array(request).then(function(data) {
          xhr.fetch(fetchUpdate, data)
        })
      }
    })
  }
  self.fetch.polyfill = true
})(this);
