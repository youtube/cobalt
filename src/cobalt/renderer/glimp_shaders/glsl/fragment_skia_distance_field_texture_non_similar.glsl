#version 100
#extension GL_OES_standard_derivatives: require
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec2 uTextureSize_Stage0;
varying vec4 vColor;
varying vec2 vtextureCoords_Stage0;

// This shader is invoked when the fragments are non-similar to the texels,
// which for example will occur if the shader is being non-uniformly scaled or
// sheared.
void main() {
  vec4 output_Stage0;
  {
    // Stage 0: DistanceFieldTexture
    vec4 texColor = texture2D(uSampler0_Stage0, vtextureCoords_Stage0).aaaa;
    float distance = 7.96875 * (texColor.r - 0.50196078431) + 0.05;
    vec2 uv = vtextureCoords_Stage0;
    vec2 st = uv*uTextureSize_Stage0;
    float afwidth;
    vec2 Jdx = dFdx(st);
    vec2 Jdy = dFdy(st);
    vec2 uv_grad;
    uv_grad = normalize(uv);
    vec2 grad = vec2(uv_grad.x*Jdx.x + uv_grad.y*Jdy.x,
                     uv_grad.x*Jdx.y + uv_grad.y*Jdy.y);
    afwidth = 0.7071*length(grad);
    float val = smoothstep(-afwidth, afwidth, distance);
    output_Stage0 = vec4(val);
  }
  gl_FragColor = (vColor * output_Stage0);
}

