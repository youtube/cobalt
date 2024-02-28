// Adds compile-time JS functions to augment the CanvasKit interface.
// Specifically, anything that should only be on the GPU version of canvaskit.
// Functions in this file are supplemented by cpu.js.
(function(CanvasKit){
    CanvasKit._extraInitializations = CanvasKit._extraInitializations || [];
    CanvasKit._extraInitializations.push(function() {
      function get(obj, attr, defaultValue) {
        if (obj && obj.hasOwnProperty(attr)) {
          return obj[attr];
        }
        return defaultValue;
      }

      CanvasKit.GetWebGLContext = function(canvas, attrs) {
        if (!canvas) {
          throw 'null canvas passed into makeWebGLContext';
        }
        var contextAttributes = {
          'alpha': get(attrs, 'alpha', 1),
          'depth': get(attrs, 'depth', 1),
          'stencil': get(attrs, 'stencil', 8),
          'antialias': get(attrs, 'antialias', 0),
          'premultipliedAlpha': get(attrs, 'premultipliedAlpha', 1),
          'preserveDrawingBuffer': get(attrs, 'preserveDrawingBuffer', 0),
          'preferLowPowerToHighPerformance': get(attrs, 'preferLowPowerToHighPerformance', 0),
          'failIfMajorPerformanceCaveat': get(attrs, 'failIfMajorPerformanceCaveat', 0),
          'enableExtensionsByDefault': get(attrs, 'enableExtensionsByDefault', 1),
          'explicitSwapControl': get(attrs, 'explicitSwapControl', 0),
          'renderViaOffscreenBackBuffer': get(attrs, 'renderViaOffscreenBackBuffer', 0),
        };

        if (attrs && attrs['majorVersion']) {
          contextAttributes['majorVersion'] = attrs['majorVersion']
        } else {
          // Default to WebGL 2 if available and not specified.
          contextAttributes['majorVersion'] = (typeof WebGL2RenderingContext !== 'undefined') ? 2 : 1;
        }

        // This check is from the emscripten version
        if (contextAttributes['explicitSwapControl']) {
          throw 'explicitSwapControl is not supported';
        }
        // Creates a WebGL context and sets it to be the current context.
        // These functions are defined in emscripten's library_webgl.js
        var handle = GL.createContext(canvas, contextAttributes);
        if (!handle) {
          return 0;
        }
        GL.makeContextCurrent(handle);
        return handle;
      };

      CanvasKit.deleteContext = function(handle) {
        GL.deleteContext(handle);
      };

      CanvasKit._setTextureCleanup({
        'deleteTexture': function(webglHandle, texHandle) {
          var tex = GL.textures[texHandle];
          if (tex) {
            GL.getContext(webglHandle).GLctx.deleteTexture(tex);
          }
          GL.textures[texHandle] = null;
        },
      });

      CanvasKit.MakeGrContext = function(ctx) {
        // Make sure we are pointing at the right WebGL context.
        if (!this.setCurrentContext(ctx)) {
          return null;
        }
        var grCtx = this._MakeGrContext();
        if (!grCtx) {
          return null;
        }
        // This context is an index into the emscripten-provided GL wrapper.
        grCtx._context = ctx;
        return grCtx;
      }

      CanvasKit.MakeOnScreenGLSurface = function(grCtx, w, h, colorspace) {
        var surface = this._MakeOnScreenGLSurface(grCtx, w, h, colorspace);
        if (!surface) {
          return null;
        }
        surface._context = grCtx._context;
        return surface;
      }

      CanvasKit.MakeRenderTarget = function(grCtx, w, h) {
        var surface = this._MakeRenderTargetWH(grCtx, w, h);
        if (!surface) {
          return null;
        }
        surface._context = grCtx._context;
        return surface;
      }

      CanvasKit.MakeRenderTarget = function(grCtx, imageInfo) {
        var surface = this._MakeRenderTargetII(grCtx, imageInfo);
        if (!surface) {
          return null;
        }
        surface._context = grCtx._context;
        return surface;
      }

      // idOrElement can be of types:
      //  - String - in which case it is interpreted as an id of a
      //          canvas element.
      //  - HTMLCanvasElement - in which the provided canvas element will
      //          be used directly.
      // colorSpace - sk_sp<ColorSpace> - one of the supported color spaces:
      //          CanvasKit.ColorSpace.SRGB
      //          CanvasKit.ColorSpace.DISPLAY_P3
      //          CanvasKit.ColorSpace.ADOBE_RGB
      CanvasKit.MakeWebGLCanvasSurface = function(idOrElement, colorSpace, attrs) {
        colorSpace = colorSpace || null;
        var canvas = idOrElement;
        var isHTMLCanvas = typeof HTMLCanvasElement !== 'undefined' && canvas instanceof HTMLCanvasElement;
        var isOffscreenCanvas = typeof OffscreenCanvas !== 'undefined' && canvas instanceof OffscreenCanvas;
        if (!isHTMLCanvas && !isOffscreenCanvas) {
          canvas = document.getElementById(idOrElement);
          if (!canvas) {
            throw 'Canvas with id ' + idOrElement + ' was not found';
          }
        }

        var ctx = this.GetWebGLContext(canvas, attrs);
        if (!ctx || ctx < 0) {
          throw 'failed to create webgl context: err ' + ctx;
        }

        var grcontext = this.MakeGrContext(ctx);

        // Note that canvas.width/height here is used because it gives the size of the buffer we're
        // rendering into. This may not be the same size the element is displayed on the page, which
        // constrolled by css, and available in canvas.clientWidth/height.
        var surface = this.MakeOnScreenGLSurface(grcontext, canvas.width, canvas.height, colorSpace);
        if (!surface) {
          Debug('falling back from GPU implementation to a SW based one');
          // we need to throw away the old canvas (which was locked to
          // a webGL context) and create a new one so we can
          var newCanvas = canvas.cloneNode(true);
          var parent = canvas.parentNode;
          parent.replaceChild(newCanvas, canvas);
          // add a class so the user can detect that it was replaced.
          newCanvas.classList.add('ck-replaced');

          return CanvasKit.MakeSWCanvasSurface(newCanvas);
        }
        return surface;
      };
      // Default to trying WebGL first.
      CanvasKit.MakeCanvasSurface = CanvasKit.MakeWebGLCanvasSurface;

      function pushTexture(tex) {
        // GL is an emscripten object that holds onto WebGL state. One item in that state is
        // an array of textures, of which the index is the handle/id.
        var texHandle = GL.textures.length;
        if (!texHandle) {
          // If our texture handle is 0, Skia interprets that as an invalid texture id.
          // As a special case, we push a null texture there so the first texture has id 1.
          GL.textures.push(null);
          texHandle = 1;
        }
        GL.textures.push(tex);
        return texHandle
      }

      CanvasKit.Surface.prototype.makeImageFromTexture = function(tex, info) {
        CanvasKit.setCurrentContext(this._context);
        var texHandle = pushTexture(tex);
        return this._makeImageFromTexture(this._context, texHandle, info);
      };

      // We try to find the natural media type (for <img> and <video>), display* for
      // https://developer.mozilla.org/en-US/docs/Web/API/VideoFrame and then fall back to
      // the height and width (to cover <canvas>, ImageBitmap or ImageData).
      function getHeight(src) {
        return src['naturalHeight'] || src['videoHeight'] || src['displayHeight'] || src['height'];
      }

      function getWidth(src) {
        return src['naturalWidth'] || src['videoWidth'] || src['displayWidth'] || src['width'];
      }

      CanvasKit.Surface.prototype.makeImageFromTextureSource = function(src, info) {
        if (!info) {
          info = {
            'height': getHeight(src),
            'width': getWidth(src),
            'colorType': CanvasKit.ColorType.RGBA_8888,
            'alphaType': CanvasKit.AlphaType.Unpremul,
          };
        }
        if (!info['colorSpace']) {
          info['colorSpace'] = CanvasKit.ColorSpace.SRGB;
        }
        if (info['colorType'] !== CanvasKit.ColorType.RGBA_8888) {
          Debug('colorType currently has no impact on makeImageFromTextureSource');
        }

        // We want to be pointing at the context associated with this surface.
        CanvasKit.setCurrentContext(this._context);
        var glCtx = GL.currentContext.GLctx;
        var newTex = glCtx.createTexture();
        glCtx.bindTexture(glCtx.TEXTURE_2D, newTex);
        if (GL.currentContext.version === 2) {
          glCtx.texImage2D(glCtx.TEXTURE_2D, 0, glCtx.RGBA, info['width'], info['height'], 0, glCtx.RGBA, glCtx.UNSIGNED_BYTE, src);
        } else {
          glCtx.texImage2D(glCtx.TEXTURE_2D, 0, glCtx.RGBA, glCtx.RGBA, glCtx.UNSIGNED_BYTE, src);
        }
        glCtx.bindTexture(glCtx.TEXTURE_2D, null);
        return this.makeImageFromTexture(newTex, info);
      };

      CanvasKit.MakeLazyImageFromTextureSource = function(src, info) {
        if (!info) {
          info = {
            'height': getHeight(src),
            'width': getWidth(src),
            'colorType': CanvasKit.ColorType.RGBA_8888,
            'alphaType': CanvasKit.AlphaType.Unpremul,
          };
        }
        if (!info['colorSpace']) {
          info['colorSpace'] = CanvasKit.ColorSpace.SRGB;
        }
        if (info['colorType'] !== CanvasKit.ColorType.RGBA_8888) {
          Debug('colorType currently has no impact on MakeLazyImageFromTextureSource');
        }

        var callbackObj = {
          'makeTexture': function() {
            // This callback function will make a texture on the current drawing surface (i.e.
            // the current WebGL context). It assumes that Skia is just about to draw the texture
            // to the desired surface, and thus the currentContext is the correct one.
            // This is a lot easier than needing to pass the surface handle from the C++ side here.
            var ctx = GL.currentContext;
            var glCtx = ctx.GLctx;
            var newTex = glCtx.createTexture();
            glCtx.bindTexture(glCtx.TEXTURE_2D, newTex);
            if (ctx.version === 2) {
              glCtx.texImage2D(glCtx.TEXTURE_2D, 0, glCtx.RGBA, info['width'], info['height'], 0, glCtx.RGBA, glCtx.UNSIGNED_BYTE, src);
            } else {
              glCtx.texImage2D(glCtx.TEXTURE_2D, 0, glCtx.RGBA, glCtx.RGBA, glCtx.UNSIGNED_BYTE, src);
            }
            glCtx.bindTexture(glCtx.TEXTURE_2D, null);
            return pushTexture(newTex);
          },
          'freeSrc': function() {
            // This callback will be executed whenever the returned image is deleted. This gives
            // us a chance to free up the src (which we now own). Generally, there's nothing
            // we need to do (we can let JS garbage collection do its thing). The one exception
            // is for https://developer.mozilla.org/en-US/docs/Web/API/VideoFrame, which we should
            // close when we are done.
          },
        }
        if (src.constructor.name === 'VideoFrame') {
          callbackObj['freeSrc'] = function() {
            src.close();
          }
        }
        return CanvasKit.Image._makeFromGenerator(info, callbackObj);
      }

      CanvasKit.setCurrentContext = function(ctx) {
        if (!ctx) {
          return false;
        }
        return GL.makeContextCurrent(ctx);
      };
    });
}(Module)); // When this file is loaded in, the high level object is "Module";
