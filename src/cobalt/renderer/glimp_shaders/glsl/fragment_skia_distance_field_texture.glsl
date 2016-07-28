#version 100
#extension GL_OES_standard_derivatives: require
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec2 uTextureSize_Stage0;
varying vec4 vColor;
varying vec2 vtextureCoords_Stage0;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: DistanceFieldTexture
    vec4 texColor = texture2D(uSampler0_Stage0, vtextureCoords_Stage0).aaaa;
    float distance = 7.96875 * (texColor.r - 0.50196078431) + 0.05;
    vec2 uv = vtextureCoords_Stage0;
    vec2 st = uv*uTextureSize_Stage0;
    float afwidth;
    afwidth = 0.7071*dFdx(st.x);
    float val = smoothstep(-afwidth, afwidth, distance);
    output_Stage0 = vec4(val);
  }
  gl_FragColor = (vColor * output_Stage0);
}
